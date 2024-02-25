#include "EventUtils.hh"

#include <event2/event.h>

#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>

static void dispatch_forward_to_event_thread(evutil_socket_t, short, void* ctx) {
  auto* fn = reinterpret_cast<std::function<void()>*>(ctx);
  (*fn)();
  delete fn;
}

void forward_to_event_thread(std::shared_ptr<struct event_base> base, std::function<void()>&& fn) {
  struct timeval tv = {0, 0};
  std::function<void()>* new_fn = new std::function<void()>(std::move(fn));
  event_base_once(base.get(), -1, EV_TIMEOUT, dispatch_forward_to_event_thread, new_fn, &tv);
}

template <>
void call_on_event_thread<void>(std::shared_ptr<struct event_base> base, std::function<void()>&& compute) {
  bool succeeded = false;
  std::string exc_what;
  std::mutex ret_lock;
  std::condition_variable ret_cv;
  std::unique_lock<std::mutex> g(ret_lock);
  forward_to_event_thread(base, [&]() -> void {
    std::lock_guard<std::mutex> g(ret_lock);
    try {
      compute();
      succeeded = true;
    } catch (const std::exception& e) {
      exc_what = e.what();
    }
    ret_cv.notify_one();
  });
  ret_cv.wait(g);
  if (!succeeded) {
    throw std::runtime_error(exc_what);
  }
}

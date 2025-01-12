#include "EventUtils.hh"

#include <event2/buffer.h>
#include <event2/event.h>

#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>

using namespace std;

static void dispatch_forward_to_event_thread(evutil_socket_t, short, void* ctx) {
  auto* fn = reinterpret_cast<function<void()>*>(ctx);
  (*fn)();
  delete fn;
}

void forward_to_event_thread(shared_ptr<struct event_base> base, function<void()>&& fn) {
  struct timeval tv = {0, 0};
  function<void()>* new_fn = new function<void()>(std::move(fn));
  event_base_once(base.get(), -1, EV_TIMEOUT, dispatch_forward_to_event_thread, new_fn, &tv);
}

template <>
void call_on_event_thread<void>(shared_ptr<struct event_base> base, function<void()>&& compute) {
  bool succeeded = false;
  string exc_what;
  mutex ret_lock;
  condition_variable ret_cv;
  unique_lock<mutex> g(ret_lock);
  forward_to_event_thread(base, [&]() -> void {
    lock_guard<mutex> g(ret_lock);
    try {
      compute();
      succeeded = true;
    } catch (const exception& e) {
      exc_what = e.what();
    }
    ret_cv.notify_one();
  });
  ret_cv.wait(g);
  if (!succeeded) {
    throw runtime_error(exc_what);
  }
}

string evbuffer_remove_str(struct evbuffer* buf, ssize_t size) {
  if (!buf) {
    return "";
  }
  if (size < 0) {
    size = static_cast<size_t>(evbuffer_get_length(buf));
  }
  string ret(size, '\0');
  ssize_t bytes_removed = evbuffer_remove(buf, ret.data(), ret.size());
  if (bytes_removed < 0) {
    throw std::runtime_error("can\'t remove data from buffer");
  }
  ret.resize(bytes_removed);
  return ret;
}

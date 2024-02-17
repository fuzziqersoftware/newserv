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

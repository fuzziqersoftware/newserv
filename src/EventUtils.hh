#pragma once

#include <event2/event.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

void forward_to_event_thread(std::shared_ptr<struct event_base> base, std::function<void()>&& fn);

template <typename T>
T call_on_event_thread(std::shared_ptr<struct event_base> base, std::function<T()>&& compute) {
  std::optional<T> ret;
  std::string exc_what;
  std::mutex ret_lock;
  std::condition_variable ret_cv;
  std::unique_lock<std::mutex> g(ret_lock);
  forward_to_event_thread(base, [&]() -> void {
    std::lock_guard<std::mutex> g(ret_lock);
    try {
      ret = compute();
    } catch (const std::exception& e) {
      exc_what = e.what();
    }
    ret_cv.notify_one();
  });
  ret_cv.wait(g);
  if (!ret.has_value()) {
    throw std::runtime_error(exc_what);
  }
  return ret.value();
}

template <>
void call_on_event_thread<void>(std::shared_ptr<struct event_base> base, std::function<void()>&& compute);

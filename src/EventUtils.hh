#pragma once

#include <event2/event.h>

#include <functional>
#include <memory>

void forward_to_event_thread(std::shared_ptr<struct event_base> base, std::function<void()>&& fn);

#include "SignalWatcher.hh"

#include <event2/event.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ReceiveCommands.hh"
#include "SendCommands.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"

using namespace std;

SignalWatcher::SignalWatcher(shared_ptr<ServerState> state)
    : log("[SignalWatcher] "),
      state(state),
      sigusr1_event(evsignal_new(this->state->base.get(), SIGUSR1, &SignalWatcher::dispatch_on_signal, this), event_free),
      sigusr2_event(evsignal_new(this->state->base.get(), SIGUSR2, &SignalWatcher::dispatch_on_signal, this), event_free) {
  event_add(this->sigusr1_event.get(), nullptr);
  event_add(this->sigusr2_event.get(), nullptr);
}

void SignalWatcher::dispatch_on_signal(evutil_socket_t signal, short what, void* ctx) {
  if (what != EV_SIGNAL) {
    throw logic_error("dispatch_on_signal called for non-signal event");
  }
  reinterpret_cast<SignalWatcher*>(ctx)->on_signal(signal);
}

void SignalWatcher::on_signal(int signum) {
  switch (signum) {
    case SIGUSR1:
      this->log.info("Received SIGUSR1; reloading config.json");
      this->state->forward_to_event_thread([s = this->state]() {
        try {
          s->load_config_early();
          s->load_config_late();
          fprintf(stderr, "Configuration update complete\n");
        } catch (const exception& e) {
          fprintf(stderr, "FAILED: %s\n", e.what());
          fprintf(stderr, "Some configuration may have been reloaded. Fix the underlying issue and try again.\n");
        }
      });
      break;
    case SIGUSR2:
      this->log.info("Received SIGUSR2; reloading config.json and all dependencies");
      this->state->forward_to_event_thread([s = this->state]() {
        try {
          s->load_all();
          fprintf(stderr, "Configuration update complete\n");
        } catch (const exception& e) {
          fprintf(stderr, "FAILED: %s\n", e.what());
          fprintf(stderr, "Some configuration may have been reloaded. Fix the underlying issue and try again.\n");
        }
      });
      break;
    default:
      this->log.warning("Unknown signal received: %d", signum);
  }
}

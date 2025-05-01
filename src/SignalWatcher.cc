#include "SignalWatcher.hh"

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
      signals(*this->state->io_context) {
  asio::co_spawn(*this->state->io_context, this->signal_handler_task(), asio::detached);
}

asio::awaitable<void> SignalWatcher::signal_handler_task() {
#ifndef PHOSG_WINDOWS
  this->signals.add(SIGUSR1);
  this->signals.add(SIGUSR2);
  for (;;) {
    int signum = co_await this->signals.async_wait(asio::use_awaitable);
    switch (signum) {
      case SIGUSR1:
        this->log.info_f("Received SIGUSR1; reloading config.json");
        try {
          this->state->load_config_early();
          this->state->load_config_late();
          phosg::fwrite_fmt(stderr, "Configuration update complete\n");
        } catch (const exception& e) {
          phosg::fwrite_fmt(stderr, "FAILED: {}\n", e.what());
          phosg::fwrite_fmt(stderr, "Some configuration may have been reloaded. Fix the underlying issue and try again.\n");
        }
        break;
      case SIGUSR2:
        this->log.info_f("Received SIGUSR2; reloading config.json and all dependencies");
        try {
          this->state->load_all(true);
          phosg::fwrite_fmt(stderr, "Configuration update complete\n");
        } catch (const exception& e) {
          phosg::fwrite_fmt(stderr, "FAILED: {}\n", e.what());
          phosg::fwrite_fmt(stderr, "Some configuration may have been reloaded. Fix the underlying issue and try again.\n");
        }
        break;
      default:
        this->log.warning_f("Unknown signal received: {}", signum);
    }
  }
#endif
  co_return;
}

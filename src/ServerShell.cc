#include "ServerShell.hh"

#include <stdio.h>
#include <string.h>

#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ChatCommands.hh"
#include "ReceiveCommands.hh"
#include "SendCommands.hh"
#include "ServerState.hh"
#include "ShellCommands.hh"
#include "StaticGameData.hh"

using namespace std;

ServerShell::ServerShell(shared_ptr<ServerState> state)
    : state(state),
      th(&ServerShell::thread_fn, this) {}

ServerShell::~ServerShell() {
  if (this->th.joinable()) {
    this->th.join();
  }
}

void ServerShell::thread_fn() {
  for (;;) {
    phosg::fwrite_fmt(stdout, "newserv> ");
    fflush(stdout);
    string command;
    uint64_t read_start_usecs = phosg::now();
    try {
      command = phosg::fgets(stdin);
    } catch (const phosg::io_error& e) {
      // Cygwin sometimes causes fgets() to fail with errno -1 when the
      // terminal window is resized. We ignore these events unless the read
      // failed immediately (which probably means it would fail again if we
      // retried immediately).
      if (phosg::now() - read_start_usecs < 1000000 || e.error != -1) {
        throw;
      }
      phosg::log_warning_f("I/O error reading from terminal: {} ({})", e.what(), e.error);
      continue;
    }

    // If command is empty (not even a \n), it's probably EOF
    if (command.empty()) {
      fputc('\n', stderr);
      this->state->io_context->stop();
      return;
    }

    phosg::strip_trailing_whitespace(command);
    phosg::strip_leading_whitespace(command);

    try {
      std::promise<deque<string>> promise;
      auto future = promise.get_future();

      asio::co_spawn(
          *this->state->io_context,
          [&]() mutable -> asio::awaitable<void> {
            try {
              promise.set_value(co_await ShellCommand::dispatch_str(this->state, command));
            } catch (...) {
              promise.set_exception(std::current_exception());
            }
          },
          asio::detached);

      for (const auto& line : future.get()) {
        phosg::fwrite_fmt(stdout, "{}\n", line);
      }
    } catch (const exit_shell&) {
      this->state->io_context->stop();
      return;
    } catch (const exception& e) {
      phosg::fwrite_fmt(stderr, "FAILED: {}\n", e.what());
    }
  }
}

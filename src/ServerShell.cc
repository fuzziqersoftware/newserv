#include "ServerShell.hh"

#include <event2/event.h>
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
    fprintf(stdout, "newserv> ");
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
      phosg::log_warning("I/O error reading from terminal: %s (%d)", e.what(), e.error);
      continue;
    }

    // If command is empty (not even a \n), it's probably EOF
    if (command.empty()) {
      fputc('\n', stderr);
      event_base_loopexit(this->state->base.get(), nullptr);
      return;
    }

    phosg::strip_trailing_whitespace(command);
    phosg::strip_leading_whitespace(command);

    try {
      auto lines = ShellCommand::dispatch_str(this->state, command);
      for (const auto& line : lines) {
        fprintf(stdout, "%s\n", line.c_str());
      }
    } catch (const exit_shell&) {
      event_base_loopexit(this->state->base.get(), nullptr);
      return;
    } catch (const exception& e) {
      fprintf(stderr, "FAILED: %s\n", e.what());
    }
  }
}

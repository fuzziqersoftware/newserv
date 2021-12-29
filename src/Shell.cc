#include "Shell.hh"

#include <event2/event.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Strings.hh>

using namespace std;



Shell::exit_shell::exit_shell() : runtime_error("shell exited") { }



Shell::Shell(std::shared_ptr<struct event_base> base,
    std::shared_ptr<ServerState> state) : base(base), state(state),
    read_event(event_new(this->base.get(), 0, EV_READ | EV_PERSIST,
      &Shell::dispatch_read_stdin, this), event_free),
    prompt_event(event_new(this->base.get(), 0, EV_TIMEOUT,
      &Shell::dispatch_print_prompt, this), event_free) {
  event_add(this->read_event.get(), NULL);

  // schedule an event to print the prompt as soon as the event loop starts
  // running. we do this so the prompt appears after any initialization
  // messages that come after starting the shell
  struct timeval tv = {0, 0};
  event_add(this->prompt_event.get(), &tv);

  this->poll.add(0, POLLIN);
}

void Shell::dispatch_print_prompt(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Shell*>(ctx)->print_prompt();
}

void Shell::print_prompt() {
  // default behavior: no prompt
}

void Shell::dispatch_read_stdin(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Shell*>(ctx)->read_stdin();
}

void Shell::read_stdin() {
  bool any_command_read = false;
  for (;;) {
    auto poll_result = this->poll.poll();
    short fd_events = 0;
    try {
      fd_events = poll_result.at(0);
    } catch (const out_of_range&) { }

    if (!(fd_events & POLLIN)) {
      break;
    }

    string command(2048, '\0');
    if (!fgets(const_cast<char*>(command.data()), command.size(), stdin)) {
      if (!any_command_read) {
        // ctrl+d probably; we should exit
        fputc('\n', stderr);
        event_base_loopexit(this->base.get(), NULL);
        return;
      } else {
        break; // probably not EOF; just no more commands for now
      }
    }

    // trim the extra data off the string
    size_t len = strlen(command.c_str());
    if (len == 0) {
      break;
    }
    if (command[len - 1] == '\n') {
      len--;
    }
    command.resize(len);
    any_command_read = true;

    try {
      execute_command(command);
    } catch (const exit_shell&) {
      event_base_loopexit(this->base.get(), NULL);
      return;
    } catch (const exception& e) {
      fprintf(stderr, "FAILED: %s\n", e.what());
    }
  }

  this->print_prompt();
}

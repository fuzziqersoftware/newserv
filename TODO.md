## General

- Find a way to silence audio in RunDOL.s
- Encapsulate BB server-side random state and make replays deterministic
- Write a simple status API
- Implement per-game logging
- Build an exception-handling abstraction in ChatCommands that shows formatted error messages in all cases
- Make reloading happen on separate threads so compression doesn't block active clients
- Implement decrypt/encrypt actions for VMS files
- Make UI strings localizable (e.g. entries in menus, welcome message, etc.)
- Figure out what causes the corruption message on PC proxy sessions and fix it
- Make $edit for DC/PC
- Add an idle connection timeout for proxy sessions

## Episode 3

- Enforce tournament deck restrictions (e.g. rank checks, No Assist option) when populating COMs at tournament start time
- Add support for recording battles on the proxy server (both in primary and spectator teams)
- Make `reload licenses` not vulnerable to online players' licenses overwriting licenses on disk somehow
- Implement ranks (based on total Meseta earned)

## PSO XBOX

- Fix receiving Guild Cards from non-Xbox players
- Make the Guild Card description field in SavedPlayerDataBB longer to accommodate XB descriptions (0x200 bytes)
- Research the F94D quest opcode

## PSOBB

- Test all quest item subcommands
- Check if Commander Blade effect works and implement it if not
- Figure out which quest flags are required for solo quests and write appropriate JSON files

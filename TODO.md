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

- Make disconnecting during a tournament match cause you to forfeit the match
- Enforce tournament deck restrictions (e.g. rank checks, No Assist option) when populating COMs at tournament start time
- It may be possible to send spectators back to the waiting room after a non-tournament battle by sending 6xB4x05 with environment 0x19, then 6xB4x3B again; try this
- Add support for recording battles on the proxy server (both in primary and spectator teams)
- When `reload ep3` happens and the defs file is changed, send the new defs file to all connected players who aren't in a game (if this even works - when exactly does the client decompress the defs file from the server?)
- Make `reload licenses` not vulnerable to online players' licenses overwriting licenses on disk somehow
- Implement ranks (based on total Meseta earned)

## PSO XBOX

- Fix receiving Guild Cards from non-Xbox players
- Make the Guild Card description field in SavedPlayerDataBB longer to accommodate XB descriptions (0x200 bytes)
- Research the F94D quest opcode

## PSOBB

- Find any remaining mismatches in enemy indexes / experience
- Fix some edge cases on the BB proxy server (e.g. Change Ship)
- Implement less-common subcommands
    - 6xD8: Add S-rank weapon special
- Test team commands
    - Test all EA subcommands (a few are still not implemented)
    - 6xC1, 6xC2, 6xCD, 6xCE: Team invites/administration (not implemented)
    - Fix invite member menu
- Implement story progress flags for unlocking quests

## General

- Find a way to silence audio in RunDOL.s
- Encapsulate BB server-side random state and make replays deterministic
- Implement choice search
- Write a simple status API
- Implement per-game logging
- Build an exception-handling abstraction in ChatCommands that shows formatted error messages in all cases
- Make reloading happen on separate threads so compression doesn't block active clients
- Implement decrypt/encrypt actions for VMS files
- Make UI strings localizable (e.g. entries in menus, welcome message, etc.)
- Figure out what causes the corruption message on PC proxy sessions and fix it
- Rewrite REL-based parsers so they don't assume any fixed offsets
- Make $edit for DC/PC
- Handle DC NTE broadcast commands (see Sylverant subcmd_dcnte_handle_bcast)

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
- Implement and test voice chat

## PSOBB

- Find any remaining mismatches in enemy indexes / experience
- Support EXP multipliers in config.json
- Implement trade window
- Fix some edge cases on the BB proxy server (e.g. Change Ship)
- Implement less-common subcommands
    - 6xAC: Sort inventory
    - 6xC1, 6xC2, 6xCD, 6xCE
    - 6xCC: Exchange item for team points
    - 6xD8: Add S-rank weapon special
    - 6xE1: Gallon's Plan quest
- Implement teams
- Implement story progress flags for unlocking quests

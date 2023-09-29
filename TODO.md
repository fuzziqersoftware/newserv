## General

- Extension data in inventories is not handled properly
- Test PSOX (blocked on Insignia private server support)
- Implement server-side drops on non-BB game versions
- Find a way to silence audio in RunDOL.s
- Implement private and overflow lobbies
- Enforce client-side size limits (e.g. for 60/62 commands) on the server side as well
- Encapsulate BB server-side random state and make replays deterministic
- Implement character and inventory replacement for battle and challenge modes
- Implement choice search
- Write a simple status API
- Implement per-game logging
- Add default values in all command structures (like we use for Episode 3 battle commands)
- Check for RCE potential in 6x6B-6x6E commands
- Fix symbol chat header (including face_spec) across PC/GC boundary
- Check size of name field in GuildCardPC
- Build an exception-handling abstraction in ChatCommands that shows formatted error messages in all cases
- Make non-BB detector encryption match more than the first 4 bytes
- Make reloading happen on separate threads so compression doesn't block active clients

## Episode 3

- Make disconnecting during a tournament match cause you to forfeit the match
- Enforce tournament deck restrictions (e.g. rank checks, No Assist option) when populating COMs at tournament start time
- It may be possible to send spectators back to the waiting room after a non-tournament battle by sending 6xB4x05 with environment 0x19, then 6xB4x3B again; try this
- Add support for recording battles on the proxy server (both in primary and spectator teams)

## PSOBB

- Find any remaining mismatches in enemy IDs / experience
- Support EXP multipliers
- Sale prices for non-rare weapons with specials are computed incorrectly when buying/selling at shops
- Replace enemy list, game episode, etc. with quest data when loading a quest
- Implement trade window
- Fix some edge cases on the BB proxy server (e.g. Change Ship)
- Implement less-common subcommands
    - 6xAC: Sort inventory
    - 6xC1, 6xC2, 6xCD, 6xCE
    - 6xCC: Exchange item for team points
    - 6xCF: Restart battle
    - 6xD0
    - 6xD2
    - 6xD5: Exchange item in quest
    - 6xD6
    - 6xD7: Paganini Photon Drop exchange
    - 6xD8: Add S-rank weapon special
    - 6xD9: Momoka item exchange
    - 6xDA: Upgrade weapon attribute
    - 6xDE: Good Luck quest
    - 6xDF
    - 6xE0
    - 6xE1: Gallon's Plan quest
- Team commands

## General

- Make reloading happen on separate threads so compression doesn't block active clients
- Implement decrypt/encrypt actions for VMS files
- Make UI strings localizable (e.g. entries in menus, welcome message, etc.)
- Add an idle connection timeout for proxy sessions
- Clean up ItemParameterTable implementation (see comment ad the top of the class definition)

## Episode 3

- Enforce tournament deck restrictions (e.g. rank checks, No Assist option) when populating COMs at tournament start time
- Make `reload licenses` not vulnerable to online players' licenses overwriting licenses on disk somehow
- Implement ranks (based on total Meseta earned)
- Support Trial Edition battles

## PSO XBOX

- Fix receiving Guild Cards from non-Xbox players
- Research the F94D quest opcode

## PSOBB

- Test all quest item subcommands
- Figure out why Pouilly Slime EXP doesn't work

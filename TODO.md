## General

- Make UI strings localizable (e.g. entries in menus, welcome message, etc.)
- Clean up ItemParameterTable implementation (see comment at the top of the class definition)
- Handle MeetUserExtensions properly in 41 and C4 commands on the proxy (rewrite the embedded 19 command and put some metadata in the persistent config, perhaps)
- Make a server patch version of story flag fixer quest
- Make proxy server handle all login commands, including sending 9C when needed
- Add $switchit command (activates switch flag(s) for nearest object, e.g. laser fence, door, fog collision)
- Add a way to persist flags across connections, at least on v3, because of Meet User + B2 enable quest interactions - maybe update the quest to patch one of the login commands so the server can tell it's enabled
- Handle items in crossplay - use the replacement table

## PSO DC

- Investigate if https://crates.io/crates/blaze-ssl-async can be used to implement the HL check server
- v2 challenge data in $savechar/$loadchar doesn't work properly

## Episode 3

- Enforce tournament deck restrictions (e.g. rank checks, No Assist option) when populating COMs at tournament start time
- Make `reload accounts` not vulnerable to online players' accounts overwriting accounts on disk somehow
- Implement ranks (based on total Meseta earned)
- Make an AR code that gets rid of the SAMPLE overlays on NTE

## PSO XBOX

- Fix receiving Guild Cards from non-Xbox players
- Research the F94D quest opcode
- Finish porting the remaining GC patches

## PSOBB

- Figure out why Pouilly Slime EXP doesn't work
- Make server-specified rare enemies work with maps loaded by the proxy
- Implement serialization for various table types (ItemPMT, ItemPT, etc.)
- Record some BB tests
- Add all necessary Guild Card number rewrites in BB commands on the proxy

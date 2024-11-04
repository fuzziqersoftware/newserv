## General

- Make UI strings localizable (e.g. entries in menus, welcome message, etc.)
- Add an idle connection timeout for proxy sessions
- Clean up ItemParameterTable implementation (see comment at the top of the class definition)
- Handle MeetUserExtensions properly in 41 and C4 commands on the proxy (rewrite the embedded 19 command and store a map of received destinations)

## PSO DC

- Investigate if https://crates.io/crates/blaze-ssl-async can be used to implement the HL check server

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

# newserv <img align="right" src="s-newserv.png" />

newserv is a game server and proxy for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself, which was originally created by Sega.

* Background
    * [History](#history)
    * [Future (and to-do list)](#future)
* [Compatibility](#compatibility)
* Setup
    * [Configuration](#configuration)
    * [Installing quests](#installing-quests)
    * [Episode 3 features](#episode-3-features)
    * [Client patch directories for PC and BB](#client-patch-directories)
    * [Memory patches and DOL files for GC](#memory-patches-and-dol-files)
    * [Using newserv as a proxy](#using-newserv-as-a-proxy)
    * [Chat commands](#chat-commands)
* How to connect
    * Connecting local clients
        * [PSO DC](#pso-dc)
        * [PSO PC](#pso-pc)
        * [PSO GC on a real GameCube](#pso-gc-on-a-real-gamecube)
        * [PSO GC on Dolphin](#pso-gc-on-dolphin)
    * [Connecting external clients](#connecting-external-clients)
* [Non-server features](#non-server-features)

## History

The history of this project essentially mirrors my development as a software engineer from the beginning of my hobby until now. If you don't care about the story, skip to the "Compatibility" or "Setup" sections below.

I originally purchased PSO GC when I heard about PSUL, and wanted to play around with running homebrew on my GameCube. This pathway eventually led to [GCARS-CS](https://github.com/fuzziqersoftware/gcars-cs), but that's another story.

<img align="left" src="s-khyps.png" /> After playing PSO for a while, both offline and online, I wrote a proxy called Khyps sometime in 2003. This was back in the days of the official Sega servers, where vulnerabilities weren't addressed in a timely manner or at all. It was common for malicious players using their own proxies or Action Replay codes (a story for another time) to send invalid commands that the servers would blindly forward, and cause the receiving clients to crash. These crashes were more than simply inconvenient; they could also corrupt your save data, destroying the hours of work you may have put into hunting items and leveling up your character.

For a while it was essentially necessary to use a proxy to go online at all, so the proxy could block these invalid commands. Khyps was designed primarily with this function in mind, though it also implemented some convenient cheats, like the ability to give yourself or other players infinite HP and allow you to teleport to different places without using an in-game teleporter.

<img align="left" src="s-khyller.png" /> After Khyps I took on the larger challenge of writing a server, which resulted in Khyller sometime in 2005. This was the first server of any type I had ever written. This project eventually evolved into a full-featured environment supporting all versions of the game that I had access to - at the time, PC, GC, and BB. (However, I suspect from reading the ancient source files that Khyller's BB support was very buggy.) As Khyller evolved, the code became increasingly cumbersome, littered with debugging filth that I never cleaned up and odd coding patterns I had picked up over the years. My understanding of the C++ language was woefully incomplete as well (as opposed to now, when it is still incomplete but not woefully so), which resulted in Khyller being essentially a C project that had a couple of classes in it.

<img align="left" src="s-aeon.png" /> Sometime in 2006 or 2007, I abandoned Khyller and rebuilt the entire thing from scratch, resulting in Aeon. Aeon was substantially cleaner in code than Khyller but still fairly hard to work with, and it lacked a few of the more arcane features I had originally written (for example, the ability to convert any quest into a download quest). In addition, the code still had some stability problems... it turns out that Aeon's concurrency primitives were simply incorrect. I had derived the concept of a mutex myself, before taking any real computer engineering classes, but had implemented it incorrectly. I made the race window as small as possible, but Aeon would still randomly crash after running seemingly fine for a few days.

At the time of its inception, Aeon was also called newserv, and you may find some beta releases floating around the Internet with filenames like `newserv-b3.zip`. I had released betas 1, 2, and 3 before I released the entire source of beta 5 and stopped working on the project when I went to college. This was around the time when I switched from writing software primarily on Windows to primarily on macOS and Linux, so Aeon beta 5 was the last server I wrote that specifically targeted Windows. (newserv, which you're looking at now, is a bit tedious to compile on Windows but does work.)

<img align="left" src="s-newserv.png" /> After a long hiatus from PSO and much professional and personal development in my technical abilities, I was reminiscing sometime in October 2018 by reading my old code archives. Somehow inspired when I came across Aeon, I spent a weekend and a couple more evenings rewriting the entire project again, cleaning up ancient patterns I had used eleven years ago, replacing entire modules with simple STL containers, and eliminating even more support files in favor of configuration autodetection. The code is now suitably modern and stable, and I'm not embarrassed by its existence, as I am by Aeon beta 5's source code and my archive of Khyller (which, thankfully, no one else ever saw).

## Future

newserv is many things - a server, a proxy, an encryption and decryption tool, a decoder of various PSO-related formats, and more. Primarily, it's a reverse-engineering project in which I try to unravel the secrets of a 20-year-old video game, for honestly no reason. Solving these problems and documenting them in code has been fun, and I'll continue to do it when my time allows.

With that said, I offer no guarantees on how or when this project will advance. Feel free to submit GitHub issues if you find bugs or have feature requests; I'd like to make the server as stable and complete as possible, but I can't promise that I'll respond to issues in a timely manner. If you feel like contributing to newserv yourself, pull requests are welcome as well.

Current known issues / missing features / things to do:
- Implement the rest of PSOBB. Major areas of work:
    - Find any remaining mismatches in enemy IDs / experience
    - Sale prices for non-rare weapons with specials are computed incorrectly when buying/selling at shops
    - Replace enemy list, game episode, etc. with quest data when loading a quest
    - Implement trade window
    - Fix some edge cases on the BB proxy server (e.g. make sure Change Ship does the right thing, which is not the same as what it should do on other versions).
- There is a function that encodes QST files, but there's no corresponding CLI option.
- Figure out what controls BML file data segment alignment.
- PSOX is not tested at all.
    - Deal with item.data2d byteswapping done by the GC client, including in the 6x6D command.
- Find a way to silence audio in RunDOL.s. Some old DOLs don't reset audio systems at load time and it's annoying to hear the crash buzz when the GC hasn't actually crashed.
- Implement private and overflow lobbies.
- Enforce client-side size limits (e.g. for 60/62 commands) on the server side as well. (For 60/62 specifically, perhaps transform them to 6C/6D if needed.)
- Encapsulate BB server-side random state and make replays deterministic.
- Make a looser form of item tracking that can be used on non-BB versions when quests replace player inventories, like in battle and challenge modes.
- Episode 3 bugs
    - Fix behavior when joining a spectator team after the beginning of a battle.
    - Disconnecting during a match turns you into a COM if there are other humans in the match, even if the match is part of a tournament. This may be incorrect behavior for tournaments.
    - Disconnecting during a tournament when there are no other humans in the match simply cancels the match (so it can be replayed) instead of forfeiting, which is almost certainly incorrect behavior. (Then again, no one likes losing tournaments to COMs...)
    - Tournament deck restrictions aren't enforced when populating COMs at tournament start time. This can cause weird behavior if, for example, a COM deck contains assist cards and the tournament rules forbid them.
    - There is a rare failure mode during battles that causes one of the clients to be disconnected.
- Code style
    - Add default values in all command structures (like we use for Episode 3 battle commands).

## Compatibility

newserv supports several versions of PSO. Specifically:
| Version        | Login        | Lobbies      | Games        | Proxy        |
|----------------|--------------|--------------|--------------|--------------|
| DC Trial       | Yes (4)      | Yes (4)      | Yes (4)      | No           |
| DC Prototype   | Yes (4)      | Yes (4)      | Yes (4)      | No           |
| DC V1          | Yes (1)      | Yes          | Yes          | Yes          |
| DC V2          | Yes (1)      | Yes          | Yes          | Yes          |
| PC             | Yes          | Yes          | Yes          | Yes          |
| GC Ep1&2 Trial | Untested (2) | Untested (2) | Untested (2) | Untested (2) |
| GC Ep1&2       | Yes          | Yes          | Yes          | Yes          |
| GC Ep1&2 Plus  | Yes          | Yes          | Yes          | Yes          |
| GC Ep3 Trial   | Yes          | Yes          | Yes          | Yes          |
| GC Ep3         | Yes          | Yes          | Yes          | Yes          |
| XBOX Ep1&2     | Untested (2) | Untested (2) | Untested (2) | Untested (2) |
| BB (vanilla)   | Yes          | Yes          | Partial (3)  | Yes          |
| BB (Tethealla) | Yes          | Yes          | Partial (3)  | Yes          |

*Notes:*
1. *DC support has only been tested with the US versions of PSO DC. Other versions probably don't work, but will be easy to add support for. Please submit a GitHub issue if you have a non-US DC version, and can provide a log from a connection attempt.*
2. *newserv's implementations of these versions are based on disassembly of the client executables and have never been tested.*
3. *Some basic features are not implemented in Blue Burst games, so the games are not very playable. A lot of work has to be done to get BB games to a playable state.*
4. *Support for PSO Dreamcast Trial Edition and the December 2000 prototype is somewhat incomplete and probably never will be complete. These versions are rather unstable and seem to crash often, but it's not obvious whether it's because they're prototypes or because newserv sends data they can't handle.*

## Setup

### Configuration

Currently newserv works on macOS, Windows, and Ubuntu Linux. It will likely work on other Linux flavors too.

There is a fairly recent macOS ARM64 release on the newserv GitHub repository. You may need to install libevent manually even if you use this release (run `brew install libevent`).

There is a fairly recent Windows release on the newserv GitHub repository also. It's built with Cygwin, and all the necessary DLL files should be included. That said, I've only tested it on my own machine and there is no CI for Windows builds like there is for macOS and Linux, so if it doesn't work for you, please open a GitHub issue to let me know.

If you're not using a release from the GitHub repository, do this to build newserv:
1. If you're on Windows, install Cygwin. While doing so, install the `cmake`, `gcc-core`, `gcc-g++`, `git`, `libevent2.1_7`, `make`, and `zlib` packages. Do the rest of these steps inside a Cygwin shell (not a Windows cmd shell or PowerShell).
2. Make sure you have CMake and libevent installed. (On macOS, `brew install cmake libevent`; on most Linuxes, `sudo apt-get install cmake libevent-dev`; on Windows, you already did this in step 1.)
3. Build and install phosg (https://github.com/fuzziqersoftware/phosg).
4. Optionally, install resource_dasm (https://github.com/fuzziqersoftware/resource_dasm). This will enable newserv to send memory patches and load DOL files on PSO GC clients. PSO GC clients can play PSO normally on newserv without this.
5. Run `cmake . && make` in the newserv directory.

After building newserv or downloading a release, do this to set it up and use it:
1. In the system/ directory, make a copy of config.example.json named config.json, and edit it appropriately.
2. If you plan to play PSO Blue Burst on newserv, set up the patch directory. See the "Client patch directories" section below.
3. Run `./newserv` in the newserv directory. This will start the game server and run the interactive shell. You may need `sudo` if newserv's built-in DNS server is enabled.
4. Use the interactive shell to add a license. Run `help` in the shell to see how to do this.
5. Set your client's network settings appropriately and start an online game. See the "Connecting local clients" or "Connecting remote clients" section to see how to get your game client to connect.

To use newserv in other ways (e.g. for translating data), see the end of this document.

### Installing quests

newserv automatically finds quests in the system/quests/ directory. To install your own quests, or to use quests you've saved using the proxy's "save files" option, just put them in that directory and name them appropriately.

Standard quest files should be named like `q###-CATEGORY-VERSION.EXT`, battle quests should be named like `b###-VERSION.EXT`, challenge quests should be named like `c###-VERSION.EXT` for Episode 1 or `d###-VERSION.EXT` for Episode 2, and Episode 3 download quests should be named like `e###-gc3.EXT`. The fields in each filename are:
- `###`: quest number (this doesn't really matter; it should just be unique across the PSO version)
- `CATEGORY`: ret = Retrieval, ext = Extermination, evt = Events, shp = Shops, vr = VR, twr = Tower, gv1/gv2/gv4 = Government (BB only), dl = Download (these don't appear during online play), 1p = Solo (BB only)
- `VERSION`: d1 = Dreamcast v1, dc = Dreamcast v2, pc = PC, gc = GameCube Episodes 1 & 2, gc3 = Episode 3, bb = Blue Burst
- `EXT`: file extension (see table below)

For example, the GameCube version of Lost HEAT SWORD is in two files named `q058-ret-gc.bin` and `q058-ret-gc.dat`. newserv knows these files are quests because they're in the system/quests/ directory, it knows they're for PSO GC because the filenames contain `-gc`, and it puts them in the Retrieval category because the filenames contain `-ret`.

The type identifiers (`b`, `c`, `d`, `e`, or `q`) and categories are configurable. See QuestCategories in config.example.json for more information on how to make new categories or edit the existing categories.

There are multiple PSO quest formats out there; newserv supports all of them. It can also decode any known format to standard .bin/.dat format. Specifically:

| Format           | Extension             | Supported  | Decode action    |
|------------------|-----------------------|------------|------------------|
| Compressed       | .bin and .dat         | Yes        | None (1)         |
| Compressed Ep3   | .bin or .mnm          | Yes (4)    | None (1)         |
| Uncompressed     | .bind and .datd       | Yes        | compress-prs (2) |
| Uncompressed Ep3 | .bind or .mnmd        | Yes (4)    | compress-prs (2) |
| VMS (DCv1)       | .bin.vms and .dat.vms | Yes        | decode-vms       |
| VMS (DCv2)       | .bin.vms and .dat.vms | Decode (3) | decode-vms (3)   |
| GCI (decrypted)  | .bin.gci and .dat.gci | Yes        | decode-gci       |
| GCI (with key)   | .bin.gci and .dat.gci | Yes        | decode-gci       |
| GCI (no key)     | .bin.gci and .dat.gci | Decode (3) | decode-gci (3)   |
| GCI (Ep3)        | .bin.gci or .mnm.gci  | Yes        | decode-gci       |
| DLQ              | .bin.dlq and .dat.dlq | Yes        | decode-dlq       |
| DLQ (Ep3)        | .bin.dlq or .mnm.dlq  | Yes        | decode-dlq       |
| QST (online)     | .qst                  | Yes        | decode-qst       |
| QST (download)   | .qst                  | Yes        | decode-qst       |

*Notes:*
1. *This is the default format. You can convert these to uncompressed format by running `newserv decompress-prs FILENAME.bin FILENAME.bind` (and similarly for .dat -> .datd)*
2. *Similar to (1), to compress an uncompressed quest file: `newserv compress-prs FILENAME.bind FILENAME.bin` (and likewise for .datd -> .dat)*
3. *Use the decode action to convert these quests to .bin/.dat format before putting them into the server's quests directory. If you know the encryption seed (serial number), pass it in as a hex string with the `--seed=` option. If you don't know the encryption seed, newserv will find it for you, which will likely take a long time.*
4. *Episode 3 online quests don't go in the system/quests directory; they instead go in the system/ep3/maps-free or system/ep3/maps-quest directories. If you want an Episode 3 quest to be available for both online play and for downloading, the file must exist in both system/quests and in one of the map directories in system/ep3.*

Episode 3 download quests consist only of a .bin file - there is no corresponding .dat file. Episode 3 download quest files may be named with the .mnm extension instead of .bin, since the format is the same as the standard map files (in system/ep3/). These files can be encoded in any of the formats described above, except .qst. There are no encrypted Episode 3 GCI formats because the game doesn't encrypt quests saved to the memory card, unlike Episodes 1&2.

When newserv indexes the quests during startup, it will warn (but not fail) if any quests are corrupt or in unrecognized formats.

Quest contents are cached in memory, but if you've changed the contents of the quests directory, you can re-index the quests without restarting the server by running `reload quests` in the interactive shell. The new quests will be available immediately, but any games with quests already in progress will continue using the old versions of the quests until those quests end.

All quests, including those originally in GCI or DLQ format, are treated as online quests unless their filenames specify the dl category. newserv allows players to download all quests, even those in non-download categories.

### Episode 3 features

The following Episode 3 features work well:
* CARD battles. Not every combination of abilities has been tested yet, so if you find a feature or card ability that doesn't work like it's supposed to, please make a GitHub issue and describe the situation (the attacking card(s), defending card(s), and ability or condition that didn't work).
* Tournaments. (But they don't work like Sega's tournaments did - see below)
* Downloading quests.
* Trading cards.
* Participating in card auctions. (The auction contents must be configured in config.json.)

The following Episode 3 features are implemented, but are only partially tested:
* Spectator teams. There is a known issue that prevents viewing battles unless you're in the spectator team when the battle begins, and spectating clients sometimes crash for an unknown reason.
* Battle replays also sometimes cause the client to crash during the replay. Using the $playrec command is therefore not recommended.

Tournaments work differently than they did on Sega's servers. Tournaments can be created with the `create-tournament` shell command, which enables players to register for them. (Use `help` to see all the arguments - there are many!) The `start-tournament` shell command starts the tournament (and prevents further registrations), but this doesn't schedule any matches. Instead, players who are ready to play their next match can all stand at the rightmost 4-player battle table in the same CARD lobby, and the tournament match will start automatically.

These tournament semantics mean that there can be multiple matches in the same tournament in play simultaneously, and not all matches in a round must be complete before the next round can begin - only the matches preceding each individual match must be complete for that match to be playable.

Because newserv gives all players 1000000 meseta, there is no reward for winning a tournament. This may change in the future.

Episode 3 state and game data is stored in the system/ep3 directory. The files in there are:
* card-definitions.mnr: Compressed card definition list, sent to Episode 3 clients at connect time. Card stats and abilities can be changed by editing this file.
* card-definitions.mnrd: Decompressed version of the above. If present, newserv will use this instead of the compressed version, since this is easier to edit.
* card-text.mnr: Compressed card text archive. Generally only used for debugging.
* com-decks.json: COM decks used in tournaments. The default decks in this file come from logs from Sega's servers, so the file doesn't include every COM deck Sega ever made - the rest are probably lost to time.
* maps-free/ and maps-quest/: Online free battle and quest maps (.mnm/.bin/.mnmd/.bind files). Free battle and quest files have exactly the same format; the only difference between the files in these directories is which section of the menu they will appear in on the client.
* tournament-state.json: State of all active tournaments. This file is automatically written when any tournament changes state for any reason (e.g. a tournament is created/started/deleted or a match is resolved).

There is no public editor for Episode 3 maps and quests, but the format is described fairly thoroughly in src/Episode3/DataIndex.hh (see the MapDefinition structure). You'll need to use `newserv decompress-prs ...` to decompress .bin or .mnm files before editing them, but you don't need to compress the files again to use them - just put the .bind or .mnmd file in the maps directory and newserv will make it available.

Like quests, Episode 3 card definitions, maps, and quests are cached in memory. If you've changed any of these files, you can run `reload ep3` in the interactive shell to make the changes take effect without restarting the server.

### Client patch directories

If you're not playing PSO Blue Burst on newserv, you can skip these steps.

newserv implements a patch server for PSO PC and PSO BB game data. Any file or directory you put in the system/patch-bb or system/patch-pc directories will be synced to clients when they connect to the patch server.

To make server startup faster, newserv caches the modification times, sizes, and checksums of the files in the patch directories. If the patch server appears to be misbehaving, try deleting the .metadata-cache.json file in the relevant patch directory to force newserv to recompute all the checksums. Also, in the case when checksums are cached, newserv may not actually load the data for a patch file until it's needed by a client. Therefore, modifying any part of the patch tree while newserv is running can cause clients to see an inconsistent view of it.

For BB clients, newserv reads some files out of the patch data to implement game logic, so it's important that certain game files are synchronized between the server and the client. newserv contains defaults for these files in the system/blueburst/map directory, but if these don't match the client's copies of the files, odd behavior will occur in games.

Specifically, the patch-bb directory should contain at least the data.gsl file and all map_*.dat files from the version of PSOBB that you want to play on newserv. You can copy these files out of the client's data directory from a clean installation, and put them in system/patch-bb/data.

Patch directory contents are cached in memory. If you've changed any of these files, you can run `reload patches` in the interactive shell to make the changes take effect without restarting the server.

### Memory patches and DOL files

Everything in this section requires resource_dasm to be installed, so newserv can use the PowerPC assembler and disassembler from its libresource_file library. If resource_dasm is not installed, newserv will still build and run, but these features will not be available.

In addition, these features are only supported for the following game versions:
* PSO GameCube Episodes 1&2 JP, USA, and EU (not Plus)
* PSO GameCube Episodes 1&2 Plus JP v1.04 (not v1.05)
* PSO GameCube Episode 3 Trial Edition
* PSO GameCube Episode 3 JP
* PSO GameCube Episode 3 USA (experimental; must be manually enabled in config.json)

You can put memory patches in the system/ppc directory with filenames like PatchName.patch.s and they will appear in the Patches menu for PSO GC clients that support patching. Memory patches are written in PowerPC assembly and are compiled when newserv is started. The PowerPC assembly system's features are documented in the comments in system/ppc/WriteMemory.s - this file is not a memory patch itself, but it describes how memory patches may be written and the restrictions that apply to them.

You can also put DOL files in the system/dol directory, and they will appear in the Programs menu. Selecting a DOL file there will load the file into the GameCube's memory and run it, just like the old homebrew loaders (PSUL and PSOload) did. For this to work, ReadMemoryWord.s, WriteMemory.s, and RunDOL.s must be present in the system/ppc directory. This has been tested on Dolphin but not on a real GameCube, so results may vary.

Like other kinds of data, functions and DOL files are cached in memory. If you've changed any of these files, you can run `reload functions` or `reload dol-files` in the interactive shell to make the changes take effect without restarting the server.

I mainly built the DOL loading functionality for documentation purposes. By now, there are many better ways to load homebrew code on an unmodified GameCube, but to my knowledge there isn't another open-source implementation of this method in existence.

### Using newserv as a proxy

If you want to play online on remote servers rather than running your own server, newserv also includes a PSO proxy. Currently this works with PSO GC and may work with PC and DC; it also works with some BB clients in specific situations.

To use the proxy for PSO DC, PC, or GC, add an entry to the corresponding ProxyDestinations dictionary in config.json, then run newserv and connect to it as normal (see below). You'll see a "Proxy server" option in the main menu, and you can pick which remote server to connect to.

To use the proxy for PSO BB, set the ProxyDestination-BB entry in config.json. If this option is set, it essentially disables the game server for all PSO BB clients - all clients will be proxied to the specified destination instead. Unfortunately, because PSO BB uses a different set of handlers for the data server phase and character selection, there's no in-game way to present the player with a list of options, like there is on PSO PC and PSO GC.

When you're on PSO DC, PC, or GC and are connected to a remote server through newserv's proxy, choosing the Change Ship or Change Block action from the lobby counter will send you back to newserv's main menu instead of the remote server's ship or block select menu. You can go back to the server you were just on by choosing it from the proxy server menu again.

There are many options available when starting a proxy session. All options are off by default unless otherwise noted. The options are:
* **Chat commands**: enables chat commands in the proxy session (on by default).
* **Chat filter**: enables escape sequences in chat messages and info board (on by default).
* **Player notifications**: shows a message when any player joins or leaves the game or lobby you're in.
* **Block pings**: blocks automatic pings sent by the client, and responds to ping commands from the server automatically. This works around a bug in Sylverant's login server.
* **Infinite HP**: automatically heals you whenever you get hit. An attack that kills you in one hit will still kill you, however.
* **Infinite TP**: automatically restores your TP whenever you use any technique.
* **Switch assist**: attempts to unlock doors that require two players in a one-player game.
* **Infinite Meseta** (Episode 3 only): gives you 1,000,000 Meseta, regardless of the value sent by the remote server.
* **Block events**: disables holiday events sent by the remote server.
* **Block patches**: prevents any B2 (patch) commands from reaching the client.
* **Save files**: saves copies of several kinds of files when they're sent by the remote server. The files are written to the current directory (which is usually the directory containing the system/ directory). These kinds of files can be saved:
    * Online quests and download quests (saved as .bin/.dat files)
    * GBA games (saved as .gba files)
    * Patches (saved as .bin files, and disassembled into PowerPC assembly if newserv is built with patch support)
    * Player data from BB sessions (saved as .bin files, which are not the same format as .nsc files)
    * Episode 3 online quests and maps (saved as .mnmd files)
    * Episode 3 download quests (saved as .mnm files)
    * Episode 3 card definitions (saved as .mnr files)
    * Episode 3 media updates (saved as .gvm, .bml, or .bin files)

The remote server will probably try to assign you a Guild Card number that doesn't match the one you have on newserv. On PSO DC, PC and GC, the proxy server rewrites the commands in transit to make it look like the remote server assigned you the same Guild Card number as you have on newserv, but if the remote server has some external integrations (e.g. forum or Discord bots), they will use the Guild Card number that the remote server believes it has assigned to you. The number assigned by the remote server is shown to you when you first connect to the remote server, and you can retrieve it in lobbies or during games with the $li command.

Some chat commands (see below) have the same basic function on the proxy server but have different effects or conditions. In addition, there are some server shell commands that affect clients on the proxy (run `help` in the shell to see what they are). If there's only one proxy session open, the shell's proxy commands will affect that session. Otherwise, you'll have to specify which session to affect with the `on` prefix - to send a chat message in LinkedSession:17205AE4, for example, you would run `on 17205AE4 chat ...`.

### Chat commands

The server's shell supports a variety of administration commands. If the interactive shell is enabled, you can enter these commands at any time, even if the prompt isn't visible. Run `help` in the server's shell to see all of the commands and how to use them.

newserv also supports a variety of commands players can use via the chat interface. Any chat message that begins with `$` is treated as a chat command. (If you actually want to send a chat message starting with `$`, type `$$` instead.)

Some commands only work on the game server and not on the proxy server. The chat commands are:

* Information commands
    * `$li`: Shows basic information about the lobby or game you're in. If you're on the proxy server, shows information about your connection instead (remote Guild Card number, client ID, etc.).
    * `$what` (game server only): Shows the type, name, and stats of the nearest item on the ground.

* Debugging commands
    * `$debug` (game server only): Enable or disable debug. You need the DEBUG permission in your user license to use this command. When debug is enabled, you'll see in-game messages from the server when you take certain actions. You'll also be placed into the highest available slot in lobbies and games instead of the lowest, which is useful for finding commands for which newserv doesn't handle client IDs properly. This setting also disables certain safeguards and allows you to do some things that might crash your client.
    * `$gc` (game server only): Send your own Guild Card to yourself.
    * `$persist` (game server only): Enable or disable persistence for the current lobby or game. This determines whether the lobby/game is deleted when the last player leaves. You need the DEBUG permission in your user license to use this command because there are no game state checks when you do this. For example, if you make a game persistent, start a quest, then leave the game, the game can't be joined by anyone but also can't be deleted.
    * `$sc <data>`: Send a command to yourself.
    * `$ss <data>` (proxy server only): Send a command to the remote server.

* Personal state commands
    * `$arrow <color-id>`: Changes your lobby arrow color.
    * `$secid <section-id>`: Sets your override section ID. After running this command, any games you create will use your override section ID for rare drops instead of your character's actual section ID. To revert to your actual section id, run `$secid` with no name after it. On the proxy server, this will not work if the remote server controls item drops (e.g. on BB, or on Schtserv with server drops enabled).
    * `$rand <seed>`: Sets your override random seed (specified as a 32-bit hex value). This will make any games you create use the given seed for rare enemies. This also makes item drops deterministic in Blue Burst games hosted by newserv. On the proxy server, this command can cause desyncs with other players in the same game, since they will not see the overridden random seed. To remove the override, run `$rand` with no arguments.
    * `$exit`: If you're in a lobby, sends you to the main menu (which ends your proxy session, if you're in one). If you're in a game or spectator team, sends you to the lobby (but does not end your proxy session if you're in one). Does nothing if you're in a non-Episode 3 game and no quest is in progress.
    * `$patch <name>`: Run a patch on your client. `<name>` must exactly match the name of a patch on the server.

* Blue Burst player commands (game server only)
    * `$bbchar <username> <password> <1-4>`: Use this command when playing on a non-BB version of PSO. If the username and password are correct, this command converts your current character to BB format and saves it on the server in the given slot. Any character already in that slot is overwritten.
    * `$edit <stat> <value>`: Modifies your character data.

* Game state commands (game server only)
    * `$maxlevel <level>`: Sets the maximum level for players to join the current game. (This only applies when joining; if a player joins and then levels up past this level during the game, they are not kicked out, but won't be able to rejoin if they leave.)
    * `$minlevel <level>`: Sets the minimum level for players to join the current game.
    * `$password <password>`: Sets the game's join password. To unlock the game, run `$password` with nothing after it.
    * `$spec`: Toggles the allow spectators flag. If any players are spectating when this flag is disabled, they will be sent back to the lobby.
    * `$saverec <name>`: Save the recording of the last Episode 3 battle.
    * `$playrec <name>`: Play a battle recording. This command creates a spectator team and replays the specified battle log within it. There is a known issue which causes spectators to crash in some cases, so use of this command is currently not recommended.

* Cheat mode commands
    * `$cheat`: Enables or disables cheat mode for the current game. All other cheat mode commands do nothing if cheat mode is disabled. By default, cheat mode is off in new games but can be enabled; there is an option in config.json that allows you to disable cheat mode entirely, or set it to on by default in new games.
    * `$infhp` / `$inftp`: Enables or disables infinite HP or TP mode. Applies to only you. In infinite HP mode, one-hit KO attacks will still kill you.
    * `$warpme <area-id>`: Warps yourself to the given area.
    * `$warpall <area-id>`: Warps everyone in the game to the given area. You must be the leader to use this command, unless you're on the proxy server.
    * `$next`: Warps yourself to the next area.
    * `$swa`: Enables or disables switch assist. When enabled, the server will attempt to automatically unlock two-player doors in solo games if you step on both switches sequentially.
    * `$item <desc>` (or `$i <desc>`): Create an item. `desc` may be a description of the item (e.g. "Hell Saber +5 0/10/25/0/10") or a string of hex data specifying the item code. Item codes are 16 hex bytes; at least 2 bytes must be specified, and all unspecified bytes are zeroes. If you are on the proxy server, you must not be using Blue Burst for this command to work. On the game server, this command works for all versions.

* Configuration commands
    * `$event <event>`: Sets the current holiday event in the current lobby. Holiday events are documented in the "Using $event" item in the information menu. If you're on the proxy server, this applies to all lobbies and games you join, but only you will see the new event - other players will not.
    * `$allevent <event>` (game server only): Sets the current holiday event in all lobbies.
    * `$song <song-id>` (Episode 3 only): Plays a specific song in the current lobby.

* Administration commands (game server only)
    * `$ann <message>`: Sends an announcement message. The message text is sent to all players in all games and lobbies.
    * `$ax <message>`: Sends a message to the server's terminal. This cannot be used to run server shell commands; it only prints text to stderr.
    * `$silence <identifier>`: Silences a player (remove their ability to chat) or unsilences a player. The identifier may be the player's name or Guild Card number.
    * `$kick <identifier>`: Disconnects a player. The identifier may be the player's name or Guild Card number.
    * `$ban <identifier>`: Bans a player. The identifier may be the player's name or Guild Card number.

### Connecting local clients

#### PSO DC

Some versions of PSO DC will connect to a private server if you just set their DNS server address (in the network configuration) to newserv's address, and enable newserv's DNS server. This will not work for other versions; for those, you'll need a cheat code. Creating such a code is beyond the scope of this document.

If you're emulating PSO DC or have a disc image, you can patch the appropriate files within the disc image to make it connect to any address you want. Creating such a patch is also beyond the scope of this document.

Finally, if you're emulating PSO DC, you can modify the loaded executable in memory to make it connect anywhere you want. There is a script included with newserv that can do this for Flycast. The script only works on macOS because it uses memwatch, which is specifically for macOS, but a similar technique could be done manually using scanmem on Linux or Cheat Engine on Windows. (The script is fairly short, and what it does should be easy to understand so you can duplicate its effects with scanmem or Cheat Engine.)

To use the script, do this:
1. Build and install memwatch (https://github.com/fuzziqersoftware/memwatch).
2. Start Flycast and run PSO. (You must run the script below after PSO is loaded - it won't work if you run it before loading the game.)
3. Run `sudo patch_flycast_memory.py <original-destination>`. Replace `<original-destination>` with the hostname that PSO wants to connect to (you can find this out by using Wireshark and looking for DNS queries). The script may take up to a minute; you can continue using Flycast while it runs, but don't start an online game until the script is done.
4. Run newserv and start an online game in PSO.

If you use this method, you'll have to run the script every time you start PSO in Flycast, but you won't have to run it again if you start another online game without restarting emulation.

Finally, the script takes an optional second argument that allows you to redirect the connection elsewhere (instead of the local machine). This allows you to connect directly to remote servers if desired.

#### PSO PC

The version of PSO PC I have has the server addresses starting at offset 0x29CB34 in pso.exe. Using a hex editor, change those to "localhost" (without quotes) if you just want to connect to a locally-running newserv instance. Alternatively, you can add an entry to the Windows hosts file (C:\Windows\System32\drivers\etc\hosts) to redirect the connection to 127.0.0.1 (localhost) or any other IP address.

#### PSO GC on a real GameCube

You can make PSO connect to newserv by setting its default gateway and DNS server addresses to newserv's address. newserv's DNS server must be running on port 53 and must be accessible to the GameCube.

If you have PSO Plus or Episode III, it won't want to connect to a server on the same local network as the GameCube itself, as determined by the GameCube's IP address and subnet mask. In the old days, one way to get around this was to create a fake network adapter on the server (or use an existing real one) that has an IP address on a different subnet, tell the GameCube that the server is the default gateway (as above), and have the server reply to the DNS request with its non-local IP address. To do this with newserv, just set LocalAddress in the config file to a different interface. For example, if the GameCube is on the 192.168.0.x network and your other adapter has address 10.0.1.6, set newserv's LocalAddress to 10.0.1.6 and set PSO's DNS server and default gateway addresses to the server's 192.168.0.x address. This may not work on modern systems or on non-Windows machines - I haven't tested it in many years.

#### PSO GC on Dolphin

If you have BBA support via a tap interface or via the HLE/built-in interface, you may be able to just set the DNS server address (as you would on a real GameCube, above) and it may work.

If you're using a version of Dolphin with tapserver support, you can make it connect to a newserv instance running on the same machine via the tapserver interface. You do not need to install or run tapserver, and this works for all PSO versions without any of the dual-interface trickery described above. To do this:
1. Set Dolphin's BBA type to tapserver (Config -> GameCube -> SP1).
2. Enable newserv's IP stack simulator according to the comments in config.json and start newserv.
3. In PSO, you have to configure the network settings manually (DHCP doesn't work), but the actual values don't matter as long as they're valid IP addresses. Example values:
  - IP address: `10.0.1.5`
  - Subnet mask: `255.255.255.0`
  - Default gateway: `10.0.1.1`
  - DNS server address 1: `10.0.1.1`
  - Leave everything else blank
4. Start an online game.

### Connecting external clients

If you want to accept connections from outside your local network, you'll need to set ExternalAddress to your public IP address in the configuration file, and you'll likely need to open some ports in your router's NAT configuration - specifically, all the TCP ports listed in PortConfiguration in config.json.

For GC clients, you'll have to use newserv's built-in DNS server or set up your own DNS server as well. If you want external clients to be able to use your DNS server, you'll have to forward UDP port 53 to your newserv instance. Remote players can then connect to your server by entering your DNS server's IP address in their client's network configuration.

### Non-server features

newserv has many CLI options, which can be used to access functionality other than the game and proxy server. Run `newserv help` to see these options and how to use them. The non-server things newserv can do are:

* Compress or decompress data in the PRS and BC0 formats (`compress-prs`, `compress-bc0`, `decompress-prs`, `decompress-bc0`)
* Compute the decompressed size of compressed PRS data without decompressing it (`prs-size`)
* Encrypt or decrypt data using any PSO version's network encryption scheme (`encrypt-data`, `decrypt-data`)
* Encrypt or decrypt data using Episode 3's trivial scheme (`encrypt-trivial-data`, `decrypt-trivial-data`)
* Encrypt or decrypt PSO GC save data (.gci files) (`encrypt-gci-save`, `decrypt-gci-save`)
* Find the likely round1 or round2 seed for a corrupt save file (`salvage-gci`)
* Run a brute-force search for a decryption seed (`find-decryption-seed`)
* Decode Shift-JIS text to UTF-16 (`decode-sjis`)
* Convert quests in .gci, .vms, .dlq, or .qst format to .bin/.dat format (`decode-gci`, `decode-vms`, `decode-dlq`, `decode-qst`)
* Connect to another PSO server and pretend to be a client (`cat-client`)
* Format Episode 3 game data in a human-readable manner (`show-ep3-data`)
* Render a human-readable description of item data (`describe-item`)
* Replay a session log for testing (`replay-log`)
* Extract the contents of a .gsl or .bml archive (`extract-gsl`, `extract-bml`)

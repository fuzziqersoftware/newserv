# newserv <img align="right" src="s-newserv.png" />

newserv is a game server and proxy for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself, which was originally created by Sega.

## History

The history of this project essentially mirrors my development as a software engineer from the beginning of my hobby until now. If you don't care about the story, skip to the "Compatibility" or "Usage" sections below.

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
- Support disconnect hooks to clean up state, like if a client disconnects during quest loading or during a trade window execution.
- Episode 3 battles aren't implemented. (Some reverse-engineering has already been done here though - see Episode3.hh/cc)
- PSOBB is not well-tested and likely will disconnect or misbehave when clients try to use unimplemented features.
    - Enemy indexes also desync slightly in most games, often in later areas, leading to incorrect EXP values being given for killed enemies.
- Fix some edge cases on the BB proxy server (e.g. make sure Change Ship does the right thing, which is not the same as what it should do on V2/V3).
- PSOX is not tested at all.
- Memory patches currently are platform-specific but not version-specific. This makes them quite a bit harder to write and use properly.
- Find a way to silence audio in RunDOL.s. Some old DOLs don't reset audio systems at load time and it's annoying to hear the crash buzz when the GC hasn't actually crashed.
- Implement private and overflow lobbies.
- Enforce client-side size limits (e.g. for 60/62 commands) on the server side as well. (For 60/62 specifically, perhaps transform them to 6C/6D if needed.)
- Encapsulate BB server-side random state and make replays deterministic.

## Compatibility

newserv supports several versions of PSO. Specifically:
| Version              | Basic commands | Lobbies       | Games         | Proxy         |
|----------------------|----------------|---------------|---------------|---------------|
| Dreamcast Trial      | Not supported  | Not supported | Not supported | Not supported |
| Dreamcast V1         | Supported (1)  | Supported     | Supported     | Supported     |
| Dreamcast V2         | Supported (1)  | Supported     | Supported     | Supported     |
| PC                   | Supported      | Supported     | Supported     | Supported     |
| GameCube Ep1&2 Trial | Untested (2)   | Untested (2)  | Untested (2)  | Untested (2)  |
| GameCube Ep1&2       | Supported      | Supported     | Supported     | Supported     |
| GameCube Ep1&2 Plus  | Supported      | Supported     | Supported     | Supported     |
| GameCube Ep3 Trial   | Supported      | Supported     | Partial (3)   | Supported     |
| GameCube Ep3         | Supported      | Supported     | Partial (3)   | Supported     |
| XBOX Ep1&2           | Untested (4)   | Untested (4)  | Untested (4)  | Untested (4)  |
| Blue Burst           | Supported      | Supported     | Partial (5)   | Supported     |

*Notes:*
1. *DC support has only been tested with the US versions of PSO DC. Other versions probably don't work, but will be easy to add. Please submit a GitHub issue if you have a non-US DC version, and can provide a log from a connection attempt.*
2. *This version only supports the modem adapter, which Dolphin does not currently emulate, so it's difficult to test.*
3. *Episode 3 players can create and join games, but CARD battles are not implemented yet. Tournaments are also not supported.*
4. *newserv's implementation of PSOX is based on disassembly of the client executable; it has never been tested with a real client and most likely doesn't work.*
5. *Some basic features are not implemented in Blue Burst games, so the games are not very playable. A lot of work has to be done to get BB games to a playable state.*

## Usage

Currently newserv should build on macOS and Ubuntu. It will likely work on other Linux flavors too. It should work on Windows as well, but I haven't tested it - the build process could be very manual. Cygwin is likely the easiest Windows environment in which to build newserv.

There is a probably-not-too-old macOS ARM64 release on the newserv GitHub repository. You may need to install libevent manually even if you use this release (run `brew install libevent`).

If you're using an older AMD64 Mac, you're running Linux, or you just want to build newserv yourself, here's what you do:
1. Make sure you have CMake and libevent installed. (`brew install cmake libevent` on macOS, `sudo apt-get install cmake libevent-dev` on most Linuxes)
2. Build and install phosg (https://github.com/fuzziqersoftware/phosg).
3. Optionally, install resource_dasm (https://github.com/fuzziqersoftware/resource_dasm). This will enable newserv to send memory patches and load DOL files on PSO GC clients. PSO GC clients can play PSO normally on newserv without this.
4. Run `cmake . && make` in the newserv directory.

After building newserv or downloading a release, do this to set it up and use it:
1. In the system/ directory, make a copy of config.example.json named config.json, and edit it appropriately.
2. Run `./newserv` in the newserv directory. This will start the game server and run the interactive shell. You may need `sudo` if newserv's built-in DNS server is enabled.
3. Use the interactive shell to add a license. Run `help` in the shell to see how to do this.
4. If you plan to play PSO Blue Burst on newserv, set up the patch directory appropriately. See the "Client patch directories" section below.
5. Set your client's network settings appropriately and start an online game. See the "Connecting local clients" or "Connecting remote clients" section to see how to get your game client to connect.

### Installing quests

newserv automatically finds quests in the system/quests/ directory. To install your own quests, or to use quests you've saved using the proxy's set-save-files option, just put them in that directory and name them appropriately.

Standard quest files should be named like `q###-CATEGORY-VERSION.EXT`, battle quests should be named like `b###-VERSION.EXT`, and challenge quests should be named like `c###-VERSION.EXT`. The fields in each filename are:
- `###`: quest number (this doesn't really matter; it should just be unique for the PSO version)
- `CATEGORY`: ret = Retrieval, ext = Extermination, evt = Events, shp = Shops, vr = VR, twr = Tower, gov = Government (BB only), dl = Download (these don't appear during online play), 1p = Solo (BB only)
- `VERSION`: d1 = Dreamcast v1, dc = Dreamcast v2, pc = PC, gc = GameCube Episodes 1 & 2, gc3 = Episode 3, bb = Blue Burst
- `EXT`: file extension (see table below)

For example, the GameCube version of Lost HEAT SWORD is in two files named `q058-ret-gc.bin` and `q058-ret-gc.dat`. newserv knows these files are quests because they're in the system/quests/ directory, it knows they're for PSO GC because the filenames contain `-gc`, and it puts them in the Retrieval category because the filenames contain `-ret`.

There are multiple PSO quest formats out there; newserv supports most of them. It can also decode any known format to standard .bin/.dat format. Specifically:

| Format                    | Extension         | Supported online? | Offline decode option     |
|---------------------------|-------------------|-------------------|---------------------------|
| Compressed                | .bin/.dat         | Yes               | None (1)                  |
| Uncompressed              | .bind/.datd       | Yes               | --compress-data (2)       |
| Unencrypted GCI           | .bin.gci/.dat.gci | Yes               | --decode-gci=FILENAME     |
| Encrypted GCI with key    | .bin.gci/.dat.gci | Yes               | --decode-gci=FILENAME     |
| Encrypted GCI without key | .bin.gci/.dat.gci | No                | --decode-gci=FILENAME (3) |
| Encrypted DLQ             | .bin.dlq/.dat.dlq | Yes               | --decode-dlq=FILENAME     |
| QST                       | .qst              | Yes               | --decode-qst=FILENAME     |

*Notes:*
1. *This is the default format. You can convert these to uncompressed format like this: `newserv --decompress-data < FILENAME.bin > FILENAME.bind`*
2. *Similar to (1), to compress an uncompressed quest file: `newserv --compress-data < FILENAME.bind > FILENAME.bin`*
3. *If you know the encryption seed (serial number), pass it in as a hex string with the `--seed=` option. If you don't know the encryption seed, newserv will find it for you, which will likely take a long time.*

Episode 3 quests consist only of a .bin file - there is no corresponding .dat file. Episode 3 .bin files can be encoded in any of the formats described above, except .qst.

When newserv indexes the quests during startup, it will warn (but not fail) if any quests are corrupt or in unrecognized formats.

If you've changed the contents of the quests directory, you can re-index the quests without restarting the server by running `reload quests` in the interactive shell. The new quests will be available immediately, but any games with quests already in progress will continue using the old versions of the quests until those quests end.

All quests, including those originally in GCI or DLQ format, are treated as online quests unless their filenames specify the dl category. newserv allows players to download all quests, even those in non-download categories.

### Client patch directories

If you're not playing PSO Blue Burst on newserv, you can skip these steps.

newserv implements a patch server for PSO PC and PSO BB game data. Any file or directory you put in the system/patch-bb or system/patch-pc directories will be synced to clients when they connect to the patch server.

To make server startup faster, newserv caches the modification times, sizes, and checksums of the files in the patch directories. If the patch server appears to be misbehaving, try deleting the .metadata-cache.json file in the relevant patch directory to force newserv to recompute all the checksums. Also, in the case when checksums are cached, newserv may not actually load the data for a patch file until it's needed by a client. Therefore, modifying any part of the patch tree while newserv is running can cause clients to see an inconsistent view of it.

For BB clients, newserv reads some files out of the patch data to implement game logic, so it's important that certain game files are synchronized between the server and the client. newserv contains defaults for these files in the system/blueburst/map directory, but if these don't match the client's copies of the files, odd behavior will occur in games.

Specifically, the patch-bb directory should contain at least the data.gsl file and all map_*.dat files from the version of PSOBB that you want to play on newserv. You can copy these files out of the client's data directory from a clean installation, and put them in system/patch-bb/data.

### Memory patches and DOL files

Everything in this section requires resource_dasm to be installed, so newserv can use the PowerPC assembler and disassembler from its libresource_file library. If resource_dasm is not installed, newserv will still build and run, but these features will not be available.

You can put memory patches in the system/ppc directory with filenames like PatchName.patch.s and they will appear in the Patches menu for PSO GC clients that support patching. Memory patches are written in PowerPC assembly and are compiled when newserv is started. The PowerPC assembly system's features are documented in the comments in system/ppc/WriteMemory.s - this file is not a memory patch itself, but it describes how memory patches may be written and the restrictions that apply to them.

You can also put DOL files in the system/dol directory, and they will appear in the Programs menu. Selecting a DOL file there will load the file into the GameCube's memory and run it, just like the old homebrew loaders (PSUL and PSOload) did. For this to work, ReadMemoryWord.s, WriteMemory.s, and RunDOL.s must be present in the system/ppc directory. This has been tested on Dolphin but not on a real GameCube, so results may vary.

I mainly built the DOL loading functionality for documentation purposes. By now, there are many better ways to load homebrew code on an unmodified GameCube, but to my knowledge there isn't another open-source implementation of this method in existence.

### Using newserv as a proxy

If you want to play online on remote servers rather than running your own server, newserv also includes a PSO proxy. Currently this works with PSO GC and may work with PC and DC; it also works with some BB clients in specific situations.

To use the proxy for PSO DC, PC, or GC, add an entry to the corresponding ProxyDestinations dictionary in config.json, then run newserv and connect to it as normal (see below). You'll see a "Proxy server" option in the main menu, and you can pick which remote server to connect to.

To use the proxy for PSO BB, set the ProxyDestination-BB entry in config.json. If this option is set, it essentially disables the game server for all PSO BB clients - all clients will be proxied to the specified destination instead. Unfortunately, because PSO BB uses a different set of handlers for the data server phase and character selection, there's no in-game way to present the player with a list of options, like there is on PSO PC and PSO GC.

When you're on PSO DC, PC, or GC and are connected to a remote server through newserv's proxy, choosing the Change Ship or Change Block action from the lobby counter will send you back to newserv's main menu instead of the remote server's ship or block select menu. You can go back to the server you were just on by choosing it from the proxy server menu again.

The remote server will probably try to assign you a Guild Card number that doesn't match the one you have on newserv. On PSO DC, PC and GC, the proxy server rewrites the commands in transit to make it look like the remote server assigned you the same Guild Card number as you have on newserv, but if the remote server has some external integrations (e.g. forum or Discord bots), they will use the Guild Card number that the remote server believes it has assigned to you. The number assigned by the remote server is shown to you when you first connect to the remote server, and you can retrieve it in lobbies or during games with the $li command.

Some chat commands (see below) have the same basic function on the proxy server but have different effects or conditions. In addition, there are some server shell commands that affect clients on the proxy (run 'help' in the shell to see what they are). All proxy commands in the server shell only work when there's exactly one client connected through the proxy, since there isn't (yet) a way to say via the shell which session you want the command to apply to.

### Chat commands

The server's shell supports a variety of administration commands. If the interactive shell is enabled, you can enter these commands at any time, even if the prompt isn't visible. Run `help` in the server's shell to see all of the commands and how to use them.

newserv also supports a variety of commands players can use via the chat interface. Any chat message that begins with `$` is treated as a chat command. (If you actually want to send a chat message starting with `$`, type `$$` instead.)

Some commands only work on the game server and not on the proxy server. The chat commands are:

* Information commands
    * `$li`: Shows basic information about the lobby or game you're in. If you're on the proxy server, shows information about your connection instead (remote Guild Card number, client ID, etc.).
    * `$what` (game server only): Shows the type, name, and stats of the nearest item on the ground.

* Debugging commands (game server only)
    * `$dbgid`: Enable or disable high ID preference. When enabled, you'll be placed into the latest available slot in lobbies and games instead of the earliest. Can be useful for finding commands for which newserv doesn't handle client IDs properly.
    * `$gc`: Send your own Guild Card to yourself.
    * `$persist`: Enable or disable persistence for the current lobby or game. This determines whether the lobby/game is deleted when the last player leaves. You need the DEBUG permission in your user license to use this command because there are no game state checks when you do this. For example, if you make a game persistent, start a quest, then leave the game, the game can't be joined by anyone but also can't be deleted.

* Personal state commands
    * `$arrow <color-id>`: Changes your lobby arrow color.
    * `$secid <section-id>`: Sets your override section ID. After running this command, any games you create will use your override section ID for rare drops instead of your character's actual section ID. To revert to your actual section id, run `$secid` with no name after it.
    * `$rand <seed>`: Sets your override random seed (specified as a 32-bit hex value). This will make any games you create use the given seed for rare enemies. This also makes item drops deterministic in Blue Burst games hosted by newserv. On the proxy server, this command can cause desyncs with other players in the same game, since they will not see the overridden random seed. To remove the override, run `$rand` with no arguments.

* Blue Burst player commands (game server only)
    * `$bbchar <username> <password> <1-4>`: Use this command when playing on a non-BB version of PSO. If the username and password are correct, this command converts your current character to BB format and saves it on the server in the given slot. Any character already in that slot is overwritten.
    * `$edit <stat> <value>`: Modifies your character data.

* Game state commands (game server only)
    * `$maxlevel <level>`: Sets the maximum level for players to join the current game.
    * `$minlevel <level>`: Sets the minimum level for players to join the current game.
    * `$password <password>`: Sets the game's join password. To unlock the game, run `$password` with nothing after it.

* Cheat mode commands
    * `$cheat`: Enables or disables cheat mode for the current game. All other cheat mode commands do nothing if cheat mode is disabled. This command does nothing on the proxy server - cheat commands are always available there.
    * `$infhp` / `$inftp`: Enables or disables infinite HP or TP mode. Applies to only you. In infinite HP mode, one-hit KO attacks will still kill you.
    * `$warp <area-id>`: Warps yourself to the given area.
    * `$next` (game server only): Warps yourself to the next area.
    * `$swa`: Enables or disables switch assist. When enabled, the server will attempt to automatically unlock two-player doors in solo games if you step on both switches sequentially.
    * `$item <data>`: Sets the next item to be dropped from an enemy or box. Item codes are 16 hex bytes; at least 2 bytes must be specified, and all unspecified bytes are zeroes. If you are on the proxy server, you must be the game leader and not using Blue Burst for this command to work. On the game server, this command works for all versions, and you do not have to be the game leader.

* Configuration commands
    * `$event <event>`: Sets the current holiday event in the current lobby. Holiday events are documented in the "Using $event" item in the information menu. If you're on the proxy server, this applies to all lobbies and games you join, but only you will see the new event - other players will not.
    * `$allevent <event>` (game server only): Sets the current holiday event in all lobbies.
    * `$song <song-id>` (game server only, Episode 3 only): Plays a specific song in the current lobby.

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

Finally, the script takes an optional second argument that allows you to redirect the connection elsewhere (instead of the local machine). THis allows you to connect directly to remote servers if desired.

#### PSO PC

The version of PSO PC I have has the server addresses starting at offset 0x29CB34 in pso.exe. Using a hex editor, change those to "localhost" (without quotes) if you just want to connect to a locally-running newserv instance. Alternatively, you can add an entry to the Windows hosts file (C:\Windows\System32\drivers\etc\hosts) to redirect the connection to 127.0.0.1 (localhost) or any other IP address.

#### PSO GC on a real GameCube

You can make PSO connect to newserv by setting its default gateway and DNS server addresses to newserv's address. newserv's DNS server must be running on port 53 and must be accessible to the GameCube.

If you have PSO Plus or Episode III, it won't want to connect to a server on the same local network as the GameCube itself, as determined by the GameCube's IP address and subnet mask. In the old days, one way to get around this was to create a fake network adapter on the server (or use an existing real one) that has an IP address on a different subnet, tell the GameCube that the server is the default gateway (as above), and have the server reply to the DNS request with its non-local IP address. To do this with newserv, just set LocalAddress in the config file to a different interface. For example, if the GameCube is on the 192.168.0.x network and your other adapter has address 10.0.1.6, set newserv's LocalAddress to 10.0.1.6 and set PSO's DNS server and default gateway addresses to the server's 192.168.0.x address. This may not work on modern systems or on non-Windows machines - I haven't tested it in many years.

#### PSO GC on Dolphin

If you have BBA support via a tap interface, you may be able to just set the DNS server address (as you would on a real GameCube, above) and it may work. This does not work on macOS, but you can use the tapserver interface instead (below).

If you're using a version of Dolphin with tapserver support (currently only the macOS version), you can make it connect to a newserv instance running on the same machine via the tapserver interface. You do not need to install or run tapserver, and this works for all PSO versions without any of the dual-interface trickery described above. To do this:
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

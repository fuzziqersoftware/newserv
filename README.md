# newserv

newserv is a game server and proxy for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself; this data was originally created by Sega.

This project is a rewrite of a rewrite of a game server that I wrote many years ago. So far, it works well with PSO GC Episodes 1 & 2, and lobbies (but not games) are implemented on Episode 3. Some basic functionality works on PSO PC and PSO BB, but there are probably still some cases that lead to errors (which will disconnect the client). The proxy works well with PSO GC and PSO BB.

Feel free to submit GitHub issues if you find bugs or have feature requests. I'd like to make the server as stable and complete as possible, but I can't promise that I'll respond to issues in a timely manner.

## Future

This project is primarily for my own nostalgia; I offer no guarantees on how or when this project will advance.

Current known issues / missing features:
- Episode 3 battles aren't implemented.
- PSOBB is not well-tested and likely will disconnect or misbehave when clients try to use unimplemented features. GC is known to be stable and mostly complete; PC is not well-tested but is likely stable and complete as well.
- Patches currently are platform-specific but not version-specific. This makes them quite a bit harder to write and use properly.
- Find a way to silence audio in RunDOL.s. Some old DOLs don't reset audio systems at load time and it's annoying to hear the crash buzz when the GC hasn't actually crashed.
- Implement private and overflow lobbies.
- Enforce client-side size limits (e.g. for 60/62 commands) on the server side as well. (For 60/62 specifically, perhaps transform them to 6C/6D if needed.)
- Encapsulate BB server-side random state and make replays deterministic.

## Usage

Currently newserv should build on macOS and Ubuntu. It will likely work on other Linux flavors too. It should work on Windows as well, but I haven't tested it - the build process could be very manual. Cygwin is likely the easiest Windows environment in which to build newserv.

There is a probably-not-too-old macOS ARM64 release on the newserv GitHub repository. You may need to install libevent manually even if you use this release (run `brew install libevent`).

If you're using an older x64 Mac, you're running Linux, or you just want to build newserv yourself, here's what you do:
1. Make sure you have CMake and libevent installed. (`brew install cmake libevent` on macOS, `sudo apt-get install cmake libevent-dev` on most Linuxes)
2. Build and install phosg (https://github.com/fuzziqersoftware/phosg).
3. Optionally, install resource_dasm (https://github.com/fuzziqersoftware/resource_dasm). This will enable newserv to run patches and load DOL files on PSO GC clients. PSO GC clients can play PSO normally on newserv without this.
4. Run `cmake . && make` on the newserv directory.

After building newserv or downloading a release, do this to set it up and use it:
1. In the system/ directory, make a copy of config.example.json named config.json, and edit it appropriately.
2. Run `./newserv` in the newserv directory. This will start the game server and run the interactive shell. You may need `sudo` if newserv's built-in DNS server is enabled.
3. Use the interactive shell to add a license. Run `help` in the shell to see how to do this.

See the "Connecting local clients" or "Connecting remote clients" section to see how to get your game client to connect.

### Installing quests

newserv automatically finds quests in the system/quests/ directory. To install your own quests, or to use quests you've saved using the proxy's set-save-files option, just put them in that directory and name them appropriately.

Standard quest file names should be like `q###-CATEGORY-VERSION.EXT`; battle quests should be named like `b###-VERSION.EXT`, and challenge quests should be named like `c###-VERSION.EXT`. The fields in each filename are:
- `###`: quest number (this doesn't really matter; it should just be unique for the version)
- `CATEGORY`: ret = Retrieval, ext = Extermination, evt = Events, shp = Shops, vr = VR, twr = Tower, gov = Government (BB only), dl = Download (these don't appear during online play), 1p = Solo (BB only)
- `VERSION`: d1 = Dreamcast v1, dc = Dreamcast v2, pc = PC, gc = GameCube Episodes 1 & 2, gc3 = Episode 3, bb = Blue Burst
- `EXT`: file extension (bin, dat, bin.gci, dat.gci, bin.dlq, dat.dlq, or qst)

There are multiple PSO quest formats out there; newserv supports most of them. Specifically, newserv can use quests in any of the following formats:
- Compressed bin/dat format: These quests consist of two files with the same base name, a .bin file and a .dat file. (This is the format you'll get if you saved a quest with set-save-files.)
- Uncompressed bin/dat format: These quests consist of two files with the same base name, a .bind file and a .datd file.
- Unencrypted GCI format: These quests also consist of a .bin and .dat file, but an encoding is applied on top of them. The filenames should end in .bin.gci and .dat.gci. (Note that there also exists an encrypted GCI format, which newserv does not support.)
- Encrypted DLQ format: These quests also consist of a .bin and .dat file, but download quest encryption is applied on top of them. The filenames should end in .bin.dlq and .dat.dlq.
- QST format: These quests consist of only a .qst file, which contains both the .bin and .dat files within it.

When newserv indexes the quests during startup, it will warn (but not fail) if any quests are corrupt or in unrecognized formats.

If you've changed the contents of the quests directory, you can re-index the quests without restarting the server by running `reload quests` in the interactive shell.

All quests, including those originally in GCI or DLQ format, are treated as online quests unless their filenames specify the dl category. newserv allows players to download all quests, even those in non-download categories.

### Patches and DOL files

Everything in this section requires resource_dasm to be installed, so newserv can use the PowerPC assembler and disassembler from its libresource_file library. If resource_dasm is not installed, newserv will still build and run, but these features will not be available.

You can put patches in the system/ppc directory with filenames like PatchName.patch.s and they will appear in the Patches menu for PSO GC clients that support patching. Patches are written in PowerPC assembly and are compiled when newserv is started. The PowerPC assembly system's features are documented in the comments in system/ppc/WriteMemory.s - this file is not a patch itself, but it describes how patches may be written and the restrictions that apply to them.

You can also put DOL files in the system/dol directory, and they will appear in the Programs menu. Selecting a DOL file there will load the file into the GameCube's memory and run it, just like the old homebrew loaders (PSUL and PSOload) did. For this to work, ReadMemoryWord.s, WriteMemory.s, and RunDOL.s must be present in the system/ppc directory. This has been tested on Dolphin but not on a real GameCube, so results may vary.

I mainly built the DOL loading functionality for documentation purposes. By now, there are many better ways to load homebrew code on an unmodified GameCube, but to my knowledge there isn't another open-source implementation of this method in existence.

### Chat commands

The server's shell supports a variety of administration commands. If the interactive shell is enabled, you can enter these commands at any time, even if the prompt isn't visible. Run `help` in the server's shell to see all of the commands and how to use them.

newserv also supports a variety of commands players can use via the chat interface. Any chat message that begins with `$` is treated as a chat command. (If you actually want to send a chat message starting with `$`, type `$$` instead.)

Some commands only work on the game server and not on the proxy server. The chat commands are:

* Information commands
    * `$li`: Shows basic information about the lobby or game you're in. If you're on the proxy server, shows information about your connection instead (remote Guild Card number, client ID, etc.).
    * `$what` (game server only): Shows the type, name, and stats of the nearest item on the ground.

* Personal state commands
    * `$arrow <color-id>`: Changes your lobby arrow color.
    * `$secid <section-id>`: Sets your override section ID. After running this command, any games you create will use your override section ID for rare drops instead of your character's actual section ID. To revert to your actual section id, run `$secid` with no name after it.
    * `$rand <seed>`: Sets your override random seed (specified as a 32-bit hex value). This will make any games you create use the given seed for rare enemies. This also makes item drops deterministic in Blue Burst games hosted by newserv. On the proxy server, this command can cause desyncs with other players in the same game, since they will not see the overridden random seed. To remove the override, run `$rand` with no arguments.

* Blue Burst player commands (game server only)
    * `$bbchar <username> <password> <1-4>`: Use this command when playing on a non-BB version of PSO. If the username and password are correct, this command converts your current character to BB format and saves it on the server in the given slot.
    * `$edit <stat> <value>`: Modifies your character data.
    * `$item <data>`: Sets the next item to be dropped from an enemy or box.

* Game state commands (game server only)
    * `$maxlevel <level>`: Sets the maximum level for players to join the current game.
    * `$minlevel <level>`: Sets the minimum level for players to join the current game.
    * `$password <password>`: Sets the game's join password. To unlock the game, run `$password` with nothing after it.

* Cheat mode commands
    * `$cheat`: Enables or disables cheat mode for the current game. All other cheat mode commands do nothing if cheat mode is disabled. This command does nothing on the proxy server - cheat commands are always available there, but are off by default.
    * `$infhp` / `$inftp`: Enables or disables infinite HP or TP mode. Applies to only you. In infinite HP mode, one-hit KO attacks will still kill you.
    * `$warp <area-id>`: Warps yourself to the given area.
    * `$next` (game server only): Warps yourself to the next area.
    * `$swa`: Enables or disables switch assist. When enabled, the server will attempt to automatically unlock two-player doors in solo games if you step on both switches sequentially.

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

### Using newserv as a proxy

If you want to play online on remote servers rather than running your own server, newserv also includes a PSO proxy. Currently this works with PSO GC and may work with PC; it also works with some BB clients in specific situations.

To use the proxy for PSO PC or PSO GC, add an entry to the ProxyDestinations dictionary in config.json, then run newserv and connect to it as normal (see below). You'll see a "Proxy server" option in the main menu, and you can pick which remote server to connect to.

To use the proxy for PSO BB, set the ProxyDestination-BB entry in config.json. If this option is set, it essentially disables the game server for all PSO BB clients - all clients will be proxied to the specified destination instead. Unfortunately, because PSO BB uses a different set of handlers for the data server phase and character selection, there's no in-game way to present the player with a list of options, like there is on PSO PC and PSO GC.

A few things to be aware of when using the proxy server:
- On PC and GC, using the Change Ship or Change Block actions from the lobby counter will send you back to newserv's main menu, not the remote server's ship select. You can go back to the server you were just on by choosing it from the proxy server menu again.
- The remote server will probably try to assign you a Guild Card number that doesn't match the one you have on newserv. On PC and GC, the proxy server rewrites the commands in transit to make it look like the remote server assigned you the same Guild Card number as you have on newserv, but if the remote server has some external integrations (e.g. forum or Discord bots), they will use the Guild Card number that the remote server believes it has assigned to you. The number assigned by the remote server is shown to you when you first connect to the remote server, and you can retrieve it in lobbies or during games with the $li command.
- There are shell commands that affect clients on the proxy (run 'help' in the shell to see what they are). All proxy commands in the shell only work when there's exactly one client connected through the proxy, since there isn't (yet) a way to say via the shell which session you want to affect.

### Connecting local clients

#### PSO PC

The version of PSO PC I have has the server addresses starting at offset 0x29CB34 in pso.exe. Using a hex editor, change those to "localhost" (without quotes) if you just want to connect to a locally-running newserv instance. Alternatively, you can add an entry to the Windows hosts file (C:\Windows\System32\drivers\etc\hosts) to redirect the connection to 127.0.0.1 (localhost) or any other IP address.

#### PSO GC on a real GameCube

You can make PSO connect to newserv by setting its default gateway and DNS server addresses to newserv's address. newserv's DNS server must be running on port 53 and accessible.

If you have PSO Plus or Episode III, it won't want to connect to a server on the same local network as the GameCube itself, as determined by the GameCube's IP address and subnet mask. In the old days, one way to get around this was to create a fake network adapter on the server (or use an existing real one) that has an IP address on a different subnet, tell the GameCube that the server is the default gateway, and have the server reply to the DNS request with its non-local IP address. To do this with newserv, just set LocalAddress in the config file to a different interface. For example, if the GameCube is on the 192.168.0.x network and your other adapter has address 10.0.1.6, set newserv's LocalAddress to 10.0.1.6 and set PSO's DNS server and default gateway addresses to the server's 192.168.0.x address. This may not work on modern systems or on non-Windows machines - I haven't tested it in many years.

#### PSO GC on Dolphin

If you have BBA support via a tap interface, you may be able to just set the DNS server address (as you would on a real GameCube, above) and it may work. This does not work on macOS, but you can use the tapserver interface instead (below).

If you're using a version of Dolphin with tapserver support (currently only the macOS version), you can make it connect to a newserv instance running on the same machine via the tapserver interface. This works for all PSO versions, including Plus and Episode III, without any of the dual-interface trickery described above. To do this:
- Set Dolphin's BBA type to tapserver (Config -> GameCube -> SP1).
- Enable newserv's IP stack simulator according to the comments in config.json, and start newserv. You do not need to install or run tapserver.
- In PSO, you have to configure the network settings manually (DHCP doesn't work), but the actual values don't matter as long as they're valid IP addresses. Example values:
  - IP address: `10.0.1.5`
  - Subnet mask: `255.255.255.0`
  - Default gateway: `10.0.1.1`
  - DNS server address 1: `10.0.1.1`
  - Leave everything else blank
- Start an online game.

### Connecting external clients

If you want to accept connections from outside your local network, you'll need to set ExternalAddress to your public IP address in the configuration file, and you'll likely need to open some ports in your router's NAT configuration - specifically, all the TCP ports listed in PortConfiguration in config.json.

For GC clients, you'll have to use newserv's built-in DNS server or set up your own DNS server as well. If you want external clients to be able to use your DNS server, you'll have to forward UDP port 53 to your newserv instance. Remote players can then connect to your server by entering your DNS server's IP address in their client's network configuration.

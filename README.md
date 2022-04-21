# newserv

newserv is a game server and proxy for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself; this data was originally created by Sega.

This project is a rewrite of a rewrite of a game server that I wrote many years ago. So far, it works well with PSO GC Episodes 1 & 2, and lobbies (but not games) are implemented on Episode 3. Some basic functionality works on PSO PC, but there are probably still some cases that lead to errors (which will disconnect the client). newserv is based on an older project of mine that supported BB as well, but I no longer have a way to test BB, so the implementation here probably doesn't work for it.

Feel free to submit GitHub issues if you find bugs or have feature requests. I'd like to make the server as stable and complete as possible, but I can't promise that I'll respond to issues in a timely manner.

## Future

This project is primarily for my own nostalgia; I offer no guarantees on how or when this project will advance.

Current known issues / missing features:
- Test all the communication features (info board, simple mail, card search, etc.)
- The trade window isn't implemented yet.
- PSO PC and PSOBB are essentially entirely untested. Only GC is fairly well-tested.
- Add all the chat commands that khyller used to have. (Most, but not all, currently exist in newserv.)

## Usage

Currently this code should build on macOS and Ubuntu. It will likely work on other Linux flavors too, but probably will not work on Windows.

So, you've read all of the above and you want to try it out? Here's what you do:
- Make sure you have CMake and libevent installed.
- Build and install phosg (https://github.com/fuzziqersoftware/phosg).
- Run `cmake . && make`.
- Rename system/config.example.json to system/config.json, and edit it appropriately.
- Run `./newserv` in the newserv directory. This will start the game server and run the interactive shell. You may need `sudo` if newserv's built-in DNS server is enabled.
- Use the interactive shell to add a license. Run `help` in the shell to see how to do this.

### Installing quests

newserv automatically finds quests in the system/quests directory. To install your own quests, or to use quests you've saved using the proxy's set-save-files option, just put them in that directory and name them appropriately.

There are multiple PSO quest formats out there; newserv supports most of them. Specifically, newserv can use quests in any of the following formats:
- bin/dat format: These quests consist of two files with the same base name, a .bin file and a .dat file.
- Unencrypted GCI format: These quests also consist of a .bin and .dat file, but an encoding is applied on top of them. The filenames should end in .bin.gci and .dat.gci.
- Encrypted DLQ format: These quests also consist of a .bin and .dat file, but downlaod quest encryption is applied on top of them. The filenames should end in .bin.dlq and .dat.dlq.
- QST format: These quests consist of only a .qst file, which contains both the .bin and .dat files within it.

Standard quest file names should be like `q###-CATEGORY-VERSION.EXT`; battle quests should be named like `b###-VERSION.EXT`, and challenge quests should be named like `c###-VERSION.EXT`. The fields in each filename are:
- `###`: quest number (this doesn't really matter; it should just be unique for the version)
- `CATEGORY`: ret = Retrieval, ext = Extermination, evt = Events, shp = Shops, vr = VR, twr = Tower, dl = Download (these don't appear during online play), 1p = Solo (BB only)
- `VERSION`: d1 = DreamCast v1, dc = DreamCast v2, pc = PC, gc = GameCube Episodes 1 & 2, gc3 = Episode 3, bb = Blue Burst
- `EXT`: file extension (bin, dat, bin.gci, dat.gci, bin.dlq, dat.dlq, or qst)

When newserv indexes the quests during startup, it will warn (but not fail) if any quests are corrupt or in unrecognized formats.

If you've changed the contents of the quests directory, you can re-index the quests without restarting the server by running `reload quests` in the interactive shell.

All quests, including those originally in GCI or DLQ format, are treated as online quests unless their filenames specify the dl category. newserv allows players to download all quests, even those in non-download categories.

### Using newserv as a proxy

If you want to play online on remote servers rather than running your own server, newserv also includes a PSO proxy. Currently this works with PSO GC and may work with PC, but not with BB.

To use the proxy, add an entry to the ProxyDestinations dictionary in config.json, then run newserv and connect to it as normal (see below). You'll see a "Proxy server" option in the main menu, and you can pick which remote server to connect to.

A few things to be aware of when using the proxy server:
- There are shell commands that affect clients on the proxy (run 'help' in the shell to see what they are). All proxy commands only work when there's exactly one client connected through the proxy, since there isn't (yet) a way to say via the shell which session you want to affect.
- Using the "change ship" or "change block" actions from the lobby counter will bring you back to newserv's main menu, not the remote server's ship select. You can go back to the server you were just on by choosing it from newserv's proxy server menu again.
- The proxy server blocks chat commands that look like newserv commands by default, but you can change this with the `set-chat-safety off` shell command if needed.

### Connecting local clients

If you're running PSO on a real GameCube, you can make it connect to newserv by setting its default gateway and DNS server addresses to newserv's address. Note that newserv's DNS server is disabled by default; you'll have to enable it in config.json.

If you have PSO Plus or Episode III, it won't want to connect to a server on the same local network as the GameCube itself, as determined by the GC's IP address and subnet mask. In the old days, one way to get around this was to create a fake network adapter on the server (or use an existing real one) that has an IP address on a different subnet, tell the GameCube that the server is the default gateway, and have the server reply to the DNS request with its non-local IP address. To do this with newserv, just set LocalAddress in the config file to a different interface. For example, if the GameCube is on the 192.168.0.x network and your other adapter has address 10.0.1.6, set newserv's LocalAddress to 10.0.1.6 and set PSO's DNS server and default gateway addresses to the server's 192.168.0.x address. This may not work on modern systems or on non-Windows machines - I haven't tested it in many years.

If you're emulating PSO using a version of Dolphin with tapserver support (currently only the macOS version), you can make it connect to a newserv instance running on the same machine via the tapserver interface. This works for all PSO versions, including Plus and Episode III, without the trickery described above. To do this:
- Set the BBA type to tapserver (Config -> GameCube -> SP1).
- Enable the IP stack simulator according to the comments in config.json, and start newserv. You do not need to install or run tapserver.
- In PSO, you have to configure the network settings manually (DHCP doesn't work), but the actual values don't matter as long as they're valid IP addresses. Example values:
  - IP address: `10.0.1.5`
  - Subnet mask: `255.255.255.0`
  - Default gateway: `10.0.1.1`
  - DNS server address 1: `10.0.1.1`
  - Leave everything else blank
- Start an online game.

### Connecting external clients

If you want to accept connections from outside your local network, you'll need to set ExternalAddress to your public IP address in the configuration file, and you'll likely need to open some ports in your router's NAT configuration. You'll need to open the following ports depending on which client versions you want to be able to connect:

    PSO PC           9100, 9300, 9420, 10000
    PSO GC 1.0 JP    9000, 9421
    PSO GC 1.1 JP    9001, 9421
    PSO GC Ep3 JP    9003, 9421
    PSO GC 1.0 US    9100, 9421
    PSO GC Ep3 US    9103, 9421
    PSO GC 1.0 EU    9200, 9421
    PSO GC 1.1 EU    9201, 9421
    PSO GC Ep3 EU    9203, 9421
    PSO BB           9422, 11000, 12000, 12004, 12005, 12008

If you want to allow external clients to use the proxy server, you'll need to open more ports:

    PSO PC                  9520
    PSO GC (all versions)   9521
    PSO BB                  9522

For GC clients, you'll have to use newserv's built-in DNS server or set up your own DNS server as well. Remote players can connect to your server by entering your DNS server's IP address in their client's network configuration. If you use newserv's built-in DNS server, you'll also need to forward UDP port 53 to your newserv instance.

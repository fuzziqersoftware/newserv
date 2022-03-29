# newserv

newserv is a game server and proxy for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself; this data was originally created by Sega.

This project is a rewrite of a rewrite of a game server that I wrote many years ago. So far, it works well with PSO GC Episodes 1 & 2, and lobbies (but not games) are implemented on Episode 3. newserv is based on an older project of mine that supported other versions (PC and BB), but I no longer have a way to test those versions, so the implementation here probably doesn't work for them.

Feel free to submit GitHub issues if you find bugs or have feature requests. I'd like to make the server as stable and complete as possible, but I can't promise that I'll respond to issues in a timely manner.

## History

In ages long past (probably 2004? I honestly can't remember), I wrote a proxy for PSO, which I named khyps. This haphazardly-glued-together mess of Windows GUI code and socket programming provided an interface to insert commands into the connection between PSO and its server, enabling some fun new features. Importantly, it also automatically blocked malformed commands which would have crashed the client, providing a safe way to navigate the wasteland that the official Sega servers had turned into after the Action Replay enable code for the game was released.

khyps soon reached "maturity" and became uninteresting, so in 2005 I began writing a PSO server. This project became known as khyller, evolving into a full-featured environment supporting all versions of the game that I had access to - PC, GC, and BB. But as this evolution occurred, the code became increasingly ugly and hard to work with, littered with debugging filth that I never cleaned up and odd coding patterns that I had picked up over the years.

Sometime in 2006 or 2007, I abandoned khyller and rebuilt the entire thing from scratch, resulting in newserv. But this newserv was not the project you're looking at now; 2007's newserv was substantially cleaner in code than khyller but was still quite ugly, and it lacked a few of the more esoteric features I had originally written (for example, the ability to convert any quest into a download quest). I felt better about working with this code, but it still had some stability problems. It turns out that 2007's newserv's concurrency implementation was simply incorrect - I had derived the concept of a mutex myself (before taking any real computer engineering classes) but implemented it incorrectly. No wonder newserv would randomly crash after running seemingly fine for a few days.

A little-known fact is that no version of khyller or newserv was ever tested with the DreamCast versions of PSO. Both projects claimed to support them, but the DC server implementations were based only on chat conversations (likely now lost to time) with other people in the community who had done research on the DC version.

Sometime in October 2018, I had some random cause to reminisce. I looked back in my old code archives and came across newserv. Somehow inspired, I spent a weekend and a couple more evenings rewriting the entire project again, cleaning up ancient patterns I had used eleven years ago, replacing entire modules with simple STL containers, and eliminating even more support files in favor of configuration autodetection. The code is now suitably modern and it no longer has insidious concurrency bugs because it's no longer concurrent - the server is now entirely event-driven.

## Future

This project is primarily for my own nostalgia; I offer no guarantees on how or when this project will advance.

Current known issues / missing features:
- Download quests are mostly implemented, but the client doesn't always accept them. It's probably a format issue in file generation.
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

For GC clients, you'll have to use newserv's built-in DNS server or set up your own DNS server as well. Remote players can connect to your server by entering your DNS server's IP address in their client's network configuration. If you use newserv's built-in DNS server, you'll also need to forward UDP port 53 to your newserv instance.

### Using newserv as a proxy

If you want to play online on remote servers rather than running your own server, newserv also includes a PSO proxy. Currently this works with PSO GC and may work with PC, but not with BB.

Run newserv like `./newserv --proxy-destination=1.1.1.1` (replace the IP address appropriately for the server you want to connect to). This works for normal clients (using the connection parameters in config.json), as well as tapserver clients.

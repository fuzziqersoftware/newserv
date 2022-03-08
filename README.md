# newserv

newserv is a game server for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself; this data was originally created by Sega.

This project is a rewrite of a rewrite of a game server that I wrote many years ago. In its current state, do not expect this server to work well - it's not very thoroughly tested and probably won't be anytime soon. But with that said, newserv does handle a single player on PSO GameCube pretty well - lobbies work, games work, quests work, chat commands (including cheat modes) work, things don't break. I haven't tested it with multiple players yet, but there are probably only minor bugs. newserv probably doesn't work at all for other versions of PSO, since I haven't tested them yet.

## History

In ages long past (probably 2004? I honestly can't remember), I wrote a proxy for PSO, which I named khyps. This haphazardly-glued-together mess of Windows GUI code and socket programming provided an interface to insert commands into the connection between PSO and its server, enabling some fun new features. Importantly, it also automatically blocked malformed commands which would have crashed the client, providing a safe way to navigate the wasteland that the official Sega servers had turned into after the Action Replay enable code for the game was released.

khyps soon reached "maturity" and became uninteresting, so in 2005 I began writing a PSO server. This project became known as khyller, evolving into a full-featured environment supporting all versions of the game that I had access to - PC, GC, and BB. But as this evolution occurred, the code became increasingly ugly and hard to work with, littered with debugging filth that I never cleaned up and odd coding patterns that I had picked up over the years.

Sometime in 2006 or 2007, I abandoned khyller and rebuilt the entire thing from scratch, resulting in newserv. But this newserv was not the project you're looking at now; 2007's newserv was substantially cleaner in code than khyller but was still quite ugly, and it lacked a few of the more esoteric features I had originally written (for example, the ability to convert any quest into a download quest). I felt better about working with this code, but it still had some stability problems. It turns out that 2007's newserv's concurrency implementation was simply incorrect - I had derived the concept of a mutex myself (before taking any real computer engineering classes) but implemented it incorrectly. No wonder newserv would randomly crash after running seemingly fine for a few days.

A little-known fact is that no version of khyller or newserv was ever tested with the DreamCast versions of PSO. Both projects claimed to support them, but the DC server implementations were based only on chat conversations (likely now lost to time) with other people in the community who had done research on the DC version.

Sometime in October 2018, I had some random cause to reminisce. I looked back in my old code archives and came across newserv. Somehow inspired, I spent a weekend and a couple more evenings rewriting the entire project again, cleaning up ancient patterns I had used eleven years ago, replacing entire modules with simple STL containers, and eliminating even more support files in favor of configuration autodetection. The code is now suitably modern and it no longer has insidious concurrency bugs because it's no longer concurrent - the server is now entirely event-driven.

## Future

This project is primarily for my own nostalgia. Feel free to peruse if you'd like. I offer no guarantees on how or when this project will advance, or even if this will ever happen.

## Usage

Currently this code should build on macOS and Ubuntu. It might build on other Linux flavors, but don't expect it to work on Windows at all.

So, you've read all of the above and you want to try it out? Here's what you do:
- Make sure you have CMake and libevent installed.
- Build and install phosg (https://github.com/fuzziqersoftware/phosg).
- Run `cmake . && make`.
- Edit system/config.json to your liking.
- Run `./newserv` in the newserv directory. This will start the game server and run the interactive shell. (You can disable the interactive shell later by editing config.json.) You may need `sudo` if newserv's built-in DNS server is enabled.
- Use the interactive shell to add a license. Run `help` in the shell to see how to do this.

### Connecting local clients

If you're running PSO on a real GameCube, you can make it connect to newserv by setting its default gateway and DNS server addresses to newserv's address. Note that newserv's DNS server is disabled by default; you'll have to enable it in config.json.

If you're emulating PSO GC using Dolphin on macOS (like I am), you can make it connect to a newserv instance running on the same machine by doing this:
- Use a build of Dolphin that has tapserver support.
- Enable the IP stack simulator according to the comments in config.json, and start newserv.
- In PSO, you have to configure the network settings manually, but the actual values don't matter as long as they're valid IP addresses. Example values:
  - IP address: `10.0.1.5`
  - Subnet mask: `255.255.255.0`
  - Default gateway: `10.0.1.1`
  - DNS server address 1: `10.0.1.1`
- Start an online game.

This setup works for all PSO versions, including Plus and Episode III.

If you want to play online on remote servers, newserv also includes a PSO proxy server. Run newserv like `./newserv --proxy-destination=1.1.1.1` (replace the IP address appropriately for the server you want to connect to). Currently this works with PSO PC and GC, but not with BB.

### Connecting external clients

If you want to accept connections from outside your local network, you'll likely need to open some ports in your router's NAT configuration. You'll need to open the following ports depending on which client versions you want to be able to connect:

    PSO PC         9100, 9300, 9420, 10000
    PSO GC 1.0 JP  9000, 9421
    PSO GC 1.1 JP  9001, 9421
    PSO GC Ep3 JP  9003, 9421
    PSO GC 1.0 US  9100, 9421
    PSO GC Ep3 US  9103, 9421
    PSO GC 1.0 EU  9200, 9421
    PSO GC 1.1 EU  9201, 9421
    PSO GC Ep3 EU  9203, 9421
    PSO BB         9422, 11000, 12000, 12004, 12005, 12008

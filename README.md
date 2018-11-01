# newserv

newserv is a game server for Phantasy Star Online (PSO).

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself; this data was originally created by Sega.

This project is a rewrite of a rewrite of a game server that I wrote many years ago. In its current state, do not expect this server to work - it builds on Mac OS, but I haven't found the time to set up a test environment where I can make the game connect to newserv.

## History

In ages long past (probably 2005? I honestly can't remember), I began writing a PSO server. This project became known as khyller, evolving into a full-featured environment supporting all versions of the game that I had access to - PC, GC, and BB. But as this evolution occurred, the code became increasingly ugly and hard to work with, littered with debugging filth that I never cleaned up and odd coding patterns that I had picked up over the years.

Sometime in 2006 or 2007, I abandoned khyller and rebuilt the entire thing from scratch, resulting in newserv. But this newserv was not the project you're looking at now; 2007's newserv was substantially cleaner in code than khyller but was still quite ugly, and it lacked a few of the more esoteric features I had originally written (for example, the ability to convert any quest into a download quest). I felt better about working with this code, but it still had some stability problems. It turns out that 2007's newserv's concurrency implementation was simply incorrect - I had derived the concept of a mutex myself (before taking any real computer engineering classes) but implemented it incorrectly. No wonder newserv would randomly crash after running seemingly fine for a few days.

Last weekend (October 2018), I had some random cause to reminisce. I looked back in my old code archives and came across newserv. Somehow inspired, I spent a weekend and a couple more evenings rewriting the entire project again, cleaning up ancient patterns I had used eleven years ago, replacing entire modules with simple STL containers, and eliminating even more support files in favor of configuration autodetection. The code is now suitably modern and the concurrency primitives it uses are correct (thought I haven't audited where exactly they're used; there are likely some missing lock contexts still).

## Future

Really, this project is mostly for my own nostalgia. Feel free to peruse if you'd like. If I'm suitably inspired again, I may boot up my copies of PSO and play around with it, and I'm sure I'll find numerous bugs the first time I do so. But I offer no guarantees on when this will happen, or if it will happen at all.

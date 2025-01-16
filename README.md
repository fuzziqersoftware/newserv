# newserv <img align="right" src="static/s-newserv.png" />

newserv is a game server, proxy, and reverse-engineering tool for Phantasy Star Online (PSO). **To quickly get started using newserv, just read the [server setup](#server-setup) and [how to connect](#how-to-connect) sections.**

This project includes code that was reverse-engineered by the community in ages long past, and has been included in many projects since then. It also includes some game data from Phantasy Star Online itself, which was originally created by Sega.

Feel free to submit GitHub issues if you find bugs or have feature requests. I'd like to make the server as stable and complete as possible, but I can't promise that I'll respond to issues in a timely manner, because this is a personal project undertaken primarily for the fun of reverse-engineering. If you want to contribute to newserv yourself, pull requests are welcome as well.

See TODO.md for a list of known issues and future work I've curated, or go to the GitHub issue tracker for issues and requests submitted by the community.

**Table of contents**
* Background
    * [History](#history)
    * [Other server projects](#other-server-projects)
    * [Developer information](#developer-information)
    * [Using newserv in other projects](#using-newserv-in-other-projects)
* [Compatibility](#compatibility)
* Setup
    * [Server setup](#server-setup)
    * [Client patch directories for PC and BB](#client-patch-directories)
    * [How to connect](#how-to-connect)
* Features and configuration
    * [User accounts](#user-accounts)
    * [Installing quests](#installing-quests)
    * [Item tables and drop modes](#item-tables-and-drop-modes)
    * [Cross-version play](#cross-version-play)
    * [Server-side saves](#server-side-saves)
    * [Episode 3 features](#episode-3-features)
    * [Memory patches, client functions, and DOL files](#memory-patches-client-functions-and-dol-files)
    * [Using newserv as a proxy](#using-newserv-as-a-proxy)
    * [Chat commands](#chat-commands)
    * [REST API](#rest-api)
* [Non-server features](#non-server-features)

# History

The history of this project essentially mirrors my development as a software engineer from the beginning of my hobby until now. If you don't care about the story, skip to the "Compatibility" or "Setup" sections below.

I originally purchased PSO GC when I heard about PSUL, and wanted to play around with running homebrew on my GameCube. This pathway eventually led to [GCARS-CS](https://github.com/fuzziqersoftware/gcars-cs), but that's another story.

<img align="left" src="static/s-khyps.png" /> After playing PSO for a while, both offline and online, I wrote a proxy called Khyps sometime in 2003. This was back in the days of the official Sega servers, where vulnerabilities weren't addressed in a timely manner or at all. It was common for malicious players using their own proxies or Action Replay codes (a story for another time) to send invalid commands that the servers would blindly forward, and cause the receiving clients to crash. These crashes were more than simply inconvenient; they could also corrupt your save data, destroying the hours of work you may have put into hunting items and leveling up your character.

For a while it was essentially necessary to use a proxy to go online at all, so the proxy could block these invalid commands. Khyps was designed primarily with this function in mind, though it also implemented some convenient cheats, like the ability to give yourself or other players infinite HP and allow you to teleport to different places without using an in-game teleporter.

<img align="left" src="static/s-khyller.png" /> After Khyps I took on the larger challenge of writing a server, which resulted in Khyller sometime in 2005. This was the first server of any type I had ever written. This project eventually evolved into a full-featured environment supporting all versions of the game that I had access to - at the time, PC, GC, and BB. (However, I suspect from reading the ancient source files that Khyller's BB support was very buggy.) As Khyller evolved, the code became increasingly cumbersome, littered with debugging filth that I never cleaned up and odd coding patterns I had picked up over the years. My understanding of the C++ language was woefully incomplete as well (as opposed to now, when it is still incomplete but not woefully so), which resulted in Khyller being essentially a C project that had a couple of classes in it.

<img align="left" src="static/s-aeon.png" /> Sometime in 2006 or 2007, I abandoned Khyller and rebuilt the entire thing from scratch, resulting in Aeon. Aeon was substantially cleaner in code than Khyller but still fairly hard to work with, and it lacked a few of the more arcane features I had originally written (for example, the ability to convert any quest into a download quest). In addition, the code still had some stability problems... it turns out that Aeon's concurrency primitives were simply incorrect. I had derived the concept of a mutex myself, before taking any real computer engineering classes, but had implemented it incorrectly. I made the race window as small as possible, but Aeon would still randomly crash after running seemingly fine for a few days.

At the time of its inception, Aeon was also called newserv, and you may find some beta releases floating around the Internet with filenames like `newserv-b3.zip`. I had released betas 1, 2, and 3 before I released the entire source of beta 5 and stopped working on the project when I went to college. This was around the time when I switched from writing software primarily on Windows to primarily on macOS and Linux, so Aeon beta 5 was the last server I wrote that specifically targeted Windows. (newserv, which you're looking at now, is a bit tedious to compile on Windows but does work.)

<img align="left" src="static/s-newserv.png" /> After a long hiatus from PSO and much professional and personal development in my technical abilities, I was reminiscing sometime in October 2018 by reading my old code archives. Somehow inspired when I came across Aeon, I spent a weekend and a couple more evenings rewriting the entire project again, cleaning up ancient patterns I had used eleven years ago, replacing entire modules with simple STL containers, and eliminating even more support files in favor of configuration autodetection. The code is now suitably modern and stable, and I'm not embarrassed by its existence, as I am by Aeon beta 5's source code and my archive of Khyller (which, thankfully, no one else ever saw).

## Other server projects

Independently of this project, there are many other PSO servers out there. Those that I know of that are (or were) public are listed here in approximate chronological order:

* (Early 2000s) **[Schtserv](https://schtserv.com/)**: The first public-access PSO server; written in Delphi by Schthack. Still active and popular as of this writing (early 2024). Schtserv is also the only other unofficial server to support all versions of PSO, including Episode 3.
* (2005) **Khyller**: An early attempt of mine to support PSO PC, GC, and BB. See above for more details.
* (2006) **Aeon**: My second attempt. Better than Khyller, but still unreliable.
* (2008) **Tethealla**: A fairly extensive implementation of PSOBB, written in C by Sodaboy. The public version of Tethealla has been [officially disowned](https://www.pioneer2.net/community/threads/tethealla-server-forums-removal.26365/) (as it is now more than 15 years old), but closed-source development continues. [Ephinea](https://ephinea.pioneer2.net/), currently the most popular PSOBB server, is the continuation of this project. Several other modern PSOBB servers are forks of the initial public version of Tethealla as well.
* (2008) **[Sylverant](https://sylverant.net/)** [(source)](https://sourceforge.net/projects/sylverant/): The second public-access PSO server; written in C by BlueCrab. Still active and popular as of this writing (early 2024).
* (2015) **[Archon](https://github.com/dcrodman/archon)**: A PSOBB server written in Go by Drew Rodman.
* (2015) **[Idola](https://github.com/HybridEidolon/idolapsoserv)**: A PSOBB server written in Rust by HybridEidolon. Functionality status unknown; the project has been archived.
* (2017) **[Aselia](https://github.com/Solybum/Aselia)**: A PSOBB server written written in C# by Soly. It seems this was planned to be open-source at some point, but that has not (yet) happened.
* (2018) **newserv**: This project right here.
* (2019) **[Mechonis](https://gitlab.com/sora3087/mechonis)**: A PSOBB server with a microservice architecture written in TypeScript by TrueVision.
* (2021) **[Phantasmal World](https://github.com/DaanVandenBosch/phantasmal-world)**: A set of PSO tools, including a web-based model viewer and quest builder, and a PSO server, written by Daan Vanden Bosch.
* (2021) **[Elseware](http://git.sharnoth.com/jake/elseware)**: A PSOBB server written in Rust by Jake.

## Developer information

There is a lot of code in this project that could be useful as a reference. Some of the more notable files are:
* **src/CommandFormats.hh**: Complete listing of all network commands used in all known versions of the game, and their formats
* **src/DCSerialNumbers.hh/cc**: PSO DC serial number validation algorithm and serial number generator
* **src/ItemData.hh**: Item format reference
* **src/ItemCreator.hh/cc**: Reverse-engineered item generator from Episodes 1&2 (used for all versions)
* **src/ItemParameterTable.hh**: Format of many structures in ItemPMT.prs
* **src/Map.hh/cc**: Map file (.dat) structure and reverse-engineered Challenge Mode random enemy generation algorithm
* **src/QuestScript.cc**: Complete listing of all quest opcodes on all versions, along with their arguments and behavior
* **src/SaveFileFormats.hh**: Definitions of save file structures for all versions
* **src/Episode3/DataIndexes.hh**: Episode 3 file structures, including card definition format and map/quest format
* **system/item-tables/names-v4.json**: Names of all items, indexed by the first 3 bytes of data1

## Using newserv in other projects

There is a fair amount of code in this project that could potentially be useful to other projects. You are free to use code from newserv in your own open-source projects; the only condition is that the contents of the LICENSE file must be included in your project if you use code from newserv. Your project does not also have to use the MIT license; you can use any license you want.

If you want to use parts of newserv in your project, there are two easy ways to do so with proper licensing:
* If you're using a lot of code from newserv, you can put a copy of newserv's LICENSE file in your repository alongside your own license file, or include the contents of newserv's license in your own license file.
* If you're only using a few files from newserv, you can copy and paste the contents of the LICENSE file into a comment at the beginning of each copied file.

# Compatibility

newserv supports all known versions of PSO, including various development prototypes. This table lists all versions that newserv supports. (NTE stands for Network Trial Edition; the GameCube beta versions were called Trial Edition instead, but we use the NTE abbreviation anyway for consistency.)

| Version         | Lobbies  | Games    | Proxy    |
|-----------------|----------|----------|----------|
| DC NTE          | Yes      | Yes      | No       |
| DC 11/2000      | Yes      | Yes      | No       |
| DC 12/2000      | Yes      | Yes      | Yes      |
| DC 01/2001      | Yes      | Yes      | Yes      |
| DC V1           | Yes      | Yes      | Yes      |
| DC 08/2001      | Yes      | Yes      | Yes      |
| DC V2           | Yes      | Yes      | Yes      |
| PC NTE          | Yes (1)  | Yes      | No       |
| PC              | Yes      | Yes      | Yes      |
| GC Ep1&2 NTE    | Yes      | Yes      | Yes      |
| GC Ep1&2        | Yes      | Yes      | Yes      |
| GC Ep1&2 Plus   | Yes      | Yes      | Yes      |
| GC Ep3 NTE      | Yes      | Yes (2)  | Yes      |
| GC Ep3          | Yes      | Yes      | Yes      |
| Xbox Ep1&2 Beta | Yes      | Yes      | Yes      |
| Xbox Ep1&2      | Yes      | Yes      | Yes      |
| BB (vanilla)    | Yes      | Yes      | Yes      |
| BB (Tethealla)  | Yes      | Yes      | Yes      |

*Notes:*
1. *This is the only version of PSO that doesn't have any way to identify the player's account - there is no serial number or username. For this reason, AllowUnregisteredUsers must be enabled in config.json to support PC NTE, and PC NTE players receive a random Guild Card number every time they connect. To prevent abuse, PC NTE support can be disabled in config.json.*
2. *Episode 3 NTE battles are not well-tested; some things may not work. See notes/ep3-nte-differences.txt for a list of known differences between NTE and the final version. NTE and non-NTE players cannot battle each other.*

# Setup

## Server setup

Currently newserv works on macOS, Windows, and Ubuntu Linux. It will likely work on other Linux flavors too.

### Windows/macOS

1. Download the latest release-windows-amd64.zip or release-macos-arm64.zip file from the [releases page](https://github.com/fuzziqersoftware/newserv/releases).
2. Extract the contents of the archive to some location on your computer.
3. (Optional) If you want to change any config options, go into the system/ folder, open config.json in a text editor, and edit it to your liking. There are comments in the file that describe what all the options do.
4. (Optional) If you plan to play Blue Burst on newserv, set up the patch directory. See [client patch directories](#client-patch-directories) for details.
5. Run the newserv executable.

### Linux

There are currently no precompiled releases for Linux. To run newserv on Linux, you'll have to build it from source - see the "Building from source" section below.

### Building from source

1. Install the packages newserv depends on.
    * If you're on Windows, install [Cygwin](https://www.cygwin.com/). While doing so, install the `cmake`, `gcc-core`, `gcc-g++`, `git`, `libevent2.1_7`, `libevent-devel`, `make`, `libiconv-devel`, and `zlib` packages. Do the rest of these steps inside a Cygwin shell (not a Windows cmd shell or PowerShell).
    * If you're on macOS, run `brew install cmake libevent libiconv`.
    * If you're on Linux, run `sudo apt-get install cmake libevent-dev` (or use your Linux distribution's package manager).
3. Build and install [phosg](https://github.com/fuzziqersoftware/phosg).
4. Optionally, install [resource_dasm](https://github.com/fuzziqersoftware/resource_dasm). This will enable newserv to send memory patches and load DOL files on PSO GC clients. PSO GC clients can play PSO normally on newserv without this.
5. Run `cmake . && make` in the newserv directory.

After building newserv, edit system/config.example.json as needed **and rename it to system/config.json** (note that this step is not necessary for the precompiled releases!), set up [client patch directories](#client-patch-directories) if you're planning to play Blue Burst, then run `./newserv` in newserv's directory.

The server has an interactive shell which can be used to make changes, such as managing user accounts, updating the server's configuration, managing Episode 3 tournaments, and more. Type `help` and press Enter to see all the commands.

On Linux and macOS, the server also responds to SIGUSR1 and SIGUSR2. SIGUSR1 does the equivalent of the shell's `reload config` command, which reloads config.json but not any dependent files (so quests, Episode 3 maps, etc. will not be reloaded). SIGUSR2 does the equivalent of the shell's `reload all` command, which reloads everything.

To use newserv in other ways (e.g. for translating data), see the end of this document.

## Client patch directories

newserv implements a patch server for PSO PC and PSO BB game data. Any file or directory you put in the system/patch-bb or system/patch-pc directories will be synced to clients when they connect to the patch server.

For Blue Burst set up, the below is mandatory for a smooth experience:

1. Browse to your chosen client's data directory.
2. Copy all the `map_*.dat` files, `unitxt_*` files and the `data.gsl` file and place them in `system/patch-bb/data`.
3. If you're using game files from the Tethealla client, make a copy of `unitxt_j.prs` inside system/patch-bb/data and name it `unitxt_e.prs`. (If `unitxt_e.prs` already exists, replace it with the copied file.)

If you don't have a BB client, or if you're using a Tethealla client from another source, Tethealla clients that are compatible with newserv can be found here: [English](https://web.archive.org/web/20240402011115/https://ragol.org/files/bb/TethVer12513_English.zip) / [Japanese](https://web.archive.org/web/20240402013127/https://ragol.org/files/bb/TethVer12513_Japanese.zip). These clients connect to 127.0.0.1 (localhost) automatically.

For BB clients, newserv reads some files out of the patch data to implement game logic, so it's important that certain game files are synchronized between the server and the client. newserv contains defaults for these files in the system/maps/bb-v4 directory, but if these don't match the client's copies of the files, odd behavior will occur in games.

To make server startup faster, newserv caches the modification times, sizes, and checksums of the files in the patch directories. If the patch server appears to be misbehaving, try deleting the .metadata-cache.json file in the relevant patch directory to force newserv to recompute all the checksums. Also, in the case when checksums are cached, newserv may not actually load the data for a patch file until it's needed by a client. Therefore, modifying any part of the patch tree while newserv is running can cause clients to see an inconsistent view of it.

Patch directory contents are cached in memory. If you've changed any of these files, you can run `reload patch-indexes` in the interactive shell to make the changes take effect without restarting the server.

## How to connect

### PSO DC

Depending on the version of PSO DC that you have, the instructions to connect to a newserv instance will vary.

If you have NTE, USv1, EUv1, or EUv2 and a Broadband Adapter, edit the broadband DNS address to newserv's IP address with newserv's DNS server running. Otherwise, it is necessary to patch the disc or use a codebreaker code to remove the Hunter License server check and/or redirect PSO to the newserv instance. Patching the disc or creating a codebreaker code is beyond the scope of this document.

### PSO DC on Flycast

If you're emulating PSO DC, the NTE, USv1, EUv1, and EUv2 versions will connect to newserv by setting the following options in Flycast's `emu.cfg` file under `[network]`:
- DNS = Your newserv's server address (newserv's DNS server must be running on port 53)
- EmulateBBA = yes
- Enable = yes

It is also necessary to save any DNS information to the flash memory of the Dreamcast to use the BBA - the easiest way to do this is to use the website option in USv2 and then choose the save to flash option.

If the server is running on the same machine as Flycast, this might not work, even if you point Flycast's DNS queries at your local IP address (instead of 127.0.0.1). In this case, you can modify the loaded executable in memory to make it connect anywhere you want. There is a script included with newserv that can do this on macOS; a similar technique could be done manually using scanmem on Linux or Cheat Engine on Windows. To use the script, do this:
1. Build and install [memwatch](https://github.com/fuzziqersoftware/memwatch).
2. Start Flycast and run PSO. (You must start PSO before running the script; it won't work if you run the script before loading the game.)
3. Run `sudo patch_flycast_memory.py <original-destination>`. Replace `<original-destination>` with the hostname that PSO wants to connect to (you can find this out by using Wireshark and looking for DNS queries). The script may take up to a minute; you can continue using Flycast while it runs, but don't start an online game until the script is done.
4. Run newserv and start an online game in PSO.

If you use this method, you'll have to run the script every time you start PSO in Flycast, but you won't have to run it again if you start another online game without restarting emulation.

If using JPv1, JPv2, or USv2, it is also necessary to remove the Hunter Licence server check, either with a disc patch or codebreaker code. Patching the disc or creating a codebreaker code is beyond the scope of this document.

### PSO PC

PSO PC has its connection addresses in `pso.exe`. Hex edit the executable with the connection address you want to connect to. Common server addresses to search for to replace are:
- pso20.sonic.isao.net
- sg207634.sonicteam.com
- pso-mp01.sonic.isao.net
- gsproduc.ath.cx
- sylverant.net

The version of PSO PC I have has the server addresses starting at offset 0x29CB34 in pso.exe. Change those addresses to "localhost" (without quotes) if you just want to connect to a locally-running newserv instance. Alternatively, you can add an entry to the Windows hosts file (C:\Windows\System32\drivers\etc\hosts) to redirect the connection to 127.0.0.1 (localhost) or any other IP address.

### PSO GC on a real GameCube

You can make PSO connect to newserv by setting the default gateway and DNS server addresses in the game's network settings to newserv's address. newserv's DNS server must be running on port 53 and must be accessible to the GameCube. If you're not playing PSO Plus or Episode III, this should be all you need to do, assuming you already set LocalAddress in config.json to your PC's private IP address.

If you have PSO Plus or Episode III, it won't want to connect to a server on the same local network as the GameCube itself, as determined by the GameCube's IP address and subnet mask. There are a couple of ways to get around this.

Sodaboy described a fairly easy method, which is to forward the PSO and DNS ports in your router's configuration to your PC's private IP address (the PSO ports are in config.json, and are all TCP; the DNS port is 53 and is UDP). Then, set LocalAddress and ExternalAddress in config.json to your external IP address (from e.g. whatismyip.com). Most routers will let you connect to your public IP address even from within the local network, but the GameCube will think it's connecting to a different network, so it won't reject the connection. If you're concerned about security and don't want your server to be publicly accessible, you can use Windows Firewall or UFW on Linux block incoming connections on the ports you opened, except for connections from the IP addresses you specify.

Another method is to use two network interfaces on the same PC, and tell the GameCube to connect to the one that appears to be on a different network. For example, if your GameCube is on the 10.0.0.x subnet and your PC's address is 10.0.0.5, you can create a fake network adapter on your PC (or use an existing real one) that has an IP address on a different subnet than the GameCube, such as 192.168.0.8. Then, in PSO's network config, set the default gateway and DNS server addresses to 192.168.0.8, and set LocalAddress in config.json to 192.168.0.8, and PSO should connect. This is what I did back in the old days when I primarily developed software on Windows, but I haven't tried it in many years.

### PSO GC on a Wii or Wii U

Using a Wii or Wii U to connect to newserv requires the Wii or vWii to be softmodded. How to do this is beyond the scope of this document.

Nintendont includes BBA emulation and is compatible with all PSO GameCube versions except Episodes I&II Trial Edition. To use Nintendont, enable BBA emulation in Nintendont's settings and follow the instructions in the above section (PSO GC on a real GameCube).

Devolution includes modem emulation and is compatible with all PSO GameCube versions including Episodes I&II Trial Edition. newserv can act as a PPP server, which Devolution can directly connect to. To do this:
1. Enable the PPPRawListen option according to the comments in config.json.
2. Start newserv.
3. In the game's network settings, set the username and password to anything (they cannot be blank), and set the phone number to the number that newserv outputs to the console during startup. (It will be near the end of all the startup log messages.) If your Wii is on the same network as newserv, use the local number; otherwise, use the external number.

### PSO GC on Dolphin

If you're using the HLE BBA type, set the BBA's DNS server address to newserv's IP address and it should work. (If newserv is on the same machine as Dolphin, you will need to use an Action Replay code directed at 127.0.0.1 to connect, as PSO rejects DNS queries from the same IP address.) Set PSO's network settings the same as listed below.

If you're using the TAP (not tapserver) BBA type, you'll have to set PSO's network settings appropriately for your tap interface. Set the DNS server address in PSO's network settings to newserv's IP address.

If you're using the tapserver BBA or modem type, you can make it connect to a newserv instance running on the same machine via the tapserver interface. To do this:
1. In the GameCube pane of the Config window, set the SP1 device to Broadband Adapter (tapserver) or Modem Adapter (tapserver).
2. Click the "..." button next to the SP1 menu. If you're using the tapserver BBA, enter `127.0.0.1:5059` in the box. If you're using the tapserver modem, enter `127.0.0.1:5058` in the box. (If newserv isn't running on the same machine as Dolphin, replace 127.0.0.1 with newserv's IP address.)
3. In PSO's network settings, enable DHCP ("Automatically obtain an IP address"), set DNS server address to "Automatic", and leave DHCP Hostname as "Not set". Leave the proxy server settings blank.
4. Start an online game.

### PSO BB

The PSO BB client has been modified and distributed in many different forms. newserv supports most, but not all, of the common distributions. Unlike other versions, it's common for various BB clients to have different map files. It's important that the client and server have the same map files, so make sure to set up the patch directory based on the client you'll be using with newserv. (See the [client patch directories](#client-patch-directories) section for instructions on setting this up.)

The original Japanese and US versions of PSO BB work with newserv (the last Japanese release can be found [here](https://archive.org/details/psobb_jp_setup_12511_20240109/)). To get them to connect to your server, do one of the following:
* Use a drop-in patcher like [AzureFlare](https://github.com/Repflez/AzureFlare).
* Edit your hosts file to redirect the client's destination address to localhost or your server's address.
* Edit psobb.exe to point to your newserv instance. The original clients are packed with various versions of ASProtect, so this is a more involved process than simply opening the executable in a hex editor and finding/replacing some strings.

Alternatively, you can use the Tethealla client ([English](https://web.archive.org/web/20240402011115/https://ragol.org/files/bb/TethVer12513_English.zip) or [Japanese](https://web.archive.org/web/20240402013127/https://ragol.org/files/bb/TethVer12513_Japanese.zip)). If the server is on the same PC as the client and you don't plan to have any external players, these Tethealla clients will automatically connect to the server without any modifications. This version of the client is not packed, and you can find the connection addresses starting at offset 0x56D724 in psobb.exe. Overwrite these addresses with your server's hostname or IP address, and you should be able to connect.

### Allowing external players to connect

If you want to accept connections from outside your local network, you'll need to set ExternalAddress to your public IP address in the configuration file, and you'll likely need to open some ports in your router's NAT configuration - specifically, all the TCP ports listed in PortConfiguration in config.json.

For GC clients, you'll have to use newserv's built-in DNS server or set up your own DNS server as well. If you want external clients to be able to use your DNS server, you'll have to forward UDP port 53 to your newserv instance. Remote players can then connect to your server by entering your DNS server's IP address in their client's network configuration.

# Server feature configuration

## User accounts

By default, newserv does not require users to pre-register before playing; the server will instead automatically create an account the first time each player connects. These accounts have no special permissions. You can view, create, edit, and delete user accounts in the server's shell (run `help` in the shell to see how to do this).

A license is a set of credentials that a player can use to log in. There are six types of licenses:
* *DC NTE licenses* consist of a 16-character serial number and 16-character access key.
* *DC licenses* consist of an 8-character hex serial number and an 8-character access key.
* *PC licenses* are the same format as DC licenses, but are used for PC v2.
* *GC licenses* consist of a 10-digit decimal serial number, a 12-character access key, and a password of up to 8 characters.
* *XB licenses* consist of a gamertag of up to 16 characters, a 16-character hex user ID, and a 16-character hex account ID.
* *BB licenses* consist of a username of up to 16 characters and a password of up to 16 characters.
Each account may have multiple licenses. To add a license to an account, use `add-license` in the shell.

On BB, character data is scoped to the license, but system and Guild Card data is scoped to the account. That is, an account with multiple BB licenses can have more than 4 characters (up to 4 per license), but they will all share the same team membership and Guild Card lists.

You may want to give your account elevated privileges. To do so, run `update-account ACCOUNT-ID flags=root` (replacing ACCOUNT-ID with your actual account-id). You can also use update-account to edit other parts of the account; see the help text for more information.

## Installing quests

newserv automatically finds quests in the subdirectories of the system/quests/ directory. To install your own quests, or to use quests you've saved using the proxy's Save Files option, just put them in one of the subdirectories there and name them appropriately. The subdirectories and their behaviors (e.g. in which game modes they should appear and for which PSO versions) is defined in the QuestCategories field in config.json.

Within the category directories, quest files should be named like `q###-VERSION-LANGUAGE.EXT` (although the `q` is ignored, and can be any letter). The fields in each filename are:
- `###`: quest number (this doesn't really matter; it should just be unique across the PSO version)
- `VERSION`: dn = Dreamcast NTE, dp = Dreamcast 11/2000 prototype, d1 = Dreamcast v1, dc = Dreamcast v2, pcn = PC NTE, pc = PC, gcn = GameCube NTE, gc = GameCube Episodes 1 & 2, gc3 = Episode 3 (see below), xb = Xbox, bb = Blue Burst
- `LANGUAGE`: j = Japanese, e = English, g = German, f = French, s = Spanish
- `EXT`: file extension (see table below)

For .dat files, the `LANGUAGE` token may be omitted. If it's present, then that .dat file will only be used for that language of the quest; if omitted, then that .dat file will be used for all languages of the quest.

Some quests (mostly battle and challenge mode quests) have additional JSON metadata files that describe how the server should handle them. This includes flags that can be used to hide the quest unless a preceding quest has been cleared, or to hide the quest unless purchased as a team reward. These metadata files are generally named similarly to their .bin and .dat counterparts, except the `VERSION` token may also be omitted if the metadata applies to all languages of the quest on all PSO versions. See system/quests/battle/b88001.json for documentation on the exact format of the JSON file.

Some quests may also include a .pvr file, which contains an image used in the quest. These files are named similarly to their .bin and .dat counterparts.

For example, the GameCube version of Lost HEAT SWORD is in two files named `q058-gc-e.bin` and `q058-gc.dat`. newserv knows these files are quests because they're in the system/quests/ directory, it knows they're for PSO GC because the filenames contain `-gc`, it knows this is the English version of the quest because the .bin filename ends with `-e` (even though the .dat filename does not), and it puts them in the Retrieval category because the files are within the retrieval/ directory within system/quests/.

The GameCube and Xbox quest formats are very similar, but newserv treats them as different. If you want to use the same quest file for GameCube and Xbox clients, you can make one a symbolic link to the other.

There are multiple PSO quest formats out there; newserv supports all of them. It can also decode any known format to standard .bin/.dat format. Specifically:

| Format           | Extension             | Supported  | Decode action    |
|------------------|-----------------------|------------|------------------|
| Compressed       | .bin and .dat         | Yes        | None (1)         |
| Compressed Ep3   | .bin or .mnm          | Yes (4)    | None (1)         |
| Uncompressed     | .bind and .datd       | Yes        | compress-prs (2) |
| Uncompressed Ep3 | .bind or .mnmd        | Yes (4)    | compress-prs (2) |
| Source           | .bin.txt and .dat     | Yes        | None (5)         |
| VMS (DCv1)       | .bin.vms and .dat.vms | Yes        | decode-vms       |
| VMS (DCv2)       | .bin.vms and .dat.vms | Decode (3) | decode-vms (3)   |
| GCI (decrypted)  | .bin.gci and .dat.gci | Yes        | decode-gci       |
| GCI (with key)   | .bin.gci and .dat.gci | Yes        | decode-gci       |
| GCI (no key)     | .bin.gci and .dat.gci | Decode (3) | decode-gci (3)   |
| GCI (Ep3 NTE)    | .bin.gci or .mnm.gci  | Decode (3) | decode-gci (3)   |
| GCI (Ep3)        | .bin.gci or .mnm.gci  | Yes        | decode-gci       |
| DLQ              | .bin.dlq and .dat.dlq | Yes        | decode-dlq       |
| DLQ (Ep3)        | .bin.dlq or .mnm.dlq  | Yes        | decode-dlq       |
| QST (online)     | .qst                  | Yes        | decode-qst       |
| QST (download)   | .qst                  | Yes        | decode-qst       |

*Notes:*
1. *This is the default format. You can convert these to uncompressed format by running `newserv decompress-prs FILENAME.bin FILENAME.bind` (and similarly for .dat -> .datd)*
2. *Similar to (1), to compress an uncompressed quest file: `newserv compress-prs FILENAME.bind FILENAME.bin` (and likewise for .datd -> .dat)*
3. *Use the decode action to convert these quests to .bin/.dat format before putting them into the server's quests directory. If you know the encryption seed (serial number), pass it in as a hex string with the `--seed=` option. If you don't know the encryption seed, newserv will find it for you, which will likely take a long time.*
4. *Episode 3 quests don't go in the system/quests directory. See the [Episode 3 section](#episode-3-features) section below.*
5. *Quest source can be assembled into a .bin or .bind file with `newserv assemble-quest-script FILENAME.txt`. See system/quests/retrieval/q058-gc-e.bin.txt for an annotated example; this is the English GameCube version of Lost HEAT SWORD.*

Episode 3 download quests consist only of a .bin file - there is no corresponding .dat file. Episode 3 download quest files may be named with the .mnm extension instead of .bin, since the format is the same as the standard map files (in system/ep3/). These files can be encoded in any of the formats described above, except .qst.

When newserv indexes the quests during startup, it will warn (but not fail) if any quests are corrupt or in unrecognized formats.

Quest contents are cached in memory, but if you've changed the contents of the quests directory, you can re-index the quests without restarting the server by running `reload quest-index` in the interactive shell. The new quests will be available immediately, but any games with quests already in progress will continue using the old versions of the quests until those quests end.

## Item tables and drop modes

newserv supports server-side item generation on all game versions, except for the earliest DC prototypes (NTE and 11/2000). By default, the game behaves as it did on the original servers - on all versions except BB, item drops are controlled by the leader client in each game, and on BB, item drops are controlled by the server.

There are five different available behaviors for item drops:
* `disabled` (or `none`): No items will drop from boxes or enemies.
* `client`: The game leader generates items, all items are visible to all players, and any player may pick up any item. This is the default mode for all game versions, except this mode cannot be used if the game leader is on BB.
* `shared`: The server generates items, all items are visible to all players, and any player may pick up any item. This is the default mode if the game leader is on BB.
* `private`: The server generates items, but each player may get a different item from any box or enemy. If a player isn't in the same area as an enemy at the time it's defeated, they won't get any item from it. Items dropped by players are visible to everyone.
* `duplicate`: The server generates items, and each player will get the same item from any box or enemy, but there is one copy of each item for each player (and each player only sees their own copy of the item). If a player isn't in the same area as an enemy at the time it's defeated, they won't get any item from it. Items dropped by players are not duplicated and are visible to everyone.

In the `private` and `duplicate` modes, there is no incentive to pick up items before another player, since other players cannot pick up the items you see dropped from boxes and enemies. However, if you pick up an item and drop it later, it can then be seen and picked up by any player.

The drop mode can be changed at any time during a game with the `$dropmode` chat command. If the mode is changed after some items have already been dropped, the existing items retain their visibility (that is, items dropped in private mode still can't be picked up by other players since they were dropped before the mode was changed). You can configure which drop modes are used by default, and which modes players are allowed to choose, in config.json. See the comments above the AllowedDropModes and DefaultDropMode keys.

In the server drop modes, the item tables used to generate common items are in the `system/item-tables/ItemPT-*` files. (The V2 files are used for V1 as well.) The rare item tables are in the `rare-table-*.json` files. Unlike the original formats, it's possible to make each enemy drop multiple different rare items at different rates, though the default tables never do this.

## Cross-version play

All versions of PSO can see and interact with each other in the lobby. By default, newserv allows V1 and V2 players to play together, and allows GC and Xbox players to play together. You can change these rules to allow all versions to play together, or to prevent versions from playing together, with the CompatibilityGroups setting in config.json.

There are several cross-version restrictions that always apply regardless of the compatibility groups setting:
* DC V1 players cannot join DC V2 games if the game creator didn't choose to allow them.
* DC V1 players cannot join games if the difficulty level is set to Ultimate or the game mode is Battle or Challenge.
* Only GC, Xbox, and BB players can join games in Episode 2.
* Only BB players can join games in Episode 4.
* Episode 3 players cannot join non-Episode 3 games, and vice versa.

V1/V2 compatibility and GC/Xbox compatibility are well-tested, but other situations are not. Not much attention has been given yet to how items should be handled across major versions; if you enable V2/GC compatibility, for example, there will likely be bugs. Please report such bugs as GitHub issues.

In cross-version play, when any of the server drop modes are used, the server uses the drop tables corresponding to the leader's version and section ID. (For example, if a DC V1 player is the game leader, rare-table-v1.json will be used, even after V2 players join.) If a BB player is the leader and the `client` drop mode is used, the server generates items as if it were in `shared` mode.

## Server-side saves

newserv has the ability to save character data on the server side. For PSO BB, this is required of course, but this feature can also be used on other PSO versions.

Each account has 4 BB character slots and 16 non-BB character file slots. The non-BB slots are independent of the BB slots, and can be accessed with the `$savechar <slot>` and `$loadchar <slot>` commands (slots are numbered 1 through 16). `$savechar` copies the character you're currently playing as and saves the data on the server, and `$loadchar` does the reverse, overwriting your current character with the data saved on the server. Note that you can load a character that was saved from a different version of PSO, which allows you to easily transfer characters between games. On v1 and v2, changes done by `$loadchar` will be undone if you join a game; to permanently save your changes, disconnect from the lobby after using the command.

There is a third command, `$bbchar <username> <password> <slot>`, which behaves similarly to `$savechar` but writes the character data to a BB character slot in a different account instead (slots are numbered 1 through 4). This can be used to "upgrade" a character to BB from an earlier version.

Exactly which data is saved and loaded depends on the game version:

| Game                 | Inventory | Character | Options/chats | Quest flags | Bank | Battle/Challenge |
|----------------------|-----------|-----------|---------------|-------------|------|------------------|
| PSO DC v1 prototypes | Yes       | Yes       | No            | No          | No   | N/A              |
| PSO DC v1            | Yes       | Yes       | No            | No          | No   | N/A              |
| PSO DC v2            | Yes       | Yes       | Yes           | Yes         | Yes  | Yes              |
| PSO PC (v2)          | Yes       | Yes       | No            | No          | No   | Save only        |
| PSO GC NTE           | Yes       | Yes       | Yes           | Yes         | Yes  | Yes              |
| PSO GC (not Plus)    | Yes       | Yes       | Yes           | Yes         | Yes  | Yes              |
| PSO GC Plus (1)      | Save only | Save only | No            | No          | No   | Save only        |
| PSO GC Ep3 (1)       | No        | Save only | No            | No          | No   | Save only        |
| PSO Xbox             | Yes       | Yes       | Yes           | Yes         | Yes  | Yes              |
| PSO BB               | Yes       | Yes       | Yes           | Yes         | Yes  | Yes              |

*Notes*:
1. *If EnableSendFunctionCallQuestNumber is enabled in config.json, then $savechar and $loadchar can save and restore all character data on these versions, just like on GC non-Plus. Episode 3 characters exist in a separate namespace; that is, you can't use $savechar and $loadchar to convert an Ep3 character to non-Ep3, or vice versa.*

## Episode 3 features

newserv supports many features unique to Episode 3:
* CARD battles. Not every combination of abilities has been tested yet, so if you find a feature or card ability that doesn't work like it's supposed to, please make a GitHub issue and describe the situation (the attacking card(s), defending card(s), and ability or condition that didn't work).
* Spectator teams.
* Tournaments. (But they work differently than Sega's tournaments did - see below)
* Downloading quests.
* Trading cards.
* Participating in card auctions. (The auction contents must be configured in config.json.)
* Decorations in lobbies. Currently only images are supported; the game also supports loading custom 3D models in lobbies, but newserv does not implement this (yet).

### Battle records

After playing a battle, you can save the record of the battle with the `$saverec` command. You can then replay the battle later by using the `$playrec` command in a lobby - this will create a spectator team and play the recording of the battle as if it were happening in realtime. Note that there is a bug in older versions of Dolphin that seems to be frequently triggered when playing battle records, which causes the emulator to crash with the message `QObject::~QObject: Timers cannot be stopped from another thread`. To avoid this, use the latest version of Dolphin.

### Tournaments

Tournaments work differently than they did on Sega's servers. Tournaments can be created with the `create-tournament` shell command, which enables players to register for them. (Use `help` to see all the arguments - there are many!) The `start-tournament` shell command starts the tournament (and prevents further registrations), but this doesn't schedule any matches. Instead, players who are ready to play their next match can all stand at the 4-player battle table near the lobby warp in the same CARD lobby, and the tournament match will start automatically.

These tournament semantics mean that there can be multiple matches in the same tournament in play simultaneously, and not all matches in a round must be complete before the next round can begin - only the matches preceding each individual match must be complete for that match to be playable.

The Meseta rewards for winning tournament matches can be configured in config.json.

### Episode 3 files

Episode 3 state and game data is stored in the system/ep3 directory. The files in there are:
* card-definitions.mnr: Compressed card definition list, sent to Episode 3 clients at connect time. Card stats and abilities can be changed by editing this file.
* card-definitions.mnrd: Decompressed version of the above. If present, newserv will use this instead of the compressed version, since this is easier to edit.
* card-text.mnr: Compressed card text archive. Generally only used for debugging.
* card-text.mnrd: Decompressed card text archive; same format as TextCardE.bin. Generally only used for debugging.
* com-decks.json: COM decks used in tournaments. The default decks in this file come from logs from Sega's servers, so the file doesn't include every COM deck Sega ever made - the rest are probably lost to time.
* maps/: Online free battle and quest maps (.mnm/.bin/.mnmd/.bind files). newserv comes with the default online maps, as well as some fan-made variations and quests to help new players get up to speed.
* maps-download/: Download maps and quests (.mnm/.bin/.mnmd/.bind files). There are two subcategories by default (download maps and Trial Edition download maps), but you can add more by editing QuestCategories in config.json. Categories that have flag 0x40 (Ep3 download) set are indexed from this directory; all others are indexed from system/quests/. Files in maps-download/ subdirectories have the same format as those in the maps/ directory, but should be named like `e###-gc3-LANGUAGE.EXT` (similar to how non-Episode 3 quests are named in the system/quests/ directory). If you want a map to be available for online play and for downloading, the file must exist in both maps/ and in a maps-download/ subdirectory (a symbolic link is acceptable).
* maps-offline/: Offline map files. These are all the offline quests and free battle maps from the client, including some debugging/test maps that were inaccessible during normal play. To make them playable online, put the files in the maps/ directory.
* tournament-state.json: State of all active tournaments. This file is automatically written when any tournament changes state for any reason (e.g. a tournament is created/started/deleted or a match is resolved).

There is no public editor for Episode 3 maps and quests, but the format is described fairly thoroughly in src/Episode3/DataIndexes.hh (see the MapDefinition structure). You'll need to use `newserv decompress-prs ...` to decompress a .bin or .mnm file before editing it, but you don't need to compress it again to use it - just put the .bind or .mnmd file in the maps directory and newserv will make it available.

Like quests, Episode 3 card definitions, maps, and quests are cached in memory. If you've changed any of these files, you can run `reload ep3-cards` or `reload ep3-maps` in the interactive shell to make the changes take effect without restarting the server.

## Memory patches, client functions, and DOL files

*Everything in this section requires resource_dasm to be installed, so newserv can use the assemblers and disassemblers from its libresource_file library. If resource_dasm is not installed, newserv will still build and run, but these features will not be available.*

You can put assembly files in the system/client-functions directory with filenames like PatchName.VERS.patch.s and they will appear in the Patches menu for clients that support client functions. Client functions are written in SH-4, PowerPC, or x86 assembly and are compiled when newserv is started. The assembly system's features are documented in the comments in system/client-functions/System/WriteMemoryGC.ppc.s.

The VERS token in client function filenames refers to the specific version of the game that the client function applies to. Some versions do not support receiving client functions at all. *Note: newserv uses the shorter GameCube versioning convention, where discs labeled DOL-XXXX-0-0Y are version 1.Y. The PSO community seems to use the convention 1.0Y in some places instead, but these are the same version. For example, the version that newserv calls v1.4 is the same as v1.04, and is labeled DOL-GPOJ-0-04 on the underside of the disc.*

The specific versions are:

| Game              | VERS | Architecture  |
|-------------------|------|---------------|
| PSO DC NTE        | 1OJ1 | Not supported |
| PSO DC 11/2000    | 1OJ2 | Not supported |
| PSO DC 12/2000    | 1OJ3 | Not supported |
| PSO DC 01/2001    | 1OJ4 | Not supported |
| PSO DC v1 JP      | 1OJF | Not supported |
| PSO DC v1 US      | 1OEF | Not supported |
| PSO DC v1 EU      | 1OPF | Not supported |
| PSO DC 08/2001    | 2OJ5 | SH-4          |
| PSO DC v2 JP      | 2OJF | SH-4          |
| PSO DC v2 US      | 2OEF | SH-4          |
| PSO DC v2 EU      | 2OPF | SH-4          |
| PSO PC (v2)       | 2OJW | Not supported |
| PSO GC NTE        | 3OJT | PowerPC       |
| PSO GC v1.2 JP    | 3OJ2 | PowerPC       |
| PSO GC v1.3 JP    | 3OJ3 | PowerPC       |
| PSO GC v1.4 JP    | 3OJ4 | PowerPC       |
| PSO GC v1.5 JP    | 3OJ5 | PowerPC (1)   |
| PSO GC v1.0 US    | 3OE0 | PowerPC       |
| PSO GC v1.1 US    | 3OE1 | PowerPC       |
| PSO GC v1.2 US    | 3OE2 | PowerPC (1)   |
| PSO GC v1.0 EU    | 3OP0 | PowerPC       |
| PSO GC Ep3 NTE    | 3SJT | PowerPC       |
| PSO GC Ep3 JP     | 3SJ0 | PowerPC       |
| PSO GC Ep3 US     | 3SE0 | PowerPC (1)   |
| PSO GC Ep3 EU     | 3SP0 | PowerPC (1)   |
| PSO Xbox Beta     | 4OJB | x86           |
| PSO Xbox JP Disc  | 4OJD | x86           |
| PSO Xbox JP TU    | 4OJU | x86           |
| PSO Xbox US Disc  | 4OED | x86           |
| PSO Xbox US TU    | 4OEU | x86           |
| PSO Xbox EU Disc  | 4OPD | x86           |
| PSO Xbox EU TU    | 4OPU | x86           |
| PSO BB JP 1.25.13 | 59NL | x86           |
| PSO BB Tethealla  | 59NL | x86           |

*Notes:*
1. *Client functions are only supported on these versions if EnableSendFunctionCallQuestNumbers is set in config.json. See the comments there for more information.*

newserv comes with a set of patches for many of the above versions, based on AR codes originally made by Ralf at GC-Forever and Aleron Ives. Many of them were originally posted in [this thread](https://www.gc-forever.com/forums/viewtopic.php?f=38&t=2050).

You can also put DOL files in the system/dol directory, and they will appear in the Programs menu for GC clients. Selecting a DOL file there will load the file into the GameCube's memory and run it, just like the old homebrew loaders (PSUL and PSOload) did. For this to work, ReadMemoryWordGC.ppc.s, WriteMemoryGC.ppc.s, and RunDOL.ppc.s must be present in the system/client-functions/System directory. This has been tested on Dolphin but not on a real GameCube, so results may vary.

Like other kinds of data, functions and DOL files are cached in memory. If you've changed any of these files, you can run `reload functions` or `reload dol-files` in the interactive shell to make the changes take effect without restarting the server.

I mainly built the DOL loading functionality for documentation purposes. By now, there are many better ways to load homebrew code on an unmodified GameCube, but to my knowledge there isn't another open-source implementation of this method in existence.

## Using newserv as a proxy

If you want to play online on remote servers rather than running your own server, newserv also includes a PSO proxy. Currently this works with PSO GC and may work with PC and DC; it also works with some BB clients in specific situations.

To use the proxy for PSO DC, PC, or GC, add an entry to the corresponding ProxyDestinations dictionary in config.json, then run newserv and connect to it as normal (see below). You'll see a "Proxy server" option in the main menu, and you can pick which remote server to connect to.

To use the proxy for PSO BB, set the ProxyDestination-BB entry in config.json. If this option is set, it essentially disables the game server for all PSO BB clients - all clients will be proxied to the specified destination instead. Unfortunately, because PSO BB uses a different set of handlers for the data server phase and character selection, there's no in-game way to present the player with a list of options, like there is on PSO PC and PSO GC.

When you're on PSO DC, PC, or GC and are connected to a remote server through newserv's proxy, choosing the Change Ship or Change Block action from the lobby counter will send you back to newserv's main menu instead of the remote server's ship or block select menu. You can go back to the server you were just on by choosing it from the proxy server menu again.

There are many options available when starting a proxy session. All options are off by default unless otherwise noted. The options are:
* **Chat commands**: enables chat commands in the proxy session (on by default).
* **Chat filter**: enables escape sequences in chat messages and info board (on by default).
* **Player notifications**: shows a message when any player joins or leaves the game or lobby you're in.
* **Block pings**: blocks automatic pings sent by the client, and responds to ping commands from the server automatically.
* **Infinite HP**: automatically heals you whenever you get hit. An attack that kills you in one hit will still kill you, however.
* **Infinite TP**: automatically restores your TP whenever you use any technique.
* **Switch assist**: unlocks doors that require two or four players in a one-player game, when you step on one of the switches.
* **Infinite Meseta** (Episode 3 only): gives you 1,000,000 Meseta, regardless of the value sent by the remote server.
* **Block events**: disables holiday events sent by the remote server.
* **Block patches**: prevents any B2 (patch) commands from reaching the client.
* **Save files**: saves copies of several kinds of files when they're sent by the remote server. The files are written to the current directory (which is usually the directory containing the system/ directory). These kinds of files can be saved:
    * Online quests and download quests (saved as .bin/.dat files)
    * GBA games (saved as .gba files)
    * Patches (saved as .bin files, and disassembled to text files if newserv is built with patch support)
    * Player data from BB sessions (saved as .bin files, which are not the same format as .nsc files)
    * Episode 3 online quests and maps (saved as .mnmd files)
    * Episode 3 download quests (saved as .mnm files)
    * Episode 3 card definitions (saved as .mnr files)
    * Episode 3 media updates (saved as .gvm, .bml, or .bin files)

The remote server will probably try to assign you a Guild Card number that doesn't match the one you have on newserv. On PSO DC, PC and GC, the proxy server rewrites the commands in transit to make it look like the remote server assigned you the same Guild Card number as you have on newserv, but if the remote server has some external integrations (e.g. forum or Discord bots), they will use the Guild Card number that the remote server believes it has assigned to you. The number assigned by the remote server is shown to you when you first connect to the remote server, and you can retrieve it in lobbies or during games with the `$li` command.

Some chat commands (see below) have the same basic function on the proxy server but have different effects or conditions. In addition, there are some server shell commands that affect clients on the proxy (run `help` in the shell to see what they are). If there's only one proxy session open, the shell's proxy commands will affect that session. Otherwise, you'll have to specify which session to affect with the `on` prefix - to send a chat message in LinkedSession:17205AE4, for example, you would run `on 17205AE4 chat ...`.

## Chat commands

newserv supports a variety of commands players can use by chatting in-game. Any chat message that begins with `$` is treated as a chat command. (If you actually want to send a chat message starting with `$`, type `$$` instead.) On the DC 11/2000 prototype, `@` is used instead of `$` for all chat commands, since `$` does not appear on the English virtual keyboard.

Some commands only work on the game server and not on the proxy server. The chat commands are:

* Information commands
    * `$li`: Show basic information about the lobby or game you're in. If you're on the proxy server, show information about your connection instead (remote Guild Card number, client ID, etc.).
    * `$si` (game server only): Show basic information about the server.
    * `$ping`: Show round-trip ping time from the server to you. On the proxy server, show the ping time from you to the proxy and from the proxy to the server.
    * `$matcount` (game server only): Show how many of each type of material you've used.
    * `$killcount` (game server only): Show the kill count on your currently-equipped weapon. If you're in a game and not on BB, the value is only accurate at the time the item enters the game.
    * `$itemnotifs <mode>`: Enable item drop notification messages. If the game has private drops enabled, you will only see a notification if the dropped item is visible to you; you won't be notified of other players' drops. The modes are:
        * `off`: No notifications are shown.
        * `rare`: You are notified when a rare item drops.
        * `on`: You are notified when any item drops, except Meseta.
        * `every`: You are notified when any item drops, including Meseta.
    * `$announcerares`: Enable or disable announcements for your rare item finds. This determines whether rare items you find will be announced to the game and server, not whether you will see announcements for others finding rare items.
    * `$what` (game server only): Show the type, name, and stats of the nearest item on the ground.
    * `$where` (game server only): Show your current floor number and coordinates. Mainly useful for debugging.
    * `$qfread <field-name>` (game server only): Show the value of a quest counter in your player data. The field names are defined in config.json.

* Debugging commands
    * `$debug` (game server only): Enable or disable debug. You need the DEBUG flag in your user account to use this command. Enabling debug does several things:
        * You'll see in-game messages from the server when you take some actions, like killing enemies, opening boxes, or flipping switches.
        * You'll see the rare seed value and floor variations when you join a game.
        * You'll be placed into the last available slot in lobbies and games instead of the first, unless you're joining a BB solo-mode game.
        * You'll be able to join games with any PSO version, not only those for which cross-version play is normally enabled. See the "Cross-version play" section above for details on this.
        * The rest of the commands in this section are enabled on the game server. (They are always enabled on the proxy server.)
    * `$readmem <address>` (game server only): Read 4 bytes from the given address and show you the values.
    * `$writemem <address> <data>` (game server only): Write data to the given address. Data is not required to be any specific size.
    * `$quest <number>` (game server only): Load a quest by quest number. Can be used to load battle or challenge quests with only one player present. Debug is not required to be enabled if the specified quest has the AllowStartFromChatCommand field set in its metadata file.
    * `$qcall <function-id>`: Call a quest function on your client.
    * `$qcheck <flag-num>` (game server only): Show the value of a quest flag. This command can be used without debug mode enabled. If you're in a game, show the value of the flag in that game; if you're in the lobby, show the saved value of that quest flag for your character (BB only).
    * `$qset <flag-num>` or `$qclear <flag-num>`: Set or clear a quest flag for everyone in the game. If you're in the lobby and on BB, set or clear the saved value of a quest flag in your character file.
    * `$qgread <flag-num>` (game server only): Show the value of a quest counter ("global flag"). This command can be used without debug mode enabled.
    * `$qgwrite <flag-num> <value>` (game server only): Set the value of a quest counter ("global flag") for yourself.
    * `$qsync <reg-num> <value>`: Set a quest register's value for yourself only. `<reg-num>` should be either rXX (e.g. r60) or fXX (e.g. f60); if the latter, `<value>` is parsed as a floating-point value instead of as an integer.
    * `$qsyncall <reg-num> <value>`: Set a quest register's value for everyone in the game. `<reg-num>` should be either rXX (e.g. r60) or fXX (e.g. f60); if the latter, `<value>` is parsed as a floating-point value instead of as an integer.
    * `$swset [floor] <flag-num>` and `$swclear [floor] <flag-num>`: Set or clear a switch flag. If floor is not given, sets or clears the flag on your current floor.
    * `$swsetall`: Set all switch flags on your current floor. This unlocks all doors, disables all laser fences, triggers all light/poison switches, etc.
    * `$gc` (game server only): Send your own Guild Card to yourself.
    * `$sc <data>`: Send a command to yourself.
    * `$ss <data>`: Send a command to the remote server (if in a proxy session) or to the game server.
    * `$sb <data>`: Send a command to yourself, and to the remote server or game server.
    * `$auction` (Episode 3 only): Bring up the CARD Auction menu, regardless of how many players are in the game or if you have a VIP card.

* Personal state commands
    * `$arrow <color-id>`: Change your lobby arrow color.
    * `$secid <section-id>`: Set your override section ID. After running this command, any games you create will use your override section ID for rare drops instead of your character's actual section ID. If you're in a game and you are the leader of the game, this also immediately changes the item tables used by the server when creating items. To revert to your actual section id, run `$secid` with no name after it. On the proxy server, this will not work if the remote server controls item drops (e.g. on BB, or on Schtserv with server drops enabled). If the server does not allow cheat mode anywhere (that is, "CheatModeBehavior" is "Off" in config.json), this command does nothing.
    * `$battle` (game server only; DC v1 only): After using this command, the next game you create will be in battle mode. (A chat command is required for this because DCv1 doesn't allow this natively.) On DCv1, the battle quests are not available, but free-roam is.
    * `$rand <seed>`: Set your override random seed (specified as a 32-bit hex value). This will make any games you create use the given seed for rare enemies. This also makes item drops deterministic in Blue Burst games hosted by newserv. On the proxy server, this command can cause desyncs with other players in the same game, since they will not see the overridden random seed. To remove the override, run `$rand` with no arguments. If the server does not allow cheat mode anywhere (that is, "CheatModeBehavior" is "Off" in config.json), this command does nothing.
    * `$ln [name-or-type]`: Set the lobby number. Visible only to you. This command exists because some non-lobby maps can be loaded as lobbies with invalid lobby numbers. See the "GC lobby types" and "Ep3 lobby types" entries in the information menu for acceptable values here. Note that non-lobby maps do not have a lobby counter, so there's no way to exit the lobby without using either `$ln` again or `$exit`. On the game server, `$ln` reloads the lobby immediately; on the proxy server, it doesn't take effect until you load another lobby yourself (which means you'll like have to use `$exit` to escape). Run this command with no argument to return to the default lobby.
    * `$swa`: Enable or disable switch assist. When enabled, the server will unlock two-player and four-player doors in non-quest games when you step on any of the required switches.
    * `$exit`: If you're in a lobby, send you to the main menu (which ends your proxy session, if you're in one). If you're in a game or spectator team, send you to the lobby (but does not end your proxy session if you're in one). Does nothing if you're in a non-Episode 3 game and no quest is in progress.
    * `$patch <name>`: Run a patch on your client. `<name>` must exactly match the name of a patch on the server.

* Character data commands (game server only)
    * `$savechar <slot>`: Save your current character data on the server in the specified slot. See the [server-side saves section](#server-side-saves) for more details.
    * `$loadchar <slot>`: Save your current character data on the server in the specified slot. See the [server-side saves section](#server-side-saves) for more details.
    * `$bbchar <username> <password> <slot>`: Save your current character data on the server in a different account's BB character slots. See the [server-side saves section](#server-side-saves) for more details.
    * `$edit <stat> <value>`: Modify your character data. See the [using $edit](#using-edit) section for details.

* Blue Burst player commands (game server only)
    * `$bank [number]`: Switch your current bank, so you can access your other character's banks (if `number` is 1-4) or your shared account bank (if `number` is 0). If `number` is not given, switch back to your current character's bank.
    * `$save`: Save your character, system, and Guild Card data immediately. (By default, your character is saved every 60 seconds while online, and your account and Guild Card data are saved whenever they change.)

* Game state commands (game server only)
    * `$maxlevel <level>`: Set the maximum level for players to join the current game. (This only applies when joining; if a player joins and then levels up past this level during the game, they are not kicked out, but won't be able to rejoin if they leave.)
    * `$minlevel <level>`: Set the minimum level for players to join the current game.
    * `$password <password>`: Set the game's join password. To unlock the game, run `$password` with nothing after it.
    * `$dropmode [mode]`: Change the way item drops behave in the current game. `mode` can be `none`, `client`, `shared`, `private`, or `duplicate`. If `mode` is not given, tells you the current drop mode without changing it. See the [item tables and drop modes section](#item-tables-and-drop-modes) for more information.
    * `$persist`: Enable or disable persistence for the current game. When persistence is on, the game will not be deleted when the last player leaves. The states of enemies, objects, and switches will be saved, and items left on the floor will not be deleted. (But if you're in the private or duplicate drop mode, items dropped by enemies are deleted - to make sure a certain item won't be deleted, you can pick it up and drop it again.) If the game is empty for too long (15 minutes by default), it is then deleted.

* Episode 3 commands (game server only)
    * `$spec`: Toggle the allow spectators flag for Episode 3 games. If any players are spectating when this flag is disabled, they are sent back to the lobby.
    * `$inftime`: Toggle infinite-time mode. Must be used before starting a battle. If infinite-time mode is on, the overall and per-phase time limits will be disabled regardless of the values chosen during battle rules setup. After completing a battle, infinite-time mode is reset to the server's default value (which can be set in Episode3BehaviorFlags in config.json).
    * `$dicerange [d:L-H] [1:L-H] [a1:L-H] [d1:L-H]`: Set override dice ranges for the next battle. The min and max dice values from the rules setup menu always apply to the ATK dice, but you can specify a different range for the DEF dice with `d:2-4` (for example). The `1:` override applies to the 1-player team in a 2v1 game (so you would set the 2-player team's desired dice range in the rules menu). You can also specify the 1-player team's ATK and DEF ranges separately with the `a1:` and `d1:` overrides. Note that these ranges will only be used if the chosen map or quest does not override them.
    * `$stat <what>`: Show a statistic about your player or team in the current battle. `<what>` can be `duration`, `fcs-destroyed`, `cards-destroyed`, `damage-given`, `damage-taken`, `opp-cards-destroyed`, `own-cards-destroyed`, `move-distance`, `cards-set`, `fcs-set`, `attack-actions-set`, `techs-set`, `assists-set`, `defenses-self`, `defenses-ally`, `cards-drawn`, `max-attack-damage`, `max-combo`, `attacks-given`, `attacks-taken`, `sc-damage`, `damage-defended`, or `rank`.
    * `$surrender`: Cause your team to immediately lose the current battle. If your story character is already defeated, you can't surrender - only your teammate can.
    * `$saverec <name>`: Save the recording of the last battle.
    * `$playrec <name>`: Play a battle recording. This command creates a spectator team immediately but the replay does not start automatically, to give other players a chance to join. To start the battle replay within the spectator team, run `$playrec` again (with no name). There is a bug in Dolphin that makes this command unstable in emulation (see the "Battle records" section above).

* Cheat mode commands
    * `$cheat` (game server only): Enable or disable cheat mode for the current game. All other cheat mode commands do nothing if cheat mode is disabled. By default, cheat mode is off in new games but can be enabled; there is an option in config.json that allows you to disable cheat mode entirely, or set it to on by default in new games. Cheat mode is always enabled on the proxy server, unless cheat mode is disabled on the entire server.
    * `$infhp`: Enable or disable infinite HP mode. Applies to only you; does not affect other players. When enabled, one-hit KO attacks will still kill you, but on most versions of the game (not DCv1, GC US 1.2, or GC JP 1.5), the server will automatically revive you if you die. On all versions except GC US 1.2 and GC JP 1.5, infinite HP also automatically cures status ailments.
    * `$inftp`: Enable or disable infinite TP mode. Applies to only you; does not affect other players.
    * `$warpme <floor-id>` (or `$warp <floor-id>`): Warp yourself to the given floor.
    * `$warpall <floor-id>`: Warp everyone in the game to the given floor. You must be the leader to use this command, unless you're on the proxy server.
    * `$next`: Warp yourself to the next floor.
    * `$item <desc>` (or `$i <desc>`): Create an item. `desc` may be a description of the item (e.g. "Hell Saber +5 0/10/25/0/10") or a string of hex data specifying the item code. Item codes are 16 hex bytes; at least 2 bytes must be specified, and all unspecified bytes are zeroes. If you are on the proxy server, you must not be using Blue Burst for this command to work. On the game server, this command works for all versions.
    * `$unset <index>` (game server only): In an Episode 3 battle, removes one of your set cards from the field. `<index>` is the index of the set card as it appears on your screen - 1 is the card next to your SC's icon, 2 is the card to the right of 1, etc. This does not cause a Hunters-side SC to lose HP, as they normally do when their items are destroyed.
    * `$dropmode [mode]` (proxy server): Change the way item drops behave in the current game, if you are not on BB. Unlike the game server version of this command, using this on the proxy server requires cheats to be enabled. This works by intercepting the drop requests sent to and from the leader. (So, if you are the leader and not using server drop mode on the remote server, it affects the entire game; otherwise, it affects only items generated by your actions.) `mode` can be `none` (no drops), `default` (normal drops), or `proxy` (use newserv's drop tables instead of the remote server's). If `mode` is not given, tells you the current drop mode without changing it.

* Aesthetic commands
    * `$event <event>`: Set the current holiday event in the current lobby. Holiday events are documented in the "Using $event" item in the information menu. If you're on the proxy server, this applies to all lobbies and games you join, but only you will see the new event - other players will not.
    * `$allevent <event>` (game server only): Set the current holiday event in all lobbies.
    * `$song <song-id>` (Episode 3 only): Play a specific song in the current lobby.

* Administration commands (game server only)
    * `$ann <message>`: Send an announcement message. The message is sent as temporary on-screen text to all players in all games and lobbies. On BB, the message appears in the scrolling top bar.
    * `$ann!`, `$ann?`, `$ann?!`: Same as `$ann`, but with `?`, omits the sender's name, and with `!`, sends the message as a Simple Mail message instead of on-screen text.
    * `$silence <identifier>`: Silence a player (remove their ability to chat) or unsilence a player. The identifier may be the player's name or Guild Card number.
    * `$kick <identifier>`: Disconnect a player. The identifier may be the player's name or Guild Card number.
    * `$ban <duration> <identifier>`: Ban a player. The duration should be of the form `10m` (minutes), `10h` (hours), `10d` (days), `10w` (weeks), `10M` (months), or `10y` (years). (Numbers other than 10 may be used, of course.) As with `$kick`, the identifier may be the player's name or Guild Card number.

### Using $edit

The $edit command modifies your character data. This command doesn't work on V3 (GameCube/Xbox). If you are on V1 or V2 (DC or PC, not BB), your changes will be undone if you join a game - to save your changes, disconnect from the lobby.

Some subcommands are always available. They are:
* `$edit mat reset power`: Clear your usage of power materials (BB only)
* `$edit mat reset mind`: Clear your usage of mind materials (BB only)
* `$edit mat reset evade`: Clear your usage of evade materials (BB only)
* `$edit mat reset def`: Clear your usage of def materials (BB only)
* `$edit mat reset luck`: Clear your usage of luck materials (BB only)
* `$edit mat reset hp`: Clear your usage of HP materials (BB only)
* `$edit mat reset tp`: Clear your usage of TP materials (BB only)
* `$edit mat reset all`: Clear your usage of all materials except HP and TP (BB only)
* `$edit mat reset every`: Clear your usage of all materials including HP and TP (BB only)
* `$edit namecolor AARRGGBB`: Set your name color (AARRGGBB specified in hex)
* `$edit language L`: Set your language (Generally only useful on BB; values for L: J = Japanese, E = English, G = German, F = French, S = Spanish, B = Simplified Chinese, T = Traditional Chinese, K = Korean)
* `$edit name NAME`: Set your character name
* `$edit npc NPC-NAME`: Set or remove an NPC skin on your character (use `none` to remove a skin). The NPC names are:
    * On all versions except DCv1 and early prototypes: `ninja`, `rico`, `sonic`, `knuckles`, `tails`
    * On GC, Xbox, and BB: `flowen`, `elly`
    * On BB only: `momoka`, `irene`, `guild`, `nurse`
* `$edit secid SECID-NAME`: Set your section ID (cheat mode is required unless your character is Level 1)

The remaining subcommands are only available if cheat mode is enabled on the server. They are:
* `$edit atp N`: Set your ATP to N until stats are updated (e.g. by leveling up)
* `$edit mst N`: Set your MST to N until stats are updated
* `$edit evp N`: Set your EVP to N until stats are updated
* `$edit dfp N`: Set your DFP to N until stats are updated
* `$edit ata N`: Set your ATA to N until stats are updated
* `$edit lck N`: Set your LCK to N until stats are updated
* `$edit hp N`: Set your HP to N until stats are updated
* `$edit meseta N`: Set the amount of Meseta in your inventory
* `$edit exp N`: Set your total amount of EXP (does not affect level)
* `$edit level N`: Set your current level (recomputes stats, but does not affect EXP)
* `$edit tech TECH-NAME LEVEL`: Set the level of one of your techniques

## REST API

newserv has an optional HTTP server that provides a way to programmatically get data from the server in realtime. This is intended for use with external integrations; for example, a web site could query this API to get the current player count to display on the home page.

The HTTP server is disabled by default, and you have to explicitly enable it in config.json if you want this functionality. **If you enable it, make sure that the HTTP port can't be accessed from the public Internet.** The API provides a lot of internal data about players and games, and it should only be accessed by programs that you've written or that you trust.

To enable the HTTP server, add a port number in the HTTPListen list in config.json. The HTTP server will listen on that port.

All returned data is JSON-encoded, and all request data (for POST requests) must also be JSON-encoded with the `Content-Type: application/json` header.

The HTTP server has the following endpoints:
* `GET /`: Returns the server's build date and revision.
* `GET /y/data/ep3-cards`: Returns the Episode 3 card definitions.
* `GET /y/data/ep3-cards-trial`: Returns the Episode 3 Trial Edition card definitions.
* `GET /y/data/common-tables`: Returns the parameters for generating common items (ItemPT files). This endpoint returns a lot of data and can be slow!
* `GET /y/data/rare-tables`: Returns a list of rare table names.
* `GET /y/data/rare-tables/<TABLE-NAME>` (for example, `/y/data/rare-tables/rare-table-v4`): Returns the contents of a rare item table.
* `GET /y/data/quests`: Returns metadata about all available quests and quest categories.
* `GET /y/data/config`: Returns the server's configuration file.
* `GET /y/accounts`: Returns information about all registered accounts.
* `GET /y/clients`: Returns information about all connected clients on the game server.
* `GET /y/proxy-clients`: Returns information about all connected clients on the proxy server.
* `GET /y/lobbies`: Returns information about all lobbies and games.
* `GET /y/server`: Returns information about the server.
* `GET /y/summary`: Returns a summary of the server's state, connected clients, active games, and proxy sessions.
* `WS /y/rare-drops/stream`: WebSocket endpoint that sends messages whenever an announceable rare item is dropped in any game. See below.
* `POST /y/shell-exec`: Runs a server shell command. Input should be a JSON dict of e.g. `{"command": "announce hello"}`; response will be a JSON dict of `{"result": "<result text>"}` or an HTTP error.

### Rare drop stream endpoint

The `/y/rare-drops/stream` endpoint provides a way to implement a drop log in e.g. Discord. For every announceable rare item, a message is sent to all connected clients on this endpoint. (Announceable rare items are items for which an in-game or server-wide text message is sent announcing the find.)

Upon connecting, you'll get the message `{"ServerType": "newserv"}`. After that, when a rare item announcement is sent, you'll get a message like this:
```
{
  "PlayerAccountID", 12345,
  "PlayerName", "SONIC",
  "PlayerVersion", "GC_V3",
  "GameName", "ttf",
  "GameDropMode", "SERVER_PRIVATE",
  "ItemData", "03000000 00010000 00000000 (0021002C) 00000000",
  "ItemDescription", "Monomate x1",
  "NotifyGame", true,
  "NotifyServer", false,
}
```

# Non-server features

newserv has many CLI options, which can be used to access functionality other than the game and proxy server. Run `newserv help` to see a full list of the options and how to use each one.

The data formats that newserv can convert to/from are:

| Format                         | Encode/compress action    | Decode/extract action        |
|--------------------------------|---------------------------|------------------------------|
| PRS compression                | `compress-prs`            | `decompress-prs`             |
| PR2/PRC compression            | `compress-pr2`            | `decompress-pr2`             |
| BC0 compression                | `compress-bc0`            | `decompress-bc0`             |
| Raw encrypted data             | `encrypt-data`            | `decrypt-data`               |
| Episode 3 command mask         | `encrypt-trivial-data`    | `decrypt-trivial-data`       |
| Challenge Mode rank text       | `encrypt-challenge-data`  | `decrypt-challenge-data`     |
| PSO DC quest file (.vms)       | None                      | `decode-vms`                 |
| PSO GC quest file (.gci)       | None                      | `decode-gci`                 |
| Download quest file (.dlq)     | None                      | `decode-dlq`                 |
| Server quest file (.qst)       | `encode-qst`              | `decode-qst`                 |
| PSO DC save file (.vms)        | `encrypt-vms-save`        | `decrypt-vms-save`           |
| PSO PC save file               | `encrypt-pc-save`         | `decrypt-pc-save`            |
| PSO GC save file (.gci)        | `encrypt-gci-save`        | `decrypt-gci-save`           |
| PSO GC snapshot file           | None                      | `decode-gci-snapshot`        |
| Quest script (.bin)            | `assemble-quest-script`   | `disassemble-quest-script`   |
| Quest map (.dat)               | None                      | `disassemble-quest-map`      |
| AFS archive                    | None                      | `extract-afs`                |
| BML archive                    | None                      | `extract-bml`                |
| GSL archive                    | None                      | `extract-gsl`                |
| GVM texture                    | `encode-gvm`              | None                         |
| Text archive                   | `encode-text-archive`     | `decode-text-archive`        |
| Unicode text set               | `encode-unicode-text-set` | `decode-unicode-text-set`    |
| Word Select data set           | None                      | `decode-word-select-set`     |
| Set data table                 | None                      | `disassemble-set-data-table` |
| Rare item table (AFS/GSL/JSON) | `convert-rare-item-set`   | `convert-rare-item-set`      |

There are several actions that don't fit well into the table above, which let you do other things:

* Compute the decompressed size of compressed PRS data without decompressing it (`prs-size`)
* Find the likely round1 or round2 seed for a corrupt save file (`salvage-gci`)
* Run a brute-force search for a decryption seed (`find-decryption-seed`)
* Format Episode 3 game data in a human-readable manner (`show-ep3-maps`, `show-ep3-cards`, `generate-ep3-cards-html`)
* Format Blue Burst battle parameter files in a human-readable manner (`show-battle-params`)
* Convert item data to a human-readable description, or vice versa (`describe-item`)
* Connect to another PSO server and pretend to be a client (`cat-client`)
* Generate or describe DC serial numbers (`generate-dc-serial-number`, `inspect-dc-serial-number`)

# Docker
Docker is new and mostly unsupported at this time. However, here are some best-effort steps to build and run in a docker container on Ubuntu Linux.
Tested on Ubuntu 22.04.4 LTS.
Note: You cannot have anything except this docker container using port 53 (DNS) on your server.

Install prerequisites
```
sudo apt install -y git
sudo apt install -y cmake. ## minimum version is 3.10. Check installed version with "cmake --version"
```

Clone repository
```
cd ~
git clone https://github.com/fuzziqersoftware/newserv/
cd ~/newserv
```

Build newserv. This will take a while. Don't forget the period at the end!
```
sudo docker build -t newserv .
```

Create persistent directories. Assuming you want to store the persistent data in your home directory
```
mkdir ~/newservPersist
mkdir ~/newservPersist/players
mkdir ~/newservPersist/teams
mkdir ~/newservPersist/licenses
```

Copy config file to config dir
```
cp ~/newserv/system/config.example.json ~/newservPersist/config.json
```

Edit config.json
```
nano ~/newservPersist/config.json
```
Pro tip:
Set "LocalAddress" to the static, LAN IP address of your server. If your server LAN IP is "192.168.0.10":
"LocalAddress": "192.168.0.10",

Set "ExternalAddress" to the WAN IP address of your network. If your WAN IP is "8.8.8.8":
"ExternalAddress": "8.8.8.8",

For Dolphin > Settings. Set SP1 to "Broadband Adapter (HLE)" Click [...] next to this, and set the DNS to the IP address of your server. Then start the game. Changes will not take affect if the game is running.

Docker run. Remember to change /home/changeme/newservPersist to your persistent directory. Do not use aliases such as '~'
```
docker run --name newserv -p 53:53/udp -p 5100:5100 -p 5110:5110 -p 5111:5111 -p 5112:5112 -p 9064:9064 -p 9100:9100 -p 9103:9103 -p 9300:9300 -p 11000:11000 -p 12000:12000 -p 12004:12004 -p 12005:12005 -v /etc/localtime:/etc/localtime:ro -v /home/changeme/newservPersist/config.json:/newserv/system/config.json -v /home/changeme/newservPersist/players:/newserv/system/players -v /home/changeme/newservPersist/teams:/newserv/system/teams -v /home/changeme/newservPersist/licenses:/newserv/system/licenses --restart no newserv:latest
```

Docker run host network mode. Remember to change /home/changeme/newservPersist to your persistent directory. Do not use aliases such as '~'
```
docker run --net host --name newserv -v /etc/localtime:/etc/localtime:ro -v /home/changeme/newservPersist/config.json:/newserv/system/config.json -v /home/changeme/newservPersist/players:/newserv/system/players -v /home/changeme/newservPersist/teams:/newserv/system/teams -v /home/changeme/newservPersist/licenses:/newserv/system/licenses --restart no newserv:latest
```

Docker compose. Remember to change /home/changeme/newservPersist to your persistent directory. Do not use aliases such as '~'
```
name: psonewserv
services:
    newserv:
        container_name: newserv
        ports:
            - 53:53/udp
            - 5100:5100
            - 5110:5110
            - 5111:5111
            - 5112:5112
            - 9064:9064
            - 9100:9100
            - 9103:9103
            - 9300:9300
            - 11000:11000
            - 12000:12000
            - 12004:12004
            - 12005:12005
        volumes:
            - /etc/localtime:/etc/localtime:ro
            - /home/changeme/newservPersist/config.json:/newserv/system/config.json
            - /home/changeme/newservPersist/players:/newserv/system/players
            - /home/changeme/newservPersist/teams:/newserv/system/teams
            - /home/changeme/newservPersist/licenses:/newserv/system/licenses
        restart: no ## Set to whatever you want.
        image: newserv:latest
```
Docker compose host network mode. Remember to change /home/changeme/newservPersist to your persistent directory. Do not use aliases such as '~'
```
name: psonewserv
services:
    newserv:
        container_name: newserv
        volumes:
            - /etc/localtime:/etc/localtime:ro
            - /home/changeme/newservPersist/config.json:/newserv/system/config.json
            - /home/changeme/newservPersist/players:/newserv/system/players
            - /home/changeme/newservPersist/teams:/newserv/system/teams
            - /home/changeme/newservPersist/licenses:/newserv/system/licenses
        restart: no ## Set to whatever you want.
        network_mode: host
        image: newserv:latest
```

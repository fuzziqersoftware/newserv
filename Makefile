OBJECTS=FileContentsCache.o Menu.o PSOProtocol.o Client.o Lobby.o \
	ServerState.o Server.o License.o PSOEncryption.o Player.o SendCommands.o \
	ChatCommands.o ReceiveSubcommands.o ReceiveCommands.o Version.o Items.o \
	LevelTable.o Compression.o Quest.o RareItemSet.o Map.o NetworkAddresses.o \
	Text.o DNSServer.o ProxyServer.o Shell.o ServerShell.o ProxyShell.o Main.o
CXX=g++
CXXFLAGS=-I/opt/local/include -I/usr/local/include -std=c++20 -g -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -Wall -Werror
LDFLAGS=-L/opt/local/lib -L/usr/local/lib -std=c++20 -levent -levent_pthreads -lphosg -lpthread

all: newserv

newserv: $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o newserv

clean:
	find . -name \*.o -delete
	rm -rf *.dSYM newserv newserv-dns gmon.out

.PHONY: clean test

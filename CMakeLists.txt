cmake_minimum_required(VERSION 3.10)



# Project setup

project(newserv)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wno-address-of-packed-member)
endif()

include_directories("/usr/local/include")
link_directories("/usr/local/lib")

set(CMAKE_BUILD_TYPE Debug)



# Executable definitions

find_path     (LIBEVENT_INCLUDE_DIR NAMES event.h)
find_library  (LIBEVENT_LIBRARY     NAMES event)
find_library  (LIBEVENT_CORE        NAMES event_core)
set (LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set (LIBEVENT_LIBRARIES
        ${LIBEVENT_LIBRARY}
        ${LIBEVENT_CORE})

add_executable(newserv
  src/ChatCommands.cc
  src/Client.cc
  src/Compression.cc
  src/DNSServer.cc
  src/Episode3.cc
  src/FileContentsCache.cc
  src/IPFrameInfo.cc
  src/IPStackSimulator.cc
  src/Items.cc
  src/LevelTable.cc
  src/License.cc
  src/Lobby.cc
  src/Main.cc
  src/Map.cc
  src/Menu.cc
  src/NetworkAddresses.cc
  src/Player.cc
  src/ProxyCommands.cc
  src/ProxyServer.cc
  src/PSOEncryption.cc
  src/PSOProtocol.cc
  src/Quest.cc
  src/RareItemSet.cc
  src/ReceiveCommands.cc
  src/ReceiveSubcommands.cc
  src/SendCommands.cc
  src/Server.cc
  src/ServerShell.cc
  src/ServerState.cc
  src/Shell.cc
  src/StaticGameData.cc
  src/Text.cc
  src/Version.cc
)
target_include_directories(newserv PUBLIC ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(newserv phosg ${LIBEVENT_LIBRARIES})



# Installation configuration

install(TARGETS newserv DESTINATION bin)

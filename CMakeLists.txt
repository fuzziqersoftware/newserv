cmake_minimum_required(VERSION 3.10)



# Project setup

project(newserv)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

include_directories("/usr/local/include")
link_directories("/usr/local/lib")



# Executable definitions

find_path     (LIBEVENT_INCLUDE_DIR NAMES event.h)
find_library  (LIBEVENT_LIBRARY     NAMES event)
find_library  (LIBEVENT_CORE        NAMES event_core)
find_library  (LIBEVENT_THREAD      NAMES event_pthreads)
set (LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set (LIBEVENT_LIBRARIES
        ${LIBEVENT_LIBRARY}
        ${LIBEVENT_CORE}
        ${LIBEVENT_THREAD})

add_executable(newserv
  src/FileContentsCache.cc
  src/Menu.cc
  src/PSOProtocol.cc
  src/Client.cc
  src/Lobby.cc
  src/ServerState.cc
  src/Server.cc
  src/License.cc
  src/PSOEncryption.cc
  src/Player.cc
  src/SendCommands.cc
  src/ChatCommands.cc
  src/ReceiveSubcommands.cc
  src/ReceiveCommands.cc
  src/Version.cc
  src/Items.cc
  src/LevelTable.cc
  src/Compression.cc
  src/Quest.cc
  src/RareItemSet.cc
  src/Map.cc
  src/NetworkAddresses.cc
  src/Text.cc
  src/DNSServer.cc
  src/ProxyServer.cc
  src/Shell.cc
  src/ServerShell.cc
  src/ProxyShell.cc
  src/Main.cc
)
target_include_directories(newserv PUBLIC ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(newserv phosg ${LIBEVENT_LIBRARIES})



# Installation configuration

install(TARGETS newserv DESTINATION bin)

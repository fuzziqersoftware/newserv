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

set(LOCAL_INCLUDE_DIR "/usr/local/include")
set(LOCAL_LIB_DIR "/usr/local/lib")
list(APPEND CMAKE_PREFIX_PATH ${LOCAL_LIB_DIR})
include_directories(${LOCAL_INCLUDE_DIR})
link_directories(${LOCAL_LIB_DIR})



# Library search

find_path     (LIBEVENT_INCLUDE_DIR NAMES event.h)
find_library  (LIBEVENT_LIBRARY     NAMES event)
find_library  (LIBEVENT_CORE        NAMES event_core)
set (LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set (LIBEVENT_LIBRARIES
        ${LIBEVENT_LIBRARY}
        ${LIBEVENT_CORE})

find_package(phosg REQUIRED)
find_package(resource_file QUIET)



# Executable definition

add_executable(newserv
    src/BattleParamsIndex.cc
    src/BMLArchive.cc
    src/CatSession.cc
    src/Channel.cc
    src/ChatCommands.cc
    src/Client.cc
    src/CommonItemSet.cc
    src/Compression.cc
    src/DNSServer.cc
    src/EnemyType.cc
    src/Episode3/AssistServer.cc
    src/Episode3/BattleRecord.cc
    src/Episode3/Card.cc
    src/Episode3/CardSpecial.cc
    src/Episode3/DataIndex.cc
    src/Episode3/DeckState.cc
    src/Episode3/MapState.cc
    src/Episode3/PlayerState.cc
    src/Episode3/PlayerStateSubordinates.cc
    src/Episode3/RulerServer.cc
    src/Episode3/Server.cc
    src/Episode3/Tournament.cc
    src/FileContentsCache.cc
    src/FunctionCompiler.cc
    src/GSLArchive.cc
    src/IPFrameInfo.cc
    src/IPStackSimulator.cc
    src/ItemCreator.cc
    src/ItemData.cc
    src/ItemParameterTable.cc
    src/Items.cc
    src/LevelTable.cc
    src/License.cc
    src/Lobby.cc
    src/Loggers.cc
    src/Main.cc
    src/Map.cc
    src/Menu.cc
    src/NetworkAddresses.cc
    src/PatchFileIndex.cc
    src/Player.cc
    src/Product.cc
    src/ProxyCommands.cc
    src/ProxyServer.cc
    src/PSOEncryption.cc
    src/PSOGCObjectGraph.cc
    src/PSOProtocol.cc
    src/Quest.cc
    src/RareItemSet.cc
    src/ReceiveCommands.cc
    src/ReceiveSubcommands.cc
    src/ReplaySession.cc
    src/SaveFileFormats.cc
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
target_link_libraries(newserv phosg ${LIBEVENT_LIBRARIES} pthread)

if(resource_file_FOUND)
    target_compile_definitions(newserv PUBLIC HAVE_RESOURCE_FILE)
    target_link_libraries(newserv resource_file)
    message(STATUS "libresource_file found; enabling patch support")
else()
    message(WARNING "libresource_file not found; disabling patch support")
endif()



# Test configuration

enable_testing()

file(GLOB TestCases ${CMAKE_SOURCE_DIR}/tests/*.test.txt)

foreach(TestCase IN ITEMS ${TestCases})
    add_test(
        NAME ${TestCase}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/newserv replay-log ${TestCase} --config=${CMAKE_SOURCE_DIR}/tests/config.json --require-password=11111111 --require-access-key=111111111111)
endforeach()

add_test(
    NAME "compression-prs"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-compression.sh prs ${CMAKE_BINARY_DIR}/newserv)
add_test(
    NAME "compression-bc0"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-compression.sh bc0 ${CMAKE_BINARY_DIR}/newserv)

add_test(
    NAME "decode-vms"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-decode-vms.sh ${CMAKE_BINARY_DIR}/newserv)

add_test(
    NAME "decode-gci"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-decode-gci.sh ${CMAKE_BINARY_DIR}/newserv)

# Installation configuration

install(TARGETS newserv DESTINATION bin)

cmake_minimum_required(VERSION 3.10)
set(CMAKE_POLICY_DEFAULT_CMP0110 NEW)



# Project setup

project(newserv)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wno-address-of-packed-member)
endif()



# Library search

find_path     (LIBEVENT_INCLUDE_DIR NAMES event.h)
find_library  (LIBEVENT_LIBRARY     NAMES event)
find_library  (LIBEVENT_CORE        NAMES event_core)
find_library  (LIBEVENT_PTHREADS    NAMES event_pthreads)
set (LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set (LIBEVENT_LIBRARIES
        ${LIBEVENT_LIBRARY}
        ${LIBEVENT_CORE}
        ${LIBEVENT_PTHREADS})

find_package(phosg REQUIRED)
find_package(Iconv REQUIRED)
find_package(resource_file QUIET)



# Git metadata

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/Revision.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/__Revision__.cc
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/Revision-generate.sh ${CMAKE_CURRENT_SOURCE_DIR}/src/Revision.cc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_custom_target(
    newserv-Revision-cc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/Revision.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/__Revision__.cc
)



# Executable definition

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Revision.cc
    src/Account.cc
    src/AFSArchive.cc
    src/BattleParamsIndex.cc
    src/BMLArchive.cc
    src/CatSession.cc
    src/Channel.cc
    src/ChatCommands.cc
    src/ChoiceSearch.cc
    src/Client.cc
    src/CommonItemSet.cc
    src/Compression.cc
    src/DCSerialNumbers.cc
    src/DNSServer.cc
    src/DownloadSession.cc
    src/EnemyType.cc
    src/Episode3/AssistServer.cc
    src/Episode3/BattleRecord.cc
    src/Episode3/Card.cc
    src/Episode3/CardSpecial.cc
    src/Episode3/DataIndexes.cc
    src/Episode3/DeckState.cc
    src/Episode3/MapState.cc
    src/Episode3/PlayerState.cc
    src/Episode3/PlayerStateSubordinates.cc
    src/Episode3/RulerServer.cc
    src/Episode3/Server.cc
    src/Episode3/Tournament.cc
    src/EventUtils.cc
    src/FileContentsCache.cc
    src/FunctionCompiler.cc
    src/GSLArchive.cc
    src/GVMEncoder.cc
    src/HTTPServer.cc
    src/IntegralExpression.cc
    src/IPFrameInfo.cc
    src/IPStackSimulator.cc
    src/IPV4RangeSet.cc
    src/ItemCreator.cc
    src/ItemData.cc
    src/ItemNameIndex.cc
    src/ItemParameterTable.cc
    src/Items.cc
    src/LevelTable.cc
    src/Lobby.cc
    src/Loggers.cc
    src/Main.cc
    src/Map.cc
    src/Menu.cc
    src/NetworkAddresses.cc
    src/PatchFileIndex.cc
    src/PatchServer.cc
    src/PlayerFilesManager.cc
    src/PlayerSubordinates.cc
    src/ProxyCommands.cc
    src/ProxyServer.cc
    src/PSOEncryption.cc
    src/PSOGCObjectGraph.cc
    src/PSOProtocol.cc
    src/Quest.cc
    src/QuestScript.cc
    src/RareItemSet.cc
    src/ReceiveCommands.cc
    src/ReceiveSubcommands.cc
    src/ReplaySession.cc
    src/Revision.cc
    src/SaveFileFormats.cc
    src/SendCommands.cc
    src/Server.cc
    src/ServerShell.cc
    src/ServerState.cc
    src/ShellCommands.cc
    src/SignalWatcher.cc
    src/StaticGameData.cc
    src/TeamIndex.cc
    src/Text.cc
    src/TextIndex.cc
    src/Version.cc
    src/WordSelectTable.cc
)

if(resource_file_FOUND)
    set(SOURCES ${SOURCES} src/AddressTranslator.cc)
endif()

add_executable(newserv ${SOURCES})
target_include_directories(newserv PUBLIC ${LIBEVENT_INCLUDE_DIR} ${Iconv_INCLUDE_DIRS})
target_link_libraries(newserv phosg::phosg ${LIBEVENT_LIBRARIES} ${Iconv_LIBRARIES} pthread)
if(resource_file_FOUND)
    target_compile_definitions(newserv PUBLIC HAVE_RESOURCE_FILE)
    target_link_libraries(newserv resource_file::resource_file)
    message(STATUS "resource_file found; enabling patch support")
else()
    message(WARNING "resource_file not found; disabling patch support")
endif()
add_dependencies(newserv newserv-Revision-cc)

# target_compile_options(newserv PRIVATE -fsanitize=address)
# target_link_options(newserv PRIVATE -fsanitize=address)



# Test configuration

enable_testing()

file(GLOB LogTestCases ${CMAKE_SOURCE_DIR}/tests/*.test.txt)
file(GLOB LogRDTestCases ${CMAKE_SOURCE_DIR}/tests/*.rdtest.txt)

foreach(LogTestCase IN ITEMS ${LogTestCases})
    add_test(
        NAME ${LogTestCase}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/newserv --replay-log=${LogTestCase} --config=${CMAKE_SOURCE_DIR}/tests/config.json)
endforeach()

if(resource_file_FOUND)
    foreach(LogRDTestCase IN ITEMS ${LogRDTestCases})
        add_test(
            NAME ${LogRDTestCase}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND ${CMAKE_BINARY_DIR}/newserv --replay-log=${LogRDTestCase} --config=${CMAKE_SOURCE_DIR}/tests/config.json)
    endforeach()
endif()

file(GLOB ScriptTestCases ${CMAKE_SOURCE_DIR}/tests/*.test.sh)

foreach(ScriptTestCase IN ITEMS ${ScriptTestCases})
    add_test(
        NAME ${ScriptTestCase}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${ScriptTestCase} ${CMAKE_BINARY_DIR}/newserv)
endforeach()



# Installation configuration

install(TARGETS newserv DESTINATION bin)

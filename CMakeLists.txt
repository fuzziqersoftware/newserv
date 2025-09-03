cmake_minimum_required(VERSION 3.22)
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

find_path(ASIO_INCLUDE_DIR NAMES asio.hpp HINTS "${WINDOWS_ENV}/include" REQUIRED)
if(WIN32)
    find_path(Iconv_INCLUDE_DIRS NAMES iconv.h HINTS "${WINDOWS_ENV}/include" REQUIRED)
    find_library(Iconv_LIBRARIES NAMES iconv HINTS "${WINDOWS_ENV}/lib" REQUIRED)
else()
    find_package(Iconv REQUIRED)
endif()
find_package(phosg REQUIRED)
find_package(resource_file REQUIRED)



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
    src/AddressTranslator.cc
    src/AFSArchive.cc
    src/AsyncHTTPServer.cc
    src/AsyncUtils.cc
    src/BattleParamsIndex.cc
    src/BMLArchive.cc
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
    src/FileContentsCache.cc
    src/FunctionCompiler.cc
    src/GameServer.cc
    src/GSLArchive.cc
    src/HTTPServer.cc
    src/ImageEncoder.cc
    src/IntegralExpression.cc
    src/IPFrameInfo.cc
    src/IPStackSimulator.cc
    src/IPV4RangeSet.cc
    src/ItemCreator.cc
    src/ItemData.cc
    src/ItemNameIndex.cc
    src/ItemParameterTable.cc
    src/Items.cc
    src/ItemTranslationTable.cc
    src/LevelTable.cc
    src/Lobby.cc
    src/Loggers.cc
    src/Main.cc
    src/Map.cc
    src/Menu.cc
    src/NetworkAddresses.cc
    src/PatchFileIndex.cc
    src/PlayerInventory.cc
    src/PlayerSubordinates.cc
    src/PPKArchive.cc
    src/ProxyCommands.cc
    src/ProxySession.cc
    src/PSOEncryption.cc
    src/PSOGCObjectGraph.cc
    src/PSOProtocol.cc
    src/Quest.cc
    src/QuestScript.cc
    src/RareItemSet.cc
    src/ReceiveCommands.cc
    src/ReceiveSubcommands.cc
    src/ReplaySession.cc
    src/SaveFileFormats.cc
    src/SendCommands.cc
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

add_executable(newserv ${SOURCES})
target_include_directories(newserv PUBLIC ${ASIO_INCLUDE_DIR} ${Iconv_INCLUDE_DIRS})
target_link_libraries(newserv phosg::phosg ${Iconv_LIBRARIES} pthread resource_file::resource_file)
if (WIN32)
    target_compile_definitions(newserv PUBLIC -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
    target_link_libraries(newserv ws2_32 mswsock bcrypt iphlpapi -static -static-libgcc -static-libstdc++)
    target_compile_options(newserv PRIVATE -Wa,-mbig-obj)
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

file(GLOB ScriptTestCases ${CMAKE_SOURCE_DIR}/tests/*.test.sh)

foreach(ScriptTestCase IN ITEMS ${ScriptTestCases})
    add_test(
        NAME ${ScriptTestCase}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${ScriptTestCase} ${CMAKE_BINARY_DIR}/newserv)
endforeach()



# Installation configuration

install(TARGETS newserv DESTINATION bin)

cmake_minimum_required(VERSION 3.13)

project(ServerOrganizer)

if (UNIX)
else ()
    message(FATAL_ERROR "only unix is supported")
endif ()

add_subdirectory(commandline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra --pedantic")

add_executable(ServerOrganizer
        src/main.cpp
        src/ArgsParser.cpp src/ArgsParser.h
        src/Common.cpp src/Common.h)

add_executable(ServerOrganizer_HeadlessServer
        src/server.cpp
        src/ArgsParser.cpp src/ArgsParser.h
        src/Common.cpp src/Common.h)

target_include_directories(ServerOrganizer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerOrganizer commandline pthread)
target_include_directories(ServerOrganizer_HeadlessServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ServerOrganizer_HeadlessServer pthread)

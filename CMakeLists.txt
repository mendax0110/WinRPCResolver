cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project(WinRpcResolver)

include_directories(include)

set (WIN_SOURCES
    src/Windows/ResolveWindows.cpp
    src/ResolverWin.cpp
)

add_executable(${PROJECT_NAME} ${WIN_INCLUDES} ${WIN_SOURCES})


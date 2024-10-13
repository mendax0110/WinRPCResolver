cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project(WinRpcResolver)

set(IMGUI_DIR externals/imgui)

file(GLOB IMGUI_SOURCE ${IMGUI_DIR}/*.cpp)

list(APPEND IMGUI_SOURCE 
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCE})

target_include_directories(
    imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

set(WIN_INCLUDES
    include/RpcMonitor.h
    include/RpcServersConfig.h
    include/FileCrawler.h
)

set (WIN_SOURCES
    src/RpcMonitor.cpp
    src/RpcServersConfig.cpp
    src/FileCrawler.cpp
    src/main.cpp
)

add_executable(${PROJECT_NAME} ${WIN_INCLUDES} ${WIN_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE advapi32 d3d11 imgui)

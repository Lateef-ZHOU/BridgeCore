cmake_minimum_required(VERSION 3.28)
project(BridgeCore VERSION 0.1.0 LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "bridge currently targets Linux")
endif()
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core)
include(FetchContent)
set(HANDLECORE_WITH_SERIAL ON CACHE BOOL "Serial transport" FORCE)
set(HANDLECORE_WITH_PCAP ON CACHE BOOL "Ethernet UDP transport" FORCE)
FetchContent_Declare(lidar_handle_core
    GIT_REPOSITORY https://github.com/Lateef-ZHOU/HandleCore.git
    GIT_TAG 3470a1d9f3826f4a666b0e53c29c001f0901fbbd)
# Only use each residence's protocol headers, not its UI/business build.
FetchContent_Declare(sqaf
    GIT_REPOSITORY https://github.com/Lateef-ZHOU/SqafDevice.git
    GIT_TAG 9f2e4fdfbcaee661c60ec17330a736596a6dac71
    SOURCE_SUBDIR include)
FetchContent_Declare(whls
    # Keep the verified body-TX revision; transport semantics do not change on publication.
    GIT_REPOSITORY https://github.com/Lateef-ZHOU/RayshiningDevice.git
    GIT_TAG 1d5e1a3cdfbab3e7d06c983aa242ce71de6f2333
    SOURCE_SUBDIR include)
FetchContent_MakeAvailable(lidar_handle_core sqaf whls)
set(CDL5_WITH_DMA ON CACHE BOOL "DMA acquisition endpoint" FORCE)
# The shared DmaProxy ABI is supplied explicitly, independently of the residence checkout.
FetchContent_Declare(cdl5
    GIT_REPOSITORY https://github.com/Lateef-ZHOU/cdl5Device.git
    GIT_TAG 53eb8c1ba3b7ed00adfc0e674c047674d14e7a23)
FetchContent_MakeAvailable(cdl5)
find_package(Boost 1.88 QUIET CONFIG COMPONENTS circular_buffer)
if(NOT TARGET Boost::circular_buffer)
    set(BOOST_INCLUDE_LIBRARIES circular_buffer)
    FetchContent_Declare(boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.xz
        URL_HASH SHA256=f48b48390380cfb94a629872346e3a81370dc498896f16019ade727ab72eb1ec)
    FetchContent_MakeAvailable(boost)
endif()
add_library(bridge_protocols STATIC
    "${sqaf_SOURCE_DIR}/include/Device/Handle/PelcoDHandle.h"
    "${whls_SOURCE_DIR}/include/Rayshining/LaserHandle.h")
target_include_directories(bridge_protocols PUBLIC
    "${sqaf_SOURCE_DIR}/include" "${whls_SOURCE_DIR}/include" src)
target_link_libraries(bridge_protocols PUBLIC Lidar::Serial Lidar::Pcap Lidar::Cdl5Dma Boost::circular_buffer)
target_compile_definitions(bridge_protocols PUBLIC QT_NO_KEYWORDS)
target_compile_options(bridge_protocols PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(bridge src/main.cpp)
target_link_libraries(bridge PRIVATE bridge_protocols)
target_compile_options(bridge PRIVATE -Wall -Wextra -Wpedantic -Werror)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
install(TARGETS bridge RUNTIME DESTINATION bin)

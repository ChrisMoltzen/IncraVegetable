# CMake build: used for iPhone/iPad (./build-ios.sh makes an Xcode project with
# it). It also builds the desktop game if you prefer CMake to the Makefile.
#
#   iOS:      ./build-ios.sh YOUR_TEAM_ID com.yourname.incravegetable
#   options:  -DINCRA_RELEASE=ON   release build (no debug screen, scrambled saves only)
#             -DINCRA_DEMO=ON      demo build (techs past INCRA_DEMO_RADIUS stay locked)
#             -DINCRA_DEMO_RADIUS=6
#
# On iOS every file in assets/ is packed into the app with tools/AssetPacker (as
# in `make release`), so there's no loose assets folder to copy into the bundle.
cmake_minimum_required(VERSION 3.21)
project(IncraVegetable VERSION 0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(INCRA_RELEASE "Release build: no debug screen, saves must be scrambled" OFF)
option(INCRA_DEMO "Demo build: only the techs near The Barn can be bought" OFF)
set(INCRA_DEMO_RADIUS 6 CACHE STRING "Demo builds: how far from The Barn techs can be bought, in tech tree rows")
if(INCRA_RELEASE)
    set(_debug_default OFF)
else()
    set(_debug_default ON)
endif()
option(INCRA_DEBUG_TOOLS "Include the F1 debug screen and its button" ${_debug_default})

set(INCRA_APP_NAME "IncraVegetable")
if(INCRA_DEMO)
    set(INCRA_APP_NAME "IncraVegetable Demo")
endif()

# ---------------------------------------------------------------------------
# iOS settings (only used when configuring with -DCMAKE_SYSTEM_NAME=iOS)
# ---------------------------------------------------------------------------
set(INCRA_BUNDLE_ID "com.example.incravegetable" CACHE STRING "iOS bundle identifier (must be unique to you)")
set(INCRA_DEVELOPMENT_TEAM "" CACHE STRING "Apple Developer Team ID used to sign the iOS app")

# ---------------------------------------------------------------------------
# SDL3: use an installed copy if there is one, otherwise download and build it.
# iOS always builds it from source as a static library.
# ---------------------------------------------------------------------------
if(NOT IOS)
    find_package(SDL3 CONFIG QUIET)
endif()
if(NOT SDL3_FOUND)
    message(STATUS "Building SDL3 from source (first build only)")
    include(FetchContent)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(IOS)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.24
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------------------
# Sources. Every .cpp in src/ is part of the game (the same list the Makefile
# uses), so new files never need adding here.
# ---------------------------------------------------------------------------
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB GAME_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set(ART_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(GLOB_RECURSE ART_FILES CONFIGURE_DEPENDS "${ART_DIR}/*")

# Pack the assets into the game on iOS and in release builds.
if(IOS OR INCRA_RELEASE)
    set(INCRA_PACK_ASSETS ON)
else()
    set(INCRA_PACK_ASSETS OFF)
endif()

if(INCRA_PACK_ASSETS)
    # AssetPacker runs on the computer doing the build, not on the device, so
    # it's compiled here with the computer's own compiler (when cross-compiling
    # for iOS, CMake's compiler makes iPhone programs, which can't run on a Mac).
    set(PACKER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/tools/AssetPacker/main.cpp)
    set(PACKER ${CMAKE_CURRENT_BINARY_DIR}/AssetPacker${CMAKE_HOST_EXECUTABLE_SUFFIX})
    if(CMAKE_HOST_APPLE)
        set(HOST_CXX xcrun --sdk macosx clang++)
    elseif(CMAKE_CROSSCOMPILING)
        find_program(HOST_CXX_PROGRAM NAMES c++ clang++ g++ REQUIRED)
        set(HOST_CXX ${HOST_CXX_PROGRAM})
    else()
        set(HOST_CXX ${CMAKE_CXX_COMPILER})
    endif()
    if(NOT EXISTS ${PACKER} OR ${PACKER_SRC} IS_NEWER_THAN ${PACKER}
       OR ${CMAKE_CURRENT_SOURCE_DIR}/include/AssetPackFormat.h IS_NEWER_THAN ${PACKER})
        message(STATUS "Building AssetPacker")
        execute_process(
            COMMAND ${HOST_CXX} -std=c++17 -O2 -I${CMAKE_CURRENT_SOURCE_DIR}/include ${PACKER_SRC} -o ${PACKER}
            RESULT_VARIABLE packer_result
            ERROR_VARIABLE packer_error)
        if(NOT packer_result EQUAL 0)
            message(FATAL_ERROR "Couldn't build tools/AssetPacker:\n${packer_error}")
        endif()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${PACKER_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/include/AssetPackFormat.h)

    # Re-packed whenever a file in assets/ changes.
    set(PACK_CPP ${CMAKE_CURRENT_BINARY_DIR}/AssetPack.cpp)
    add_custom_command(
        OUTPUT ${PACK_CPP}
        COMMAND ${PACKER} ${ART_DIR} ${PACK_CPP}
        DEPENDS ${ART_FILES} ${PACKER}
        COMMENT "Packing assets/ into the game"
        VERBATIM)
    list(APPEND GAME_SOURCES ${PACK_CPP})
endif()

# ---------------------------------------------------------------------------
# The game
# ---------------------------------------------------------------------------
if(IOS)
    set(IOS_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Assets.xcassets)
    add_executable(IncraVegetable MACOSX_BUNDLE ${GAME_SOURCES} ${GAME_HEADERS} ${IOS_ASSETS})
    set_source_files_properties(${IOS_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(IncraVegetable PROPERTIES
        OUTPUT_NAME "IncraVegetable"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "${INCRA_APP_NAME}"
        RESOURCE "${IOS_ASSETS}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${INCRA_BUNDLE_ID}"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "1"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" # iPhone and iPad
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${INCRA_DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        # Lets the same project run on a simulator as well as a real device.
        XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "iphoneos iphonesimulator"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "YES"
    )
else()
    add_executable(IncraVegetable WIN32 ${GAME_SOURCES} ${GAME_HEADERS})
    if(INCRA_DEMO)
        set_target_properties(IncraVegetable PROPERTIES OUTPUT_NAME "${INCRA_APP_NAME}")
    endif()
    if(NOT INCRA_PACK_ASSETS)
        # While developing, the game reads straight from the project's assets/
        # so F5 shows your changes instantly.
        target_compile_definitions(IncraVegetable PRIVATE INCRA_SOURCE_ASSETS_DIR="${ART_DIR}")
    endif()
endif()

target_include_directories(IncraVegetable PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(IncraVegetable PRIVATE SDL3::SDL3)
target_compile_definitions(IncraVegetable PRIVATE
    INCRA_DEBUG_TOOLS=$<BOOL:${INCRA_DEBUG_TOOLS}>
    $<$<BOOL:${INCRA_PACK_ASSETS}>:INCRA_PACKED_ASSETS>
    $<$<BOOL:${INCRA_RELEASE}>:INCRA_RELEASE>
    $<$<BOOL:${INCRA_DEMO}>:INCRA_DEMO=1>
    $<$<BOOL:${INCRA_DEMO}>:INCRA_DEMO_RADIUS=${INCRA_DEMO_RADIUS}>)

if(MSVC)
    target_compile_options(IncraVegetable PRIVATE /W4 /utf-8 /bigobj)
else()
    target_compile_options(IncraVegetable PRIVATE -Wall -Wextra)
endif()

# Put SDL3.dll next to the .exe on Windows so it runs straight away.
if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET IncraVegetable POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:IncraVegetable>)
endif()

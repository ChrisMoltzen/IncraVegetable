# CMake build: used for iPhone/iPad (./build-ios.sh makes an Xcode project with
# it). It also builds the desktop game if you prefer CMake to the Makefile.
#
#   iOS:      ./build-ios.sh YOUR_TEAM_ID com.yourname.incravegetable
#   Android:  make android (Gradle runs this file through android/app/build.gradle)
#   options:  -DINCRA_RELEASE=ON   release build (no debug screen, scrambled saves only)
#             -DINCRA_DEMO=ON      demo build (techs past INCRA_DEMO_RADIUS stay locked)
#             -DINCRA_DEMO_RADIUS=6
#
# On iOS and Android every file in assets/ is packed into the app with
# tools/AssetPacker (as in `make release`), so there are no loose asset files to
# copy into the app.
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
# Game Center achievements need the Game Center capability, which a free Apple
# account ("Personal Team") can't sign. Turn this off to build with one.
option(INCRA_GAME_CENTER "iOS: Game Center achievements" ON)
# The build number App Store Connect sees (CFBundleVersion). Every upload needs a
# bigger one than the last, e.g. ./build-ios.sh TEAM com.me.app -DINCRA_BUILD_NUMBER=2
set(INCRA_BUILD_NUMBER "1" CACHE STRING "iOS: build number (raise it for each App Store upload)")

# Product > Archive must hold only the app. Xcode "installs" every target into the
# archive unless it's told to skip it, and SDL3's static library (and its headers)
# would make it a "Generic Xcode Archive" that can't be validated or uploaded. So
# everything skips install except the app itself (set on it further down).
if(IOS)
    set(CMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL "YES")
endif()

# Steam achievements on desktop: -DINCRA_STEAM=ON -DINCRA_STEAMWORKS_SDK=/path/to/steamworks_sdk
option(INCRA_STEAM "Desktop: Steam achievements (needs the Steamworks SDK)" OFF)
set(INCRA_STEAMWORKS_SDK "" CACHE PATH "The unzipped Steamworks SDK (the folder with public/ and redistributable_bin/)")
set(INCRA_STEAM_APPID "480" CACHE STRING "Your Steam app id, for steam_appid.txt in development builds (480 = Valve's test app)")

# ---------------------------------------------------------------------------
# SDL3: use an installed copy if there is one, otherwise download and build it.
# iOS always builds it from source as a static library; Android builds it from
# source as libSDL3.so, which goes into the app next to the game.
# ---------------------------------------------------------------------------
if(NOT IOS AND NOT ANDROID)
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
    if(IOS)
        foreach(t SDL3-static SDL_uclibc)
            if(TARGET ${t})
                set_target_properties(${t} PROPERTIES XCODE_ATTRIBUTE_SKIP_INSTALL "YES")
            endif()
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# Sources. Every .cpp in src/ is part of the game (the same list the Makefile
# uses), so new files never need adding here.
# ---------------------------------------------------------------------------
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB GAME_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set(ART_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(GLOB_RECURSE ART_FILES CONFIGURE_DEPENDS "${ART_DIR}/*")

# Pack the assets into the game on phones and in release builds.
if(IOS OR ANDROID OR INCRA_RELEASE)
    set(INCRA_PACK_ASSETS ON)
else()
    set(INCRA_PACK_ASSETS OFF)
endif()

if(INCRA_PACK_ASSETS)
    # AssetPacker runs on the computer doing the build, not on the device.
    set(PACKER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/tools/AssetPacker/main.cpp)
    if(NOT CMAKE_CROSSCOMPILING)
        # Building for this computer: the packer is just another program in the build.
        add_executable(AssetPacker ${PACKER_SRC})
        target_include_directories(AssetPacker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set(PACKER AssetPacker) # a target name: CMake runs the built program
    else()
        # Building for a phone: CMake's compiler makes phone programs, which can't run
        # here, so the packer is compiled now with the computer's own C++ compiler.
        # (Pick one with -DINCRA_HOST_CXX=path/to/clang++ if the wrong one is found.)
        set(INCRA_HOST_CXX "" CACHE STRING "C++ compiler for programs that run during the build (AssetPacker)")
        if(CMAKE_HOST_WIN32)
            set(_exe ".exe")
        else()
            set(_exe "")
        endif()
        set(PACKER ${CMAKE_CURRENT_BINARY_DIR}/AssetPacker${_exe})
        set(_static "")
        if(INCRA_HOST_CXX)
            set(HOST_CXX ${INCRA_HOST_CXX})
        elseif(CMAKE_HOST_APPLE)
            set(HOST_CXX xcrun --sdk macosx clang++)
        else()
            find_program(HOST_CXX_PROGRAM NAMES c++ g++ clang++)
            if(NOT HOST_CXX_PROGRAM)
                message(FATAL_ERROR "The build needs a C++ compiler for this computer, to pack assets/ into the game.\n"
                    "Windows: install MSYS2's g++ (the one the Makefile uses) and add its bin folder (e.g. "
                    "C:/msys64/ucrt64/bin) to PATH, then restart Android Studio. Or pass -DINCRA_HOST_CXX=path/to/g++.exe")
            endif()
            set(HOST_CXX ${HOST_CXX_PROGRAM})
        endif()
        if(CMAKE_HOST_WIN32)
            # Built in, so it runs without MinGW's DLLs on PATH (Gradle's build often doesn't have them).
            set(_static -static)
        endif()
        if(NOT EXISTS ${PACKER} OR ${PACKER_SRC} IS_NEWER_THAN ${PACKER}
           OR ${CMAKE_CURRENT_SOURCE_DIR}/include/AssetPackFormat.h IS_NEWER_THAN ${PACKER})
            message(STATUS "Building AssetPacker with ${HOST_CXX}")
            execute_process(
                COMMAND ${HOST_CXX} -std=c++17 -O2 ${_static} -I${CMAKE_CURRENT_SOURCE_DIR}/include ${PACKER_SRC} -o ${PACKER}
                RESULT_VARIABLE packer_result
                ERROR_VARIABLE packer_error)
            if(NOT packer_result EQUAL 0 OR NOT EXISTS ${PACKER})
                file(REMOVE ${PACKER})
                message(FATAL_ERROR "Couldn't build tools/AssetPacker with ${HOST_CXX}:\n${packer_error}")
            endif()
        endif()
        # Check it runs (with no arguments it prints its usage and returns 1).
        execute_process(COMMAND ${PACKER} RESULT_VARIABLE packer_run OUTPUT_QUIET ERROR_QUIET)
        if(NOT packer_run STREQUAL "1")
            file(REMOVE ${PACKER})
            message(FATAL_ERROR "tools/AssetPacker was built with ${HOST_CXX} but won't run (${packer_run}). "
                "Pass a different compiler with -DINCRA_HOST_CXX=path/to/compiler.")
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
    set(IOS_SOURCES "")
    if(INCRA_GAME_CENTER)
        set(IOS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/GameCenter.mm)
        set_source_files_properties(${IOS_SOURCES} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    endif()
    add_executable(IncraVegetable MACOSX_BUNDLE ${GAME_SOURCES} ${GAME_HEADERS} ${IOS_SOURCES} ${IOS_ASSETS})
    if(INCRA_GAME_CENTER)
        target_compile_definitions(IncraVegetable PRIVATE INCRA_GAME_CENTER)
        target_link_libraries(IncraVegetable PRIVATE "-framework GameKit" "-framework UIKit")
        set_target_properties(IncraVegetable PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/IncraVegetable.entitlements")
    endif()
    set_source_files_properties(${IOS_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(IncraVegetable PROPERTIES
        OUTPUT_NAME "IncraVegetable"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "${INCRA_APP_NAME}"
        RESOURCE "${IOS_ASSETS}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${INCRA_BUNDLE_ID}"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${INCRA_BUILD_NUMBER}"
        # The app is the one thing that goes into the archive (see SKIP_INSTALL above).
        XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
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
elseif(ANDROID)
    # On Android the game is a library: SDL's Java activity (android/app/src/main/java/
    # org/libsdl/app) loads libSDL3.so and then libmain.so, and calls its SDL_main.
    add_library(IncraVegetable SHARED ${GAME_SOURCES} ${GAME_HEADERS})
    set_target_properties(IncraVegetable PROPERTIES OUTPUT_NAME "main")
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
# Steam: link the Steamworks library and put it (and, for development builds,
# steam_appid.txt) next to the game.
if(INCRA_STEAM AND NOT IOS AND NOT ANDROID)
    if(NOT EXISTS "${INCRA_STEAMWORKS_SDK}/public/steam/steam_api.h")
        message(FATAL_ERROR "INCRA_STEAM is on but INCRA_STEAMWORKS_SDK (${INCRA_STEAMWORKS_SDK}) isn't the Steamworks SDK folder")
    endif()
    set(_steam_bin "${INCRA_STEAMWORKS_SDK}/redistributable_bin")
    if(WIN32)
        set(_steam_lib "${_steam_bin}/win64/steam_api64.lib")
        set(_steam_dll "${_steam_bin}/win64/steam_api64.dll")
    elseif(APPLE)
        set(_steam_lib "${_steam_bin}/osx/libsteam_api.dylib")
        set(_steam_dll "${_steam_lib}")
        set_target_properties(IncraVegetable PROPERTIES BUILD_RPATH "@executable_path" INSTALL_RPATH "@executable_path")
    else()
        set(_steam_lib "${_steam_bin}/linux64/libsteam_api.so")
        set(_steam_dll "${_steam_lib}")
        set_target_properties(IncraVegetable PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
    endif()
    target_include_directories(IncraVegetable PRIVATE "${INCRA_STEAMWORKS_SDK}/public")
    target_compile_definitions(IncraVegetable PRIVATE INCRA_STEAM)
    target_link_libraries(IncraVegetable PRIVATE "${_steam_lib}")
    add_custom_command(TARGET IncraVegetable POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_steam_dll}" $<TARGET_FILE_DIR:IncraVegetable>)
    if(NOT INCRA_RELEASE)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/steam_appid.txt" "${INCRA_STEAM_APPID}")
        add_custom_command(TARGET IncraVegetable POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/steam_appid.txt"
                    $<TARGET_FILE_DIR:IncraVegetable>)
    endif()
endif()

if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET IncraVegetable POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:IncraVegetable>)
endif()

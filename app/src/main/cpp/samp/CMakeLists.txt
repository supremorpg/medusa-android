cmake_minimum_required(VERSION 3.12)
project(samp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    add_definitions(-DVER_x32=true)
elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
    add_definitions(-DVER_x32=false)
endif()

# Add source files
file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/**/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/**/**/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/**/**/**/**/**/*.c*
        ${CMAKE_CURRENT_LIST_DIR}/**/**/**/**/**/**/**/**/**/*.c*
        )

list(APPEND SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/game/GTASAEngineApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampAppCommandApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampCollisionApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampFileLoaderApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampInputApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampIplApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampMapSourceInventory.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampMapSourceParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game/SampStreamingApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/java/xyron_bridge.cpp
        ${CMAKE_CURRENT_LIST_DIR}/java/sistemas.cpp
        ${CMAKE_CURRENT_LIST_DIR}/net/nettransport.cpp
        ${CMAKE_CURRENT_LIST_DIR}/perf/PerformanceProfiler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ui/Native3DTextLabels.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ui/NativeChatBox.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ui/NativeNameTags.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ui/NativeRadarCrosshair.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ui/NativeScoreboard.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ui/SampUiController.cpp
        )
list(REMOVE_DUPLICATES SOURCES)
list(FILTER SOURCES EXCLUDE REGEX ".*/cef/.*")

# Add include directories
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/util/
        ${CMAKE_CURRENT_LIST_DIR}/vendor/armhook
        ${CMAKE_CURRENT_LIST_DIR}/vendor/imgui
        ${CMAKE_CURRENT_LIST_DIR}/vendor/
        ${CMAKE_CURRENT_LIST_DIR}/game/RW/
        ${CMAKE_CURRENT_LIST_DIR}/game/Core/
)

# Opus
#add_library(libopus STATIC IMPORTED)
#set_target_properties(libopus PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/vendor/opus/libopus.a)

add_library(bass_prebuilt SHARED IMPORTED)
set_target_properties(bass_prebuilt PROPERTIES
        IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/vendor/bass/libs/${ANDROID_ABI}/libbass.so)

add_library(samp SHARED ${SOURCES})
target_link_libraries(samp log dl EGL GLESv3 opus shadowhook::shadowhook bass_prebuilt)

set(SAMP_EXTREME_FLAGS
        -O3
        -flto=thin
        -fno-fast-math
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -fvectorize
        -fslp-vectorize
        -funroll-loops
        -fvisibility=hidden
        -pthread
        -Wall
        -w)

set(SAMP_ARCH_FLAGS)
if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
        list(APPEND SAMP_ARCH_FLAGS -mfpu=neon -mfloat-abi=softfp)
endif()

target_compile_options(samp PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${SAMP_EXTREME_FLAGS} ${SAMP_ARCH_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${SAMP_EXTREME_FLAGS} ${SAMP_ARCH_FLAGS} -fexceptions>)

target_link_options(samp PRIVATE
        -flto=thin
        -Wl,--gc-sections
        -Wl,--icf=safe)

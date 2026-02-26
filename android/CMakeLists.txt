project(Webrtc)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME Webrtc)
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 20)

# Enable Raw Props parsing in react-native (for Nitro Views)
add_compile_options(-DRN_SERIALIZABLE_STATE=1)
set(CMAKE_CXX_CLANG_TIDY "${ANDROID_TOOLCHAIN_ROOT}/bin/clang-tidy")

get_filename_component(TOP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
get_filename_component(LIBDATACHANNEL_PATH ${TOP_PATH}/3rdparty/output/android/libdatachannel/${ANDROID_ABI} ABSOLUTE)
get_filename_component(FFMPEG_PATH ${TOP_PATH}/3rdparty/output/android/ffmpeg/${ANDROID_ABI} ABSOLUTE)

file(GLOB CPP_SOURCES
        ${TOP_PATH}/cpp/*.cpp
        ${TOP_PATH}/cpp/FFmpeg/*.cpp
        ${TOP_PATH}/cpp/Hybrid/*.cpp
)

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED
        src/main/cpp/cpp-adapter.cpp
        src/main/cpp/NativeMicrophone.cpp
        ${CPP_SOURCES}
)

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/Webrtc+autolinking.cmake)

# Set up local includes
include_directories(
        "src/main/cpp"
        "${TOP_PATH}/cpp"
        "${TOP_PATH}/cpp/FFmpeg"
        "${TOP_PATH}/cpp/Hybrid"
        ${LIBDATACHANNEL_PATH}/include
        ${FFMPEG_PATH}/include
)

find_library(LOG_LIB log)
find_package(oboe REQUIRED CONFIG)

# Link all libraries together
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android                                   # <-- Android core
        oboe::oboe
        ${LIBDATACHANNEL_PATH}/lib/libdatachannel.so
        ${FFMPEG_PATH}/lib/libavformat.so
        ${FFMPEG_PATH}/lib/libavcodec.so
        ${FFMPEG_PATH}/lib/libswscale.so
        ${FFMPEG_PATH}/lib/libswresample.so
        ${FFMPEG_PATH}/lib/libavutil.so
)

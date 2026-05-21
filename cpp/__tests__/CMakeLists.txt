cmake_minimum_required(VERSION 3.14)
project(testcpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_VERBOSE_MAKEFILE ON)
get_filename_component(TOP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(LIBDATACHANNEL_PATH ${TOP_PATH}/3rdparty/output/unittest/libdatachannel)
set(FFMPEG_PATH ${TOP_PATH}/3rdparty/output/unittest/ffmpeg)
set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)
find_package(ZLIB REQUIRED)
find_library(OPUS_LIBRARY NAMES opus)
find_library(X264_LIBRARY NAMES x264)
find_library(X265_LIBRARY NAMES x265)

foreach(REQUIRED_LIBRARY OPUS_LIBRARY X264_LIBRARY X265_LIBRARY)
    if(NOT ${REQUIRED_LIBRARY})
        message(FATAL_ERROR "Could not find ${REQUIRED_LIBRARY}")
    endif()
endforeach()

file(GLOB CPP_SOURCES
    ${TOP_PATH}/cpp/*.cpp
    ${TOP_PATH}/cpp/FFmpeg/*.cpp
    ${TOP_PATH}/cpp/__tests__/*.cpp
    ${TOP_PATH}/cpp/__tests__/FFmpeg/*.cpp
)

add_executable(
    testcpp
    ${CPP_SOURCES}
)

target_include_directories(
    testcpp
    PUBLIC
    ${TOP_PATH}/cpp/
    ${TOP_PATH}/cpp/FFmpeg/
)

target_include_directories(
    testcpp
    SYSTEM
    PUBLIC
    ${LIBDATACHANNEL_PATH}/include
    ${FFMPEG_PATH}/include
)

target_link_libraries(
    testcpp
    PRIVATE
    ${LIBDATACHANNEL_PATH}/lib/libdatachannel.a
    ${LIBDATACHANNEL_PATH}/lib/libjuice.a
    ${LIBDATACHANNEL_PATH}/lib/libsrtp2.a
    ${LIBDATACHANNEL_PATH}/lib/libusrsctp.a
    ${LIBDATACHANNEL_PATH}/lib/libmbedtls.a
    ${LIBDATACHANNEL_PATH}/lib/libmbedx509.a
    ${LIBDATACHANNEL_PATH}/lib/libmbedcrypto.a
    ${FFMPEG_PATH}/lib/libavformat.a
    ${FFMPEG_PATH}/lib/libavcodec.a
    ${FFMPEG_PATH}/lib/libswscale.a
    ${FFMPEG_PATH}/lib/libswresample.a
    ${FFMPEG_PATH}/lib/libavutil.a
    ${OPUS_LIBRARY}
    ${X264_LIBRARY}
    ${X265_LIBRARY}
    ZLIB::ZLIB
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(testcpp)

#!/bin/bash
set -euo pipefail
TOP_DIR="$(cd "$(dirname "$0")" && pwd)"
FFMPEG_DIR="${TOP_DIR}/build/ffmpeg/unittest"
OUTPUT_DIR="${TOP_DIR}/output/unittest/ffmpeg"

(
    mkdir -p $FFMPEG_DIR
    cd $FFMPEG_DIR
    ../../../repo/ffmpeg/configure \
        --prefix=install \
        --pkg-config-flags="--static" \
        --disable-everything \
        --disable-shared --enable-static \
        --enable-gpl \
        --disable-asm \
        --disable-iconv \
        --disable-avdevice \
        --enable-avfilter \
        --enable-filter=afftdn \
        --enable-filter=agate \
        --enable-filter=dynaudnorm \
        --enable-filter=ebur128 \
        --enable-filter=highpass \
        --enable-filter=lowpass \
        --enable-filter=aresample \
        --enable-swresample \
        --enable-avcodec \
        --enable-avformat \
        --enable-swscale \
        --enable-avutil \
        --disable-audiotoolbox \
        --disable-videotoolbox \
        --disable-mediacodec \
        --enable-libopus \
        --enable-libx264 \
        --enable-libx265 \
        --disable-jni \
        --enable-protocol=file \
        --enable-decoder=h264 \
        --enable-decoder=hevc \
        --enable-parser=h264 \
        --enable-parser=hevc \
        --enable-encoder=libopus \
        --enable-decoder=libopus \
        --enable-encoder=libx264 \
        --enable-encoder=libx265 \
        --enable-encoder=png \
        --enable-encoder=aac \
        --enable-decoder=aac \
        --enable-parser=opus \
        --enable-muxer=mp4

    make -j install
)

(
    rm -rf $OUTPUT_DIR
    mkdir -p $OUTPUT_DIR/lib
    cp -r $FFMPEG_DIR/install/lib/*.a $OUTPUT_DIR/lib
    cp -r $FFMPEG_DIR/install/include $OUTPUT_DIR/include
)

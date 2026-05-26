#!/bin/bash
set -euo pipefail
TOP_DIR="$(cd "$(dirname "$0")" && pwd)"
ARCHS=("armv7" "aarch64" "x86" "x86_64")
CPUS=("armv7-a" "armv8-a" "i686" "x86-64")
ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")
TOOLCHAIN_ARCHS=("armv7a-linux-androideabi" "aarch64-linux-android" "i686-linux-android" "x86_64-linux-android")

NDK_VERSION="27.1.12297006"

for i in "${!ARCHS[@]}"; do
    ABI="${ABIS[$i]}"
    ARCH="${ARCHS[$i]}"
    CPU="${CPUS[$i]}"
    TOOLCHAIN_ARCH="${TOOLCHAIN_ARCHS[$i]}"
    OPUS_DIR="$TOP_DIR/build/opus/android/$ABI"
    FFMPEG_DIR="$TOP_DIR/build/ffmpeg/android/$ABI"
    OUTPUT_DIR="$TOP_DIR/output/android/ffmpeg/$ABI"

    cmake -B $OPUS_DIR -DCMAKE_TOOLCHAIN_FILE=$ANDROID_HOME/ndk/${NDK_VERSION}/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=$ABI \
        -DANDROID_PLATFORM=android-24 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$OPUS_DIR/install \
        -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        repo/opus
    cmake --build $OPUS_DIR --config Release --target install

    (
        mkdir -p $FFMPEG_DIR
        cd $FFMPEG_DIR
        PKG_CONFIG_PATH="$OPUS_DIR/install/lib/pkgconfig:${PKG_CONFIG_PATH:-}" \
        ../../../../repo/ffmpeg/configure \
            --host-os=darwin-x86_64 --target-os=android \
            --enable-cross-compile --arch=$ARCH --cpu=$CPU \
            --sysroot=$ANDROID_HOME/ndk/${NDK_VERSION}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot \
            --sysinclude=$ANDROID_HOME/ndk/${NDK_VERSION}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include/ \
            --cc=$ANDROID_HOME/ndk/${NDK_VERSION}/toolchains/llvm/prebuilt/darwin-x86_64/bin/${TOOLCHAIN_ARCH}24-clang \
            --cxx=$ANDROID_HOME/ndk/${NDK_VERSION}/toolchains/llvm/prebuilt/darwin-x86_64/bin/${TOOLCHAIN_ARCH}24-clang++ \
            --strip=$ANDROID_HOME/ndk/${NDK_VERSION}/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip \
            --pkg-config-flags="--static" \
            --extra-cflags="-I$OPUS_DIR/install/include" \
            --extra-ldflags="-L$OPUS_DIR/install/lib" \
            --prefix=$FFMPEG_DIR/install \
            --disable-everything \
            --enable-shared --disable-static \
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
            --enable-libopus \
            --enable-mediacodec \
            --enable-protocol=file \
            --enable-jni \
            --enable-decoder=h264 \
            --enable-decoder=hevc \
            --enable-parser=h264 \
            --enable-parser=hevc \
            --enable-encoder=h264_mediacodec \
            --enable-encoder=hevc_mediacodec \
            --enable-encoder=png \
            --enable-decoder=libopus \
            --enable-encoder=libopus \
            --enable-encoder=aac \
            --enable-decoder=aac \
            --enable-parser=opus \
            --enable-muxer=mp4

        make -j install
    )

    (
        rm -rf $OUTPUT_DIR
        mkdir -p $OUTPUT_DIR/lib
        cp -r $FFMPEG_DIR/install/lib/*.so $OUTPUT_DIR/lib
        cp -r $FFMPEG_DIR/install/include $OUTPUT_DIR/include
    )
done

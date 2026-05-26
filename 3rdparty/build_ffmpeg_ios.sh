#!/bin/bash
set -euo pipefail
TOP_DIR="$(cd "$(dirname "$0")" && pwd)"
SDKS=("iphoneos" "iphonesimulator" "iphonesimulator")
ARCHS=("arm64" "arm64" "x86_64")
FLAGS=("-miphoneos-version-min=15.1" "-mios-simulator-version-min=15.1" "-mios-simulator-version-min=15.1")

for i in "${!SDKS[@]}"; do
    SDK="${SDKS[$i]}"
    ARCH="${ARCHS[$i]}"
    FLAG="${FLAGS[$i]}"
    OPUS_DIR="$TOP_DIR/build/opus/$SDK/$ARCH"
    FFMPEG_DIR="$TOP_DIR/build/ffmpeg/$SDK/$ARCH"

    cmake -B $OPUS_DIR -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_OSX_SYSROOT=$SDK \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.1 \
        -DCMAKE_INSTALL_PREFIX=$OPUS_DIR/install \
        -DCMAKE_BUILD_TYPE=Release \
        repo/opus
    cmake --build $OPUS_DIR --config Release --target install

    (
        mkdir -p $FFMPEG_DIR
        cd $FFMPEG_DIR
        export PKG_CONFIG_PATH="$OPUS_DIR/install/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
        ../../../../repo/ffmpeg/configure \
            --sysroot="$(xcrun --sdk $SDK --show-sdk-path)" \
            --enable-cross-compile --arch=$ARCH \
            --prefix=$FFMPEG_DIR/install \
            --cc="clang -arch $ARCH" \
            --cxx="clang++ -arch $ARCH" \
            --pkg-config-flags="--static" \
            --extra-cflags="$FLAG -I$OPUS_DIR/install/include" \
            --extra-ldflags="$FLAG -L$OPUS_DIR/install/lib" \
            --disable-everything \
            --disable-shared --enable-static \
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
            --enable-videotoolbox \
            --enable-libopus \
            --disable-mediacodec \
            --enable-protocol=file \
            --enable-decoder=h264 \
            --enable-decoder=hevc \
            --enable-parser=h264 \
            --enable-parser=hevc \
            --enable-encoder=h264_videotoolbox \
            --enable-encoder=hevc_videotoolbox \
            --enable-encoder=png \
            --enable-decoder=libopus \
            --enable-encoder=libopus \
            --enable-encoder=aac \
            --enable-decoder=aac \
            --enable-parser=opus \
            --enable-muxer=mp4

        make -j install
    )
    libtool -static -o $FFMPEG_DIR/libffmpeg.a \
        $OPUS_DIR/install/lib/libopus.a \
        $FFMPEG_DIR/install/lib/libavcodec.a \
        $FFMPEG_DIR/install/lib/libavformat.a \
        $FFMPEG_DIR/install/lib/libavfilter.a \
        $FFMPEG_DIR/install/lib/libswscale.a \
        $FFMPEG_DIR/install/lib/libswresample.a \
        $FFMPEG_DIR/install/lib/libavutil.a
done

lipo -create \
    build/ffmpeg/iphoneos/arm64/libffmpeg.a \
    -output build/ffmpeg/iphoneos/libffmpeg.a

lipo -create \
    build/ffmpeg/iphonesimulator/arm64/libffmpeg.a \
    build/ffmpeg/iphonesimulator/x86_64/libffmpeg.a \
    -output build/ffmpeg/iphonesimulator/libffmpeg.a

rm -rf output/ios/ffmpeg.xcframework
xcodebuild -create-xcframework \
  -library build/ffmpeg/iphoneos/libffmpeg.a \
  -headers build/ffmpeg/iphoneos/arm64/install/include \
  -library build/ffmpeg/iphonesimulator/libffmpeg.a \
  -headers build/ffmpeg/iphonesimulator/arm64/install/include \
  -output output/ios/ffmpeg.xcframework

#!/bin/bash

BUILD_TYPE="Release"

if [[ "$1" == "--debug" ]]; then
    BUILD_TYPE="Debug"
    echo "Building SDL version with debug symbols..."
else
    echo "Building SDL version (release)..."
fi

BUILD_DIR="build-sdl"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_SDL=ON

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

echo "Compiling..."
cmake --build . --config "$BUILD_TYPE"

if [ $? -eq 0 ]; then
    echo "Build successful!"
    ls -la gameboy_sdl 2>/dev/null || ls -la Release/gameboy_sdl 2>/dev/null || echo "Binary location may vary, check $BUILD_DIR/"
else
    echo "Build failed!"
    exit 1
fi

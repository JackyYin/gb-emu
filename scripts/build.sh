#!/bin/bash

BUILD_TYPE="Release"

if [[ "$1" == "--debug" ]]; then
    BUILD_TYPE="Debug"
    echo "Building with debug symbols..."
else
    echo "Building release..."
fi

if [ ! -d "build" ]; then
    echo "Build directory not found. Running setup..."
    ./scripts/setup.sh
fi

cd build

echo "Compiling..."
emmake cmake --build . --config "$BUILD_TYPE"

if [ $? -eq 0 ]; then
    echo "Build successful! Output in dist/"
    ls -la ../dist/
else
    echo "Build failed!"
    exit 1
fi

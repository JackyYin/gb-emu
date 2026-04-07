#!/bin/bash

echo "Setting up build directory..."

if ! command -v emcmake &> /dev/null; then
    echo "Error: Emscripten not found. Please install and activate the SDK first."
    echo "Run:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

mkdir -p build
cd build

echo "Configuring with CMake..."
emcmake cmake ..

echo "Setup complete. Run ./scripts/build.sh to compile."

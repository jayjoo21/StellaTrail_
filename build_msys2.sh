#!/bin/bash
# Build with MSYS2/MinGW (alternative to Visual Studio)
# Requirements: pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-cmake

set -e
cmake -B build -G "MinGW Makefiles" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
echo "Build complete: build/StellaTrail.exe"

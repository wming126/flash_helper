#!/bin/bash

# Ensure build directory exists
mkdir -p build
cd build

# Check for cmake
if ! command -v cmake &> /dev/null
then
    echo "Error: cmake not found. Please install it with 'sudo apt install cmake'"
    exit 1
fi

# Build and generate packages
echo "Building FlashHelper..."
cmake ..
make -j$(nproc)

echo "Generating .deb and .rpm packages..."
cpack -G DEB
cpack -G RPM

echo "Done! Packages are in the build/ directory."

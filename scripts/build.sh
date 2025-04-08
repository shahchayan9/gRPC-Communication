#!/bin/bash

# Set the build directory
BUILD_DIR="./build"

# Create build directory if it doesn't exist
mkdir -p $BUILD_DIR

# Generate Python proto files
./scripts/generate_python_proto.sh

# Run CMake
cd $BUILD_DIR
cmake ..

# Build the project
make -j$(nproc)

echo "Build completed successfully"

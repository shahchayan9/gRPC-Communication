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
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS doesn't have nproc
    make -j$(sysctl -n hw.ncpu)
else
    make -j$(nproc)
fi

echo "Build completed successfully"

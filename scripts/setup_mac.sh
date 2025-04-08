#!/bin/bash

# Install required packages
brew install cmake
brew install protobuf
brew install grpc

# Install Python dependencies
pip3 install grpcio grpcio-tools protobuf

echo "Setup completed successfully"

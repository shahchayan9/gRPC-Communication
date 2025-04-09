#!/bin/bash

# Create directories
mkdir -p python_client/generated

# Generate Python code from proto file
python3 -m grpc_tools.protoc \
    -I./proto \
    --python_out=./python_client/generated \
    --grpc_python_out=./python_client/generated \
    ./proto/data_service.proto

# Create __init__.py files
touch python_client/__init__.py
touch python_client/generated/__init__.py

echo "Generated Python proto files successfully"

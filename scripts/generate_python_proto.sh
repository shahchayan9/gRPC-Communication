#!/bin/bash

# Create directory for generated Python files
mkdir -p python_client/generated

# Generate Python code from proto file
python3 -m grpc_tools.protoc \
    -I./proto \
    --python_out=./python_client/generated \
    --grpc_python_out=./python_client/generated \
    ./proto/data_service.proto

# Fix imports in generated files
sed -i '' 's/import data_service_pb2/import python_client.generated.data_service_pb2/g' python_client/generated/data_service_pb2_grpc.py

# Create __init__.py files to make imports work
touch python_client/__init__.py
touch python_client/generated/__init__.py

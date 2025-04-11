#!/bin/bash

echo "Generating Python protocol buffer files..."

# Create necessary directories
mkdir -p python_client/generated

# Remove any existing generated files
rm -f python_client/generated/*.py

# Generate protobuf files with correct paths
python3 -m grpc_tools.protoc \
    -I./proto \
    --python_out=./python_client/generated \
    --grpc_python_out=./python_client/generated \
    ./proto/data_service.proto

# Fix import paths in generated files
if [ -f python_client/generated/data_service_pb2_grpc.py ]; then
    sed -i '' 's/import data_service_pb2 as data__service__pb2/from . import data_service_pb2 as data__service__pb2/g' python_client/generated/data_service_pb2_grpc.py
    echo "Fixed import in data_service_pb2_grpc.py"
else
    echo "Warning: data_service_pb2_grpc.py not found"
fi

# Create __init__.py files
echo "# Python package" > python_client/__init__.py
echo "# Generated package" > python_client/generated/__init__.py

echo "Python proto files generated successfully!"

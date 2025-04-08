#!/bin/bash

# Set the build directory
BUILD_DIR="./build"

# Create build directory if it doesn't exist
mkdir -p $BUILD_DIR

# Create and use a virtual environment for Python
if [ ! -d "venv" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv venv
fi

# Activate virtual environment
echo "Activating virtual environment..."
source venv/bin/activate

# Install Python dependencies
echo "Installing required Python packages..."
pip install grpcio grpcio-tools protobuf

# Make sure proto directory exists
mkdir -p proto

# Check if the proto file exists, if not create it
if [ ! -f "proto/data_service.proto" ]; then
    echo "Creating proto file..."
    cat > proto/data_service.proto << 'PROTOEOF'
syntax = "proto3";

package dataservice;

// Service definition
service DataService {
  // Request-response method
  rpc QueryData (QueryRequest) returns (QueryResponse) {}
  
  // One-way message
  rpc SendData (DataMessage) returns (Empty) {}
  
  // Streaming response
  rpc StreamData (QueryRequest) returns (stream DataChunk) {}
}

// Message definitions
message QueryRequest {
  string query_id = 1;
  string query_string = 2;
  repeated string parameters = 3;
}

message QueryResponse {
  string query_id = 1;
  bool success = 2;
  string message = 3;
  repeated DataEntry results = 4;
}

message DataMessage {
  string message_id = 1;
  string source = 2;
  string destination = 3;
  bytes data = 4;
}

message DataChunk {
  string chunk_id = 1;
  bytes data = 2;
  bool is_last = 3;
}

message DataEntry {
  string key = 1;
  oneof value {
    string string_value = 2;
    int32 int_value = 3;
    double double_value = 4;
    bool bool_value = 5;
  }
}

message Empty {}
PROTOEOF
fi

# Make sure json.hpp exists
if [ ! -f "src/common/config/json.hpp" ]; then
    echo "Downloading json.hpp..."
    mkdir -p src/common/config
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -o src/common/config/json.hpp
fi

# Generate Python proto files
echo "Generating Python proto files..."
mkdir -p python_client/generated
python -m grpc_tools.protoc -I./proto \
    --python_out=./python_client/generated \
    --grpc_python_out=./python_client/generated \
    ./proto/data_service.proto || echo "Warning: Proto generation failed, but continuing build..."

# Fix imports in generated files - use different sed syntax for macOS
if [ -f "python_client/generated/data_service_pb2_grpc.py" ]; then
    if [[ "$(uname)" == "Darwin" ]]; then
        sed -i '' 's/import data_service_pb2/import python_client.generated.data_service_pb2/g' python_client/generated/data_service_pb2_grpc.py
    else
        sed -i 's/import data_service_pb2/import python_client.generated.data_service_pb2/g' python_client/generated/data_service_pb2_grpc.py
    fi
fi

# Create __init__.py files to make imports work
touch python_client/__init__.py
touch python_client/generated/__init__.py

# Run CMake
cd $BUILD_DIR
cmake ..

# Determine number of cores for parallel build
if [[ "$(uname)" == "Darwin" ]]; then
    cores=$(sysctl -n hw.ncpu)
else
    cores=$(nproc)
fi

# Build the project
make -j$cores

echo "Build completed"
# Deactivate virtual environment
deactivate

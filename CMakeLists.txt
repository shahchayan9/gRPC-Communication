cmake_minimum_required(VERSION 3.10)
project(Mini2Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Include protobuf directories
include_directories(${Protobuf_INCLUDE_DIRS})

# Generate protobuf code
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# Compile proto files
set(PROTO_FILES ${PROTO_SRC_DIR}/data_service.proto)

# Generate C++ code from proto
add_custom_command(
    OUTPUT 
        ${PROTO_GENERATED_DIR}/data_service.pb.cc
        ${PROTO_GENERATED_DIR}/data_service.pb.h
        ${PROTO_GENERATED_DIR}/data_service.grpc.pb.cc
        ${PROTO_GENERATED_DIR}/data_service.grpc.pb.h
    COMMAND protoc
    ARGS --cpp_out=${PROTO_GENERATED_DIR}
         --grpc_out=${PROTO_GENERATED_DIR}
         --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`
         -I${PROTO_SRC_DIR}
         ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
)

# Include generated directories
include_directories(${PROTO_GENERATED_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/common)

# Add subdirectories for each process
add_subdirectory(src/process_a)
add_subdirectory(src/process_b)
add_subdirectory(src/process_c)
add_subdirectory(src/process_d)
add_subdirectory(src/process_e)
add_subdirectory(src/common)
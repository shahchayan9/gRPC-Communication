cmake_minimum_required(VERSION 3.10)
project(Mini2Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For macOS, explicitly set library paths
if(APPLE)
    # Set path to Homebrew libraries
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
    
    # Define libraries directly
    set(PROTOBUF_LIBRARIES protobuf)
    set(GRPC_LIBRARIES grpc++ grpc)
    
    # Create imported targets to mimic find_package behavior
    add_library(grpc++ SHARED IMPORTED)
    set_target_properties(grpc++ PROPERTIES IMPORTED_LOCATION "/opt/homebrew/lib/libgrpc++.dylib")
    
    add_library(grpc SHARED IMPORTED)
    set_target_properties(grpc PROPERTIES IMPORTED_LOCATION "/opt/homebrew/lib/libgrpc.dylib")
    
    # Create namespace alias for gRPC::grpc++
    add_library(gRPC::grpc++ ALIAS grpc++)
else()
    # For non-macOS systems, use standard find_package
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG REQUIRED)
endif()

# Generate protobuf code
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# Compile proto files
set(PROTO_FILES ${PROTO_SRC_DIR}/data_service.proto)

# Add custom command to generate C++ code from proto
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

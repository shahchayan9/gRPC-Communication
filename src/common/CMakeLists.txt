# Common library
add_library(mini2_common
    shared_memory/shared_memory.cpp
    config/config_loader.cpp
    data/data_structures.cpp
    grpc/data_service.cpp
)

target_include_directories(mini2_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GENERATED_DIR}
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    # Linux
    set(PLATFORM_LIBS pthread rt)
else()
    # macOS or other platforms
    set(PLATFORM_LIBS pthread)
endif()

# Link with system libraries and protobuf/gRPC
target_link_libraries(mini2_common
    ${PLATFORM_LIBS}
    proto_lib
    ${Protobuf_LIBRARIES}
    /opt/homebrew/lib/libgrpc++.dylib
)
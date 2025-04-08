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

# Link with system libraries
if(APPLE)
    target_link_libraries(mini2_common
        pthread
        ${PROTOBUF_LIBRARIES}
        ${GRPC_LIBRARIES}
    )
else()
    target_link_libraries(mini2_common
        pthread
        rt  # For shared memory
        ${Protobuf_LIBRARIES}
        gRPC::grpc++
    )
endif()

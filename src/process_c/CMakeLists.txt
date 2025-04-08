add_executable(process_c
    process_c.cpp
)

target_link_libraries(process_c
    mini2_common
    ${Protobuf_LIBRARIES}
    gRPC::grpc++
)

add_executable(process_b
    process_b.cpp
)

target_link_libraries(process_b
    mini2_common
    ${Protobuf_LIBRARIES}
    gRPC::grpc++
)

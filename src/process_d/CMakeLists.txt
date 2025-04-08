add_executable(process_d
    process_d.cpp
)

target_link_libraries(process_d
    mini2_common
    ${Protobuf_LIBRARIES}
    gRPC::grpc++
)

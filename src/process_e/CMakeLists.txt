add_executable(process_e
    process_e.cpp
)

target_link_libraries(process_e
    mini2_common
    ${Protobuf_LIBRARIES}
    gRPC::grpc++
)

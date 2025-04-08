add_executable(process_a
    process_a.cpp
)

if(APPLE)
    target_link_libraries(process_a
        mini2_common
        ${PROTOBUF_LIBRARIES}
        ${GRPC_LIBRARIES}
    )
else()
    target_link_libraries(process_a
        mini2_common
        ${Protobuf_LIBRARIES}
        gRPC::grpc++
    )
endif()

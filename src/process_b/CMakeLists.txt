add_executable(process_b
process_b.cpp
)

# Only link 'rt' if not on macOS
if(APPLE)
    target_link_libraries(process_b
        mini2_common
        ${Protobuf_LIBRARIES}
        proto_lib
    )
else()
    target_link_libraries(process_b
        mini2_common
        ${Protobuf_LIBRARIES}
        proto_lib
        rt
    )
endif()

add_executable(process_c
process_c.cpp
)

# Only link 'rt' if not on macOS
if(APPLE)
    target_link_libraries(process_c
        mini2_common
        ${Protobuf_LIBRARIES}
        proto_lib
    )
else()
    target_link_libraries(process_c
        mini2_common
        ${Protobuf_LIBRARIES}
        proto_lib
        rt
    )
endif()

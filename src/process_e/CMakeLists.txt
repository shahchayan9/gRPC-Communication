add_executable(process_e
process_e.cpp
)

# Only link 'rt' if not on macOS
if(APPLE)
    target_link_libraries(process_e
        mini2_common
        ${Protobuf_LIBRARIES}
        proto_lib
    )
else()
    target_link_libraries(process_e
        mini2_common
        ${Protobuf_LIBRARIES}
        proto_lib
        rt
    )
endif()

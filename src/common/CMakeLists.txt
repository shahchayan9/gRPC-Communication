# Common library
add_library(mini2_common
    shared_memory/shared_memory.cpp
    config/config_loader.cpp
    data/data_structures.cpp
)

target_include_directories(mini2_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with system libraries
target_link_libraries(mini2_common
    pthread
    rt  # For shared memory
)
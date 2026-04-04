add_library(run STATIC
    src/cam_hold.cpp
)

target_include_directories(run
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(state STATIC
    src/state.cpp
)

target_include_directories(state
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(dev STATIC
    src/dev.cpp
)

target_include_directories(dev
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

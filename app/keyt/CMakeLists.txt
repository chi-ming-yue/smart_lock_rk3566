add_executable(keyt
    src/main.cpp
)

target_link_libraries(keyt PRIVATE pthread)

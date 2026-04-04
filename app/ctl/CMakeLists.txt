add_executable(ctl
    src/main.cpp
)

target_link_libraries(ctl
    PRIVATE
        state
        run
        dev
        face_pipe
)

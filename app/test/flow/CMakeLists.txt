add_executable(t_flow
    src/main.cpp
)
target_link_libraries(t_flow PRIVATE state run dev face_pipe)

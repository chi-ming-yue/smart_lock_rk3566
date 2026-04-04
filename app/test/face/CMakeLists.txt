add_executable(t_face
    src/main.cpp
)
target_link_libraries(t_face PRIVATE face_pipe)

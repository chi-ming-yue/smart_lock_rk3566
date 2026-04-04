add_library(face_pipe STATIC
    src/face_pipe.cpp
)

target_compile_definitions(face_pipe PUBLIC TOTAL_ENABLE_RKNN_FACE_CORE)
target_include_directories(face_pipe
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(face_pipe
    PUBLIC
        rknn_face
        "${RKNN_LIB}"
        "${SQLITE3_LIB}"
        "${GSTREAMER_LIB}"
        "${GSTREAMER_APP_LIB}"
        "${GSTREAMER_VIDEO_LIB}"
        "${GOBJECT_LIB}"
        "${GLIB_LIB}"
        ${OPENCV_LIBS}
        pthread
        dl
        rt
)

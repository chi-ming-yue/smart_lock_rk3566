add_library(rknn_face STATIC
    src/camera.cpp
    src/face_alignment.cpp
    src/face_database.cpp
    src/face_recognizer.cpp
    src/mobilefacenet.cpp
    src/rknn_common.cpp
    src/visualize.cpp
    src/yolo_face.cpp
)

target_include_directories(rknn_face
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OPENCV_INCLUDE_DIR}
        ${RKNN_INCLUDE_DIR}
        ${GSTREAMER_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIR}
        ${GLIB_CONFIG_INCLUDE_DIR}
        ${SYSROOT}/usr/include
)

cmake_minimum_required(VERSION 3.10)
project(smart_lock_rk3566 CXX)

set(CMAKE_SKIP_RPATH TRUE)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

if(NOT DEFINED TOOLCHAIN_ROOT)
    set(TOOLCHAIN_ROOT "/opt/atk-dlrk356x-toolchain" CACHE PATH "Buildroot host toolchain root")
endif()
if(NOT DEFINED SYSROOT)
    set(SYSROOT "${TOOLCHAIN_ROOT}/aarch64-buildroot-linux-gnu/sysroot" CACHE PATH "Buildroot sysroot")
endif()

set(RKNN_INCLUDE_DIR "${SYSROOT}/usr/include/rknn" CACHE PATH "RKNN include directory")
set(RKNN_LIB "${SYSROOT}/usr/lib/librknnrt.so" CACHE FILEPATH "RKNN runtime library")
set(SQLITE3_LIB "${SYSROOT}/usr/lib/libsqlite3.so" CACHE FILEPATH "SQLite3 library")
set(GSTREAMER_INCLUDE_DIR "${SYSROOT}/usr/include/gstreamer-1.0" CACHE PATH "GStreamer include directory")
set(GLIB_INCLUDE_DIR "${SYSROOT}/usr/include/glib-2.0" CACHE PATH "GLib include directory")
set(GLIB_CONFIG_INCLUDE_DIR "${SYSROOT}/usr/lib/glib-2.0/include" CACHE PATH "GLib config include directory")
set(GSTREAMER_LIB "${SYSROOT}/usr/lib/libgstreamer-1.0.so" CACHE FILEPATH "GStreamer library")
set(GSTREAMER_APP_LIB "${SYSROOT}/usr/lib/libgstapp-1.0.so" CACHE FILEPATH "GStreamer app library")
set(GSTREAMER_VIDEO_LIB "${SYSROOT}/usr/lib/libgstvideo-1.0.so" CACHE FILEPATH "GStreamer video library")
set(GOBJECT_LIB "${SYSROOT}/usr/lib/libgobject-2.0.so" CACHE FILEPATH "GObject library")
set(GLIB_LIB "${SYSROOT}/usr/lib/libglib-2.0.so" CACHE FILEPATH "GLib library")
set(RK_FACE_LIB_DIR "/home/chiming/sd3/rk3566_face_rec" CACHE PATH "RK3566 face-rec OpenCV runtime libs")
set(OPENCV_INCLUDE_DIR "/home/chiming/sd3/lubancat_ai_manual_code/example/3rdparty/opencv/opencv-linux-aarch64/include" CACHE PATH "OpenCV include directory")
set(OPENCV_CORE_LIB "${RK_FACE_LIB_DIR}/libopencv_core.so.3.4" CACHE FILEPATH "OpenCV core runtime library")
set(OPENCV_IMGPROC_LIB "${RK_FACE_LIB_DIR}/libopencv_imgproc.so.3.4" CACHE FILEPATH "OpenCV imgproc runtime library")
set(OPENCV_IMGCODECS_LIB "${RK_FACE_LIB_DIR}/libopencv_imgcodecs.so.3.4" CACHE FILEPATH "OpenCV imgcodecs runtime library")
set(OPENCV_VIDEOIO_LIB "${RK_FACE_LIB_DIR}/libopencv_videoio.so.3.4" CACHE FILEPATH "OpenCV videoio runtime library")
set(OPENCV_HIGHGUI_LIB "${RK_FACE_LIB_DIR}/libopencv_highgui.so.3.4" CACHE FILEPATH "OpenCV highgui runtime library")
set(OPENCV_LIBS
    "${OPENCV_CORE_LIB}"
    "${OPENCV_IMGPROC_LIB}"
    "${OPENCV_IMGCODECS_LIB}"
    "${OPENCV_HIGHGUI_LIB}"
    "${OPENCV_VIDEOIO_LIB}"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(KMOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/kmod")

add_subdirectory(third/rknn_face)
add_subdirectory(core/state)
add_subdirectory(core/run)
add_subdirectory(hal/dev)
add_subdirectory(face/pipe)
add_subdirectory(app/test)
add_subdirectory(app/ctl)
add_subdirectory(drv/mydev)

#ifndef RK3566_FACE_REC_CAMERA_H_
#define RK3566_FACE_REC_CAMERA_H_

#include <string>

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

struct CameraOptions {
    int width;
    int height;
    int rotate;
    bool mirror;

    CameraOptions() : width(1280), height(720), rotate(0), mirror(false) {
    }
};

class FrameSource {
public:
    FrameSource();
    ~FrameSource();

    bool open_camera(int camera_index, const CameraOptions &options, std::string *error_message);
    bool open_image(const std::string &path, std::string *error_message);
    bool open_video(const std::string &path, std::string *error_message);
    bool read(cv::Mat *frame, std::string *error_message);
    bool wake(std::string *error_message);
    bool sleep(std::string *error_message);
    void close();
    bool active() const;

private:
    enum Mode {
        MODE_NONE,
        MODE_CAMERA,
        MODE_IMAGE,
        MODE_VIDEO
    };

    Mode mode_;
    bool image_consumed_;
    int camera_rotate_;
    bool camera_mirror_;
    int camera_width_;
    int camera_height_;
    GstElement *camera_pipeline_;
    GstAppSink *camera_sink_;
    bool camera_sleeping_;
    cv::Mat image_frame_;
    cv::VideoCapture capture_;
};

#endif  // RK3566_FACE_REC_CAMERA_H_

#include "camera.h"

#include <iostream>
#include <sstream>

#include <opencv2/imgproc.hpp>

namespace {

bool EnsureGStreamerInitialized(std::string *error_message) {
    static bool initialized = false;
    if (initialized) {
        return true;
    }
    GError *error = NULL;
    if (!gst_init_check(NULL, NULL, &error)) {
        if (error_message != NULL) {
            *error_message = error != NULL ? error->message : "gst_init_check failed";
        }
        if (error != NULL) {
            g_error_free(error);
        }
        return false;
    }
    initialized = true;
    return true;
}

std::string PopGStreamerBusMessage(GstElement *pipeline) {
    if (pipeline == NULL) {
        return "";
    }
    GstBus *bus = gst_element_get_bus(pipeline);
    if (bus == NULL) {
        return "";
    }
    GstMessage *message = gst_bus_pop_filtered(bus,
                                               static_cast<GstMessageType>(GST_MESSAGE_ERROR |
                                                                           GST_MESSAGE_WARNING |
                                                                           GST_MESSAGE_EOS));
    std::string text;
    if (message != NULL) {
        GError *error = NULL;
        gchar *debug = NULL;
        switch (GST_MESSAGE_TYPE(message)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(message, &error, &debug);
                text = error != NULL ? error->message : "gstreamer error";
                break;
            case GST_MESSAGE_WARNING:
                gst_message_parse_warning(message, &error, &debug);
                text = error != NULL ? error->message : "gstreamer warning";
                break;
            case GST_MESSAGE_EOS:
                text = "gstreamer EOS";
                break;
            default:
                break;
        }
        if (debug != NULL && *debug != '\0') {
            text += " (";
            text += debug;
            text += ")";
        }
        if (error != NULL) {
            g_error_free(error);
        }
        if (debug != NULL) {
            g_free(debug);
        }
        gst_message_unref(message);
    }
    gst_object_unref(bus);
    return text;
}

bool StartCameraPipeline(const std::string &pipeline_text,
                         const CameraOptions &options,
                         GstElement **pipeline_out,
                         GstAppSink **sink_out,
                         std::string *error_message) {
    if (pipeline_out == NULL || sink_out == NULL) {
        if (error_message != NULL) {
            *error_message = "invalid camera pipeline output";
        }
        return false;
    }

    *pipeline_out = NULL;
    *sink_out = NULL;

    GError *gst_error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_text.c_str(), &gst_error);
    if (pipeline == NULL) {
        if (error_message != NULL) {
            *error_message = gst_error != NULL ? gst_error->message : "gst_parse_launch failed";
        }
        if (gst_error != NULL) {
            g_error_free(gst_error);
        }
        return false;
    }

    GstElement *sink_element = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (sink_element == NULL) {
        if (error_message != NULL) {
            *error_message = "failed to get appsink from gstreamer pipeline";
        }
        gst_object_unref(pipeline);
        return false;
    }

    GstAppSink *sink = GST_APP_SINK(sink_element);
    gst_app_sink_set_drop(sink, TRUE);
    gst_app_sink_set_max_buffers(sink, 1);
    gst_app_sink_set_emit_signals(sink, FALSE);
    gst_app_sink_set_wait_on_eos(sink, FALSE);

    const GstStateChangeReturn state_ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        if (error_message != NULL) {
            *error_message = "failed to set gstreamer pipeline to PLAYING";
        }
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }

    GstState current_state = GST_STATE_NULL;
    GstState pending_state = GST_STATE_NULL;
    const GstStateChangeReturn wait_ret =
        gst_element_get_state(pipeline, &current_state, &pending_state, 3 * GST_SECOND);
    if (wait_ret == GST_STATE_CHANGE_FAILURE) {
        if (error_message != NULL) {
            *error_message = "failed to start gstreamer pipeline";
            const std::string bus_message = PopGStreamerBusMessage(pipeline);
            if (!bus_message.empty()) {
                *error_message += ": " + bus_message;
            }
        }
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }

    *pipeline_out = pipeline;
    *sink_out = sink;
    std::cout << "[info] camera open mode=gstreamer appsink" << std::endl;
    std::cout << "[info] camera requested width=" << options.width
              << " height=" << options.height
              << " rotate=" << options.rotate
              << " mirror=" << (options.mirror ? 1 : 0) << std::endl;
    return true;
}

void ApplyCameraTransform(const CameraOptions &options, cv::Mat *frame) {
    if (frame == NULL || frame->empty()) {
        return;
    }
    if (options.mirror) {
        cv::flip(*frame, *frame, 1);
    }
    switch (options.rotate) {
        case 90:
            cv::rotate(*frame, *frame, cv::ROTATE_90_CLOCKWISE);
            break;
        case 180:
            cv::rotate(*frame, *frame, cv::ROTATE_180);
            break;
        case 270:
            cv::rotate(*frame, *frame, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        default:
            break;
    }
}

bool ConvertSampleToBgr(GstSample *sample, cv::Mat *frame, std::string *error_message) {
    if (sample == NULL || frame == NULL) {
        if (error_message != NULL) {
            *error_message = "invalid camera sample";
        }
        return false;
    }

    GstCaps *caps = gst_sample_get_caps(sample);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstVideoInfo video_info;
    if (caps == NULL || buffer == NULL || !gst_video_info_from_caps(&video_info, caps)) {
        if (error_message != NULL) {
            *error_message = "failed to parse camera sample";
        }
        return false;
    }

    GstMapInfo map_info;
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
        if (error_message != NULL) {
            *error_message = "failed to map camera buffer";
        }
        return false;
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *format_name = structure != NULL ? gst_structure_get_string(structure, "format") : NULL;
    const std::string format = format_name != NULL ? format_name : "";
    const int width = GST_VIDEO_INFO_WIDTH(&video_info);
    const int height = GST_VIDEO_INFO_HEIGHT(&video_info);
    const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&video_info, 0);

    bool ok = true;
    if (format == "BGR") {
        cv::Mat wrapped(height, width, CV_8UC3, map_info.data, stride);
        *frame = wrapped.clone();
    } else if (format == "BGRx" || format == "BGRA") {
        cv::Mat wrapped(height, width, CV_8UC4, map_info.data, stride);
        cv::cvtColor(wrapped, *frame, cv::COLOR_BGRA2BGR);
    } else if (format == "RGB") {
        cv::Mat wrapped(height, width, CV_8UC3, map_info.data, stride);
        cv::cvtColor(wrapped, *frame, cv::COLOR_RGB2BGR);
    } else if (format == "RGBx" || format == "RGBA") {
        cv::Mat wrapped(height, width, CV_8UC4, map_info.data, stride);
        cv::cvtColor(wrapped, *frame, cv::COLOR_RGBA2BGR);
    } else if (format == "NV12" || format == "NV21") {
        const size_t uv_offset = GST_VIDEO_INFO_PLANE_OFFSET(&video_info, 1);
        const int uv_stride = GST_VIDEO_INFO_PLANE_STRIDE(&video_info, 1);
        cv::Mat y_plane(height, width, CV_8UC1, map_info.data, stride);
        cv::Mat uv_plane(height / 2, width / 2, CV_8UC2, map_info.data + uv_offset, uv_stride);
        cv::cvtColorTwoPlane(y_plane,
                             uv_plane,
                             *frame,
                             format == "NV12" ? cv::COLOR_YUV2BGR_NV12 : cv::COLOR_YUV2BGR_NV21);
    } else {
        ok = false;
        if (error_message != NULL) {
            *error_message = "unsupported camera sample format: " + format;
        }
    }

    gst_buffer_unmap(buffer, &map_info);
    return ok;
}

}  // namespace

FrameSource::FrameSource()
    : mode_(MODE_NONE),
      image_consumed_(false),
      camera_rotate_(0),
      camera_mirror_(false),
      camera_width_(0),
      camera_height_(0),
      camera_pipeline_(NULL),
      camera_sink_(NULL),
      camera_sleeping_(false) {
}

FrameSource::~FrameSource() {
    close();
}

bool FrameSource::open_camera(int camera_index,
                              const CameraOptions &options,
                              std::string *error_message) {
    close();

    if (!EnsureGStreamerInitialized(error_message)) {
        return false;
    }

    const std::string device_path = "/dev/video" + std::to_string(camera_index);
    const int requested_width = options.width > 0 ? options.width : 1280;
    const int requested_height = options.height > 0 ? options.height : 720;
    std::ostringstream constrained_pipeline_text;
    constrained_pipeline_text << "v4l2src device=" << device_path
                              << " ! video/x-raw,width=" << requested_width
                              << ",height=" << requested_height
                              << " ! videoconvert ! "
                              << "appsink name=sink "
                              << "drop=true sync=false max-buffers=1";
    std::ostringstream fallback_pipeline_text;
    fallback_pipeline_text << "v4l2src device=" << device_path
                           << " ! videoconvert ! "
                           << "appsink name=sink "
                           << "drop=true sync=false max-buffers=1";

    std::cout << "[info] trying camera mode=gstreamer appsink" << std::endl;
    if (!StartCameraPipeline(constrained_pipeline_text.str(),
                             options,
                             &camera_pipeline_,
                             &camera_sink_,
                             error_message)) {
        const std::string first_error = (error_message != NULL) ? *error_message : "";
        std::cout << "[warn] constrained camera pipeline failed";
        if (!first_error.empty()) {
            std::cout << ": " << first_error;
        }
        std::cout << std::endl;
        if (!StartCameraPipeline(fallback_pipeline_text.str(),
                                 options,
                                 &camera_pipeline_,
                                 &camera_sink_,
                                 error_message)) {
            if (error_message != NULL && !first_error.empty()) {
                *error_message = first_error + "; fallback failed: " + *error_message;
            }
            return false;
        }
    }

    camera_rotate_ = options.rotate;
    camera_mirror_ = options.mirror;
    camera_width_ = requested_width;
    camera_height_ = requested_height;
    camera_sleeping_ = false;
    mode_ = MODE_CAMERA;
    return true;
}

bool FrameSource::open_image(const std::string &path, std::string *error_message) {
    close();
    image_frame_ = cv::imread(path);
    if (image_frame_.empty()) {
        if (error_message != NULL) {
            *error_message = "failed to read image: " + path;
        }
        return false;
    }
    mode_ = MODE_IMAGE;
    image_consumed_ = false;
    return true;
}

bool FrameSource::open_video(const std::string &path, std::string *error_message) {
    close();
    if (!capture_.open(path)) {
        if (error_message != NULL) {
            *error_message = "failed to open video: " + path;
        }
        return false;
    }
    mode_ = MODE_VIDEO;
    return true;
}

bool FrameSource::read(cv::Mat *frame, std::string *error_message) {
    if (frame == NULL) {
        if (error_message != NULL) {
            *error_message = "frame output pointer is null";
        }
        return false;
    }

    if (mode_ == MODE_IMAGE) {
        if (image_consumed_) {
            frame->release();
            return false;
        }
        *frame = image_frame_.clone();
        image_consumed_ = true;
        return !frame->empty();
    }

    if (mode_ == MODE_CAMERA) {
        if (camera_sleeping_) {
            if (error_message != NULL) {
                *error_message = "camera source is sleeping";
            }
            return false;
        }
        if (camera_sink_ == NULL) {
            if (error_message != NULL) {
                *error_message = "camera sink is not initialized";
            }
            return false;
        }

        GstSample *sample = gst_app_sink_try_pull_sample(camera_sink_, 5 * GST_SECOND);
        if (sample == NULL) {
            if (error_message != NULL) {
                *error_message = "failed to pull camera sample";
                const std::string bus_message = PopGStreamerBusMessage(camera_pipeline_);
                if (!bus_message.empty()) {
                    *error_message += ": " + bus_message;
                } else {
                    *error_message += " at " + std::to_string(camera_width_) + "x" +
                                      std::to_string(camera_height_);
                }
            }
            return false;
        }

        if (!ConvertSampleToBgr(sample, frame, error_message)) {
            gst_sample_unref(sample);
            return false;
        }
        gst_sample_unref(sample);

        CameraOptions options;
        options.rotate = camera_rotate_;
        options.mirror = camera_mirror_;
        ApplyCameraTransform(options, frame);
        return !frame->empty();
    }

    if (mode_ == MODE_VIDEO) {
        if (!capture_.read(*frame)) {
            if (error_message != NULL) {
                *error_message = "failed to read frame";
            }
            return false;
        }
        return !frame->empty();
    }

    if (error_message != NULL) {
        *error_message = "frame source is not open";
    }
    return false;
}

bool FrameSource::wake(std::string *error_message)
{
    if (mode_ != MODE_CAMERA) {
        if (error_message != NULL) {
            error_message->clear();
        }
        return true;
    }
    if (camera_pipeline_ == NULL) {
        if (error_message != NULL) {
            *error_message = "camera pipeline is not initialized";
        }
        return false;
    }
    if (!camera_sleeping_) {
        if (error_message != NULL) {
            error_message->clear();
        }
        return true;
    }

    const GstStateChangeReturn state_ret = gst_element_set_state(camera_pipeline_, GST_STATE_PLAYING);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        if (error_message != NULL) {
            *error_message = "failed to resume gstreamer pipeline";
        }
        return false;
    }

    GstState current_state = GST_STATE_NULL;
    GstState pending_state = GST_STATE_NULL;
    const GstStateChangeReturn wait_ret =
        gst_element_get_state(camera_pipeline_, &current_state, &pending_state, 3 * GST_SECOND);
    if (wait_ret == GST_STATE_CHANGE_FAILURE) {
        if (error_message != NULL) {
            *error_message = "failed to wake camera pipeline";
            const std::string bus_message = PopGStreamerBusMessage(camera_pipeline_);
            if (!bus_message.empty()) {
                *error_message += ": " + bus_message;
            }
        }
        return false;
    }

    camera_sleeping_ = false;
    if (error_message != NULL) {
        error_message->clear();
    }
    return true;
}

bool FrameSource::sleep(std::string *error_message)
{
    if (mode_ != MODE_CAMERA || camera_pipeline_ == NULL) {
        if (error_message != NULL) {
            error_message->clear();
        }
        return true;
    }
    if (camera_sleeping_) {
        if (error_message != NULL) {
            error_message->clear();
        }
        return true;
    }

    const GstStateChangeReturn state_ret = gst_element_set_state(camera_pipeline_, GST_STATE_PAUSED);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        if (error_message != NULL) {
            *error_message = "failed to pause gstreamer pipeline";
        }
        return false;
    }

    GstState current_state = GST_STATE_NULL;
    GstState pending_state = GST_STATE_NULL;
    const GstStateChangeReturn wait_ret =
        gst_element_get_state(camera_pipeline_, &current_state, &pending_state, 3 * GST_SECOND);
    if (wait_ret == GST_STATE_CHANGE_FAILURE) {
        if (error_message != NULL) {
            *error_message = "failed to sleep camera pipeline";
            const std::string bus_message = PopGStreamerBusMessage(camera_pipeline_);
            if (!bus_message.empty()) {
                *error_message += ": " + bus_message;
            }
        }
        return false;
    }

    camera_sleeping_ = true;
    if (error_message != NULL) {
        error_message->clear();
    }
    return true;
}

bool FrameSource::active() const
{
    if (mode_ == MODE_CAMERA) {
        return camera_pipeline_ != NULL && !camera_sleeping_;
    }
    return mode_ != MODE_NONE;
}

void FrameSource::close() {
    capture_.release();
    if (camera_pipeline_ != NULL) {
        gst_element_set_state(camera_pipeline_, GST_STATE_NULL);
    }
    if (camera_sink_ != NULL) {
        gst_object_unref(camera_sink_);
        camera_sink_ = NULL;
    }
    if (camera_pipeline_ != NULL) {
        gst_object_unref(camera_pipeline_);
        camera_pipeline_ = NULL;
    }
    image_frame_.release();
    image_consumed_ = false;
    camera_rotate_ = 0;
    camera_mirror_ = false;
    camera_width_ = 0;
    camera_height_ = 0;
    camera_sleeping_ = false;
    mode_ = MODE_NONE;
}

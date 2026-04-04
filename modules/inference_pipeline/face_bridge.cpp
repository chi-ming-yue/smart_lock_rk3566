#include "face_bridge.h"

#ifdef TOTAL_ENABLE_RKNN_FACE_CORE
#include <deque>
#include <cmath>
#include <map>
#include <vector>
#include <iostream>
#include <cstring>
#include <opencv2/imgcodecs.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include "visualize.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>

#include "camera.h"
#include "face_alignment.h"
#include "face_database.h"
#include "face_recognizer.h"
#include "mobilefacenet.h"
#include "yolo_face.h"

namespace {

struct TemporalSmoothingConfig {
    size_t window_size;
    size_t minimum_votes;

    TemporalSmoothingConfig() : window_size(5), minimum_votes(3) {
    }
};

std::string SelectCandidateName(const RecognitionResult& result)
{
    if (!result.name.empty() && result.name != "unknown") {
        return result.name;
    }
    if (!result.best_match_name.empty() && result.best_match_name != "unknown") {
        return result.best_match_name;
    }
    return "unknown";
}

RecognitionResult ApplyTemporalSmoothing(const RecognitionResult& raw_result,
                                         std::deque<RecognitionResult>* history,
                                         const TemporalSmoothingConfig& config,
                                         float threshold)
{
    if (history == NULL) {
        return raw_result;
    }

    history->push_back(raw_result);
    while (history->size() > config.window_size) {
        history->pop_front();
    }

    std::map<std::string, int> vote_counts;
    std::map<std::string, float> similarity_sums;
    for (size_t i = 0; i < history->size(); ++i) {
        const std::string candidate = SelectCandidateName((*history)[i]);
        if (candidate == "unknown") {
            continue;
        }
        vote_counts[candidate] += 1;
        similarity_sums[candidate] += (*history)[i].similarity;
    }

    std::string best_name = "unknown";
    int best_votes = 0;
    float best_average_similarity = 0.0f;
    for (std::map<std::string, int>::const_iterator it = vote_counts.begin();
         it != vote_counts.end();
         ++it) {
        const float average_similarity = similarity_sums[it->first] / static_cast<float>(it->second);
        if (it->second > best_votes ||
            (it->second == best_votes && average_similarity > best_average_similarity)) {
            best_name = it->first;
            best_votes = it->second;
            best_average_similarity = average_similarity;
        }
    }

    RecognitionResult smoothed = raw_result;
    smoothed.best_match_name = best_name;
    smoothed.similarity = best_average_similarity;
    smoothed.is_unknown = true;
    smoothed.name = "unknown";
    if (best_name != "unknown" &&
        best_votes >= static_cast<int>(config.minimum_votes) &&
        best_average_similarity >= threshold) {
        smoothed.name = best_name;
        smoothed.is_unknown = false;
    }
    return smoothed;
}


bool IsFaceBoxComplete(const FaceBox& box, const cv::Mat& frame)
{
    if (frame.empty()) {
        return false;
    }

    const int min_width = std::max(32, frame.cols / 16);
    const int min_height = std::max(32, frame.rows / 16);
    const int right = box.x + box.width;
    const int bottom = box.y + box.height;

    if (box.width < min_width || box.height < min_height) {
        return false;
    }

    const float expand_x = static_cast<float>(std::max(4, box.width / 10));
    const float expand_y = static_cast<float>(std::max(4, box.height / 10));
    for (int i = 0; i < 5; ++i) {
        const float px = box.landmarks[i * 2 + 0];
        const float py = box.landmarks[i * 2 + 1];
        if (px < 0.0f || py < 0.0f) {
            return false;
        }
        if (px < static_cast<float>(box.x) - expand_x || px > static_cast<float>(right) + expand_x) {
            return false;
        }
        if (py < static_cast<float>(box.y) - expand_y || py > static_cast<float>(bottom) + expand_y) {
            return false;
        }
    }
    return true;
}

double ScoreFaceCandidate(const FaceBox& box, const cv::Mat& frame)
{
    if (frame.empty() || box.width <= 0 || box.height <= 0) {
        return -1.0;
    }

    const double frame_area = static_cast<double>(frame.cols) * static_cast<double>(frame.rows);
    if (frame_area <= 0.0) {
        return -1.0;
    }

    const double area_ratio = (static_cast<double>(box.width) * static_cast<double>(box.height)) / frame_area;
    const double size_term = std::min(1.0, area_ratio / 0.12);

    const double center_x = static_cast<double>(box.x) + static_cast<double>(box.width) * 0.5;
    const double center_y = static_cast<double>(box.y) + static_cast<double>(box.height) * 0.5;
    const double dx = center_x - static_cast<double>(frame.cols) * 0.5;
    const double dy = center_y - static_cast<double>(frame.rows) * 0.5;
    const double norm_x = dx / std::max(1.0, static_cast<double>(frame.cols) * 0.5);
    const double norm_y = dy / std::max(1.0, static_cast<double>(frame.rows) * 0.5);
    const double center_term = std::max(0.0, 1.0 - (norm_x * norm_x + norm_y * norm_y));

    const double score_term = std::max(0.0, std::min(1.0, static_cast<double>(box.score)));
    return 0.65 * size_term + 0.25 * center_term + 0.10 * score_term;
}

FaceBox SelectBestFace(const std::vector<FaceBox>& faces, const cv::Mat& frame)
{
    std::vector<size_t> complete_faces;
    complete_faces.reserve(faces.size());
    for (size_t i = 0; i < faces.size(); ++i) {
        if (IsFaceBoxComplete(faces[i], frame)) {
            complete_faces.push_back(i);
        }
    }

    const bool use_complete_faces = !complete_faces.empty();
    size_t best_index = use_complete_faces ? complete_faces[0] : 0;
    double best_rank = ScoreFaceCandidate(faces[best_index], frame);

    if (use_complete_faces) {
        for (size_t idx = 1; idx < complete_faces.size(); ++idx) {
            const size_t i = complete_faces[idx];
            const double rank = ScoreFaceCandidate(faces[i], frame);
            if (rank > best_rank) {
                best_index = i;
                best_rank = rank;
            }
        }
    } else {
        for (size_t i = 1; i < faces.size(); ++i) {
            const double rank = ScoreFaceCandidate(faces[i], frame);
            if (rank > best_rank) {
                best_index = i;
                best_rank = rank;
            }
        }
    }

    return faces[best_index];
}

bool EnsureGStreamerInitialized(std::string* error_message)
{
    static bool initialized = false;
    if (initialized) {
        return true;
    }

    GError* error = NULL;
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

std::string PopGStreamerBusMessage(GstElement* pipeline)
{
    if (pipeline == NULL) {
        return "";
    }
    GstBus* bus = gst_element_get_bus(pipeline);
    if (bus == NULL) {
        return "";
    }
    GstMessage* message = gst_bus_pop_filtered(bus,
                                               static_cast<GstMessageType>(GST_MESSAGE_ERROR |
                                                                           GST_MESSAGE_WARNING |
                                                                           GST_MESSAGE_EOS));
    std::string text;
    if (message != NULL) {
        GError* error = NULL;
        gchar* debug = NULL;
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

void StopDisplayPipeline(GstElement** pipeline, GstAppSrc** appsrc)
{
    if (pipeline != NULL && *pipeline != NULL) {
        gst_element_set_state(*pipeline, GST_STATE_NULL);
        gst_object_unref(*pipeline);
        *pipeline = NULL;
    }
    if (appsrc != NULL) {
        *appsrc = NULL;
    }
}

bool StartDisplayPipeline(int width,
                          int height,
                          GstElement** pipeline_out,
                          GstAppSrc** appsrc_out,
                          std::string* sink_name,
                          std::string* error_message)
{
    if (pipeline_out == NULL || appsrc_out == NULL) {
        if (error_message != NULL) {
            *error_message = "invalid display pipeline output";
        }
        return false;
    }
    *pipeline_out = NULL;
    *appsrc_out = NULL;
    if (sink_name != NULL) {
        sink_name->clear();
    }

    if (!EnsureGStreamerInitialized(error_message)) {
        return false;
    }

    struct CandidateSink {
        const char* name;
        const char* pipeline_tail;
    };
    static const CandidateSink kSinks[] = {
        {"kmssink", "videoconvert ! kmssink sync=false"},
        {"waylandsink", "videoconvert ! waylandsink sync=false"},
        {"autovideosink", "videoconvert ! autovideosink sync=false"},
    };

    std::string last_error;
    for (size_t i = 0; i < sizeof(kSinks) / sizeof(kSinks[0]); ++i) {
        std::ostringstream pipeline_text;
        pipeline_text << "appsrc name=preview_src is-live=true block=false do-timestamp=true format=time "
                      << "caps=video/x-raw,format=BGR,width=" << width
                      << ",height=" << height
                      << ",framerate=30/1 ! " << kSinks[i].pipeline_tail;

        GError* gst_error = NULL;
        GstElement* pipeline = gst_parse_launch(pipeline_text.str().c_str(), &gst_error);
        if (pipeline == NULL) {
            last_error = gst_error != NULL ? gst_error->message : "gst_parse_launch failed";
            if (gst_error != NULL) {
                g_error_free(gst_error);
            }
            continue;
        }

        GstElement* appsrc_element = gst_bin_get_by_name(GST_BIN(pipeline), "preview_src");
        if (appsrc_element == NULL) {
            last_error = "failed to get appsrc from display pipeline";
            gst_object_unref(pipeline);
            continue;
        }

        GstAppSrc* appsrc = GST_APP_SRC(appsrc_element);
        gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_STREAM);
        gst_app_src_set_max_bytes(appsrc, static_cast<guint64>(width) * static_cast<guint64>(height) * 3ULL * 2ULL);
        gst_app_src_set_leaky_type(appsrc, GST_APP_LEAKY_TYPE_DOWNSTREAM);

        GstStateChangeReturn state_ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (state_ret == GST_STATE_CHANGE_FAILURE) {
            last_error = "failed to set display pipeline to PLAYING";
            const std::string bus_message = PopGStreamerBusMessage(pipeline);
            if (!bus_message.empty()) {
                last_error += ": ";
                last_error += bus_message;
            }
            gst_object_unref(appsrc_element);
            gst_object_unref(pipeline);
            continue;
        }

        GstState current_state = GST_STATE_NULL;
        GstState pending_state = GST_STATE_NULL;
        GstStateChangeReturn wait_ret = gst_element_get_state(pipeline,
                                                              &current_state,
                                                              &pending_state,
                                                              3 * GST_SECOND);
        if (wait_ret == GST_STATE_CHANGE_FAILURE) {
            last_error = "failed to start display pipeline";
            const std::string bus_message = PopGStreamerBusMessage(pipeline);
            if (!bus_message.empty()) {
                last_error += ": ";
                last_error += bus_message;
            }
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(appsrc_element);
            gst_object_unref(pipeline);
            continue;
        }

        *pipeline_out = pipeline;
        *appsrc_out = appsrc;
        if (sink_name != NULL) {
            *sink_name = kSinks[i].name;
        }
        return true;
    }

    if (error_message != NULL) {
        *error_message = last_error.empty() ? "no usable display sink" : last_error;
    }
    return false;
}

bool PushFrameToDisplay(GstElement** pipeline,
                        GstAppSrc** appsrc,
                        std::string* sink_name,
                        const cv::Mat& frame,
                        std::string* error_message)
{
    if (frame.empty()) {
        if (error_message != NULL) {
            *error_message = "display frame is empty";
        }
        return false;
    }

    if (*pipeline == NULL || *appsrc == NULL) {
        if (!StartDisplayPipeline(frame.cols, frame.rows, pipeline, appsrc, sink_name, error_message)) {
            return false;
        }
        std::cout << "[info] display open mode=gstreamer sink=" << *sink_name
                  << " size=" << frame.cols << "x" << frame.rows << std::endl;
    }

    cv::Mat contiguous = frame.isContinuous() ? frame : frame.clone();
    const size_t bytes = contiguous.total() * contiguous.elemSize();
    GstBuffer* buffer = gst_buffer_new_allocate(NULL, bytes, NULL);
    if (buffer == NULL) {
        if (error_message != NULL) {
            *error_message = "failed to allocate display buffer";
        }
        return false;
    }
    gst_buffer_fill(buffer, 0, contiguous.data, bytes);
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 30);

    GstFlowReturn flow_ret = gst_app_src_push_buffer(*appsrc, buffer);
    if (flow_ret != GST_FLOW_OK) {
        if (error_message != NULL) {
            std::ostringstream message;
            message << "display push failed ret=" << static_cast<int>(flow_ret);
            const std::string bus_message = PopGStreamerBusMessage(*pipeline);
            if (!bus_message.empty()) {
                message << ": " << bus_message;
            }
            *error_message = message.str();
        }
        StopDisplayPipeline(pipeline, appsrc);
        if (sink_name != NULL) {
            sink_name->clear();
        }
        return false;
    }
    return true;
}

}  // namespace
#endif

#ifdef TOTAL_ENABLE_RKNN_FACE_CORE
struct FaceBridge::Impl {
    FrameSource source;
    FaceDatabase database;
    FaceRecognizer recognizer;
    YoloFaceDetector detector;
    MobileFaceNetExtractor extractor;
    std::deque<RecognitionResult> recognition_history;
    GstElement* display_pipeline;
    GstAppSrc* display_appsrc;
    std::string display_sink_name;
    int frame_index;
    int no_face_frames;
    bool display_available;
    bool warned_headless;

    Impl()
        : display_pipeline(NULL),
          display_appsrc(NULL),
          frame_index(0),
          no_face_frames(0),
          display_available(false),
          warned_headless(false) {
    }

    ~Impl()
    {
        StopDisplayPipeline(&display_pipeline, &display_appsrc);
    }
};
#else
struct FaceBridge::Impl {
};
#endif

FaceBridge::FaceBridge()
    : initialized_(false),
      impl_(new Impl())
{
    simulated_result_.matched = false;
    simulated_result_.similarity = 0.0f;
}

FaceBridge::~FaceBridge() = default;

bool FaceBridge::Initialize(const FaceBridgeConfig& config, std::string* error)
{
    config_ = config;
#ifdef TOTAL_ENABLE_RKNN_FACE_CORE
    std::string source_error;
    if (!config.image_path.empty()) {
        if (!impl_->source.open_image(config.image_path, &source_error)) {
            if (error != NULL) {
                *error = source_error;
            }
            return false;
        }
    } else if (!config.video_path.empty()) {
        if (!impl_->source.open_video(config.video_path, &source_error)) {
            if (error != NULL) {
                *error = source_error;
            }
            return false;
        }
    } else {
        CameraOptions camera_options;
        camera_options.width = config.camera_width;
        camera_options.height = config.camera_height;
        camera_options.rotate = config.camera_rotate;
        camera_options.mirror = config.camera_mirror;
        if (!impl_->source.open_camera(config.camera_index, camera_options, &source_error)) {
            if (error != NULL) {
                *error = source_error;
            }
            return false;
        }
    }

    if (!impl_->database.load(config.db_path, error)) {
        return false;
    }
    if (!impl_->detector.Init(config.det_model_path)) {
        if (error != NULL) {
            *error = impl_->detector.last_error();
        }
        return false;
    }
    if (!impl_->extractor.Init(config.rec_model_path)) {
        if (error != NULL) {
            *error = impl_->extractor.last_error();
        }
        return false;
    }

    impl_->recognizer.reset(impl_->database.records(), config.threshold);
    impl_->display_available = config.display_preview && config.image_path.empty();
    impl_->warned_headless = false;
    impl_->display_sink_name.clear();
    StopDisplayPipeline(&impl_->display_pipeline, &impl_->display_appsrc);
#endif

    initialized_ = true;
    if (error != NULL) {
        error->clear();
    }
    return true;
}

bool FaceBridge::IsReady() const
{
    return initialized_;
}

FaceMatchResult FaceBridge::PollRecognition()
{
#ifdef TOTAL_ENABLE_RKNN_FACE_CORE
    if (initialized_) {
        FaceMatchResult result;
        result.detected = false;
        result.matched = false;
        result.similarity = 0.0f;

        cv::Mat frame;
        std::string error_message;
        if (!impl_->source.read(&frame, &error_message) || frame.empty()) {
            if (!error_message.empty()) {
                std::cerr << "[warn] frame read failed: " << error_message << std::endl;
            } else {
                std::cerr << "[warn] frame read returned empty frame" << std::endl;
            }
            return result;
        }
        ++impl_->frame_index;
        if (impl_->frame_index == 1 || (impl_->frame_index % 30) == 0) {
            std::cout << "[info] frame ok index=" << impl_->frame_index
                      << " size=" << frame.cols << "x" << frame.rows << std::endl;
        }
        if (!config_.debug_frame_path.empty() &&
            (impl_->frame_index == 1 || (impl_->frame_index % 30) == 0)) {
            if (cv::imwrite(config_.debug_frame_path, frame)) {
                std::cout << "[info] debug frame saved path=" << config_.debug_frame_path
                          << " index=" << impl_->frame_index << std::endl;
            } else {
                std::cerr << "[warn] failed to save debug frame path="
                          << config_.debug_frame_path << std::endl;
            }
        }

        std::vector<FaceBox> faces;
        if (!impl_->detector.Detect(frame, &faces)) {
            std::cerr << "[warn] detector failed: " << impl_->detector.last_error() << std::endl;
            return result;
        }
        if (faces.empty()) {
            ++impl_->no_face_frames;
            impl_->recognition_history.clear();
            if (impl_->no_face_frames == 1 || (impl_->no_face_frames % 30) == 0) {
                std::cout << "[info] no face detected frame=" << impl_->frame_index
                          << " size=" << frame.cols << "x" << frame.rows << std::endl;
            }
            if (impl_->display_available) {
                std::string display_error;
                if (!PushFrameToDisplay(&impl_->display_pipeline,
                                        &impl_->display_appsrc,
                                        &impl_->display_sink_name,
                                        frame,
                                        &display_error)) {
                    impl_->display_available = false;
                    if (!impl_->warned_headless) {
                        std::cerr << "[warn] display disabled: " << display_error << std::endl;
                        std::cerr << "[warn] continuing in headless mode; use Ctrl+C to stop" << std::endl;
                        impl_->warned_headless = true;
                    }
                }
            }
            return result;
        }
        impl_->no_face_frames = 0;

        FaceBox best_face = SelectBestFace(faces, frame);
        result.detected = true;

        if (!IsFaceBoxComplete(best_face, frame)) {
            impl_->recognition_history.clear();
            std::cout << "[info] incomplete face box frame=" << impl_->frame_index
                      << " box=(" << best_face.x << "," << best_face.y
                      << "," << best_face.width << "x" << best_face.height << ")"
                      << std::endl;
            if (impl_->display_available) {
                RecognitionResult preview_result;
                preview_result.name = "unknown";
                preview_result.best_match_name = "unknown";
                preview_result.similarity = 0.0f;
                preview_result.is_unknown = true;
                DrawFaceResult(&frame, best_face, preview_result);
                std::string display_error;
                if (!PushFrameToDisplay(&impl_->display_pipeline,
                                        &impl_->display_appsrc,
                                        &impl_->display_sink_name,
                                        frame,
                                        &display_error)) {
                    impl_->display_available = false;
                    if (!impl_->warned_headless) {
                        std::cerr << "[warn] display disabled: " << display_error << std::endl;
                        std::cerr << "[warn] continuing in headless mode; use Ctrl+C to stop" << std::endl;
                        impl_->warned_headless = true;
                    }
                }
            }
            return result;
        }

        const cv::Mat crop = CropFaceForRecognition(frame, best_face);
        if (crop.empty()) {
            std::cerr << "[warn] crop face returned empty image"
                      << " box=(" << best_face.x << "," << best_face.y
                      << "," << best_face.width << "x" << best_face.height << ")"
                      << std::endl;
            return result;
        }

        std::vector<float> feature;
        if (!impl_->extractor.Extract(crop, &feature)) {
            std::cerr << "[warn] feature extraction failed: " << impl_->extractor.last_error() << std::endl;
            return result;
        }

        const RecognitionResult raw_result = impl_->recognizer.recognize(feature);
        RecognitionResult recognition = raw_result;
        if (config_.image_path.empty()) {
            const TemporalSmoothingConfig smoothing_config;
            recognition = ApplyTemporalSmoothing(raw_result,
                                                 &impl_->recognition_history,
                                                 smoothing_config,
                                                 config_.threshold);
        }
        if (!config_.debug_face_dir.empty() && !recognition.is_unknown && !recognition.name.empty()) {
            mkdir(config_.debug_face_dir.c_str(), 0777);
            std::ostringstream face_path;
            face_path << config_.debug_face_dir
                      << "/match_" << impl_->frame_index
                      << "_" << recognition.name
                      << "_" << recognition.similarity
                      << ".jpg";
            if (cv::imwrite(face_path.str(), crop)) {
                std::cout << "[info] matched face saved path=" << face_path.str()
                          << " index=" << impl_->frame_index << std::endl;
            } else {
                std::cerr << "[warn] failed to save matched face path=" << face_path.str() << std::endl;
            }
        }
        std::cout << "[face] " << recognition.name << " " << recognition.similarity;
        if (recognition.is_unknown && recognition.best_match_name != "unknown") {
            std::cout << " best=" << recognition.best_match_name;
        }
        std::cout << std::endl;

        if (impl_->display_available) {
            DrawFaceResult(&frame, best_face, recognition);
            std::string display_error;
            if (!PushFrameToDisplay(&impl_->display_pipeline,
                                    &impl_->display_appsrc,
                                    &impl_->display_sink_name,
                                    frame,
                                    &display_error)) {
                impl_->display_available = false;
                if (!impl_->warned_headless) {
                    std::cerr << "[warn] display disabled: " << display_error << std::endl;
                    std::cerr << "[warn] continuing in headless mode; use Ctrl+C to stop" << std::endl;
                    impl_->warned_headless = true;
                }
            }
        }

        result.matched = !recognition.is_unknown;
        result.name = result.matched ? recognition.name : recognition.best_match_name;
        result.similarity = recognition.similarity;
        return result;
    }
#endif
    return simulated_result_;
}

void FaceBridge::SetSimulatedMatch(bool matched, const std::string& name, float similarity)
{
    simulated_result_.matched = matched;
    simulated_result_.name = name;
    simulated_result_.similarity = similarity;
}

#ifndef TOTAL_FACE_FACE_BRIDGE_H
#define TOTAL_FACE_FACE_BRIDGE_H

#include <memory>
#include <string>

struct FaceBridgeConfig {
    std::string det_model_path;
    std::string rec_model_path;
    std::string db_path;
    std::string image_path;
    std::string video_path;
    std::string debug_frame_path;
    std::string debug_face_dir;
    bool display_preview;
    int camera_index;
    int camera_width;
    int camera_height;
    int camera_rotate;
    float threshold;
    bool camera_mirror;

    FaceBridgeConfig()
        : display_preview(false),
          camera_index(0),
          camera_width(1280),
          camera_height(720),
          camera_rotate(0),
          threshold(0.60f),
          camera_mirror(false)
    {
    }
};

struct FaceMatchResult {
    bool detected;
    bool matched;
    std::string name;
    float similarity;
};

class FaceBridge {
public:
    FaceBridge();
    ~FaceBridge();

    bool Initialize(const FaceBridgeConfig& config, std::string* error);
    bool IsReady() const;
    FaceMatchResult PollRecognition();

    void SetSimulatedMatch(bool matched, const std::string& name, float similarity);

private:
    struct Impl;

    bool initialized_;
    FaceBridgeConfig config_;
    FaceMatchResult simulated_result_;
    std::unique_ptr<Impl> impl_;
};

#endif

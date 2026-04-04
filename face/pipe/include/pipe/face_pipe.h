#ifndef TOTAL_FACE_FACE_BRIDGE_H
#define TOTAL_FACE_FACE_BRIDGE_H

#include <memory>
#include <string>

struct FaceCfg {
    std::string det_model_path;
    std::string rec_model_path;
    std::string rec_model_fp32_path;
    std::string rec_model_i8_path;
    std::string rec_mode;
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
    int rec_error_limit;
    float threshold;
    bool camera_mirror;

    FaceCfg()
        : display_preview(false),
          camera_index(0),
          camera_width(1280),
          camera_height(720),
          camera_rotate(0),
          rec_error_limit(3),
          threshold(0.60f),
          camera_mirror(false)
    {
        rec_mode = "fp32";
    }
};

struct FaceHit {
    bool detected;
    bool matched;
    bool rec_fallback;
    std::string name;
    std::string rec_mode;
    float similarity;
};

class FacePipe {
public:
    FacePipe();
    ~FacePipe();

    bool Initialize(const FaceCfg& config, std::string* error);
    bool IsReady() const;
    bool Activate(std::string* error);
    void Deactivate();
    bool IsActive() const;
    FaceHit PollRecognition();

    void SetSimulatedMatch(bool matched, const std::string& name, float similarity);

private:
    struct Impl;

    bool initialized_;
    FaceCfg config_;
    FaceHit simulated_result_;
    std::unique_ptr<Impl> impl_;
};

#endif

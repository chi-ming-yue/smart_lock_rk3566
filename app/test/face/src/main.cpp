#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <string>
#include <vector>

#include "pipe/face_pipe.h"

namespace {

int ParseInt(const std::string& value, int fallback)
{
    const int parsed = std::atoi(value.c_str());
    return parsed > 0 ? parsed : fallback;
}

float ParseFloat(const std::string& value, float fallback)
{
    const float parsed = static_cast<float>(std::atof(value.c_str()));
    return parsed > 0.0f ? parsed : fallback;
}

bool HasImageSuffix(const std::string& name)
{
    const std::string::size_type pos = name.find_last_of('.');
    if (pos == std::string::npos) {
        return false;
    }
    const std::string ext = name.substr(pos + 1);
    return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp";
}

std::vector<std::string> ListImages(const std::string& dir)
{
    std::vector<std::string> paths;
    DIR* handle = opendir(dir.c_str());
    if (handle == NULL) {
        return paths;
    }
    struct dirent* entry = NULL;
    while ((entry = readdir(handle)) != NULL) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        if (HasImageSuffix(name)) {
            paths.push_back(dir + "/" + name);
        }
    }
    closedir(handle);
    std::sort(paths.begin(), paths.end());
    return paths;
}

int RunOne(const FaceCfg& base_cfg, const std::string& image_path)
{
    FaceCfg cfg = base_cfg;
    cfg.image_path = image_path;
    FacePipe pipe;
    std::string error;
    if (!pipe.Initialize(cfg, &error)) {
        std::cerr << "init failed: " << image_path << " error=" << error << std::endl;
        return 1;
    }
    const FaceHit hit = pipe.PollRecognition();
    std::cout << image_path
              << " detected=" << (hit.detected ? 1 : 0)
              << " matched=" << (hit.matched ? 1 : 0)
              << " rec_mode=" << hit.rec_mode
              << " rec_fallback=" << (hit.rec_fallback ? 1 : 0)
              << " name=" << hit.name
              << " similarity=" << hit.similarity
              << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    FaceCfg cfg;
    std::string image_dir;
    std::vector<std::string> images;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--det-model" && i + 1 < argc) {
            cfg.det_model_path = argv[++i];
        } else if (arg == "--rec-model" && i + 1 < argc) {
            cfg.rec_model_path = argv[++i];
        } else if (arg == "--rec-model-fp32" && i + 1 < argc) {
            cfg.rec_model_fp32_path = argv[++i];
        } else if (arg == "--rec-model-i8" && i + 1 < argc) {
            cfg.rec_model_i8_path = argv[++i];
        } else if (arg == "--rec-mode" && i + 1 < argc) {
            cfg.rec_mode = argv[++i];
        } else if (arg == "--rec-error-limit" && i + 1 < argc) {
            cfg.rec_error_limit = ParseInt(argv[++i], cfg.rec_error_limit);
        } else if (arg == "--db" && i + 1 < argc) {
            cfg.db_path = argv[++i];
        } else if (arg == "--image" && i + 1 < argc) {
            images.push_back(argv[++i]);
        } else if (arg == "--image-dir" && i + 1 < argc) {
            image_dir = argv[++i];
        } else if (arg == "--debug-face-dir" && i + 1 < argc) {
            cfg.debug_face_dir = argv[++i];
        } else if (arg == "--debug-frame" && i + 1 < argc) {
            cfg.debug_frame_path = argv[++i];
        } else if (arg == "--threshold" && i + 1 < argc) {
            cfg.threshold = ParseFloat(argv[++i], cfg.threshold);
        } else if (arg == "--rotate" && i + 1 < argc) {
            cfg.camera_rotate = ParseInt(argv[++i], cfg.camera_rotate);
        }
    }

    if (!image_dir.empty()) {
        const std::vector<std::string> dir_images = ListImages(image_dir);
        images.insert(images.end(), dir_images.begin(), dir_images.end());
    }

    if (cfg.det_model_path.empty() || cfg.rec_model_path.empty() || cfg.db_path.empty() || images.empty()) {
        std::cerr << "usage: t_face --det-model <path> --rec-model <path> --db <path> "
                     "[--rec-model-fp32 <path>] [--rec-model-i8 <path>] [--rec-mode <fp32|i8>] "
                     "[--image <path>] [--image-dir <dir>] [--debug-face-dir <dir>] [--threshold <f>] [--rotate <deg>]"
                  << std::endl;
        return 1;
    }

    int failed = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        failed += RunOne(cfg, images[i]);
    }
    return failed == 0 ? 0 : 1;
}

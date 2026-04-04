#ifndef RK3566_FACE_REC_MOBILEFACENET_H_
#define RK3566_FACE_REC_MOBILEFACENET_H_

#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "rknn_api.h"

class MobileFaceNetExtractor {
public:
    MobileFaceNetExtractor();
    ~MobileFaceNetExtractor();

    bool Init(const std::string& model_path);
    void Release();
    bool IsReady() const;

    bool Extract(const cv::Mat& face_crop, std::vector<float>* feature);
    const std::string& last_error() const;

private:
    struct InputBuffer {
        std::vector<unsigned char> bytes;
        rknn_tensor_type tensor_type;
        rknn_tensor_format tensor_format;
    };

    bool QueryModelInfo();
    bool Preprocess(const cv::Mat& face_crop, InputBuffer* input_buffer) const;
    bool RunInference(const InputBuffer& input_buffer, std::vector<float>* feature);
    static void Normalize(std::vector<float>* feature);

    rknn_context ctx_;
    rknn_input_output_num io_num_;
    std::vector<rknn_tensor_attr> input_attrs_;
    std::vector<rknn_tensor_attr> output_attrs_;
    int input_width_;
    int input_height_;
    int input_channels_;
    bool initialized_;
    std::string last_error_;
};

#endif  // RK3566_FACE_REC_MOBILEFACENET_H_

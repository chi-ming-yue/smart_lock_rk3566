#ifndef RK3566_FACE_REC_YOLO_FACE_H_
#define RK3566_FACE_REC_YOLO_FACE_H_

#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "face_types.h"
#include "rknn_api.h"

class YoloFaceDetector {
public:
    YoloFaceDetector();
    ~YoloFaceDetector();

    bool Init(const std::string& model_path);
    void Release();
    bool IsReady() const;

    bool Detect(const cv::Mat& frame, std::vector<FaceBox>* faces);
    const std::string& last_error() const;

private:
    struct Proposal {
        float x;
        float y;
        float w;
        float h;
        float score;
        float landmarks[10];

        Proposal() : x(0.0f), y(0.0f), w(0.0f), h(0.0f), score(0.0f) {
            for (int i = 0; i < 10; ++i) {
                landmarks[i] = -1.0f;
            }
        }
    };

    bool QueryModelInfo();
    bool Preprocess(const cv::Mat& frame, std::vector<unsigned char>* input_data, float* scale, int* x_pad, int* y_pad) const;
    bool RunInference(const std::vector<unsigned char>& input_data, std::vector<rknn_output>* outputs);
    void ReleaseOutputs(std::vector<rknn_output>* outputs);
    bool DecodeOutputs(const std::vector<rknn_output>& outputs, int frame_width, int frame_height, float scale, int x_pad, int y_pad, std::vector<FaceBox>* faces) const;
    void AppendOutputProposals(const rknn_output& output, const rknn_tensor_attr& attr, int stride, float threshold, std::vector<Proposal>* proposals) const;
    static float Sigmoid(float x);
    static float IoU(const Proposal& a, const Proposal& b);

    rknn_context ctx_;
    rknn_input_output_num io_num_;
    std::vector<rknn_tensor_attr> input_attrs_;
    std::vector<rknn_tensor_attr> output_attrs_;
    int input_width_;
    int input_height_;
    int input_channels_;
    bool quantized_;
    bool initialized_;
    float conf_threshold_;
    float nms_threshold_;
    std::string last_error_;
};

#endif  // RK3566_FACE_REC_YOLO_FACE_H_

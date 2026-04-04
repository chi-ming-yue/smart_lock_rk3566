#include "yolo_face.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {

const int kAnchors[3][6] = {
    {4, 5, 8, 10, 13, 16},
    {23, 29, 43, 55, 73, 105},
    {146, 217, 231, 300, 335, 433},
};
const int kPropBoxSize = 16;

bool ReadBinaryFile(const std::string& path, std::vector<unsigned char>* bytes, std::string* error)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        if (error) {
            *error = "failed to open model file: " + path;
        }
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streampos length = input.tellg();
    if (length <= 0) {
        if (error) {
            *error = "model file is empty: " + path;
        }
        return false;
    }
    bytes->resize(static_cast<size_t>(length));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(bytes->data()), length);
    return static_cast<bool>(input);
}

int QueryAttr(rknn_context ctx, rknn_query_cmd cmd, uint32_t index, rknn_tensor_attr* attr)
{
    std::memset(attr, 0, sizeof(*attr));
    attr->index = index;
    return rknn_query(ctx, cmd, attr, sizeof(*attr));
}

float Dequantize(int8_t value, int32_t zp, float scale)
{
    return (static_cast<float>(value) - static_cast<float>(zp)) * scale;
}

}  // namespace

YoloFaceDetector::YoloFaceDetector()
    : ctx_(0),
      input_width_(640),
      input_height_(640),
      input_channels_(3),
      quantized_(false),
      initialized_(false),
      conf_threshold_(0.5f),
      nms_threshold_(0.45f)
{
    std::memset(&io_num_, 0, sizeof(io_num_));
}

YoloFaceDetector::~YoloFaceDetector()
{
    Release();
}

bool YoloFaceDetector::Init(const std::string& model_path)
{
    Release();

    std::vector<unsigned char> model_data;
    if (!ReadBinaryFile(model_path, &model_data, &last_error_)) {
        return false;
    }

    const int ret = rknn_init(&ctx_, model_data.data(), model_data.size(), 0, NULL);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_init failed for detector: " + std::to_string(ret);
        ctx_ = 0;
        return false;
    }

    if (!QueryModelInfo()) {
        Release();
        return false;
    }

    initialized_ = true;
    return true;
}

void YoloFaceDetector::Release()
{
    if (ctx_ != 0) {
        rknn_destroy(ctx_);
        ctx_ = 0;
    }
    input_attrs_.clear();
    output_attrs_.clear();
    std::memset(&io_num_, 0, sizeof(io_num_));
    quantized_ = false;
    initialized_ = false;
}

bool YoloFaceDetector::IsReady() const
{
    return initialized_;
}

const std::string& YoloFaceDetector::last_error() const
{
    return last_error_;
}

bool YoloFaceDetector::QueryModelInfo()
{
    int ret = rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &io_num_, sizeof(io_num_));
    if (ret != RKNN_SUCC) {
        last_error_ = "failed to query detector IO count: " + std::to_string(ret);
        return false;
    }
    if (io_num_.n_input != 1 || io_num_.n_output < 3) {
        last_error_ = "unexpected YOLOv5Face IO count";
        return false;
    }

    input_attrs_.resize(io_num_.n_input);
    output_attrs_.resize(io_num_.n_output);
    for (uint32_t i = 0; i < io_num_.n_input; ++i) {
        ret = QueryAttr(ctx_, RKNN_QUERY_INPUT_ATTR, i, &input_attrs_[i]);
        if (ret != RKNN_SUCC) {
            last_error_ = "failed to query detector input attr: " + std::to_string(ret);
            return false;
        }
    }
    for (uint32_t i = 0; i < io_num_.n_output; ++i) {
        ret = QueryAttr(ctx_, RKNN_QUERY_OUTPUT_ATTR, i, &output_attrs_[i]);
        if (ret != RKNN_SUCC) {
            last_error_ = "failed to query detector output attr: " + std::to_string(ret);
            return false;
        }
    }

    const rknn_tensor_attr& input = input_attrs_[0];
    if (input.fmt == RKNN_TENSOR_NCHW) {
        input_channels_ = static_cast<int>(input.dims[1]);
        input_height_ = static_cast<int>(input.dims[2]);
        input_width_ = static_cast<int>(input.dims[3]);
    } else {
        input_height_ = static_cast<int>(input.dims[1]);
        input_width_ = static_cast<int>(input.dims[2]);
        input_channels_ = static_cast<int>(input.dims[3]);
    }
    quantized_ = output_attrs_[0].qnt_type == RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC &&
                 output_attrs_[0].type != RKNN_TENSOR_FLOAT16 &&
                 output_attrs_[0].type != RKNN_TENSOR_FLOAT32;
    return true;
}

bool YoloFaceDetector::Preprocess(const cv::Mat& frame, std::vector<unsigned char>* input_data, float* scale, int* x_pad, int* y_pad) const
{
    if (frame.empty()) {
        return false;
    }

    const float sx = static_cast<float>(input_width_) / static_cast<float>(frame.cols);
    const float sy = static_cast<float>(input_height_) / static_cast<float>(frame.rows);
    *scale = std::min(sx, sy);

    const int resized_w = std::max(1, static_cast<int>(std::round(frame.cols * (*scale))));
    const int resized_h = std::max(1, static_cast<int>(std::round(frame.rows * (*scale))));
    *x_pad = (input_width_ - resized_w) / 2;
    *y_pad = (input_height_ - resized_h) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(resized_w, resized_h));

    cv::Mat canvas(input_height_, input_width_, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(*x_pad, *y_pad, resized_w, resized_h)));

    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);

    input_data->assign(static_cast<size_t>(input_width_ * input_height_ * input_channels_), 0);
    if (input_attrs_[0].fmt == RKNN_TENSOR_NHWC) {
        std::memcpy(input_data->data(), rgb.data, input_data->size());
        return true;
    }

    for (int c = 0; c < input_channels_; ++c) {
        for (int y = 0; y < input_height_; ++y) {
            for (int x = 0; x < input_width_; ++x) {
                (*input_data)[c * input_width_ * input_height_ + y * input_width_ + x] =
                    rgb.at<cv::Vec3b>(y, x)[c];
            }
        }
    }
    return true;
}

bool YoloFaceDetector::RunInference(const std::vector<unsigned char>& input_data, std::vector<rknn_output>* outputs)
{
    rknn_input input;
    std::memset(&input, 0, sizeof(input));
    input.index = 0;
    input.type = RKNN_TENSOR_UINT8;
    input.fmt = input_attrs_[0].fmt;
    input.buf = const_cast<unsigned char*>(input_data.data());
    input.size = static_cast<uint32_t>(input_data.size());

    int ret = rknn_inputs_set(ctx_, 1, &input);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_inputs_set failed for detector: " + std::to_string(ret);
        return false;
    }

    ret = rknn_run(ctx_, NULL);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_run failed for detector: " + std::to_string(ret);
        return false;
    }

    outputs->assign(io_num_.n_output, rknn_output());
    for (uint32_t i = 0; i < io_num_.n_output; ++i) {
        (*outputs)[i].index = i;
        (*outputs)[i].want_float = quantized_ ? 0 : 1;
    }
    ret = rknn_outputs_get(ctx_, io_num_.n_output, outputs->data(), NULL);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_outputs_get failed for detector: " + std::to_string(ret);
        outputs->clear();
        return false;
    }
    return true;
}

void YoloFaceDetector::ReleaseOutputs(std::vector<rknn_output>* outputs)
{
    if (!outputs->empty() && ctx_ != 0) {
        rknn_outputs_release(ctx_, static_cast<uint32_t>(outputs->size()), outputs->data());
    }
    outputs->clear();
}

float YoloFaceDetector::Sigmoid(float x)
{
    return 1.0f / (1.0f + std::exp(-x));
}

float YoloFaceDetector::IoU(const Proposal& a, const Proposal& b)
{
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.w, b.x + b.w);
    const float bottom = std::min(a.y + a.h, b.y + b.h);
    const float inter_w = std::max(0.0f, right - left);
    const float inter_h = std::max(0.0f, bottom - top);
    const float inter_area = inter_w * inter_h;
    const float union_area = a.w * a.h + b.w * b.h - inter_area;
    if (union_area <= 1e-6f) {
        return 0.0f;
    }
    return inter_area / union_area;
}

void YoloFaceDetector::AppendOutputProposals(const rknn_output& output,
                                             const rknn_tensor_attr& attr,
                                             int stride,
                                             float threshold,
                                             std::vector<Proposal>* proposals) const
{
    const int grid_h = static_cast<int>(attr.dims[2]);
    const int grid_w = static_cast<int>(attr.dims[3]);
    const int grid_len = grid_h * grid_w;

    for (int a = 0; a < 3; ++a) {
        for (int y = 0; y < grid_h; ++y) {
            for (int x = 0; x < grid_w; ++x) {
                const int base = (kPropBoxSize * a) * grid_len + y * grid_w + x;
                float objectness = 0.0f;
                float cls_conf = 0.0f;
                float tx = 0.0f;
                float ty = 0.0f;
                float tw = 0.0f;
                float th = 0.0f;
                float landmarks[10] = {0.0f};
                const float anchor_w = static_cast<float>(kAnchors[std::min(2, stride == 8 ? 0 : (stride == 16 ? 1 : 2))][a * 2]);
                const float anchor_h = static_cast<float>(kAnchors[std::min(2, stride == 8 ? 0 : (stride == 16 ? 1 : 2))][a * 2 + 1]);

                if (quantized_) {
                    const int8_t* data = reinterpret_cast<const int8_t*>(output.buf);
                    objectness = Sigmoid(Dequantize(data[(kPropBoxSize * a + 4) * grid_len + y * grid_w + x], attr.zp, attr.scale));
                    cls_conf = Sigmoid(Dequantize(data[(kPropBoxSize * a + 15) * grid_len + y * grid_w + x], attr.zp, attr.scale));
                    tx = Sigmoid(Dequantize(data[base + 0 * grid_len], attr.zp, attr.scale));
                    ty = Sigmoid(Dequantize(data[base + 1 * grid_len], attr.zp, attr.scale));
                    tw = Sigmoid(Dequantize(data[base + 2 * grid_len], attr.zp, attr.scale));
                    th = Sigmoid(Dequantize(data[base + 3 * grid_len], attr.zp, attr.scale));
                    for (int point = 0; point < 5; ++point) {
                        landmarks[point * 2 + 0] = Dequantize(data[base + (5 + point * 2 + 0) * grid_len], attr.zp, attr.scale);
                        landmarks[point * 2 + 1] = Dequantize(data[base + (5 + point * 2 + 1) * grid_len], attr.zp, attr.scale);
                    }
                } else {
                    const float* data = reinterpret_cast<const float*>(output.buf);
                    objectness = Sigmoid(data[(kPropBoxSize * a + 4) * grid_len + y * grid_w + x]);
                    cls_conf = Sigmoid(data[(kPropBoxSize * a + 15) * grid_len + y * grid_w + x]);
                    tx = Sigmoid(data[base + 0 * grid_len]);
                    ty = Sigmoid(data[base + 1 * grid_len]);
                    tw = Sigmoid(data[base + 2 * grid_len]);
                    th = Sigmoid(data[base + 3 * grid_len]);
                    for (int point = 0; point < 5; ++point) {
                        landmarks[point * 2 + 0] = data[base + (5 + point * 2 + 0) * grid_len];
                        landmarks[point * 2 + 1] = data[base + (5 + point * 2 + 1) * grid_len];
                    }
                }

                const float score = objectness * cls_conf;
                if (score < threshold) {
                    continue;
                }

                float box_x = (tx * 2.0f - 0.5f + static_cast<float>(x)) * stride;
                float box_y = (ty * 2.0f - 0.5f + static_cast<float>(y)) * stride;
                float box_w = tw * 2.0f;
                float box_h = th * 2.0f;
                box_w = box_w * box_w * static_cast<float>(kAnchors[std::min(2, stride == 8 ? 0 : (stride == 16 ? 1 : 2))][a * 2]);
                box_h = box_h * box_h * static_cast<float>(kAnchors[std::min(2, stride == 8 ? 0 : (stride == 16 ? 1 : 2))][a * 2 + 1]);

                Proposal proposal;
                proposal.x = box_x - box_w * 0.5f;
                proposal.y = box_y - box_h * 0.5f;
                proposal.w = box_w;
                proposal.h = box_h;
                proposal.score = score;
                for (int point = 0; point < 5; ++point) {
                    proposal.landmarks[point * 2 + 0] = (landmarks[point * 2 + 0] * anchor_w + static_cast<float>(x)) * stride;
                    proposal.landmarks[point * 2 + 1] = (landmarks[point * 2 + 1] * anchor_h + static_cast<float>(y)) * stride;
                }
                proposals->push_back(proposal);
            }
        }
    }
}

bool YoloFaceDetector::DecodeOutputs(const std::vector<rknn_output>& outputs,
                                     int frame_width,
                                     int frame_height,
                                     float scale,
                                     int x_pad,
                                     int y_pad,
                                     std::vector<FaceBox>* faces) const
{
    std::vector<Proposal> proposals;
    for (size_t i = 0; i < outputs.size() && i < 3; ++i) {
        const int grid_h = static_cast<int>(output_attrs_[i].dims[2]);
        if (grid_h <= 0) {
            continue;
        }
        const int stride = input_height_ / grid_h;
        AppendOutputProposals(outputs[i], output_attrs_[i], stride, conf_threshold_, &proposals);
    }

    std::sort(proposals.begin(), proposals.end(), [](const Proposal& lhs, const Proposal& rhs) {
        return lhs.score > rhs.score;
    });

    std::vector<Proposal> selected;
    for (size_t i = 0; i < proposals.size(); ++i) {
        bool keep = true;
        for (size_t j = 0; j < selected.size(); ++j) {
            if (IoU(proposals[i], selected[j]) > nms_threshold_) {
                keep = false;
                break;
            }
        }
        if (keep) {
            selected.push_back(proposals[i]);
        }
    }

    faces->clear();
    for (size_t i = 0; i < selected.size(); ++i) {
        const Proposal& p = selected[i];
        int x = static_cast<int>((p.x - static_cast<float>(x_pad)) / scale);
        int y = static_cast<int>((p.y - static_cast<float>(y_pad)) / scale);
        int w = static_cast<int>(p.w / scale);
        int h = static_cast<int>(p.h / scale);

        x = std::max(0, std::min(frame_width - 1, x));
        y = std::max(0, std::min(frame_height - 1, y));
        w = std::max(0, std::min(frame_width - x, w));
        h = std::max(0, std::min(frame_height - y, h));
        if (w <= 1 || h <= 1) {
            continue;
        }

        FaceBox face;
        face.x = x;
        face.y = y;
        face.width = w;
        face.height = h;
        face.score = p.score;
        for (int point = 0; point < 5; ++point) {
            const float raw_x = (p.landmarks[point * 2 + 0] - static_cast<float>(x_pad)) / scale;
            const float raw_y = (p.landmarks[point * 2 + 1] - static_cast<float>(y_pad)) / scale;
            face.landmarks[point * 2 + 0] = std::max(0.0f, std::min(raw_x, static_cast<float>(frame_width - 1)));
            face.landmarks[point * 2 + 1] = std::max(0.0f, std::min(raw_y, static_cast<float>(frame_height - 1)));
        }
        faces->push_back(face);
    }
    return true;
}

bool YoloFaceDetector::Detect(const cv::Mat& frame, std::vector<FaceBox>* faces)
{
    if (!initialized_) {
        last_error_ = "detector model is not initialized";
        return false;
    }
    if (frame.empty()) {
        last_error_ = "input frame is empty";
        return false;
    }

    std::vector<unsigned char> input_data;
    float scale = 1.0f;
    int x_pad = 0;
    int y_pad = 0;
    if (!Preprocess(frame, &input_data, &scale, &x_pad, &y_pad)) {
        last_error_ = "failed to preprocess frame";
        return false;
    }

    std::vector<rknn_output> outputs;
    if (!RunInference(input_data, &outputs)) {
        return false;
    }

    const bool ok = DecodeOutputs(outputs, frame.cols, frame.rows, scale, x_pad, y_pad, faces);
    ReleaseOutputs(&outputs);
    if (!ok) {
        last_error_ = "failed to decode detector outputs";
        return false;
    }
    return true;
}

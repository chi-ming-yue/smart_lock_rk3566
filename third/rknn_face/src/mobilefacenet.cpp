#include "mobilefacenet.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "rknn_common.h"

namespace {

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
    if (!input) {
        if (error) {
            *error = "failed to read model file: " + path;
        }
        return false;
    }
    return true;
}

int QueryAttr(rknn_context ctx, rknn_query_cmd cmd, uint32_t index, rknn_tensor_attr* attr)
{
    std::memset(attr, 0, sizeof(*attr));
    attr->index = index;
    return rknn_query(ctx, cmd, attr, sizeof(*attr));
}

}  // namespace

MobileFaceNetExtractor::MobileFaceNetExtractor()
    : ctx_(0),
      input_width_(112),
      input_height_(112),
      input_channels_(3),
      initialized_(false)
{
    std::memset(&io_num_, 0, sizeof(io_num_));
}

MobileFaceNetExtractor::~MobileFaceNetExtractor()
{
    Release();
}

bool MobileFaceNetExtractor::Init(const std::string& model_path)
{
    Release();

    std::vector<unsigned char> model_data;
    if (!ReadBinaryFile(model_path, &model_data, &last_error_)) {
        return false;
    }

    const int ret = rknn_init(&ctx_, model_data.data(), model_data.size(), 0, NULL);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_init failed for MobileFaceNet: " + std::to_string(ret);
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

void MobileFaceNetExtractor::Release()
{
    if (ctx_ != 0) {
        rknn_destroy(ctx_);
        ctx_ = 0;
    }
    input_attrs_.clear();
    output_attrs_.clear();
    std::memset(&io_num_, 0, sizeof(io_num_));
    initialized_ = false;
}

bool MobileFaceNetExtractor::IsReady() const
{
    return initialized_;
}

const std::string& MobileFaceNetExtractor::last_error() const
{
    return last_error_;
}

bool MobileFaceNetExtractor::QueryModelInfo()
{
    int ret = rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &io_num_, sizeof(io_num_));
    if (ret != RKNN_SUCC) {
        last_error_ = "failed to query MobileFaceNet IO count: " + std::to_string(ret);
        return false;
    }
    if (io_num_.n_input != 1 || io_num_.n_output < 1) {
        last_error_ = "unexpected MobileFaceNet IO count";
        return false;
    }

    input_attrs_.resize(io_num_.n_input);
    output_attrs_.resize(io_num_.n_output);
    for (uint32_t i = 0; i < io_num_.n_input; ++i) {
        ret = QueryAttr(ctx_, RKNN_QUERY_INPUT_ATTR, i, &input_attrs_[i]);
        if (ret != RKNN_SUCC) {
            last_error_ = "failed to query MobileFaceNet input attr: " + std::to_string(ret);
            return false;
        }
    }
    for (uint32_t i = 0; i < io_num_.n_output; ++i) {
        ret = QueryAttr(ctx_, RKNN_QUERY_OUTPUT_ATTR, i, &output_attrs_[i]);
        if (ret != RKNN_SUCC) {
            last_error_ = "failed to query MobileFaceNet output attr: " + std::to_string(ret);
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
    std::cout << "[info] mobilefacenet input: " << tensor_attr_to_string(input_attrs_[0]) << std::endl;
    std::cout << "[info] mobilefacenet output: " << tensor_attr_to_string(output_attrs_[0]) << std::endl;
    return true;
}

bool MobileFaceNetExtractor::Preprocess(const cv::Mat& face_crop, InputBuffer* input_buffer) const
{
    if (face_crop.empty()) {
        return false;
    }

    cv::Mat resized;
    cv::resize(face_crop, resized, cv::Size(input_width_, input_height_));
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    input_buffer->bytes.clear();
    input_buffer->tensor_type = input_attrs_[0].type;
    input_buffer->tensor_format = input_attrs_[0].fmt;
    const bool nchw = input_attrs_[0].fmt == RKNN_TENSOR_NCHW;
    const size_t element_count = static_cast<size_t>(input_width_ * input_height_ * input_channels_);

    if (input_attrs_[0].type == RKNN_TENSOR_UINT8) {
        input_buffer->bytes.assign(element_count, 0);
        for (int y = 0; y < input_height_; ++y) {
            for (int x = 0; x < input_width_; ++x) {
                const cv::Vec3b pixel = rgb.at<cv::Vec3b>(y, x);
                for (int c = 0; c < input_channels_; ++c) {
                    if (nchw) {
                        input_buffer->bytes[c * input_height_ * input_width_ + y * input_width_ + x] = pixel[c];
                    } else {
                        input_buffer->bytes[(y * input_width_ + x) * input_channels_ + c] = pixel[c];
                    }
                }
            }
        }
        return true;
    }

    if (input_attrs_[0].type == RKNN_TENSOR_INT8) {
        input_buffer->bytes.assign(element_count, 0);
        for (int y = 0; y < input_height_; ++y) {
            for (int x = 0; x < input_width_; ++x) {
                const cv::Vec3b pixel = rgb.at<cv::Vec3b>(y, x);
                for (int c = 0; c < input_channels_; ++c) {
                    const float normalized = (static_cast<float>(pixel[c]) - 127.5f) / 127.5f;
                    const float quantized = normalized / input_attrs_[0].scale + static_cast<float>(input_attrs_[0].zp);
                    const int clipped = std::max(-128, std::min(127, static_cast<int>(std::round(quantized))));
                    if (nchw) {
                        reinterpret_cast<int8_t*>(input_buffer->bytes.data())[c * input_height_ * input_width_ + y * input_width_ + x] =
                            static_cast<int8_t>(clipped);
                    } else {
                        reinterpret_cast<int8_t*>(input_buffer->bytes.data())[(y * input_width_ + x) * input_channels_ + c] =
                            static_cast<int8_t>(clipped);
                    }
                }
            }
        }
        return true;
    }

    input_buffer->bytes.assign(element_count * sizeof(float), 0);
    float* input_data = reinterpret_cast<float*>(input_buffer->bytes.data());
    for (int y = 0; y < input_height_; ++y) {
        for (int x = 0; x < input_width_; ++x) {
            const cv::Vec3b pixel = rgb.at<cv::Vec3b>(y, x);
            for (int c = 0; c < input_channels_; ++c) {
                const float normalized = (static_cast<float>(pixel[c]) - 127.5f) / 127.5f;
                if (nchw) {
                    input_data[c * input_height_ * input_width_ + y * input_width_ + x] = normalized;
                } else {
                    input_data[(y * input_width_ + x) * input_channels_ + c] = normalized;
                }
            }
        }
    }

    return true;
}

bool MobileFaceNetExtractor::RunInference(const InputBuffer& input_buffer, std::vector<float>* feature)
{
    rknn_input input;
    std::memset(&input, 0, sizeof(input));
    input.index = 0;
    input.type = input_buffer.tensor_type;
    input.fmt = input_buffer.tensor_format;
    input.buf = const_cast<unsigned char*>(input_buffer.bytes.data());
    input.size = static_cast<uint32_t>(input_buffer.bytes.size());

    int ret = rknn_inputs_set(ctx_, 1, &input);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_inputs_set failed for MobileFaceNet: " + std::to_string(ret);
        return false;
    }

    ret = rknn_run(ctx_, NULL);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_run failed for MobileFaceNet: " + std::to_string(ret);
        return false;
    }

    rknn_output output;
    std::memset(&output, 0, sizeof(output));
    output.index = 0;
    output.want_float = 1;
    ret = rknn_outputs_get(ctx_, 1, &output, NULL);
    if (ret != RKNN_SUCC) {
        last_error_ = "rknn_outputs_get failed for MobileFaceNet: " + std::to_string(ret);
        return false;
    }

    const float* data = reinterpret_cast<const float*>(output.buf);
    feature->assign(data, data + output_attrs_[0].n_elems);
    rknn_outputs_release(ctx_, 1, &output);
    return true;
}

void MobileFaceNetExtractor::Normalize(std::vector<float>* feature)
{
    float norm = 0.0f;
    for (size_t i = 0; i < feature->size(); ++i) {
        norm += (*feature)[i] * (*feature)[i];
    }
    norm = std::sqrt(norm);
    if (norm <= 1e-8f) {
        return;
    }
    for (size_t i = 0; i < feature->size(); ++i) {
        (*feature)[i] /= norm;
    }
}

bool MobileFaceNetExtractor::Extract(const cv::Mat& face_crop, std::vector<float>* feature)
{
    if (!initialized_) {
        last_error_ = "MobileFaceNet model is not initialized";
        return false;
    }

    InputBuffer input_buffer;
    if (!Preprocess(face_crop, &input_buffer)) {
        last_error_ = "failed to preprocess face crop";
        return false;
    }

    if (!RunInference(input_buffer, feature)) {
        return false;
    }

    Normalize(feature);
    return true;
}

#include "face_alignment.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>

namespace {

cv::Rect ComputeExpandedFaceRect(const FaceBox& box, float expand_ratio)
{
    const float extra_w = static_cast<float>(box.width) * expand_ratio;
    const float extra_h = static_cast<float>(box.height) * expand_ratio;

    const int left = static_cast<int>(std::floor(static_cast<float>(box.x) - extra_w));
    const int top = static_cast<int>(std::floor(static_cast<float>(box.y) - extra_h));
    const int right = static_cast<int>(std::ceil(static_cast<float>(box.x + box.width) + extra_w));
    const int bottom = static_cast<int>(std::ceil(static_cast<float>(box.y + box.height) + extra_h));
    return cv::Rect(left, top, right - left, bottom - top);
}

cv::Mat PadFrameForFace(const cv::Mat& frame,
                        const FaceBox& box,
                        float expand_ratio,
                        FaceBox* padded_box,
                        cv::Rect* padded_roi)
{
    if (frame.empty() || padded_box == NULL || padded_roi == NULL) {
        return cv::Mat();
    }

    const cv::Rect expanded = ComputeExpandedFaceRect(box, expand_ratio);
    const int pad_left = std::max(0, -expanded.x);
    const int pad_top = std::max(0, -expanded.y);
    const int pad_right = std::max(0, expanded.x + expanded.width - frame.cols);
    const int pad_bottom = std::max(0, expanded.y + expanded.height - frame.rows);

    cv::Mat padded;
    if (pad_left == 0 && pad_top == 0 && pad_right == 0 && pad_bottom == 0) {
        padded = frame;
    } else {
        cv::copyMakeBorder(frame,
                           padded,
                           pad_top,
                           pad_bottom,
                           pad_left,
                           pad_right,
                           cv::BORDER_REFLECT_101);
    }

    *padded_box = box;
    padded_box->x += pad_left;
    padded_box->y += pad_top;
    for (int i = 0; i < 5; ++i) {
        if (padded_box->landmarks[i * 2] >= 0.0f) {
            padded_box->landmarks[i * 2] += static_cast<float>(pad_left);
        }
        if (padded_box->landmarks[i * 2 + 1] >= 0.0f) {
            padded_box->landmarks[i * 2 + 1] += static_cast<float>(pad_top);
        }
    }

    *padded_roi = cv::Rect(expanded.x + pad_left,
                           expanded.y + pad_top,
                           expanded.width,
                           expanded.height);
    if (padded_roi->x < 0 || padded_roi->y < 0 ||
        padded_roi->x + padded_roi->width > padded.cols ||
        padded_roi->y + padded_roi->height > padded.rows ||
        padded_roi->width <= 0 || padded_roi->height <= 0) {
        return cv::Mat();
    }

    if (padded.data == frame.data) {
        return frame;
    }
    return padded;
}

}  // namespace

cv::Rect ClampFaceRect(const FaceBox& box, const cv::Size& image_size, float expand_ratio)
{
    if (image_size.width <= 0 || image_size.height <= 0) {
        return cv::Rect();
    }

    cv::Rect roi = ComputeExpandedFaceRect(box, expand_ratio);
    roi &= cv::Rect(0, 0, image_size.width, image_size.height);
    if (roi.width <= 0 || roi.height <= 0) {
        return cv::Rect();
    }

    return roi;
}


bool HasValidFaceLandmarks(const FaceBox& box)
{
    for (int i = 0; i < 5; ++i) {
        if (box.landmarks[i * 2] < 0.0f || box.landmarks[i * 2 + 1] < 0.0f) {
            return false;
        }
    }
    return true;
}

cv::Mat AlignFaceForRecognition(const cv::Mat& frame, const FaceBox& box, const cv::Size& output_size)
{
    if (frame.empty() || output_size.width <= 0 || output_size.height <= 0 || !HasValidFaceLandmarks(box)) {
        return cv::Mat();
    }

    static const cv::Point2f kTemplate[5] = {
        cv::Point2f(38.2946f, 51.6963f),
        cv::Point2f(73.5318f, 51.5014f),
        cv::Point2f(56.0252f, 71.7366f),
        cv::Point2f(41.5493f, 92.3655f),
        cv::Point2f(70.7299f, 92.2041f),
    };

    const float scale_x = static_cast<float>(output_size.width) / 112.0f;
    const float scale_y = static_cast<float>(output_size.height) / 112.0f;

    const cv::Point2f left_eye(box.landmarks[0], box.landmarks[1]);
    const cv::Point2f right_eye(box.landmarks[2], box.landmarks[3]);
    const cv::Point2f mouth_center((box.landmarks[6] + box.landmarks[8]) * 0.5f,
                                   (box.landmarks[7] + box.landmarks[9]) * 0.5f);
    const cv::Point2f src_center((left_eye.x + right_eye.x) * 0.5f,
                                 (left_eye.y + right_eye.y) * 0.5f);
    const cv::Point2f dst_left_eye(kTemplate[0].x * scale_x, kTemplate[0].y * scale_y);
    const cv::Point2f dst_right_eye(kTemplate[1].x * scale_x, kTemplate[1].y * scale_y);
    const cv::Point2f dst_mouth_center((kTemplate[3].x + kTemplate[4].x) * 0.5f * scale_x,
                                       (kTemplate[3].y + kTemplate[4].y) * 0.5f * scale_y);
    const cv::Point2f dst_center((dst_left_eye.x + dst_right_eye.x) * 0.5f,
                                 (dst_left_eye.y + dst_right_eye.y) * 0.5f);

    const float src_dx = right_eye.x - left_eye.x;
    const float src_dy = right_eye.y - left_eye.y;
    const float dst_dx = dst_right_eye.x - dst_left_eye.x;
    const float dst_dy = dst_right_eye.y - dst_left_eye.y;
    const float src_eye_dist = std::sqrt(src_dx * src_dx + src_dy * src_dy);
    const float dst_eye_dist = std::sqrt(dst_dx * dst_dx + dst_dy * dst_dy);
    if (src_eye_dist < 1e-3f || dst_eye_dist < 1e-3f) {
        return cv::Mat();
    }

    const float src_angle = std::atan2(src_dy, src_dx);
    const float dst_angle = std::atan2(dst_dy, dst_dx);
    const float angle_deg = static_cast<float>((dst_angle - src_angle) * 180.0 / CV_PI);
    const double scale = static_cast<double>(dst_eye_dist / src_eye_dist);

    cv::Mat transform = cv::getRotationMatrix2D(src_center, angle_deg, scale);
    const double transformed_x = transform.at<double>(0, 0) * src_center.x +
                                 transform.at<double>(0, 1) * src_center.y +
                                 transform.at<double>(0, 2);
    const double transformed_y = transform.at<double>(1, 0) * src_center.x +
                                 transform.at<double>(1, 1) * src_center.y +
                                 transform.at<double>(1, 2);
    transform.at<double>(0, 2) += dst_center.x - transformed_x;
    transform.at<double>(1, 2) += dst_center.y - transformed_y;

    const double mouth_after_y = transform.at<double>(1, 0) * mouth_center.x +
                                 transform.at<double>(1, 1) * mouth_center.y +
                                 transform.at<double>(1, 2);
    transform.at<double>(1, 2) += dst_mouth_center.y - mouth_after_y;

    cv::Mat aligned;
    cv::warpAffine(frame,
                   aligned,
                   transform,
                   output_size,
                   cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT,
                   cv::Scalar(0, 0, 0));
    return aligned;
}

cv::Mat CropFaceForRecognition(const cv::Mat& frame, const FaceBox& box, float expand_ratio)
{
    FaceBox padded_box;
    cv::Rect padded_roi;
    const cv::Mat padded_frame = PadFrameForFace(frame, box, expand_ratio, &padded_box, &padded_roi);
    if (padded_frame.empty()) {
        return cv::Mat();
    }

    const cv::Mat aligned = AlignFaceForRecognition(padded_frame, padded_box);
    if (!aligned.empty()) {
        return aligned;
    }

    if (padded_roi.width <= 0 || padded_roi.height <= 0) {
        return cv::Mat();
    }
    return padded_frame(padded_roi).clone();
}

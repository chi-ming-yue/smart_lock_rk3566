#ifndef RK3566_FACE_REC_FACE_ALIGNMENT_H_
#define RK3566_FACE_REC_FACE_ALIGNMENT_H_

#include <opencv2/core.hpp>

#include "face_types.h"

cv::Rect ClampFaceRect(const FaceBox& box, const cv::Size& image_size, float expand_ratio);
bool HasValidFaceLandmarks(const FaceBox& box);
cv::Mat AlignFaceForRecognition(const cv::Mat& frame, const FaceBox& box, const cv::Size& output_size = cv::Size(112, 112));
cv::Mat CropFaceForRecognition(const cv::Mat& frame, const FaceBox& box, float expand_ratio = 0.15f);

#endif  // RK3566_FACE_REC_FACE_ALIGNMENT_H_

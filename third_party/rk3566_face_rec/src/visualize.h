#ifndef RK3566_FACE_REC_VISUALIZE_H_
#define RK3566_FACE_REC_VISUALIZE_H_

#include <opencv2/core.hpp>

#include "face_types.h"

void DrawFaceResult(cv::Mat* frame, const FaceBox& box, const RecognitionResult& result);

#endif  // RK3566_FACE_REC_VISUALIZE_H_

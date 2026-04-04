#include "visualize.h"

#include <sstream>

#include <opencv2/imgproc.hpp>

void DrawFaceResult(cv::Mat* frame, const FaceBox& box, const RecognitionResult& result)
{
    if (frame == NULL || frame->empty()) {
        return;
    }

    const cv::Scalar color = result.is_unknown ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
    const cv::Rect rect(box.x, box.y, box.width, box.height);
    cv::rectangle(*frame, rect, color, 2);
    static const cv::Scalar kLandmarkColors[5] = {
        cv::Scalar(255, 0, 0),
        cv::Scalar(0, 255, 0),
        cv::Scalar(0, 0, 255),
        cv::Scalar(255, 255, 0),
        cv::Scalar(0, 255, 255),
    };
    for (int i = 0; i < 5; ++i) {
        if (box.landmarks[i * 2] >= 0.0f && box.landmarks[i * 2 + 1] >= 0.0f) {
            cv::circle(*frame,
                       cv::Point(static_cast<int>(box.landmarks[i * 2]), static_cast<int>(box.landmarks[i * 2 + 1])),
                       2,
                       kLandmarkColors[i],
                       cv::FILLED,
                       cv::LINE_AA);
        }
    }

    std::ostringstream label;
    label << (result.name.empty() ? "unknown" : result.name) << " " << cv::format("%.3f", result.similarity);

    int baseline = 0;
    const cv::Size label_size = cv::getTextSize(label.str(), cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
    const int text_x = std::max(0, box.x);
    const int text_y = std::max(label_size.height + 4, box.y - 6);
    const cv::Rect text_bg(text_x, text_y - label_size.height - 4, label_size.width + 8, label_size.height + 8);

    cv::rectangle(*frame, text_bg, color, cv::FILLED);
    cv::putText(*frame,
                label.str(),
                cv::Point(text_x + 4, text_y),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(255, 255, 255),
                2,
                cv::LINE_AA);
}

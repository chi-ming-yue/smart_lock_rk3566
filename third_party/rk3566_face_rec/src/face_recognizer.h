#ifndef RK3566_FACE_REC_FACE_RECOGNIZER_H_
#define RK3566_FACE_REC_FACE_RECOGNIZER_H_

#include <vector>

#include "face_types.h"

class FaceRecognizer {
public:
    FaceRecognizer();
    FaceRecognizer(const std::vector<FaceRecord> &records, float threshold);

    void reset(const std::vector<FaceRecord> &records, float threshold);
    RecognitionResult recognize(const std::vector<float> &feature) const;

    static bool normalize_feature(std::vector<float> *feature);

private:
    std::vector<FaceRecord> records_;
    float threshold_;
};

#endif  // RK3566_FACE_REC_FACE_RECOGNIZER_H_

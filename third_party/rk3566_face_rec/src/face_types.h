#ifndef RK3566_FACE_REC_FACE_TYPES_H_
#define RK3566_FACE_REC_FACE_TYPES_H_

#include <string>
#include <vector>

struct FaceBox {
    int x;
    int y;
    int width;
    int height;
    float score;
    float landmarks[10];

    FaceBox()
        : x(0),
          y(0),
          width(0),
          height(0),
          score(0.0f) {
        for (int i = 0; i < 10; ++i) {
            landmarks[i] = -1.0f;
        }
    }
};

struct FaceRecord {
    std::string name;
    std::vector<float> feature;
};

struct RecognitionResult {
    std::string name;
    std::string best_match_name;
    float similarity;
    bool is_unknown;
};

#endif  // RK3566_FACE_REC_FACE_TYPES_H_

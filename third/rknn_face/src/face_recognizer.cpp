#include "face_recognizer.h"

#include <cmath>

namespace {

float dot_product(const std::vector<float> &lhs, const std::vector<float> &rhs) {
    if (lhs.size() != rhs.size()) {
        return -1.0f;
    }

    float result = 0.0f;
    for (size_t i = 0; i < lhs.size(); ++i) {
        result += lhs[i] * rhs[i];
    }
    return result;
}

}  // namespace

FaceRecognizer::FaceRecognizer()
    : threshold_(0.60f) {
}

FaceRecognizer::FaceRecognizer(const std::vector<FaceRecord> &records, float threshold)
    : records_(records),
      threshold_(threshold) {
}

void FaceRecognizer::reset(const std::vector<FaceRecord> &records, float threshold) {
    records_ = records;
    threshold_ = threshold;
}

RecognitionResult FaceRecognizer::recognize(const std::vector<float> &feature) const {
    RecognitionResult result;
    result.name = "unknown";
    result.best_match_name = "unknown";
    result.similarity = 0.0f;
    result.is_unknown = true;

    std::vector<float> normalized = feature;
    if (!normalize_feature(&normalized) || records_.empty()) {
        return result;
    }

    float best_similarity = -1.0f;
    const FaceRecord *best_record = NULL;
    for (size_t i = 0; i < records_.size(); ++i) {
        if (records_[i].feature.size() != normalized.size()) {
            continue;
        }
        const float similarity = dot_product(normalized, records_[i].feature);
        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_record = &records_[i];
        }
    }

    if (best_record == NULL) {
        return result;
    }

    result.similarity = best_similarity;
    result.best_match_name = best_record->name;
    if (best_similarity >= threshold_) {
        result.name = best_record->name;
        result.is_unknown = false;
    }

    return result;
}

bool FaceRecognizer::normalize_feature(std::vector<float> *feature) {
    if (feature == NULL || feature->empty()) {
        return false;
    }

    float squared_sum = 0.0f;
    for (size_t i = 0; i < feature->size(); ++i) {
        squared_sum += (*feature)[i] * (*feature)[i];
    }
    if (squared_sum <= 0.0f) {
        return false;
    }

    const float norm = std::sqrt(squared_sum);
    for (size_t i = 0; i < feature->size(); ++i) {
        (*feature)[i] /= norm;
    }

    return true;
}

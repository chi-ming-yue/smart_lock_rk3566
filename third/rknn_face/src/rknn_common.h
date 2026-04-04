#ifndef RK3566_FACE_REC_RKNN_COMMON_H_
#define RK3566_FACE_REC_RKNN_COMMON_H_

#include <string>
#include <vector>

#include "rknn_api.h"

bool load_model_file(const std::string &path, std::vector<unsigned char> *bytes, std::string *error_message);
std::string tensor_attr_to_string(const rknn_tensor_attr &attr);

#endif  // RK3566_FACE_REC_RKNN_COMMON_H_

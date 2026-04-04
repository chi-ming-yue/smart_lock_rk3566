#ifndef RK3566_FACE_REC_FACE_DATABASE_H_
#define RK3566_FACE_REC_FACE_DATABASE_H_

#include <string>
#include <vector>

#include "face_types.h"

class FaceDatabase {
public:
    FaceDatabase();

    bool load(const std::string &db_path, std::string *error_message);
    bool initialize(const std::string &db_path, std::string *error_message);
    bool clear(std::string *error_message);
    bool upsert_record(const FaceRecord &record, std::string *error_message);

    const std::vector<FaceRecord> &records() const;
    bool empty() const;
    size_t feature_size() const;

private:
    std::vector<FaceRecord> records_;
    size_t feature_size_;
    std::string db_path_;
};

#endif  // RK3566_FACE_REC_FACE_DATABASE_H_

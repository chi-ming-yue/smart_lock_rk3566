#include "face_database.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>

#include "sqlite3.h"

namespace {

std::string trim(const std::string &value) {
    const std::string whitespace = " \t\r\n";
    const std::string::size_type begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return "";
    }
    const std::string::size_type end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

bool parse_feature_text(const std::string &text, std::vector<float> *feature) {
    if (feature == NULL) {
        return false;
    }

    feature->clear();
    std::string cleaned = trim(text);
    if (cleaned.size() < 2 || cleaned.front() != '[' || cleaned.back() != ']') {
        return false;
    }
    cleaned = cleaned.substr(1, cleaned.size() - 2);
    std::stringstream stream(cleaned);
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        char *end = NULL;
        const float value = std::strtof(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0') {
            return false;
        }
        feature->push_back(value);
    }
    return !feature->empty();
}

bool normalize_feature(std::vector<float> *feature) {
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

std::string feature_to_json(const std::vector<float> &feature) {
    std::ostringstream stream;
    stream << "[";
    for (size_t i = 0; i < feature.size(); ++i) {
        if (i != 0) {
            stream << ",";
        }
        stream << feature[i];
    }
    stream << "]";
    return stream.str();
}

bool ensure_schema(sqlite3 *db, std::string *error_message) {
    static const char *kSql =
        "CREATE TABLE IF NOT EXISTS persons ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL,"
        "feature TEXT NOT NULL)";
    char *errmsg = NULL;
    const int ret = sqlite3_exec(db, kSql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        if (error_message != NULL) {
            *error_message = errmsg != NULL ? errmsg : "failed to create schema";
        }
        if (errmsg != NULL) {
            sqlite3_free(errmsg);
        }
        return false;
    }
    return true;
}

}  // namespace

FaceDatabase::FaceDatabase()
    : feature_size_(0) {
}

bool FaceDatabase::load(const std::string &db_path, std::string *error_message) {
    if (error_message == NULL) {
        return false;
    }

    *error_message = "";
    records_.clear();
    feature_size_ = 0;
    db_path_ = db_path;

    sqlite3 *db = NULL;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        *error_message = "failed to open database: " + db_path;
        if (db != NULL) {
            sqlite3_close(db);
        }
        return false;
    }

    static const char *kSql = "SELECT name, feature FROM persons ORDER BY id";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, kSql, -1, &stmt, NULL) != SQLITE_OK) {
        *error_message = "failed to prepare persons query";
        sqlite3_close(db);
        return false;
    }

    bool ok = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name_text = sqlite3_column_text(stmt, 0);
        const unsigned char *feature_text = sqlite3_column_text(stmt, 1);
        if (name_text == NULL || feature_text == NULL) {
            *error_message = "database row contains null field";
            ok = false;
            break;
        }

        FaceRecord record;
        record.name = reinterpret_cast<const char *>(name_text);
        if (!parse_feature_text(reinterpret_cast<const char *>(feature_text), &record.feature)) {
            *error_message = "failed to parse feature vector for person: " + record.name;
            ok = false;
            break;
        }
        if (!normalize_feature(&record.feature)) {
            *error_message = "failed to normalize feature vector for person: " + record.name;
            ok = false;
            break;
        }

        if (feature_size_ == 0) {
            feature_size_ = record.feature.size();
        } else if (feature_size_ != record.feature.size()) {
            *error_message = "inconsistent feature vector length in database";
            ok = false;
            break;
        }

        records_.push_back(record);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (!ok) {
        records_.clear();
        feature_size_ = 0;
        return false;
    }
    if (records_.empty()) {
        *error_message = "database does not contain any persons";
        return false;
    }

    return true;
}

bool FaceDatabase::initialize(const std::string &db_path, std::string *error_message) {
    if (error_message == NULL) {
        return false;
    }
    *error_message = "";
    sqlite3 *db = NULL;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        *error_message = "failed to open database: " + db_path;
        if (db != NULL) {
            sqlite3_close(db);
        }
        return false;
    }
    const bool ok = ensure_schema(db, error_message);
    sqlite3_close(db);
    if (ok) {
        db_path_ = db_path;
        records_.clear();
        feature_size_ = 0;
    }
    return ok;
}

bool FaceDatabase::clear(std::string *error_message) {
    if (error_message == NULL) {
        return false;
    }
    *error_message = "";
    if (db_path_.empty()) {
        *error_message = "database is not initialized";
        return false;
    }
    sqlite3 *db = NULL;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
        *error_message = "failed to open database: " + db_path_;
        if (db != NULL) {
            sqlite3_close(db);
        }
        return false;
    }
    char *errmsg = NULL;
    const int ret = sqlite3_exec(db, "DELETE FROM persons", NULL, NULL, &errmsg);
    sqlite3_close(db);
    if (ret != SQLITE_OK) {
        *error_message = errmsg != NULL ? errmsg : "failed to clear persons table";
        if (errmsg != NULL) {
            sqlite3_free(errmsg);
        }
        return false;
    }
    records_.clear();
    feature_size_ = 0;
    return true;
}

bool FaceDatabase::upsert_record(const FaceRecord &record, std::string *error_message) {
    if (error_message == NULL) {
        return false;
    }
    *error_message = "";
    if (record.name.empty() || record.feature.empty()) {
        *error_message = "invalid record";
        return false;
    }
    if (db_path_.empty()) {
        *error_message = "database is not initialized";
        return false;
    }

    std::vector<float> normalized = record.feature;
    if (!normalize_feature(&normalized)) {
        *error_message = "failed to normalize feature for " + record.name;
        return false;
    }

    sqlite3 *db = NULL;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
        *error_message = "failed to open database: " + db_path_;
        if (db != NULL) {
            sqlite3_close(db);
        }
        return false;
    }
    if (!ensure_schema(db, error_message)) {
        sqlite3_close(db);
        return false;
    }

    const std::string feature_json = feature_to_json(normalized);
    sqlite3_stmt *stmt = NULL;

    static const char *kUpdateSql = "UPDATE persons SET feature = ? WHERE name = ?";
    if (sqlite3_prepare_v2(db, kUpdateSql, -1, &stmt, NULL) != SQLITE_OK) {
        *error_message = "failed to prepare update statement";
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, feature_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.name.c_str(), -1, SQLITE_TRANSIENT);
    int step_ret = sqlite3_step(stmt);
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    stmt = NULL;
    if (step_ret != SQLITE_DONE) {
        *error_message = "failed to update record for " + record.name;
        sqlite3_close(db);
        return false;
    }

    if (changed == 0) {
        static const char *kInsertSql = "INSERT INTO persons(name, feature) VALUES(?, ?)";
        if (sqlite3_prepare_v2(db, kInsertSql, -1, &stmt, NULL) != SQLITE_OK) {
            *error_message = "failed to prepare insert statement";
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_text(stmt, 1, record.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, feature_json.c_str(), -1, SQLITE_TRANSIENT);
        step_ret = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (step_ret != SQLITE_DONE) {
            *error_message = "failed to insert record for " + record.name;
            sqlite3_close(db);
            return false;
        }
    }
    sqlite3_close(db);

    bool updated = false;
    for (size_t i = 0; i < records_.size(); ++i) {
        if (records_[i].name == record.name) {
            records_[i].feature = normalized;
            updated = true;
            break;
        }
    }
    if (!updated) {
        FaceRecord stored = record;
        stored.feature = normalized;
        records_.push_back(stored);
    }
    if (feature_size_ == 0) {
        feature_size_ = normalized.size();
    }
    return true;
}

const std::vector<FaceRecord> &FaceDatabase::records() const {
    return records_;
}

bool FaceDatabase::empty() const {
    return records_.empty();
}

size_t FaceDatabase::feature_size() const {
    return feature_size_;
}

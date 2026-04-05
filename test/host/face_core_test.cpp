#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "sqlite3.h"

#include "face_database.h"
#include "face_recognizer.h"
#include "pipe/face_vote.h"

namespace {

struct TempPath {
    std::string path;

    explicit TempPath(const std::string& prefix)
    {
        std::string pattern = "/tmp/" + prefix + "_XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        const int fd = mkstemp(&buffer[0]);
        if (fd >= 0) {
            close(fd);
            std::remove(&buffer[0]);
            path = &buffer[0];
        }
    }

    ~TempPath()
    {
        if (!path.empty()) {
            std::remove(path.c_str());
            std::remove((path + ".hnsw").c_str());
            std::remove((path + ".hnsw.meta").c_str());
        }
    }
};

bool ExecSql(sqlite3* db, const std::string& sql, std::string* error)
{
    char* errmsg = NULL;
    const int ret = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errmsg);
    if (ret == SQLITE_OK) {
        return true;
    }
    if (error != NULL) {
        *error = errmsg != NULL ? errmsg : "sqlite exec failed";
    }
    if (errmsg != NULL) {
        sqlite3_free(errmsg);
    }
    return false;
}

std::string FeatureJson(float a, float b, float c)
{
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "[%.6f,%.6f,%.6f]", a, b, c);
    return buffer;
}

bool CreateValidDb(const std::string& path, std::string* error)
{
    sqlite3* db = NULL;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        *error = "open failed";
        return false;
    }

    const char* schema =
        "CREATE TABLE persons ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE NOT NULL);"
        "CREATE TABLE person_embeddings ("
        "id INTEGER PRIMARY KEY,"
        "person_id INTEGER NOT NULL,"
        "slot_index INTEGER NOT NULL,"
        "feature TEXT NOT NULL,"
        "UNIQUE(person_id, slot_index),"
        "FOREIGN KEY(person_id) REFERENCES persons(id) ON DELETE CASCADE);";
    if (!ExecSql(db, schema, error)) {
        sqlite3_close(db);
        return false;
    }

    if (!ExecSql(db, "INSERT INTO persons(id, name) VALUES (1, 'alice'), (2, 'bob');", error)) {
        sqlite3_close(db);
        return false;
    }

    for (int slot = 0; slot < 20; ++slot) {
        const float base = 1.0f + static_cast<float>(slot) * 0.01f;
        std::string sql = "INSERT INTO person_embeddings(person_id, slot_index, feature) VALUES";
        sql += "(1," + std::to_string(slot) + ",'" + FeatureJson(base, 0.1f, 0.0f) + "'),";
        sql += "(2," + std::to_string(slot) + ",'" + FeatureJson(0.0f, base, 0.1f) + "');";
        if (!ExecSql(db, sql, error)) {
            sqlite3_close(db);
            return false;
        }
    }

    sqlite3_close(db);
    return true;
}

bool TestRejectsOldSchema()
{
    TempPath db_path("face_old_schema");
    std::string error;
    sqlite3* db = NULL;
    if (sqlite3_open(db_path.path.c_str(), &db) != SQLITE_OK) {
        return false;
    }
    const bool ok = ExecSql(db,
                            "CREATE TABLE persons (id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL, feature TEXT NOT NULL);",
                            &error);
    sqlite3_close(db);
    if (!ok) {
        std::cerr << error << std::endl;
        return false;
    }

    FaceDatabase database;
    std::string load_error;
    if (database.load(db_path.path, &load_error)) {
        std::cerr << "expected old schema to fail" << std::endl;
        return false;
    }
    return load_error.find("schema") != std::string::npos;
}

bool TestRequiresExact20Embeddings()
{
    TempPath db_path("face_invalid_count");
    std::string error;
    if (!CreateValidDb(db_path.path, &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    sqlite3* db = NULL;
    if (sqlite3_open(db_path.path.c_str(), &db) != SQLITE_OK) {
        return false;
    }
    const bool ok = ExecSql(db, "DELETE FROM person_embeddings WHERE person_id = 2 AND slot_index = 19;", &error);
    sqlite3_close(db);
    if (!ok) {
        std::cerr << error << std::endl;
        return false;
    }

    FaceDatabase database;
    std::string load_error;
    if (database.load(db_path.path, &load_error)) {
        std::cerr << "expected invalid embedding count to fail" << std::endl;
        return false;
    }
    return load_error.find("20") != std::string::npos;
}

bool TestBuildsAndLoadsIndexCache()
{
    TempPath db_path("face_cache");
    std::string error;
    if (!CreateValidDb(db_path.path, &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    FaceDatabase database;
    if (!database.load(db_path.path, &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    FaceRecognizer recognizer;
    if (!recognizer.reset(database.embeddings(),
                          database.dataset_digest(),
                          db_path.path + ".hnsw",
                          db_path.path + ".hnsw.meta",
                          0.60f,
                          &error)) {
        std::cerr << error << std::endl;
        return false;
    }
    if (recognizer.used_cached_index()) {
        std::cerr << "first build should not use cache" << std::endl;
        return false;
    }

    FaceRecognizer cached_recognizer;
    if (!cached_recognizer.reset(database.embeddings(),
                                 database.dataset_digest(),
                                 db_path.path + ".hnsw",
                                 db_path.path + ".hnsw.meta",
                                 0.60f,
                                 &error)) {
        std::cerr << error << std::endl;
        return false;
    }
    if (!cached_recognizer.used_cached_index()) {
        std::cerr << "expected second reset to use cache" << std::endl;
        return false;
    }

    std::vector<float> query;
    query.push_back(1.0f);
    query.push_back(0.1f);
    query.push_back(0.0f);
    const RecognitionResult result = cached_recognizer.recognize(query);
    return !result.is_unknown && result.name == "alice";
}

bool TestThreeRoundsTwoWins()
{
    TemporalVoteConfig config;
    config.window_size = 3;
    config.minimum_votes = 2;

    std::deque<RecognitionResult> history;
    RecognitionResult one;
    one.name = "alice";
    one.best_match_name = "alice";
    one.similarity = 0.83f;
    one.is_unknown = false;

    RecognitionResult two;
    two.name = "unknown";
    two.best_match_name = "alice";
    two.similarity = 0.10f;
    two.is_unknown = true;

    RecognitionResult three = one;
    const RecognitionResult first = ApplyTemporalVote(one, &history, config, 0.60f);
    const RecognitionResult second = ApplyTemporalVote(two, &history, config, 0.60f);
    const RecognitionResult third = ApplyTemporalVote(three, &history, config, 0.60f);

    return first.is_unknown && second.is_unknown && !third.is_unknown && third.name == "alice";
}

}  // namespace

int main()
{
    const struct {
        const char* name;
        bool (*fn)();
    } tests[] = {
        {"rejects_old_schema", TestRejectsOldSchema},
        {"requires_exact_20_embeddings", TestRequiresExact20Embeddings},
        {"builds_and_loads_index_cache", TestBuildsAndLoadsIndexCache},
        {"three_rounds_two_wins", TestThreeRoundsTwoWins},
    };

    int failed = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const bool ok = tests[i].fn();
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << tests[i].name << std::endl;
        if (!ok) {
            ++failed;
        }
    }
    return failed == 0 ? 0 : 1;
}

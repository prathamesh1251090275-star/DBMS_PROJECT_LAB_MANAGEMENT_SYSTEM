#include "Db.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

static std::string g_dbPath;

void Db::setPath(const std::string& path) { g_dbPath = path; }

Db::Db() {
    int rc = sqlite3_open_v2(g_dbPath.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "cannot allocate connection";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DbError("open failed: " + msg, rc);
    }
    sqlite3_extended_result_codes(db_, 1);
    sqlite3_busy_timeout(db_, 5000);  // wait up to 5 s if another request holds the write lock
    char* err = nullptr;
    // Foreign keys are OFF by default in SQLite and must be enabled on EVERY connection.
    if (sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "pragma failed";
        sqlite3_free(err);
        sqlite3_close(db_);
        throw DbError(msg, SQLITE_ERROR);
    }
}

Db::~Db() {
    if (db_) sqlite3_close(db_);
}

// Small helper that makes sure a prepared statement is always finalized.
namespace {
struct Stmt {
    sqlite3_stmt* st = nullptr;
    ~Stmt() { sqlite3_finalize(st); }
};

DbError lastError(sqlite3* db, const std::string& what) {
    return DbError(what + ": " + sqlite3_errmsg(db), sqlite3_extended_errcode(db));
}

void bindAll(sqlite3* db, sqlite3_stmt* st, const Params& params) {
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i) + 1;  // SQLite parameters start at 1
        int rc = SQLITE_OK;
        const Param& p = params[i];
        if (std::holds_alternative<std::nullptr_t>(p)) {
            rc = sqlite3_bind_null(st, idx);
        } else if (std::holds_alternative<long long>(p)) {
            rc = sqlite3_bind_int64(st, idx, std::get<long long>(p));
        } else if (std::holds_alternative<double>(p)) {
            rc = sqlite3_bind_double(st, idx, std::get<double>(p));
        } else {
            const std::string& s = std::get<std::string>(p);
            rc = sqlite3_bind_text(st, idx, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
        }
        if (rc != SQLITE_OK) throw lastError(db, "bind failed");
    }
}
}  // namespace

Json::Value Db::query(const std::string& sql, const Params& params) {
    Stmt s;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s.st, nullptr) != SQLITE_OK) throw lastError(db_, "prepare failed");
    bindAll(db_, s.st, params);

    Json::Value rows(Json::arrayValue);
    int rc;
    while ((rc = sqlite3_step(s.st)) == SQLITE_ROW) {
        Json::Value row(Json::objectValue);
        int cols = sqlite3_column_count(s.st);
        for (int c = 0; c < cols; ++c) {
            const char* name = sqlite3_column_name(s.st, c);
            switch (sqlite3_column_type(s.st, c)) {
                case SQLITE_INTEGER: row[name] = static_cast<Json::Int64>(sqlite3_column_int64(s.st, c)); break;
                case SQLITE_FLOAT:   row[name] = sqlite3_column_double(s.st, c); break;
                case SQLITE_TEXT:    row[name] = reinterpret_cast<const char*>(sqlite3_column_text(s.st, c)); break;
                default:             row[name] = Json::nullValue; break;
            }
        }
        rows.append(row);
    }
    if (rc != SQLITE_DONE) throw lastError(db_, "step failed");
    return rows;
}

int Db::execute(const std::string& sql, const Params& params) {
    Stmt s;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s.st, nullptr) != SQLITE_OK) throw lastError(db_, "prepare failed");
    bindAll(db_, s.st, params);
    int rc = sqlite3_step(s.st);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) throw lastError(db_, "step failed");
    return sqlite3_changes(db_);
}

long long Db::lastInsertId() { return sqlite3_last_insert_rowid(db_); }

void Db::runScript(const std::string& sqlText) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sqlText.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "script failed";
        sqlite3_free(err);
        throw DbError(msg, rc);
    }
}

void Db::begin() { execute("BEGIN IMMEDIATE"); }
void Db::commit() { execute("COMMIT"); }
void Db::rollback() { execute("ROLLBACK"); }

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void Db::initialize(const std::string& schemaFile, const std::string& sampleFile, bool loadSample) {
    namespace fs = std::filesystem;
    fs::path parent = fs::path(g_dbPath).parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    Db db;
    // WAL mode lets readers and one writer work at the same time (setting is stored in the file).
    db.query("PRAGMA journal_mode = WAL");

    bool isNew = db.query("SELECT name FROM sqlite_master WHERE type='table' AND name='Users'").empty();
    db.runScript(readFile(schemaFile));  // all CREATE ... IF NOT EXISTS, so always safe
    if (isNew && loadSample) {
        db.runScript(readFile(sampleFile));
        std::cout << "Database created with sample data: " << g_dbPath << std::endl;
    } else if (isNew) {
        std::cout << "Empty database created: " << g_dbPath << std::endl;
    }

    // Sanity check: foreign keys must really be enforced on our connections.
    auto fk = db.query("PRAGMA foreign_keys");
    if (fk.empty() || fk[0]["foreign_keys"].asInt() != 1) throw std::runtime_error("foreign_keys pragma is not ON");
}

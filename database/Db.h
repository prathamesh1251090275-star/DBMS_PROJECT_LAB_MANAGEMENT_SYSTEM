// Db.h - a very small wrapper around the SQLite C API.
// One Db object = one database connection. Create one per HTTP request
// (opening SQLite is cheap) and it closes itself when it goes out of scope.
#pragma once
#include <sqlite3.h>
#include <json/json.h>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// A bound parameter can be NULL, an integer, a decimal or text.
using Param = std::variant<std::nullptr_t, long long, double, std::string>;
using Params = std::vector<Param>;

// Thrown when SQLite reports an error. "what()" is for the server log only.
struct DbError : std::runtime_error {
    int code;  // SQLite extended result code
    DbError(const std::string& msg, int c) : std::runtime_error(msg), code(c) {}
    bool isBusy() const { return (code & 0xff) == SQLITE_BUSY || (code & 0xff) == SQLITE_LOCKED; }
    bool isConstraint() const { return (code & 0xff) == SQLITE_CONSTRAINT; }
};

class Db {
public:
    // Where the .db file lives (set once at startup from the config file).
    static void setPath(const std::string& path);
    // Creates the file + tables the first time. Never touches existing data.
    static void initialize(const std::string& schemaFile, const std::string& sampleFile, bool loadSample);

    Db();   // opens the connection, sets busy timeout and foreign_keys = ON
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    // SELECT: returns a JSON array of row objects {column: value}
    Json::Value query(const std::string& sql, const Params& params = {});
    // INSERT / UPDATE / DELETE: returns the number of rows changed
    int execute(const std::string& sql, const Params& params = {});
    long long lastInsertId();
    // Run a trusted multi-statement script (used only for schema.sql / sample_data.sql)
    void runScript(const std::string& sqlText);

    void begin();     // BEGIN IMMEDIATE  (takes the write lock right away)
    void commit();
    void rollback();

private:
    sqlite3* db_ = nullptr;
};

// RAII transaction: if commit() is not called, the destructor rolls back.
class Transaction {
public:
    explicit Transaction(Db& db) : db_(db) { db_.begin(); }
    ~Transaction() {
        if (!done_) {
            try { db_.rollback(); } catch (...) {}
        }
    }
    void commit() { db_.commit(); done_ = true; }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

private:
    Db& db_;
    bool done_ = false;
};

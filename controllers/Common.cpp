#include "Common.h"

#include <algorithm>
#include <cctype>
#include <cmath>

using namespace drogon;

Reply errorReply(int status, const std::string& message) {
    Reply r;
    r.status = status;
    r.body["error"] = message;
    return r;
}

Reply messageReply(int status, const std::string& message) {
    Reply r;
    r.status = status;
    r.body["message"] = message;
    return r;
}

// Turn a database error into a safe message. The real error goes to the server log only.
static Reply fromDbError(const DbError& e) {
    LOG_ERROR << "Database error: " << e.what();
    if (e.isBusy()) return errorReply(503, "The database is busy. Please try again.");
    if (e.isConstraint()) {
        switch (e.code) {
            case SQLITE_CONSTRAINT_UNIQUE:
            case SQLITE_CONSTRAINT_PRIMARYKEY:
                return errorReply(409, "A record with the same unique value already exists.");
            case SQLITE_CONSTRAINT_FOREIGNKEY:
                return errorReply(409, "A referenced record does not exist, or this record is still in use.");
            default:
                return errorReply(409, "The data violates a database rule (check the values).");
        }
    }
    return errorReply(500, "Internal server error.");
}

void run(const HttpRequestPtr& req, Callback&& cb, const std::vector<std::string>& roles, Handler fn) {
    Reply reply;
    try {
        auto session = req->session();
        if (!session || !session->find("user_id")) throw ApiError{401, "Please log in."};
        long long uid = session->get<long long>("user_id");

        Db db;
        // Re-check the user on every request, so a deactivated user or changed role takes effect immediately.
        auto rows = db.query("SELECT user_id, full_name, role, is_active FROM Users WHERE user_id = ?", {uid});
        if (rows.empty() || rows[0]["is_active"].asInt() != 1) {
            session->clear();
            throw ApiError{401, "Session expired. Please log in again."};
        }
        CurrentUser user{uid, rows[0]["role"].asString(), rows[0]["full_name"].asString()};

        if (!roles.empty() && std::find(roles.begin(), roles.end(), user.role) == roles.end())
            throw ApiError{403, "You do not have permission to perform this action."};

        reply = fn(db, req, user);
    } catch (const ApiError& e) {
        reply = errorReply(e.status, e.message);
    } catch (const DbError& e) {
        reply = fromDbError(e);
    } catch (const std::exception& e) {
        LOG_ERROR << "Unexpected error: " << e.what();
        reply = errorReply(500, "Internal server error.");
    }
    auto resp = HttpResponse::newHttpJsonResponse(reply.body);
    resp->setStatusCode(static_cast<HttpStatusCode>(reply.status));
    cb(resp);
}

// ---------------- validation helpers ----------------

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

const Json::Value& jsonBody(const HttpRequestPtr& req) {
    auto j = req->getJsonObject();
    if (!j || !j->isObject()) throw ApiError{400, "Request body must be a JSON object."};
    return *j;
}

std::string optString(const Json::Value& j, const char* key, size_t maxLen) {
    if (!j.isMember(key) || j[key].isNull()) return "";
    if (!j[key].isString()) throw ApiError{400, std::string(key) + " must be text."};
    std::string v = trim(j[key].asString());
    if (v.size() > maxLen) throw ApiError{400, std::string(key) + " is too long."};
    return v;
}

std::string reqString(const Json::Value& j, const char* key, size_t maxLen) {
    std::string v = optString(j, key, maxLen);
    if (v.empty()) throw ApiError{400, std::string(key) + " is required."};
    return v;
}

Param optParam(const Json::Value& j, const char* key, size_t maxLen) {
    std::string v = optString(j, key, maxLen);
    if (v.empty()) return nullptr;
    return v;
}

long long reqInt(const Json::Value& j, const char* key, long long minV, long long maxV) {
    if (!j.isMember(key) || !j[key].isIntegral()) throw ApiError{400, std::string(key) + " must be a whole number."};
    long long v = j[key].asInt64();
    if (v < minV || v > maxV)
        throw ApiError{400, std::string(key) + " must be between " + std::to_string(minV) + " and " + std::to_string(maxV) + "."};
    return v;
}

bool isValidDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && !isdigit(static_cast<unsigned char>(s[i]))) return false;
    int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
    if (y < 1900 || y > 2200 || m < 1 || m > 12 || d < 1) return false;
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxDay = days[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) maxDay = 29;
    return d <= maxDay;
}

std::string reqDate(const Json::Value& j, const char* key) {
    std::string v = reqString(j, key, 10);
    if (!isValidDate(v)) throw ApiError{400, std::string(key) + " must be a valid date (YYYY-MM-DD)."};
    return v;
}

Param optDate(const Json::Value& j, const char* key) {
    std::string v = optString(j, key, 10);
    if (v.empty()) return nullptr;
    if (!isValidDate(v)) throw ApiError{400, std::string(key) + " must be a valid date (YYYY-MM-DD)."};
    return v;
}

long long moneyToPaise(const Json::Value& j, const char* key) {
    if (!j.isMember(key) || j[key].isNull()) return 0;
    if (!j[key].isNumeric()) throw ApiError{400, std::string(key) + " must be a number."};
    double v = j[key].asDouble();
    if (v < 0 || v > 10000000) throw ApiError{400, std::string(key) + " must be between 0 and 10000000."};
    return std::llround(v * 100.0);  // stored as whole paise to avoid floating-point errors
}

std::string todayString(Db& db) {
    return db.query("SELECT date('now','localtime') AS d")[0]["d"].asString();
}

long long optQueryInt(const HttpRequestPtr& req, const char* name) {
    std::string v = req->getParameter(name);
    if (v.empty()) return 0;
    if (v.size() > 12) throw ApiError{400, std::string(name) + " must be a number."};
    for (char c : v)
        if (!isdigit(static_cast<unsigned char>(c))) throw ApiError{400, std::string(name) + " must be a number."};
    return std::stoll(v);
}

void requireId(long long id) {
    if (id <= 0) throw ApiError{404, "Record not found."};
}

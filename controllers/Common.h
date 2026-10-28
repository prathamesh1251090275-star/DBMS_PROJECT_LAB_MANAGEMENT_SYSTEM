// Common.h - helpers shared by all controllers:
//   * run()      : checks login + role, opens the database, runs the handler, converts errors to JSON
//   * req...()   : read and validate fields from the JSON request body
#pragma once
#include <drogon/drogon.h>

#include <functional>
#include <string>
#include <vector>

#include "../database/Db.h"

// Thrown by business rules / validation. The message is safe to show to the user.
struct ApiError {
    int status;
    std::string message;
};

struct CurrentUser {
    long long id;
    std::string role;  // Admin, LabAssistant, Student, Faculty
    std::string name;
};

struct Reply {
    int status = 200;
    Json::Value body;
};

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;
using Handler = std::function<Reply(Db&, const drogon::HttpRequestPtr&, const CurrentUser&)>;

// roles = list of roles allowed to call the endpoint (empty list = any logged-in user).
// This is where permissions are ENFORCED on the server.
void run(const drogon::HttpRequestPtr& req, Callback&& cb, const std::vector<std::string>& roles, Handler fn);

// Build {"error": "..."} replies / simple success replies
Reply errorReply(int status, const std::string& message);
Reply messageReply(int status, const std::string& message);

// ---- request body helpers (throw ApiError 400 when the input is invalid) ----
const Json::Value& jsonBody(const drogon::HttpRequestPtr& req);
std::string reqString(const Json::Value& j, const char* key, size_t maxLen);          // required, non-empty
std::string optString(const Json::Value& j, const char* key, size_t maxLen);          // optional, "" if absent
Param optParam(const Json::Value& j, const char* key, size_t maxLen);                 // NULL if absent/empty
long long reqInt(const Json::Value& j, const char* key, long long minV, long long maxV);
std::string reqDate(const Json::Value& j, const char* key);                           // YYYY-MM-DD, required
Param optDate(const Json::Value& j, const char* key);                                 // NULL if absent/empty
long long moneyToPaise(const Json::Value& j, const char* key);                        // rupees -> paise
bool isValidDate(const std::string& s);
std::string todayString(Db& db);                                                      // today's date from SQLite

// Optional integer query-string parameter (?lab_id=2). Returns 0 when absent; 400 if not a number.
long long optQueryInt(const drogon::HttpRequestPtr& req, const char* name);
// Path ids ({1}) must be positive numbers; anything else -> 404.
void requireId(long long id);

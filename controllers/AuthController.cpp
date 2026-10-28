// Login / logout / "who am I". Sessions are kept on the server; the browser only holds a random session id cookie.
#include <ctime>
#include <map>
#include <mutex>

#include "../services/Password.h"
#include "Common.h"
#include "Routes.h"

using namespace drogon;

// --- very small brute-force protection: 5 wrong passwords -> 5 minute lock for that username ---
namespace {
struct Attempt {
    int fails = 0;
    std::time_t lockedUntil = 0;
};
std::mutex g_mutex;
std::map<std::string, Attempt> g_attempts;
const int kMaxFails = 5;
const int kLockSeconds = 300;

bool isLocked(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_attempts.find(name);
    return it != g_attempts.end() && it->second.lockedUntil > std::time(nullptr);
}
void recordFailure(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Attempt& a = g_attempts[name];
    if (++a.fails >= kMaxFails) {
        a.lockedUntil = std::time(nullptr) + kLockSeconds;
        a.fails = 0;
    }
}
void clearFailures(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_attempts.erase(name);
}
std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}
}  // namespace

static Reply doLogin(const HttpRequestPtr& req) {
    const Json::Value& j = jsonBody(req);
    std::string username = lower(reqString(j, "username", 50));
    std::string password = reqString(j, "password", 128);

    if (isLocked(username)) return errorReply(429, "Too many failed attempts. Try again in a few minutes.");

    Db db;
    auto rows = db.query(
        "SELECT user_id, full_name, role, password_hash, is_active FROM Users WHERE lower(username) = ?", {username});

    // Always run one hash verification, so response time does not reveal whether the username exists.
    static const std::string dummyHash = hashPassword("not-a-real-password");
    bool ok = false;
    if (!rows.empty() && !rows[0]["password_hash"].isNull() && rows[0]["is_active"].asInt() == 1)
        ok = verifyPassword(rows[0]["password_hash"].asString(), password);
    else
        verifyPassword(dummyHash, password);

    if (!ok) {
        recordFailure(username);
        return errorReply(401, "Invalid username or password.");  // same message for every failure reason
    }
    clearFailures(username);

    auto session = req->session();
    session->clear();
    session->insert("user_id", static_cast<long long>(rows[0]["user_id"].asInt64()));  // type must match get<long long> in run()

    Reply r;
    r.body["user_id"] = rows[0]["user_id"];
    r.body["full_name"] = rows[0]["full_name"];
    r.body["role"] = rows[0]["role"];
    return r;
}

void registerAuthRoutes() {
    app().registerHandler(
        "/api/auth/login",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            Reply reply;
            try {
                reply = doLogin(req);
            } catch (const ApiError& e) {
                reply = errorReply(e.status, e.message);
            } catch (const std::exception& e) {
                LOG_ERROR << "Login error: " << e.what();
                reply = errorReply(500, "Internal server error.");
            }
            auto resp = HttpResponse::newHttpJsonResponse(reply.body);
            resp->setStatusCode(static_cast<HttpStatusCode>(reply.status));
            cb(resp);
        },
        {Post});

    app().registerHandler(
        "/api/auth/logout",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            if (req->session()) req->session()->clear();
            cb(HttpResponse::newHttpJsonResponse(messageReply(200, "Logged out.").body));
        },
        {Post});

    app().registerHandler(
        "/api/auth/me",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            run(req, std::move(cb), {}, [](Db&, const HttpRequestPtr&, const CurrentUser& u) {
                Reply r;
                r.body["user_id"] = static_cast<Json::Int64>(u.id);
                r.body["full_name"] = u.name;
                r.body["role"] = u.role;
                return r;
            });
        },
        {Get});
}

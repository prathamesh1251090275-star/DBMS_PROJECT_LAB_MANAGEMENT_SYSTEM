// Labs, Categories (simple lookup tables) and Users.
#include <regex>

#include "../services/Password.h"
#include "Common.h"
#include "Routes.h"

using namespace drogon;
using Cb = std::function<void(const HttpResponsePtr&)>;
static const std::vector<std::string> kAdmin = {"Admin"};
static const std::vector<std::string> kStaff = {"Admin", "LabAssistant"};

// ---------- Labs and Categories: same pattern, so one small helper ----------
struct LookupSpec {
    std::string path;      // "/api/labs"
    std::string table;     // "Labs"
    std::string idCol;     // "lab_id"
    std::string nameCol;   // "lab_name"
    std::string extraCol;  // "location" (Labs) - empty for Categories
    std::string label;     // used in messages
};

static void registerLookup(const LookupSpec& s) {
    // All strings in `s` are constants from this file, never user input.
    app().registerHandler(
        s.path,
        [s](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), {}, [s](Db& db, const HttpRequestPtr&, const CurrentUser&) {
                Reply r;
                r.body = db.query("SELECT * FROM " + s.table + " ORDER BY " + s.nameCol);
                return r;
            });
        },
        {Get});

    app().registerHandler(
        s.path,
        [s](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kAdmin, [s](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                const Json::Value& j = jsonBody(req);
                std::string name = reqString(j, s.nameCol.c_str(), 100);
                Param desc = optParam(j, "description", 500);
                Params p{name};
                std::string sql = "INSERT INTO " + s.table + " (" + s.nameCol;
                std::string vals = "?";
                if (!s.extraCol.empty()) { sql += ", " + s.extraCol; vals += ", ?"; p.push_back(optParam(j, s.extraCol.c_str(), 150)); }
                sql += ", description) VALUES (" + vals + ", ?)";
                p.push_back(desc);
                db.execute(sql, p);
                Reply r;
                r.status = 201;
                r.body[s.idCol] = static_cast<Json::Int64>(db.lastInsertId());
                r.body["message"] = s.label + " added.";
                return r;
            });
        },
        {Post});

    app().registerHandler(
        s.path + "/{1}",
        [s](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [s, id](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                requireId(id);
                const Json::Value& j = jsonBody(req);
                Params p{reqString(j, s.nameCol.c_str(), 100)};
                std::string sql = "UPDATE " + s.table + " SET " + s.nameCol + " = ?";
                if (!s.extraCol.empty()) { sql += ", " + s.extraCol + " = ?"; p.push_back(optParam(j, s.extraCol.c_str(), 150)); }
                sql += ", description = ? WHERE " + s.idCol + " = ?";
                p.push_back(optParam(j, "description", 500));
                p.push_back(id);
                if (db.execute(sql, p) != 1) throw ApiError{404, s.label + " not found."};
                return messageReply(200, s.label + " updated.");
            });
        },
        {Put});

    app().registerHandler(
        s.path + "/{1}",
        [s](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [s, id](Db& db, const HttpRequestPtr&, const CurrentUser&) {
                requireId(id);
                int n = 0;
                try {
                    n = db.execute("DELETE FROM " + s.table + " WHERE " + s.idCol + " = ?", {id});
                } catch (const DbError& e) {
                    if (e.isConstraint()) throw ApiError{409, "Cannot delete: this " + s.label + " is used by equipment."};
                    throw;
                }
                if (n != 1) throw ApiError{404, s.label + " not found."};
                return messageReply(200, s.label + " deleted.");
            });
        },
        {Delete});
}

// ---------- Users ----------
static void checkRole(const std::string& r) {
    if (r != "Admin" && r != "LabAssistant" && r != "Student" && r != "Faculty")
        throw ApiError{400, "role must be Admin, LabAssistant, Student or Faculty."};
}

static std::string readUsername(const Json::Value& j) {
    std::string u = reqString(j, "username", 50);
    if (!std::regex_match(u, std::regex("^[A-Za-z0-9._-]{3,50}$")))
        throw ApiError{400, "username must be 3-50 characters: letters, digits, dot, dash or underscore."};
    return u;
}

static void registerUsers() {
    app().registerHandler(
        "/api/users",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kStaff, [](Db& db, const HttpRequestPtr&, const CurrentUser& me) {
                Reply r;
                if (me.role == "Admin") {
                    r.body = db.query(
                        "SELECT user_id, full_name, department, role, email, phone, username, is_active, created_at, "
                        "(password_hash IS NOT NULL) AS has_password FROM Users ORDER BY full_name");
                } else {  // lab assistants only need the list of people who can borrow
                    r.body = db.query(
                        "SELECT user_id, full_name, department, role FROM Users "
                        "WHERE role IN ('Student','Faculty') AND is_active = 1 ORDER BY full_name");
                }
                return r;
            });
        },
        {Get});

    app().registerHandler(
        "/api/users",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kAdmin, [](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                const Json::Value& j = jsonBody(req);
                std::string role = reqString(j, "role", 20);
                checkRole(role);
                std::string password = optString(j, "password", 128);
                Param hash = nullptr;
                if (!password.empty()) { checkPasswordRules(password); hash = hashPassword(password); }
                db.execute(
                    "INSERT INTO Users (full_name, department, role, email, phone, username, password_hash) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?)",
                    {reqString(j, "full_name", 100), optParam(j, "department", 100), role, optParam(j, "email", 150),
                     optParam(j, "phone", 20), readUsername(j), hash});
                Reply r;
                r.status = 201;
                r.body["user_id"] = static_cast<Json::Int64>(db.lastInsertId());
                r.body["message"] = "User added.";
                return r;
            });
        },
        {Post});

    app().registerHandler(
        "/api/users/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [id](Db& db, const HttpRequestPtr& req, const CurrentUser& me) {
                requireId(id);
                const Json::Value& j = jsonBody(req);
                std::string role = reqString(j, "role", 20);
                checkRole(role);
                long long active = reqInt(j, "is_active", 0, 1);
                if (id == me.id && (role != me.role || active != 1))
                    throw ApiError{409, "You cannot change your own role or deactivate yourself."};
                Transaction tx(db);
                if (db.execute(
                        "UPDATE Users SET full_name = ?, department = ?, role = ?, email = ?, phone = ?, username = ?, "
                        "is_active = ? WHERE user_id = ?",
                        {reqString(j, "full_name", 100), optParam(j, "department", 100), role, optParam(j, "email", 150),
                         optParam(j, "phone", 20), readUsername(j), active, id}) != 1)
                    throw ApiError{404, "User not found."};
                std::string password = optString(j, "password", 128);  // optional: only changes if given
                if (!password.empty()) {
                    checkPasswordRules(password);
                    db.execute("UPDATE Users SET password_hash = ? WHERE user_id = ?", {hashPassword(password), id});
                }
                tx.commit();
                return messageReply(200, "User updated.");
            });
        },
        {Put});

    app().registerHandler(
        "/api/users/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [id](Db& db, const HttpRequestPtr&, const CurrentUser& me) {
                requireId(id);
                if (id == me.id) throw ApiError{409, "You cannot delete your own account."};
                int n = 0;
                try {
                    n = db.execute("DELETE FROM Users WHERE user_id = ?", {id});
                } catch (const DbError& e) {
                    if (e.isConstraint())
                        throw ApiError{409, "This user has issue/maintenance history. Deactivate the user instead of deleting."};
                    throw;
                }
                if (n != 1) throw ApiError{404, "User not found."};
                return messageReply(200, "User deleted.");
            });
        },
        {Delete});
}

void registerLookupRoutes() {
    registerLookup({"/api/labs", "Labs", "lab_id", "lab_name", "location", "Laboratory"});
    registerLookup({"/api/categories", "Categories", "category_id", "category_name", "", "Category"});
    registerUsers();
}

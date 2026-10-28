#include "../services/IssueService.h"
#include "Common.h"
#include "Routes.h"

using namespace drogon;
using Cb = std::function<void(const HttpResponsePtr&)>;
static const std::vector<std::string> kStaff = {"Admin", "LabAssistant"};

void registerIssueRoutes() {
    // List issues. Staff see everything; students/faculty are FORCED to their own records here on the server.
    app().registerHandler(
        "/api/issues",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), {}, [](Db& db, const HttpRequestPtr& req, const CurrentUser& me) {
                std::string sql =
                    "SELECT io.issue_id, io.user_id, u.full_name AS borrower, u.role AS borrower_role, "
                    "       io.equipment_id, e.equipment_name, io.quantity_issued, io.outstanding, "
                    "       io.issue_date, io.due_date, io.status, ib.full_name AS issued_by_name, "
                    "       CASE WHEN io.outstanding > 0 AND io.due_date < date('now','localtime') THEN 1 ELSE 0 END AS is_overdue "
                    "FROM Issue_Outstanding io "
                    "JOIN Users u ON u.user_id = io.user_id "
                    "JOIN Equipment e ON e.equipment_id = io.equipment_id "
                    "JOIN Users ib ON ib.user_id = io.issued_by WHERE 1 = 1 ";
                Params p;
                bool staff = (me.role == "Admin" || me.role == "LabAssistant");
                if (!staff) {
                    sql += "AND io.user_id = ? ";
                    p.push_back(me.id);
                } else if (long long uid = optQueryInt(req, "user_id")) {
                    sql += "AND io.user_id = ? ";
                    p.push_back(uid);
                }
                std::string status = req->getParameter("status");
                if (status == "outstanding") sql += "AND io.outstanding > 0 ";
                else if (!status.empty()) { sql += "AND io.status = ? "; p.push_back(status); }
                sql += "ORDER BY io.issue_id DESC";
                Reply r;
                r.body = db.query(sql, p);
                return r;
            });
        },
        {Get});

    app().registerHandler(
        "/api/issues",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kStaff, [](Db& db, const HttpRequestPtr& req, const CurrentUser& me) {
                const Json::Value& j = jsonBody(req);
                Reply r;
                r.status = 201;
                r.body = issueEquipment(db, reqInt(j, "user_id", 1, 1000000000), reqInt(j, "equipment_id", 1, 1000000000),
                                        reqInt(j, "quantity", 1, 1000000), reqDate(j, "due_date"), me.id);
                return r;
            });
        },
        {Post});

    app().registerHandler(
        "/api/issues/{1}/returns",
        [](const HttpRequestPtr& req, Cb&& cb, long long issueId) {
            run(req, std::move(cb), kStaff, [issueId](Db& db, const HttpRequestPtr& req, const CurrentUser& me) {
                requireId(issueId);
                const Json::Value& j = jsonBody(req);
                Reply r;
                r.status = 201;
                r.body = returnEquipment(db, issueId, reqInt(j, "quantity", 1, 1000000), optString(j, "remarks", 300), me.id);
                return r;
            });
        },
        {Post});
}

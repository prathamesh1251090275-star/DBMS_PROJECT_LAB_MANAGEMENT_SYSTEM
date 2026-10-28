#include "../services/MaintenanceService.h"
#include "Common.h"
#include "Routes.h"

using namespace drogon;
using Cb = std::function<void(const HttpResponsePtr&)>;
static const std::vector<std::string> kStaff = {"Admin", "LabAssistant"};
static const std::vector<std::string> kAdmin = {"Admin"};

void registerMaintenanceRoutes() {
    app().registerHandler(
        "/api/maintenance",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kStaff, [](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                std::string sql =
                    "SELECT m.maintenance_id, m.equipment_id, e.equipment_name, m.quantity_affected, "
                    "       m.problem_description, u.full_name AS reported_by_name, m.reported_date, "
                    "       m.maintenance_date, m.completed_date, m.repair_cost_paise / 100.0 AS repair_cost, m.status "
                    "FROM Maintenance_Records m "
                    "JOIN Equipment e ON e.equipment_id = m.equipment_id "
                    "JOIN Users u ON u.user_id = m.reported_by WHERE 1 = 1 ";
                Params p;
                std::string status = req->getParameter("status");
                if (status == "open") sql += "AND m.status IN ('Reported','In Progress') ";
                else if (!status.empty()) { sql += "AND m.status = ? "; p.push_back(status); }
                sql += "ORDER BY m.maintenance_id DESC";
                Reply r;
                r.body = db.query(sql, p);
                return r;
            });
        },
        {Get});

    // Lab assistants and admins can report damaged equipment
    app().registerHandler(
        "/api/maintenance",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kStaff, [](Db& db, const HttpRequestPtr& req, const CurrentUser& me) {
                const Json::Value& j = jsonBody(req);
                Reply r;
                r.status = 201;
                r.body = reportDamage(db, reqInt(j, "equipment_id", 1, 1000000000), reqInt(j, "quantity_affected", 1, 1000000),
                                      reqString(j, "problem_description", 500), me.id);
                return r;
            });
        },
        {Post});

    // Only the admin manages the repair workflow
    app().registerHandler(
        "/api/maintenance/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [id](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                requireId(id);
                const Json::Value& j = jsonBody(req);
                bool hasCost = j.isMember("repair_cost") && !j["repair_cost"].isNull();
                Reply r;
                r.body = updateMaintenance(db, id, reqString(j, "status", 20), optDate(j, "maintenance_date"),
                                           hasCost ? moneyToPaise(j, "repair_cost") : 0, hasCost);
                return r;
            });
        },
        {Put});
}

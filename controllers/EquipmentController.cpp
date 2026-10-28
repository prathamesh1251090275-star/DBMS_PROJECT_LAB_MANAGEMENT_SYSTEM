#include "../services/EquipmentService.h"
#include "Common.h"
#include "Routes.h"

using namespace drogon;

static const char* kEquipmentSelect =
    "SELECT e.equipment_id, e.equipment_name, e.category_id, c.category_name, e.lab_id, l.lab_name, "
    "       e.total_quantity, e.available_quantity, e.condition_status, e.purchase_date, e.description "
    "FROM Equipment e "
    "JOIN Categories c ON c.category_id = e.category_id "
    "JOIN Labs l ON l.lab_id = e.lab_id ";

using Cb = std::function<void(const HttpResponsePtr&)>;
static const std::vector<std::string> kAdmin = {"Admin"};

void registerEquipmentRoutes() {
    // List + search + filters. Everyone who is logged in may view equipment.
    app().registerHandler(
        "/api/equipment",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), {}, [](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                // The SQL text below is built only from FIXED fragments; every user value is bound with "?".
                std::string sql = std::string(kEquipmentSelect) + "WHERE 1 = 1 ";
                Params p;
                std::string q = req->getParameter("q");
                if (!q.empty()) {
                    sql += "AND (e.equipment_name LIKE ? OR CAST(e.equipment_id AS TEXT) = ?) ";
                    p.push_back("%" + q + "%");
                    p.push_back(q);
                }
                if (long long lab = optQueryInt(req, "lab_id")) { sql += "AND e.lab_id = ? "; p.push_back(lab); }
                if (long long cat = optQueryInt(req, "category_id")) { sql += "AND e.category_id = ? "; p.push_back(cat); }
                std::string cond = req->getParameter("condition");
                if (!cond.empty()) { sql += "AND e.condition_status = ? "; p.push_back(cond); }
                sql += "ORDER BY e.equipment_name";
                Reply r;
                r.body = db.query(sql, p);
                return r;
            });
        },
        {Get});

    app().registerHandler(
        "/api/equipment/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), {}, [id](Db& db, const HttpRequestPtr&, const CurrentUser&) {
                requireId(id);
                auto rows = db.query(std::string(kEquipmentSelect) + "WHERE e.equipment_id = ?", {id});
                if (rows.empty()) throw ApiError{404, "Equipment not found."};
                Reply r;
                r.body = rows[0];
                return r;
            });
        },
        {Get});

    // Admin only: add, edit, delete
    app().registerHandler(
        "/api/equipment",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kAdmin, [](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                Reply r;
                r.status = 201;
                r.body = createEquipment(db, jsonBody(req));
                return r;
            });
        },
        {Post});

    app().registerHandler(
        "/api/equipment/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [id](Db& db, const HttpRequestPtr& req, const CurrentUser&) {
                requireId(id);
                Reply r;
                r.body = updateEquipment(db, id, jsonBody(req));
                return r;
            });
        },
        {Put});

    app().registerHandler(
        "/api/equipment/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long id) {
            run(req, std::move(cb), kAdmin, [id](Db& db, const HttpRequestPtr&, const CurrentUser&) {
                requireId(id);
                Reply r;
                r.body = deleteEquipment(db, id);
                return r;
            });
        },
        {Delete});
}

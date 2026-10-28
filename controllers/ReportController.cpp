// Dashboard numbers and the six reports.
#include "Common.h"
#include "Routes.h"

using namespace drogon;
using Cb = std::function<void(const HttpResponsePtr&)>;
static const std::vector<std::string> kStaff = {"Admin", "LabAssistant"};
static const std::vector<std::string> kAdmin = {"Admin"};

// Shared SELECT for "issued" and "overdue" reports
static const char* kIssuedSelect =
    "SELECT io.issue_id, u.full_name AS borrower, u.department, e.equipment_name, l.lab_name, "
    "       io.quantity_issued, io.outstanding, io.issue_date, io.due_date, "
    "       CAST(julianday(date('now','localtime')) - julianday(io.due_date) AS INTEGER) AS days_overdue "
    "FROM Issue_Outstanding io "
    "JOIN Users u ON u.user_id = io.user_id "
    "JOIN Equipment e ON e.equipment_id = io.equipment_id "
    "JOIN Labs l ON l.lab_id = e.lab_id "
    "WHERE io.outstanding > 0 ";

// Small helper so each report is one line to register.
static void registerSimple(const std::string& path, const std::vector<std::string>& roles, const std::string& sql) {
    app().registerHandler(
        path,
        [roles, sql](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), roles, [sql](Db& db, const HttpRequestPtr&, const CurrentUser&) {
                Reply r;
                r.body = db.query(sql);
                return r;
            });
        },
        {Get});
}

void registerReportRoutes() {
    app().registerHandler(
        "/api/dashboard",
        [](const HttpRequestPtr& req, Cb&& cb) {
            run(req, std::move(cb), kStaff, [](Db& db, const HttpRequestPtr&, const CurrentUser&) {
                Reply r;
                auto eq = db.query("SELECT COUNT(*) AS types, COALESCE(SUM(total_quantity),0) AS total, "
                                   "COALESCE(SUM(available_quantity),0) AS available FROM Equipment")[0];
                auto issued = db.query("SELECT COALESCE(SUM(outstanding),0) AS q FROM Issue_Outstanding")[0];
                auto maint = db.query("SELECT COALESCE(SUM(quantity_affected),0) AS q FROM Maintenance_Records "
                                      "WHERE status IN ('Reported','In Progress')")[0];
                auto over = db.query("SELECT COUNT(*) AS n, COALESCE(SUM(outstanding),0) AS q FROM Issue_Outstanding "
                                     "WHERE outstanding > 0 AND due_date < date('now','localtime')")[0];
                r.body["equipment_types"] = eq["types"];
                r.body["total_quantity"] = eq["total"];
                r.body["available_quantity"] = eq["available"];
                r.body["issued_quantity"] = issued["q"];
                r.body["under_maintenance_quantity"] = maint["q"];
                r.body["overdue_issues"] = over["n"];
                r.body["overdue_quantity"] = over["q"];
                return r;
            });
        },
        {Get});

    // 1. Available inventory (staff)
    registerSimple("/api/reports/inventory", kStaff,
                   "SELECT e.equipment_id, e.equipment_name, c.category_name, l.lab_name, e.total_quantity, "
                   "e.available_quantity, e.condition_status FROM Equipment e "
                   "JOIN Categories c ON c.category_id = e.category_id JOIN Labs l ON l.lab_id = e.lab_id "
                   "ORDER BY e.equipment_name");
    // 2. Currently issued (staff)
    registerSimple("/api/reports/issued", kStaff, std::string(kIssuedSelect) + "ORDER BY io.due_date");
    // 3. Overdue returns (staff)
    registerSimple("/api/reports/overdue", kStaff,
                   std::string(kIssuedSelect) + "AND io.due_date < date('now','localtime') ORDER BY io.due_date");
    // 4. Equipment under maintenance (admin)
    registerSimple("/api/reports/maintenance", kAdmin,
                   "SELECT m.maintenance_id, e.equipment_name, l.lab_name, m.quantity_affected, m.problem_description, "
                   "m.reported_date, m.status FROM Maintenance_Records m "
                   "JOIN Equipment e ON e.equipment_id = m.equipment_id JOIN Labs l ON l.lab_id = e.lab_id "
                   "WHERE m.status IN ('Reported','In Progress') ORDER BY m.reported_date");
    // 5. Equipment by laboratory (admin) - GROUP BY + aggregates
    registerSimple("/api/reports/by-lab", kAdmin,
                   "SELECT l.lab_id, l.lab_name, COUNT(e.equipment_id) AS equipment_types, "
                   "COALESCE(SUM(e.total_quantity),0) AS total_quantity, COALESCE(SUM(e.available_quantity),0) AS available_quantity "
                   "FROM Labs l LEFT JOIN Equipment e ON e.lab_id = l.lab_id GROUP BY l.lab_id, l.lab_name ORDER BY l.lab_name");

    // 6. Issue history by user. Students/faculty may only see their own history.
    app().registerHandler(
        "/api/reports/user-history/{1}",
        [](const HttpRequestPtr& req, Cb&& cb, long long userId) {
            run(req, std::move(cb), {}, [userId](Db& db, const HttpRequestPtr&, const CurrentUser& me) {
                requireId(userId);
                bool staff = (me.role == "Admin" || me.role == "LabAssistant");
                if (!staff && userId != me.id) throw ApiError{403, "You can only view your own history."};
                Reply r;
                r.body = db.query(
                    "SELECT io.issue_id, e.equipment_name, io.quantity_issued, io.outstanding, io.issue_date, io.due_date, "
                    "io.status, CASE WHEN io.outstanding > 0 AND io.due_date < date('now','localtime') THEN 1 ELSE 0 END AS is_overdue "
                    "FROM Issue_Outstanding io JOIN Equipment e ON e.equipment_id = io.equipment_id "
                    "WHERE io.user_id = ? ORDER BY io.issue_date DESC",
                    {userId});
                return r;
            });
        },
        {Get});
}

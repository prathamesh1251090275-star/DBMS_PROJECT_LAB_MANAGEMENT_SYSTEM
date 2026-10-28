-- Demonstration queries for faculty. Run in DB Browser for SQLite ("Execute SQL") or:
--   sqlite3 database/LabEquipmentDB.db < docs/queries.sql
-- Each block starts with "-- @Qn | title".
.headers on
.mode column

-- @Q1 | INNER JOIN: equipment with its lab and category
-- Joins three tables through foreign keys, so names are shown instead of IDs.
SELECT e.equipment_id, e.equipment_name, c.category_name, l.lab_name, e.total_quantity, e.available_quantity
FROM Equipment e
JOIN Categories c ON c.category_id = e.category_id
JOIN Labs l ON l.lab_id = e.lab_id
ORDER BY l.lab_name, e.equipment_name;

-- @Q2 | GROUP BY + COUNT + SUM: stock per laboratory
-- LEFT JOIN keeps labs that have no equipment; COALESCE turns NULL sums into 0.
SELECT l.lab_name, COUNT(e.equipment_id) AS equipment_types,
       COALESCE(SUM(e.total_quantity), 0) AS total_units, COALESCE(SUM(e.available_quantity), 0) AS available_units
FROM Labs l LEFT JOIN Equipment e ON e.lab_id = l.lab_id
GROUP BY l.lab_id, l.lab_name
ORDER BY total_units DESC;

-- @Q3 | View + JOIN: items currently issued (not yet fully returned)
-- Issue_Outstanding is a VIEW that computes outstanding = issued - SUM(returns).
SELECT io.issue_id, u.full_name AS borrower, e.equipment_name, io.quantity_issued, io.outstanding, io.due_date
FROM Issue_Outstanding io
JOIN Users u ON u.user_id = io.user_id
JOIN Equipment e ON e.equipment_id = io.equipment_id
WHERE io.outstanding > 0
ORDER BY io.due_date;

-- @Q4 | Filtering with date functions: overdue returns and days late
-- Overdue is computed from today's date, never stored, so it is always correct.
SELECT io.issue_id, u.full_name AS borrower, e.equipment_name, io.outstanding, io.due_date,
       CAST(julianday(date('now','localtime')) - julianday(io.due_date) AS INTEGER) AS days_late
FROM Issue_Outstanding io
JOIN Users u ON u.user_id = io.user_id
JOIN Equipment e ON e.equipment_id = io.equipment_id
WHERE io.outstanding > 0 AND io.due_date < date('now','localtime')
ORDER BY days_late DESC;

-- @Q5 | Subquery: equipment with more units available than the average item
-- The inner SELECT computes one number (the average) which the outer query compares against.
SELECT equipment_name, available_quantity
FROM Equipment
WHERE available_quantity > (SELECT AVG(available_quantity) FROM Equipment)
ORDER BY available_quantity DESC;

-- @Q6 | Correlated subquery (NOT EXISTS): people who have never borrowed anything
SELECT u.user_id, u.full_name, u.role
FROM Users u
WHERE u.role IN ('Student','Faculty')
  AND NOT EXISTS (SELECT 1 FROM Issue_Records i WHERE i.user_id = u.user_id);

-- @Q7 | GROUP BY + HAVING: borrowers who took equipment more than once
SELECT u.full_name, COUNT(*) AS times_borrowed, SUM(i.quantity_issued) AS units_borrowed
FROM Issue_Records i JOIN Users u ON u.user_id = i.user_id
GROUP BY u.user_id, u.full_name
HAVING COUNT(*) > 1;

-- @Q8 | LEFT JOIN + aggregate: issue history of user 4 with total returned
-- LEFT JOIN keeps issues that have no return rows yet.
SELECT i.issue_id, e.equipment_name, i.quantity_issued, COALESCE(SUM(r.quantity_returned), 0) AS returned,
       i.status, i.issue_date, i.due_date
FROM Issue_Records i
JOIN Equipment e ON e.equipment_id = i.equipment_id
LEFT JOIN Return_Records r ON r.issue_id = i.issue_id
WHERE i.user_id = 4
GROUP BY i.issue_id
ORDER BY i.issue_date;

-- @Q9 | Maintenance cost: SUM of completed repairs per equipment (money stored as paise)
SELECT e.equipment_name, COUNT(*) AS repairs, SUM(m.repair_cost_paise) / 100.0 AS total_cost_rupees
FROM Maintenance_Records m JOIN Equipment e ON e.equipment_id = m.equipment_id
WHERE m.status = 'Completed'
GROUP BY e.equipment_id;

-- @Q10 | Most borrowed equipment (aggregate + ORDER BY + LIMIT)
SELECT e.equipment_name, SUM(i.quantity_issued) AS units_issued
FROM Issue_Records i JOIN Equipment e ON e.equipment_id = i.equipment_id
GROUP BY e.equipment_id
ORDER BY units_issued DESC
LIMIT 3;

-- @Q11 | Inventory consistency check: total = available + issued + under maintenance
-- Every row must say OK. This is the invariant the C++ transactions protect.
SELECT e.equipment_name, e.total_quantity AS total, e.available_quantity AS available,
       COALESCE((SELECT SUM(outstanding) FROM Issue_Outstanding o WHERE o.equipment_id = e.equipment_id), 0) AS issued,
       COALESCE((SELECT SUM(quantity_affected) FROM Maintenance_Records m
                 WHERE m.equipment_id = e.equipment_id AND m.status IN ('Reported','In Progress')), 0) AS in_repair,
       CASE WHEN e.total_quantity = e.available_quantity
              + COALESCE((SELECT SUM(outstanding) FROM Issue_Outstanding o WHERE o.equipment_id = e.equipment_id), 0)
              + COALESCE((SELECT SUM(quantity_affected) FROM Maintenance_Records m
                          WHERE m.equipment_id = e.equipment_id AND m.status IN ('Reported','In Progress')), 0)
            THEN 'OK' ELSE 'MISMATCH' END AS check_result
FROM Equipment e;

-- @Q12 | Transaction demo: issue 2 Raspberry Pi, look at the result, then ROLLBACK
-- BEGIN ... ROLLBACK undoes everything, so the data is unchanged afterwards.
BEGIN;
UPDATE Equipment SET available_quantity = available_quantity - 2 WHERE equipment_id = 5 AND available_quantity >= 2;
INSERT INTO Issue_Records (user_id, equipment_id, issued_by, quantity_issued, due_date) VALUES (4, 5, 2, 2, date('now','localtime','+7 days'));
SELECT 'inside transaction' AS moment, available_quantity FROM Equipment WHERE equipment_id = 5;
ROLLBACK;
SELECT 'after rollback' AS moment, available_quantity FROM Equipment WHERE equipment_id = 5;

-- @Q13 | Constraint demo: the database itself refuses impossible data
-- (run with foreign keys ON, which the C++ server always does)
PRAGMA foreign_keys = ON;
UPDATE Equipment SET available_quantity = 999 WHERE equipment_id = 1;

-- @Q14 | Index usage: EXPLAIN QUERY PLAN shows SQLite uses idx_issue_user instead of scanning the whole table
EXPLAIN QUERY PLAN SELECT * FROM Issue_Records WHERE user_id = 4;

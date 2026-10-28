-- ============================================================
-- LabEquipmentDB  -  SQLite schema (3NF)
-- Safe to run more than once: every CREATE uses IF NOT EXISTS.
-- Dates are stored as ISO-8601 TEXT ('YYYY-MM-DD').
-- Money is stored as INTEGER paise (Rs 1500.50 -> 150050) so there
-- are no floating-point rounding errors.
-- NOTE: PRAGMA foreign_keys is per-connection; the C++ code turns it
-- ON every time it opens the database.
-- ============================================================
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS Labs (
    lab_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    lab_name    TEXT NOT NULL UNIQUE CHECK (length(trim(lab_name)) > 0),
    location    TEXT,
    description TEXT
);

CREATE TABLE IF NOT EXISTS Categories (
    category_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    category_name TEXT NOT NULL UNIQUE CHECK (length(trim(category_name)) > 0),
    description   TEXT
);

CREATE TABLE IF NOT EXISTS Users (
    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name     TEXT NOT NULL CHECK (length(trim(full_name)) > 0),
    department    TEXT,
    role          TEXT NOT NULL CHECK (role IN ('Admin','LabAssistant','Student','Faculty')),
    email         TEXT UNIQUE,
    phone         TEXT,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT,                       -- NULL = cannot log in yet
    is_active     INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    created_at    TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS Equipment (
    equipment_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    equipment_name     TEXT NOT NULL CHECK (length(trim(equipment_name)) > 0),
    category_id        INTEGER NOT NULL REFERENCES Categories(category_id),
    lab_id             INTEGER NOT NULL REFERENCES Labs(lab_id),
    total_quantity     INTEGER NOT NULL CHECK (total_quantity >= 0),
    available_quantity INTEGER NOT NULL,
    condition_status   TEXT NOT NULL DEFAULT 'Good' CHECK (condition_status IN ('Good','Fair','Poor')),
    purchase_date      TEXT CHECK (purchase_date IS NULL OR date(purchase_date) IS purchase_date),
    description        TEXT,
    CHECK (available_quantity >= 0 AND available_quantity <= total_quantity)
);

CREATE TABLE IF NOT EXISTS Issue_Records (
    issue_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id         INTEGER NOT NULL REFERENCES Users(user_id),      -- borrower
    equipment_id    INTEGER NOT NULL REFERENCES Equipment(equipment_id),
    issued_by       INTEGER NOT NULL REFERENCES Users(user_id),      -- lab assistant / admin
    quantity_issued INTEGER NOT NULL CHECK (quantity_issued > 0),
    issue_date      TEXT NOT NULL DEFAULT (date('now','localtime')) CHECK (date(issue_date) IS issue_date),
    due_date        TEXT NOT NULL CHECK (date(due_date) IS due_date),
    status          TEXT NOT NULL DEFAULT 'Issued' CHECK (status IN ('Issued','Partially Returned','Returned')),
    CHECK (due_date >= issue_date)
);

CREATE TABLE IF NOT EXISTS Return_Records (
    return_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    issue_id          INTEGER NOT NULL REFERENCES Issue_Records(issue_id),
    quantity_returned INTEGER NOT NULL CHECK (quantity_returned > 0),
    return_date       TEXT NOT NULL DEFAULT (date('now','localtime')) CHECK (date(return_date) IS return_date),
    received_by       INTEGER NOT NULL REFERENCES Users(user_id),
    remarks           TEXT
);

CREATE TABLE IF NOT EXISTS Maintenance_Records (
    maintenance_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    equipment_id        INTEGER NOT NULL REFERENCES Equipment(equipment_id),
    quantity_affected   INTEGER NOT NULL CHECK (quantity_affected > 0),
    problem_description TEXT NOT NULL CHECK (length(trim(problem_description)) > 0),
    reported_by         INTEGER NOT NULL REFERENCES Users(user_id),
    reported_date       TEXT NOT NULL DEFAULT (date('now','localtime')) CHECK (date(reported_date) IS reported_date),
    maintenance_date    TEXT CHECK (maintenance_date IS NULL OR date(maintenance_date) IS maintenance_date),
    completed_date      TEXT CHECK (completed_date IS NULL OR date(completed_date) IS completed_date),
    repair_cost_paise   INTEGER NOT NULL DEFAULT 0 CHECK (repair_cost_paise >= 0),
    status              TEXT NOT NULL DEFAULT 'Reported'
                        CHECK (status IN ('Reported','In Progress','Completed','Written Off'))
);

-- Indexes (foreign keys and frequently filtered columns)
CREATE INDEX IF NOT EXISTS idx_equipment_lab      ON Equipment(lab_id);
CREATE INDEX IF NOT EXISTS idx_equipment_category ON Equipment(category_id);
CREATE INDEX IF NOT EXISTS idx_equipment_name     ON Equipment(equipment_name);
CREATE INDEX IF NOT EXISTS idx_issue_user         ON Issue_Records(user_id);
CREATE INDEX IF NOT EXISTS idx_issue_equipment    ON Issue_Records(equipment_id);
CREATE INDEX IF NOT EXISTS idx_issue_status_due   ON Issue_Records(status, due_date);
CREATE INDEX IF NOT EXISTS idx_return_issue       ON Return_Records(issue_id);
CREATE INDEX IF NOT EXISTS idx_maint_equipment    ON Maintenance_Records(equipment_id);
CREATE INDEX IF NOT EXISTS idx_maint_status       ON Maintenance_Records(status);

-- View: every issue with its outstanding (not yet returned) quantity.
-- "Outstanding" and "overdue" are COMPUTED, never stored, so they cannot go stale.
CREATE VIEW IF NOT EXISTS Issue_Outstanding AS
SELECT i.*,
       i.quantity_issued
         - COALESCE((SELECT SUM(r.quantity_returned)
                     FROM Return_Records r
                     WHERE r.issue_id = i.issue_id), 0) AS outstanding
FROM Issue_Records i;

# DBMS Project Report – College Lab Equipment Management System

## 1. Problem and objectives
Labs track equipment on paper or spreadsheets, so stock counts drift, overdue items go unnoticed and damaged items stay "available". This system keeps one accurate inventory, records every issue/return/repair, and reports available, issued, overdue and damaged equipment.

## 2. Technology
HTML/CSS/JavaScript frontend → HTTP + JSON → C++17 backend (Drogon web framework) → SQLite 3 file `LabEquipmentDB.db` (accessed with the SQLite C API). Passwords: Argon2id via libsodium.

## 3. Roles
| Action | Admin | Lab Assistant | Student/Faculty |
|---|:-:|:-:|:-:|
| Manage labs, categories, equipment, users | ✔ | | |
| View equipment availability | ✔ | ✔ | ✔ |
| Issue / return equipment, report damage | ✔ | ✔ | |
| Update maintenance (repair status/cost) | ✔ | | |
| Dashboard, inventory/issued/overdue reports | ✔ | ✔ | |
| Maintenance and by-lab reports | ✔ | | |
| Own issue history and due dates | ✔ | ✔ | ✔ (own only) |
Enforced in the C++ `run()` function for every request – not just hidden in the UI.

## 4. Database design
See `ER_DIAGRAM.md`. Seven tables, all one-to-many.
| Table | Why it exists |
|---|---|
| Users | One table for all people; `role` decides permissions. Students/faculty/staff share the same attributes. |
| Labs, Categories | Lookup tables: a lab/category name is stored once and referenced by key. |
| Equipment | Current stock of each equipment *type* (quantity-based: 20 multimeters = one row). |
| Issue_Records | One row per issue event (who, what, how many, when, due). |
| Return_Records | One row per return. Separate because one issue can have several partial returns. |
| Maintenance_Records | Repair history; one equipment can be repaired many times. |

**Keys:** every table has an `INTEGER PRIMARY KEY AUTOINCREMENT` (AUTOINCREMENT prevents re-using deleted IDs, which keeps history unambiguous). Foreign keys: Equipment→Labs/Categories; Issue_Records→Users (twice)/Equipment; Return_Records→Issue_Records/Users; Maintenance_Records→Equipment/Users. `PRAGMA foreign_keys = ON` is executed on every connection (SQLite default is OFF).

**Constraints:** NOT NULL, UNIQUE (lab/category name, username, email), CHECK (roles, statuses, quantities > 0, `0 ≤ available ≤ total`, `due_date ≥ issue_date`, valid ISO dates via `date(x) IS x`, cost ≥ 0). **Indexes** on all foreign keys, equipment name and (status, due_date).

**Data types:** dates are ISO-8601 text (`YYYY-MM-DD`) so text comparison equals date comparison. Money is stored as **integer paise** (₹1500.50 → 150050) to avoid floating-point rounding; the API converts to/from rupees.

## 5. Normalization
* **1NF** – atomic values, no repeating groups (multiple returns are rows, not columns).
* **2NF** – every table has a single-column key, so no partial dependency.
* **3NF** – no non-key attribute depends on another non-key attribute: lab name/category name live only in Labs/Categories; a borrower's name lives only in Users.
* **Anomalies avoided:** *update* (rename a lab once), *insert* (a lab/category can exist without equipment), *delete* (deleting a return does not erase the issue; FK rules block deleting referenced rows).
* **Deliberate, controlled redundancy:** `Equipment.available_quantity` and `Issue_Records.status` could be derived. They are kept for speed and because the brief requires them; they are always updated in the same transaction as the event that changes them. `overdue` and `outstanding` are **computed** (view/query), never stored, because they change every day.

**Inventory invariant:** `total = available + outstanding issued + units under open maintenance`. Check query: `docs/QUERIES.md` Q11 (all rows OK).

## 6. How history is preserved
Issues and returns are never edited or deleted. A return adds a row; the issue row only changes `status`. Equipment/users with history cannot be deleted (409), users are deactivated instead, so old records always resolve to real names.

## 7. Inventory consistency and concurrency
* **Issue** (one transaction, `BEGIN IMMEDIATE`): validate borrower → `UPDATE Equipment SET available = available − ? WHERE id = ? AND available >= ?` → if 0 rows changed, reject (409) → insert issue → `COMMIT`.
* **Return**: read outstanding = issued − Σreturns inside the transaction → reject if 0 (duplicate) or > outstanding → insert return → `available += qty` (1 row must change) → set status → `COMMIT`.
* **Maintenance**: report subtracts from available; *Completed* adds back; *Written Off* reduces total; closed records cannot change again (prevents double add-back).
* `BEGIN IMMEDIATE` takes SQLite's write lock at the start, so two requests cannot interleave; the second waits (busy timeout 5 s). **Tested:** 20 simultaneous requests for 5 remaining units → exactly 5 succeeded, 15 got 409, available ended at 0.
* The CHECK constraint is a second safety net: even a C++ bug cannot make stock negative or above total.
* WAL journal mode lets readers continue while a write is in progress.

## 8. Backend design
`controllers/` receive HTTP and call `run()` (login + role check + error mapping); `services/` hold the transaction rules; `database/Db.*` wraps prepared statements (`sqlite3_prepare_v2`, `sqlite3_bind_*`, `sqlite3_step`, `sqlite3_finalize` in a destructor). Every user value is bound as a parameter; dynamic SQL only appends fixed text fragments. One `Db` connection per request. `Transaction` is an RAII class: no `commit()` → automatic `ROLLBACK`.

## 9. Security
Argon2id hashes, no default password (admin sets one with `--set-password`), brute-force lock after 5 failures, session cookie HttpOnly + SameSite=Lax, role check and active-user check each request, generic error messages (SQLite messages only in server logs), input validation (lengths, types, dates, enums), no CORS (same origin), HTML escaping in the frontend to stop XSS.

## 10. Limitations / future work
Not tested on Windows or in real browsers by the author of this code (see README). Sessions are in memory (restart = log in again). No HTTPS (use a reverse proxy). SQLite allows one writer at a time – fine for a college lab, not for thousands of concurrent writers. No pagination. Possible extensions: per-unit asset tags, email reminders, CSV export.

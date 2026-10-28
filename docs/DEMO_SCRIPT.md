# 10-minute Faculty Demonstration Script
Prepare: build, `--set-password` for `admin`, `rahul.assist`, `aarav`; start the server; open DB Browser for SQLite on `database/LabEquipmentDB.db`.

1. **Architecture (1 min).** Browser → Drogon C++ API → SQLite file. Show folder structure. Explain engine vs file vs viewer.
2. **ER diagram + schema (2 min).** Show `docs/ER_DIAGRAM.md`. In DB Browser show the 7 tables, a CHECK constraint and the `Issue_Outstanding` view.
3. **Queries (2 min).** Run Q2 (GROUP BY), Q4 (overdue), Q11 (consistency, all OK), Q12 (transaction + ROLLBACK), Q13 (constraint rejects bad data).
4. **Admin (1 min).** Log in as `admin`: dashboard cards, Equipment search/filter, add and edit an item. Try to delete *Digital Multimeter* → blocked (has history).
5. **Lab assistant (2 min).** Log in as `rahul.assist`: Issue 3 Raspberry Pi to Aarav (available 10 → 7). Try issuing 20 → refused. Return 1 (partial), then 2 (full), then try returning again → refused. Report 2 Pis damaged → available drops; try to issue more than remain → refused.
6. **Admin maintenance (30 s).** Update the record to Completed with cost → available restored.
7. **Student (30 s).** Log in as `aarav`: only Equipment and My Issues; own due dates and the Overdue badge. Show that a student calling `/api/reports/overdue` gets 403 (`curl`).
8. **Safety (1 min).** Show `bash tests/api_test.sh` result (87 passed) including the 20-parallel-request test; show the password column is an Argon2id hash.

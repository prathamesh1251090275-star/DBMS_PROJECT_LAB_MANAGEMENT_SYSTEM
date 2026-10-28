# Testing Checklist and Results

## Automated (run on Ubuntu 24.04 – all passed)
| Suite | Result |
|---|---|
| `tests/db_constraints_test.sh` – 14 invalid statements all rejected by SQLite | 14/14 |
| `tests/api_test.sh` – login, roles, search/filter, SQL-injection attempt, CRUD, validation, issue/return rules, maintenance workflow, write-off, **20 parallel issue requests**, reports, users/labs/categories, logout, final inventory invariant | 87/87 |
| `docs/queries.sql` – 14 demo queries executed, outputs in `QUERIES.md` | OK |
| Frontend smoke test (jsdom, live server): login + every page for Admin, Lab Assistant, Student render real data, 0 JS errors | OK |

## Manual checklist for you (real browser) – tick each
- [ ] Wrong password shows an error; correct password opens the dashboard
- [ ] Admin: add / edit / delete equipment; search by name and ID; filters by lab, category, condition
- [ ] Admin: add lab, category, user (with password), edit, deactivate; user with history cannot be deleted
- [ ] Assistant: issue; over-issue message; partial return; full return; duplicate return message
- [ ] Assistant: report damage; available drops; admin completes; available returns
- [ ] Reports page: all six reports load (Admin); Assistant sees inventory/issued/overdue/history
- [ ] Student: only Equipment and My Issues; overdue badge visible
- [ ] Mobile width (≈400 px): menu button opens sidebar, tables scroll sideways
- [ ] Log out, then press Back: data is not shown (API returns 401)

## Resetting for a demo
Stop the server, delete `database/LabEquipmentDB.db*`, run `lab_server --init-db`, set passwords again.

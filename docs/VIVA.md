# Viva Questions and Answers

## ER diagram
**Q1. What are the entities and relationships?** Labs, Categories, Users, Equipment, Issue_Records, Return_Records, Maintenance_Records. Lab–Equipment 1:N, Category–Equipment 1:N, Equipment–Issue 1:N, Issue–Return 1:N, Equipment–Maintenance 1:N, User–Issue 1:N (borrower and issuer).
**Q2. Why is there no many-to-many relationship?** User↔Equipment is M:N conceptually, but Issue_Records resolves it into two 1:N relationships and stores attributes of the event (quantity, dates).
**Q3. Why does Users appear twice with Issue_Records?** Two roles: `user_id` (borrower) and `issued_by` (staff). Two foreign keys to the same table.
**Q4. Why a separate Return_Records table?** One issue can be returned in parts; putting returns inside the issue would need repeating columns (violates 1NF).

## Normalization
**Q5. Which normal form and why?** 3NF: atomic values, no partial dependency (single-column keys), no transitive dependency (names stored once in lookup tables).
**Q6. What anomalies would a single flat table have?** Update (renaming a lab in many rows), insertion (cannot add a lab with no equipment), deletion (deleting the last issue loses the equipment/user data).
**Q7. You store `available_quantity` – isn't that redundant?** Yes, deliberate denormalization for fast stock checks; kept correct by doing every change in a transaction, and verified by the invariant query. `overdue` is not stored because it changes daily.
**Q8. Why is money stored as integer paise?** Floating-point cannot represent decimals like 0.1 exactly; integers avoid rounding errors.

## Keys
**Q9. Primary key vs foreign key?** PK uniquely identifies a row (NOT NULL, unique). FK is a column that must match a PK in another table; it enforces referential integrity.
**Q10. UNIQUE vs PRIMARY KEY?** A table has one PK; many UNIQUE columns possible (username, email). 
**Q11. What happens if you delete a lab that has equipment?** The FK blocks it (`FOREIGN KEY constraint failed`) → API returns 409. (SQLite only does this if `PRAGMA foreign_keys=ON` for that connection, which our code sets.)
**Q12. AUTOINCREMENT?** Guarantees IDs are never reused after deletion.

## Joins and aggregates
**Q13. INNER vs LEFT JOIN?** INNER keeps only matches; LEFT keeps all left rows (Q2 shows labs with no equipment; Q8 shows issues with no returns).
**Q14. WHERE vs HAVING?** WHERE filters rows before grouping; HAVING filters groups after aggregation (Q7).
**Q15. COUNT(*) vs COUNT(col)?** COUNT(*) counts rows; COUNT(col) skips NULLs.
**Q16. What is a view and why use one?** A saved query. `Issue_Outstanding` computes outstanding quantity so every query reuses the same logic.
**Q17. Correlated subquery?** Inner query references the outer row (Q6, NOT EXISTS).
**Q18. What is an index?** A sorted structure that avoids full table scans; Q14 shows `SEARCH ... USING INDEX`.

## Transactions
**Q19. ACID?** Atomicity (all or nothing), Consistency (constraints hold), Isolation (concurrent requests do not interfere), Durability (committed data survives crashes).
**Q20. Why is issuing a transaction?** It changes two tables (Equipment, Issue_Records). If the insert failed after the stock update, stock would be lost. ROLLBACK undoes both.
**Q21. How do you stop two users issuing the last item?** `BEGIN IMMEDIATE` serialises writers, and the stock check is inside the UPDATE (`WHERE available_quantity >= ?`); 0 rows changed → reject. Tested with 20 parallel requests: exactly 5 of 5 units issued.
**Q22. What is SQLITE_BUSY?** Another connection holds the write lock; we set a 5-second busy timeout, then return HTTP 503.
**Q23. WAL mode?** Writes go to a log file so readers are not blocked by a writer.

## SQLite vs client-server
**Q24. Difference between SQLite and SQL Server/MySQL?** SQLite is an embedded library storing the database in one file, no separate server process, no user accounts, one writer at a time. SQL Server/MySQL run as servers with network access, users/permissions and many concurrent writers.
**Q25. Why SQLite here?** Zero installation, portable file, ideal for a single-college deployment and demos. Limitation: not for heavy concurrent writes.
**Q26. Engine vs file vs management tool?** Engine = code inside our program; file = `LabEquipmentDB.db`; tool = DB Browser for SQLite.

## C++ backend, prepared statements, HTTP
**Q27. What is a prepared statement and why use it?** SQL with `?` placeholders compiled once; values are bound separately, so input can never change the SQL structure → prevents SQL injection. Test: searching `' OR 1=1 --` returns nothing.
**Q28. Walk through `Db::query`.** `sqlite3_prepare_v2` → bind each parameter → loop `sqlite3_step` while SQLITE_ROW, converting columns to JSON → `sqlite3_finalize` (done by the `Stmt` destructor even on exceptions).
**Q29. What is RAII in your code?** `Transaction` rolls back in its destructor unless `commit()` was called; `Stmt` finalizes statements; `Db` closes the connection.
**Q30. How are permissions enforced?** `run(req, cb, roles, handler)` checks the session, reloads the user, compares the role with the allowed list and returns 401/403 before the handler runs. Students asking for `/api/issues` are forced to their own `user_id` on the server.
**Q31. How are passwords stored?** Argon2id hash with random salt (libsodium `crypto_pwhash_str`); never plain text; verified with `crypto_pwhash_str_verify`.
**Q32. GET vs POST vs PUT vs DELETE?** Read / create (or action) / update / remove. Status codes: 200, 201, 400, 401, 403, 404, 409.
**Q33. Why 409 for "not enough stock"?** The request is valid but conflicts with the current state of the data.
**Q34. What is a session?** Server remembers the logged-in user under a random ID stored in an HttpOnly cookie, so JavaScript cannot read it.

## Inventory consistency
**Q35. State the invariant.** total = available + outstanding issued + under open maintenance (Q11).
**Q36. How are partial returns prevented from exceeding?** outstanding = issued − SUM(returns) computed inside the transaction; reject if quantity > outstanding; duplicates rejected when outstanding is 0.
**Q37. What if the C++ code had a bug?** CHECK(`0 ≤ available ≤ total`) stops the bad update and the whole transaction rolls back.
**Q38. Why can't equipment under maintenance be issued?** Reporting damage subtracts those units from `available_quantity`, and issuing only takes from available.

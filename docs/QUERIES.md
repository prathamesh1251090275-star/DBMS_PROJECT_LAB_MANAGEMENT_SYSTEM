# Demonstration queries with real output

Outputs were produced by running each block on a fresh database created from `schema.sql` + `sample_data.sql` on 2026-10-27.
Dates in the sample data are relative to the day the database was created, so your dates will differ; the numbers and structure will match.

## Q1: INNER JOIN: equipment with its lab and category

Joins three tables through foreign keys, so names are shown instead of IDs.

```sql
SELECT e.equipment_id, e.equipment_name, c.category_name, l.lab_name, e.total_quantity, e.available_quantity
FROM Equipment e
JOIN Categories c ON c.category_id = e.category_id
JOIN Labs l ON l.lab_id = e.lab_id
ORDER BY l.lab_name, e.equipment_name;
```

**Output**

```
equipment_id  equipment_name       category_name          lab_name              total_quantity  available_quantity
------------  -------------------  ---------------------  --------------------  --------------  ------------------
7             Desktop Computer     Computing Devices      Computer Lab          30              25                
1             Digital Multimeter   Measuring Instruments  Electronics Lab       20              18                
3             Function Generator   Measuring Instruments  Electronics Lab       6               6                 
2             Oscilloscope 100MHz  Measuring Instruments  Electronics Lab       8               7                 
6             Soldering Station    Hand Tools             Electronics Lab       12              11                
4             Arduino Uno Kit      Development Boards     Embedded Systems Lab  15              13                
5             Raspberry Pi 4       Development Boards     Embedded Systems Lab  10              10                
8             Compound Microscope  Optical Instruments    Physics Lab           10              10                
9             Vernier Caliper      Measuring Instruments  Physics Lab           25              21
```

## Q2: GROUP BY + COUNT + SUM: stock per laboratory

LEFT JOIN keeps labs that have no equipment; COALESCE turns NULL sums into 0.

```sql
SELECT l.lab_name, COUNT(e.equipment_id) AS equipment_types,
       COALESCE(SUM(e.total_quantity), 0) AS total_units, COALESCE(SUM(e.available_quantity), 0) AS available_units
FROM Labs l LEFT JOIN Equipment e ON e.lab_id = l.lab_id
GROUP BY l.lab_id, l.lab_name
ORDER BY total_units DESC;
```

**Output**

```
lab_name              equipment_types  total_units  available_units
--------------------  ---------------  -----------  ---------------
Electronics Lab       4                46           42             
Physics Lab           2                35           31             
Computer Lab          1                30           25             
Embedded Systems Lab  2                25           23
```

## Q3: View + JOIN: items currently issued (not yet fully returned)

Issue_Outstanding is a VIEW that computes outstanding = issued - SUM(returns).

```sql
SELECT io.issue_id, u.full_name AS borrower, e.equipment_name, io.quantity_issued, io.outstanding, io.due_date
FROM Issue_Outstanding io
JOIN Users u ON u.user_id = io.user_id
JOIN Equipment e ON e.equipment_id = io.equipment_id
WHERE io.outstanding > 0
ORDER BY io.due_date;
```

**Output**

```
issue_id  borrower          equipment_name      quantity_issued  outstanding  due_date  
--------  ----------------  ------------------  ---------------  -----------  ----------
1         Aarav Sharma      Digital Multimeter  3                2            2026-10-17
4         Prof. Anil Gupta  Desktop Computer    5                5            2026-10-22
2         Priya Deshmukh    Arduino Uno Kit     2                2            2026-10-31
5         Aarav Sharma      Vernier Caliper     4                4            2026-11-02
```

## Q4: Filtering with date functions: overdue returns and days late

Overdue is computed from today's date, never stored, so it is always correct.

```sql
SELECT io.issue_id, u.full_name AS borrower, e.equipment_name, io.outstanding, io.due_date,
       CAST(julianday(date('now','localtime')) - julianday(io.due_date) AS INTEGER) AS days_late
FROM Issue_Outstanding io
JOIN Users u ON u.user_id = io.user_id
JOIN Equipment e ON e.equipment_id = io.equipment_id
WHERE io.outstanding > 0 AND io.due_date < date('now','localtime')
ORDER BY days_late DESC;
```

**Output**

```
issue_id  borrower          equipment_name      outstanding  due_date    days_late
--------  ----------------  ------------------  -----------  ----------  ---------
1         Aarav Sharma      Digital Multimeter  2            2026-10-17  10       
4         Prof. Anil Gupta  Desktop Computer    5            2026-10-22  5
```

## Q5: Subquery: equipment with more units available than the average item

The inner SELECT computes one number (the average) which the outer query compares against.

```sql
SELECT equipment_name, available_quantity
FROM Equipment
WHERE available_quantity > (SELECT AVG(available_quantity) FROM Equipment)
ORDER BY available_quantity DESC;
```

**Output**

```
equipment_name      available_quantity
------------------  ------------------
Desktop Computer    25                
Vernier Caliper     21                
Digital Multimeter  18
```

## Q6: Correlated subquery (NOT EXISTS): people who have never borrowed anything



```sql
SELECT u.user_id, u.full_name, u.role
FROM Users u
WHERE u.role IN ('Student','Faculty')
  AND NOT EXISTS (SELECT 1 FROM Issue_Records i WHERE i.user_id = u.user_id);
```

**Output**

```
user_id  full_name       role   
-------  --------------  -------
8        Prof. Neha Rao  Faculty
```

## Q7: GROUP BY + HAVING: borrowers who took equipment more than once



```sql
SELECT u.full_name, COUNT(*) AS times_borrowed, SUM(i.quantity_issued) AS units_borrowed
FROM Issue_Records i JOIN Users u ON u.user_id = i.user_id
GROUP BY u.user_id, u.full_name
HAVING COUNT(*) > 1;
```

**Output**

```
full_name     times_borrowed  units_borrowed
------------  --------------  --------------
Aarav Sharma  2               7
```

## Q8: LEFT JOIN + aggregate: issue history of user 4 with total returned

LEFT JOIN keeps issues that have no return rows yet.

```sql
SELECT i.issue_id, e.equipment_name, i.quantity_issued, COALESCE(SUM(r.quantity_returned), 0) AS returned,
       i.status, i.issue_date, i.due_date
FROM Issue_Records i
JOIN Equipment e ON e.equipment_id = i.equipment_id
LEFT JOIN Return_Records r ON r.issue_id = i.issue_id
WHERE i.user_id = 4
GROUP BY i.issue_id
ORDER BY i.issue_date;
```

**Output**

```
issue_id  equipment_name      quantity_issued  returned  status              issue_date  due_date  
--------  ------------------  ---------------  --------  ------------------  ----------  ----------
1         Digital Multimeter  3                1         Partially Returned  2026-10-10  2026-10-17
5         Vernier Caliper     4                0         Issued              2026-10-26  2026-11-02
```

## Q9: Maintenance cost: SUM of completed repairs per equipment (money stored as paise)



```sql
SELECT e.equipment_name, COUNT(*) AS repairs, SUM(m.repair_cost_paise) / 100.0 AS total_cost_rupees
FROM Maintenance_Records m JOIN Equipment e ON e.equipment_id = m.equipment_id
WHERE m.status = 'Completed'
GROUP BY e.equipment_id;
```

**Output**

```
equipment_name      repairs  total_cost_rupees
------------------  -------  -----------------
Digital Multimeter  1        1500.0
```

## Q10: Most borrowed equipment (aggregate + ORDER BY + LIMIT)



```sql
SELECT e.equipment_name, SUM(i.quantity_issued) AS units_issued
FROM Issue_Records i JOIN Equipment e ON e.equipment_id = i.equipment_id
GROUP BY e.equipment_id
ORDER BY units_issued DESC
LIMIT 3;
```

**Output**

```
equipment_name      units_issued
------------------  ------------
Desktop Computer    5           
Vernier Caliper     4           
Digital Multimeter  3
```

## Q11: Inventory consistency check: total = available + issued + under maintenance

Every row must say OK. This is the invariant the C++ transactions protect.

```sql
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
```

**Output**

```
equipment_name       total  available  issued  in_repair  check_result
-------------------  -----  ---------  ------  ---------  ------------
Digital Multimeter   20     18         2       0          OK          
Oscilloscope 100MHz  8      7          0       1          OK          
Function Generator   6      6          0       0          OK          
Arduino Uno Kit      15     13         2       0          OK          
Raspberry Pi 4       10     10         0       0          OK          
Soldering Station    12     11         0       1          OK          
Desktop Computer     30     25         5       0          OK          
Compound Microscope  10     10         0       0          OK          
Vernier Caliper      25     21         4       0          OK
```

## Q12: Transaction demo: issue 2 Raspberry Pi, look at the result, then ROLLBACK

BEGIN ... ROLLBACK undoes everything, so the data is unchanged afterwards.

```sql
BEGIN;
UPDATE Equipment SET available_quantity = available_quantity - 2 WHERE equipment_id = 5 AND available_quantity >= 2;
INSERT INTO Issue_Records (user_id, equipment_id, issued_by, quantity_issued, due_date) VALUES (4, 5, 2, 2, date('now','localtime','+7 days'));
SELECT 'inside transaction' AS moment, available_quantity FROM Equipment WHERE equipment_id = 5;
ROLLBACK;
SELECT 'after rollback' AS moment, available_quantity FROM Equipment WHERE equipment_id = 5;
```

**Output**

```
moment              available_quantity
------------------  ------------------
inside transaction  8                 
moment          available_quantity
--------------  ------------------
after rollback  10
```

## Q13: Constraint demo: the database itself refuses impossible data

(run with foreign keys ON, which the C++ server always does)

```sql
PRAGMA foreign_keys = ON;
UPDATE Equipment SET available_quantity = 999 WHERE equipment_id = 1;
```

**Output**

```
Runtime error near line 2: CHECK constraint failed: available_quantity >= 0 AND available_quantity <= total_quantity (19)
```

## Q14: Index usage: EXPLAIN QUERY PLAN shows SQLite uses idx_issue_user instead of scanning the whole table



```sql
EXPLAIN QUERY PLAN SELECT * FROM Issue_Records WHERE user_id = 4;
```

**Output**

```
QUERY PLAN
`--SEARCH Issue_Records USING INDEX idx_issue_user (user_id=?)
```

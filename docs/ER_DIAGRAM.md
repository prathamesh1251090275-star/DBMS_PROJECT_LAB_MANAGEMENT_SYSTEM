# ER Diagram and Relational Schema

```mermaid
erDiagram
    LABS       ||--o{ EQUIPMENT : houses
    CATEGORIES ||--o{ EQUIPMENT : classifies
    EQUIPMENT  ||--o{ ISSUE_RECORDS : "is issued in"
    EQUIPMENT  ||--o{ MAINTENANCE_RECORDS : "is repaired in"
    USERS      ||--o{ ISSUE_RECORDS : "borrows (user_id)"
    USERS      ||--o{ ISSUE_RECORDS : "issues (issued_by)"
    ISSUE_RECORDS ||--o{ RETURN_RECORDS : "is returned by"
    USERS      ||--o{ RETURN_RECORDS : "receives (received_by)"
    USERS      ||--o{ MAINTENANCE_RECORDS : "reports (reported_by)"

    LABS { int lab_id PK  text lab_name UK  text location  text description }
    CATEGORIES { int category_id PK  text category_name UK  text description }
    USERS { int user_id PK  text full_name  text department  text role  text email UK  text phone  text username UK  text password_hash  int is_active }
    EQUIPMENT { int equipment_id PK  text equipment_name  int category_id FK  int lab_id FK  int total_quantity  int available_quantity  text condition_status  text purchase_date  text description }
    ISSUE_RECORDS { int issue_id PK  int user_id FK  int equipment_id FK  int issued_by FK  int quantity_issued  text issue_date  text due_date  text status }
    RETURN_RECORDS { int return_id PK  int issue_id FK  int quantity_returned  text return_date  int received_by FK  text remarks }
    MAINTENANCE_RECORDS { int maintenance_id PK  int equipment_id FK  int quantity_affected  text problem_description  int reported_by FK  text reported_date  text maintenance_date  text completed_date  int repair_cost_paise  text status }
```

## Text version (draw this in the viva)
```
 Labs 1───N Equipment N───1 Categories
              │1                 
      ┌───────┴────────┐
      N                N
 Issue_Records   Maintenance_Records
      │1
      N
 Return_Records
 Users 1───N Issue_Records (borrower)   Users 1───N Issue_Records (issued_by)
 Users 1───N Return_Records (received_by)   Users 1───N Maintenance_Records (reported_by)
```

## Relational schema (PK underlined with *, FK → target)
* **Labs**(*lab_id*, lab_name UNIQUE, location, description)
* **Categories**(*category_id*, category_name UNIQUE, description)
* **Users**(*user_id*, full_name, department, role ∈ {Admin, LabAssistant, Student, Faculty}, email UNIQUE, phone, username UNIQUE, password_hash, is_active, created_at)
* **Equipment**(*equipment_id*, equipment_name, category_id → Categories, lab_id → Labs, total_quantity ≥ 0, available_quantity with 0 ≤ available ≤ total, condition_status ∈ {Good, Fair, Poor}, purchase_date, description)
* **Issue_Records**(*issue_id*, user_id → Users, equipment_id → Equipment, issued_by → Users, quantity_issued > 0, issue_date, due_date ≥ issue_date, status ∈ {Issued, Partially Returned, Returned})
* **Return_Records**(*return_id*, issue_id → Issue_Records, quantity_returned > 0, return_date, received_by → Users, remarks)
* **Maintenance_Records**(*maintenance_id*, equipment_id → Equipment, quantity_affected > 0, problem_description, reported_by → Users, reported_date, maintenance_date, completed_date, repair_cost_paise ≥ 0, status ∈ {Reported, In Progress, Completed, Written Off})
* **View Issue_Outstanding** = Issue_Records + computed `outstanding = quantity_issued − SUM(returns)`.

All relationships are one-to-many. There are no many-to-many relationships; Issue_Records is already the "borrowing event" table between Users and Equipment.

# College Lab Equipment Management System

A full-stack College Lab Equipment Management System with an SQLite database (3NF), Node.js / Express backend, and an HTML/CSS/vanilla JS single-page frontend.

---

## 🌐 Live Application Links

- **Development URL**: [https://ais-dev-trysu2depusolfezm4w4dc-986672292917.asia-east1.run.app](https://ais-dev-trysu2depusolfezm4w4dc-986672292917.asia-east1.run.app)
- **Shared / Preview URL**: [https://ais-pre-trysu2depusolfezm4w4dc-986672292917.asia-east1.run.app](https://ais-pre-trysu2depusolfezm4w4dc-986672292917.asia-east1.run.app)

---

## 👥 Demo Accounts & Passwords

The system comes pre-seeded with sample users for all roles. All credentials are fully active and tested:

| Role | Username | Password | Features / Permissions |
|---|---|---|---|
| **Admin** | `admin` | `AdminPass#2026` *(or `admin123`)* | Full access: Dashboard, Equipment CRUD, Issue & Return, Maintenance approvals, Reports, Laboratories, Categories, and User management. |
| **Lab Assistant** | `rahul.assist` | `Assist#2026pw` *(or `rahul123`)* | Dashboard, View Equipment, Issue equipment to students/faculty, Process returns, Report damaged items, and View reports. |
| **Lab Assistant (Alt)** | `sneha.assist` | `Assist#2026pw` | Same permissions as rahul.assist. |
| **Student** | `aarav` | `Student#2026pw` *(or `student123`)* | View available equipment and view personal issued equipment history (`My Issues`). |
| **Student** | `priya` | `Student#2026pw` | View equipment and personal issue history. |
| **Student** | `rohan` | `Student#2026pw` | View equipment and personal issue history. |
| **Faculty** | `anil` | `Faculty#2026pw` | View equipment and personal issue history. |
| **Faculty** | `neha` | `Faculty#2026pw` | View equipment and personal issue history. |

---

## 🚀 How to Run the Project Locally

### Prerequisites
- **Node.js**: v20 or v22+ (uses built-in `node:sqlite`)
- **npm**: v10+

### Step-by-Step Instructions

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/prathamesh1251090275-star/Lab-Management-system.git
   cd Lab-Management-system
   ```

2. **Install Dependencies**:
   ```bash
   npm install
   ```

3. **Start the Development Server**:
   ```bash
   npm run dev
   ```
   The application will start on **`http://localhost:3000`** (or port specified in `$PORT`).

4. **Build for Production**:
   ```bash
   npm run build
   ```

5. **Start Production Server**:
   ```bash
   npm start
   ```

---

## 📂 Project Architecture

```
Lab-Management-system/
├── database/
│   ├── schema.sql             # Exact 3NF SQLite schema with foreign keys and view
│   ├── sample_data.sql        # Preloaded labs, categories, equipment, and users
│   └── LabEquipmentDB.db      # Live SQLite database file
├── public/
│   ├── css/style.css          # Clean college-themed styling
│   └── js/app.js              # Single-page application logic with API client
├── server.ts                  # Express backend with SQLite, CORS, and auth
├── index.html                 # Main frontend entry point
├── package.json               # Node.js dependencies and scripts
└── README.md                  # Project documentation & demo credentials
```

---

## 🔌 API Endpoints Summary

All API endpoints are under `/api`:

### Authentication & Sessions
- `POST /api/auth/login` - Sign in (`{ username, password }`), returns `{ user_id, full_name, role, token }`
- `POST /api/auth/logout` - Sign out
- `GET /api/auth/me` - Get current session user

### Dashboard & Analytics
- `GET /api/dashboard` - Real-time statistics (total types, total count, available, issued, under maintenance, overdue)

### Equipment
- `GET /api/equipment` - Filter by query `q`, `lab_id`, `category_id`, or `condition`
- `GET /api/equipment/:id` - Fetch single item
- `POST /api/equipment` - Create equipment (Admin)
- `PUT /api/equipment/:id` - Update equipment (Admin)
- `DELETE /api/equipment/:id` - Delete equipment with history protection (Admin)

### Issues & Returns
- `GET /api/issues` - Filter by status (`outstanding`, `Issued`, `Returned`)
- `POST /api/issues` - Issue equipment (Admin, Lab Assistant)
- `POST /api/issues/:id/returns` - Record return (Admin, Lab Assistant)

### Maintenance
- `GET /api/maintenance` - View open or all maintenance records
- `POST /api/maintenance` - Report damaged units (Admin, Lab Assistant)
- `PUT /api/maintenance/:id` - Update status (`In Progress`, `Completed`, `Written Off`) and repair cost (Admin)

### Lookups & Users
- `GET /api/labs`, `POST /api/labs`, `PUT /api/labs/:id`, `DELETE /api/labs/:id`
- `GET /api/categories`, `POST /api/categories`, `PUT /api/categories/:id`, `DELETE /api/categories/:id`
- `GET /api/users`, `POST /api/users`, `PUT /api/users/:id`, `DELETE /api/users/:id`

### Reports
- `GET /api/reports/inventory` - Available inventory
- `GET /api/reports/issued` - Currently issued items
- `GET /api/reports/overdue` - Overdue items with days late
- `GET /api/reports/maintenance` - Equipment under repair
- `GET /api/reports/by-lab` - Equipment grouped by lab
- `GET /api/reports/user-history/:id` - Borrowing history by user

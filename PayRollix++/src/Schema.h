#pragma once
// Keep names and types compatible with existing Payrollix++ SQLite databases.
inline constexpr char Schema[] = R"SQL(
CREATE TABLE IF NOT EXISTS users (
 id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL,
 password_hash TEXT NOT NULL, full_name TEXT, role TEXT DEFAULT 'user',
 created_at DATETIME DEFAULT CURRENT_TIMESTAMP, last_login DATETIME);
CREATE TABLE IF NOT EXISTS departments (
 id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, description TEXT,
 manager_id INTEGER, created_at DATETIME DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS pay_periods (
 id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, type TEXT NOT NULL,
 start_date DATE, end_date DATE, pay_date DATE, status TEXT DEFAULT 'open',
 created_at DATETIME DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS payroll (
 id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, gross_salary REAL DEFAULT 0,
 net REAL DEFAULT 0, federal_tax REAL DEFAULT 0, state_tax REAL DEFAULT 0,
 social_security REAL DEFAULT 0, medicare REAL DEFAULT 0, health_insurance REAL DEFAULT 0,
 retirement_401k REAL DEFAULT 0, other_deductions REAL DEFAULT 0, overtime_hours REAL DEFAULT 0,
 overtime_rate REAL DEFAULT 1.5, overtime_pay REAL DEFAULT 0, hourly_rate REAL DEFAULT 0,
 regular_hours REAL DEFAULT 0, bio TEXT, role TEXT, company TEXT, phone TEXT, email TEXT,
 address TEXT, notes TEXT, department_id INTEGER, pay_period_id INTEGER, hire_date DATE,
 status TEXT DEFAULT 'active', created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 FOREIGN KEY(department_id) REFERENCES departments(id),
 FOREIGN KEY(pay_period_id) REFERENCES pay_periods(id));
CREATE TABLE IF NOT EXISTS employment_history (
 id INTEGER PRIMARY KEY AUTOINCREMENT, employee_id INTEGER NOT NULL, event_type TEXT NOT NULL,
 previous_value TEXT, new_value TEXT, effective_date DATE, notes TEXT,
 created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 FOREIGN KEY(employee_id) REFERENCES payroll(id));
CREATE TABLE IF NOT EXISTS audit_log (
 id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, action TEXT NOT NULL,
 table_name TEXT, record_id INTEGER, details TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
 FOREIGN KEY(user_id) REFERENCES users(id));
CREATE TABLE IF NOT EXISTS payslips (
 id INTEGER PRIMARY KEY AUTOINCREMENT, employee_id INTEGER NOT NULL, pay_period_id INTEGER,
 gross_pay REAL, net_pay REAL, deductions_json TEXT,
 generated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 FOREIGN KEY(employee_id) REFERENCES payroll(id),
 FOREIGN KEY(pay_period_id) REFERENCES pay_periods(id));
CREATE TABLE IF NOT EXISTS winui_payslip_details (
 payslip_id INTEGER PRIMARY KEY, employee_name TEXT, employee_role TEXT, company TEXT,
 department TEXT, base REAL, overtime REAL, federal REAL, state REAL, ss REAL, medicare REAL,
 health REAL, retirement REAL, other REAL,
 FOREIGN KEY(payslip_id) REFERENCES payslips(id));
)SQL";

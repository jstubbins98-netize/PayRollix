#include "Store.h"
#include "Security.h"
#include "PayrollCalculator.h"
#include "Schema.h"
#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h> // Portable core tests only; the desktop app is Windows-only.
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace payroll {
std::string Get(Row const& r, std::string const& k) {
    auto it = r.find(k);
    return it == r.end() ? "" : it->second;
}
double Amount(Row const& r, std::string const& k) {
    auto s = Get(r, k);
    if (s.empty()) return 0;
    size_t used = 0;
    double n = std::stod(s, &used);
    if (used != s.size() || !std::isfinite(n)) throw std::runtime_error("Invalid number: " + k);
    return n;
}
int Id(Row const& r, std::string const& k) { return static_cast<int>(Amount(r, k)); }
std::string Decimal(double n) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(2) << n;
    return out.str();
}
bool ValidDate(std::string const& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
    if (y < 1900 || m < 1 || m > 12) return false;
    int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) days[1] = 29;
    return d >= 1 && d <= days[m - 1];
}
namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
Statement Prepare(sqlite3* db, std::string const& sql, Params const& params) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
    Statement statement(raw, sqlite3_finalize);
    if (sqlite3_bind_parameter_count(raw) != static_cast<int>(params.size()))
        throw std::runtime_error("Database parameter count mismatch.");
    for (size_t i = 0; i < params.size(); ++i) {
        int index = static_cast<int>(i + 1), result;
        auto const& v = params[i];
        if (std::holds_alternative<std::nullptr_t>(v)) result = sqlite3_bind_null(raw, index);
        else if (auto p = std::get_if<int>(&v)) result = sqlite3_bind_int(raw, index, *p);
        else if (auto p = std::get_if<double>(&v)) result = sqlite3_bind_double(raw, index, *p);
        else {
            auto const& s = std::get<std::string>(v);
            result = sqlite3_bind_text(raw, index, s.data(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
        }
        if (result != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    }
    return statement;
}
void PasswordPolicy(std::string const& p) {
    if (p.size() < 12) throw std::runtime_error("Use a password with at least 12 characters.");
}
}
Store::Store(std::filesystem::path path) : path_(std::move(path)) { Open(); }
Store::~Store() { if (db_) sqlite3_close(db_); }
void Store::Open() {
    auto filename = path_.u8string();
    int result = sqlite3_open_v2(filename.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (result != SQLITE_OK) {
        std::string error = db_ ? sqlite3_errmsg(db_) : "Cannot allocate database connection.";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(error);
    }
    sqlite3_busy_timeout(db_, 5000);
    // Legacy databases can contain dangling references. Do not silently delete those records.
    // New writes validate relationships in the service operations.
    Exec("PRAGMA foreign_keys=OFF");
    Exec("PRAGMA synchronous=FULL");
}
Rows Store::Query(std::string const& sql, Params const& p) {
    auto s = Prepare(db_, sql, p);
    Rows rows;
    int result;
    while ((result = sqlite3_step(s.get())) == SQLITE_ROW) {
        Row row;
        for (int i = 0; i < sqlite3_column_count(s.get()); ++i) {
            auto text = sqlite3_column_text(s.get(), i);
            row[sqlite3_column_name(s.get(), i)] = text ? reinterpret_cast<char const*>(text) : "";
        }
        rows.push_back(std::move(row));
    }
    if (result != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
    return rows;
}
int Store::Exec(std::string const& sql, Params const& p) {
    auto s = Prepare(db_, sql, p);
    if (sqlite3_step(s.get()) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}
void Store::Transaction(std::function<void()> const& work) {
    Exec("BEGIN IMMEDIATE");
    try { work(); Exec("COMMIT"); }
    catch (...) { sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); throw; }
}
void Store::Initialize() {
    Transaction([&] {
        char* error = nullptr;
        if (sqlite3_exec(db_, Schema, nullptr, nullptr, &error) != SQLITE_OK) {
            std::string message = error ? error : "Schema creation failed.";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    });
}
void Store::Audit(std::string const& a, std::string const& t, int id, std::string const& d) {
    Value user = userId_ ? Value(userId_) : Value(nullptr);
    Exec("INSERT INTO audit_log(user_id,action,table_name,record_id,details) VALUES(?,?,?,?,?)",
         {user, a, t, id, d});
}
bool Store::HasUsers() { return !Query("SELECT id FROM users LIMIT 1").empty(); }
void Store::CreateAdmin(std::string const& u, std::string const& p) {
    if (u.empty() || u.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("Enter a username.");
    PasswordPolicy(p);
    Transaction([&] {
        if (HasUsers()) throw std::runtime_error("An administrator already exists. Sign in instead.");
        Exec("INSERT INTO users(username,password_hash,full_name,role) VALUES(?,?,?,'admin')",
             {u, security::Hash(p), u});
        Audit("CREATE", "users", 0, "Initial administrator created");
    });
}
bool Store::Login(std::string const& u, std::string const& p) {
    auto rows = Query("SELECT id,password_hash FROM users WHERE username=?", {u});
    if (rows.empty() || !security::Verify(p, Get(rows.front(), "password_hash"))) return false;
    int newId = Id(rows.front());
    Transaction([&] {
        // Upgrade the original Qt unsalted SHA-256 hash after a successful login.
        if (Get(rows.front(), "password_hash").rfind("pbkdf2$", 0) != 0)
            Exec("UPDATE users SET password_hash=? WHERE id=?", {security::Hash(p), newId});
        Exec("UPDATE users SET last_login=CURRENT_TIMESTAMP WHERE id=?", {newId});
        Exec("INSERT INTO audit_log(user_id,action,table_name,record_id,details) VALUES(?,'LOGIN','users',?,'Signed in')", {newId, newId});
    });
    userId_ = newId;
    username_ = u;
    return true;
}
void Store::ChangePassword(std::string const& oldPassword, std::string const& p) {
    PasswordPolicy(p);
    auto r = Query("SELECT password_hash FROM users WHERE id=?", {userId_});
    if (r.empty() || !security::Verify(oldPassword, Get(r[0], "password_hash")))
        throw std::runtime_error("The current password is incorrect.");
    Transaction([&] {
        Exec("UPDATE users SET password_hash=? WHERE id=?", {security::Hash(p), userId_});
        Audit("UPDATE", "users", userId_, "Changed password");
    });
}
void Store::SignOut() {
    if (userId_) Audit("LOGOUT", "users", userId_, "Signed out");
    userId_ = 0;
    username_.clear();
}
int Store::SaveEmployee(Row const& f, int id) {
    if (Get(f, "name").find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("Employee name is required.");
    if (!ValidDate(Get(f, "hire_date"))) throw std::runtime_error("Enter a valid hire date (YYYY-MM-DD).");
    auto status = Get(f, "status");
    if (status != "active" && status != "inactive" && status != "terminated")
        throw std::runtime_error("Select an employee status.");
    for (auto key : {"gross_salary","hourly_rate","overtime_hours","overtime_rate",
                     "health_insurance","retirement_401k","other_deductions","regular_hours"}) {
        double n = Amount(f, key);
        if (n < 0 || n > 1e9) throw std::runtime_error(std::string("Out-of-range amount: ") + key);
    }
    if (Amount(f, "overtime_rate") < 1 || Amount(f, "overtime_rate") > 5)
        throw std::runtime_error("Overtime multiplier must be between 1 and 5.");
    int department = Id(f, "department_id");
    if (department && Query("SELECT id FROM departments WHERE id=?", {department}).empty())
        throw std::runtime_error("The selected department no longer exists.");
    auto stub = PayrollCalculator::calculate(Amount(f,"gross_salary"), Amount(f,"overtime_hours"),
        Amount(f,"hourly_rate"), Amount(f,"overtime_rate"), Amount(f,"health_insurance"),
        Amount(f,"retirement_401k"), Amount(f,"other_deductions"));
    std::vector<std::string> keys = {"name","role","company","phone","email","address","bio","notes",
        "hire_date","status","gross_salary","hourly_rate","overtime_hours","overtime_rate","regular_hours",
        "health_insurance","retirement_401k","other_deductions"};
    Params params;
    for (auto const& k : keys) params.push_back(Get(f, k));
    params.push_back(department ? Value(department) : Value(nullptr));
    for (double v : {stub.net,stub.federalTax,stub.stateTax,stub.socialSecurity,stub.medicare,stub.overtimePay})
        params.push_back(v);
    std::vector<std::string> all = keys;
    for (auto k : {"department_id","net","federal_tax","state_tax","social_security","medicare","overtime_pay"})
        all.push_back(k);
    Transaction([&] {
        Row old;
        if (id) {
            auto rows = Query("SELECT * FROM payroll WHERE id=?", {id});
            if (rows.empty()) throw std::runtime_error("Employee no longer exists.");
            old = rows.front();
        }
        std::string sql;
        if (id) {
            sql = "UPDATE payroll SET ";
            for (auto const& k : all) sql += k + "=?,";
            sql += "updated_at=CURRENT_TIMESTAMP WHERE id=?";
            params.push_back(id);
        } else {
            sql = "INSERT INTO payroll(";
            std::string marks;
            for (auto const& k : all) { sql += k + ","; marks += "?,"; }
            sql.back() = ')';
            marks.back() = ')';
            sql += " VALUES(" + marks;
        }
        int inserted = Exec(sql, params);
        if (!id) id = inserted;
        for (auto k : {"gross_salary","role","status"}) {
            bool changed = !old.empty() && (std::string(k) == "gross_salary"
                ? std::abs(Amount(old,k) - Amount(f,k)) > 0.000001 : Get(old,k) != Get(f,k));
            if (changed)
                Exec("INSERT INTO employment_history(employee_id,event_type,previous_value,new_value,effective_date,notes) "
                     "VALUES(?,?,?,?,date('now'),'Recorded when employee was edited')",
                     {id, std::string(k), Get(old,k), Get(f,k)});
        }
        Audit(old.empty() ? "CREATE" : "UPDATE", "payroll", id, "Saved employee " + Get(f, "name"));
    });
    return id;
}
void Store::DeleteEmployee(int id) {
    Transaction([&] {
        if (!Query("SELECT id FROM payslips WHERE employee_id=? LIMIT 1", {id}).empty())
            throw std::runtime_error("This employee has payslips. Set their status to terminated instead of deleting payroll history.");
        if (Query("SELECT id FROM payroll WHERE id=?", {id}).empty())
            throw std::runtime_error("Employee no longer exists.");
        Exec("DELETE FROM employment_history WHERE employee_id=?", {id});
        Exec("UPDATE departments SET manager_id=NULL WHERE manager_id=?", {id});
        Exec("DELETE FROM payroll WHERE id=?", {id});
        Audit("DELETE", "payroll", id, "Deleted employee and employment notes");
    });
}
void Store::Process(std::vector<int> const& employees, int period) {
    if (employees.empty()) throw std::runtime_error("Select at least one employee.");
    Transaction([&] {
        auto p = Query("SELECT id FROM pay_periods WHERE id=? AND status='open'", {period});
        if (p.empty()) throw std::runtime_error("Select an open pay period.");
        for (int id : employees) {
            auto r = Query("SELECT p.*,COALESCE(d.name,'Unassigned') AS department FROM payroll p "
                           "LEFT JOIN departments d ON d.id=p.department_id "
                           "WHERE p.id=? AND (p.status='active' OR p.status IS NULL)", {id});
            if (r.empty()) throw std::runtime_error("A selected employee is not active. Refresh the list.");
            if (!Query("SELECT id FROM payslips WHERE employee_id=? AND pay_period_id=?", {id,period}).empty())
                throw std::runtime_error("Payroll already exists for an employee in this period. Nothing in this batch was changed.");
            auto const& e = r[0];
            auto s = PayrollCalculator::calculate(Amount(e,"gross_salary"),Amount(e,"overtime_hours"),
                Amount(e,"hourly_rate"),Amount(e,"overtime_rate") == 0 ? 1.5 : Amount(e,"overtime_rate"),
                Amount(e,"health_insurance"),Amount(e,"retirement_401k"),Amount(e,"other_deductions"));
            Exec("UPDATE payroll SET net=?,federal_tax=?,state_tax=?,social_security=?,medicare=?,"
                 "overtime_pay=?,pay_period_id=?,updated_at=CURRENT_TIMESTAMP WHERE id=?",
                 {s.net,s.federalTax,s.stateTax,s.socialSecurity,s.medicare,s.overtimePay,period,id});
            std::string json = "{\"federal\":"+Decimal(s.federalTax)+",\"state\":"+Decimal(s.stateTax)+
                ",\"ss\":"+Decimal(s.socialSecurity)+",\"medicare\":"+Decimal(s.medicare)+"}";
            int slip = Exec("INSERT INTO payslips(employee_id,pay_period_id,gross_pay,net_pay,deductions_json) "
                            "VALUES(?,?,?,?,?)", {id,period,s.gross,s.net,json});
            Exec("INSERT INTO winui_payslip_details(payslip_id,employee_name,employee_role,company,department,base,"
                 "overtime,federal,state,ss,medicare,health,retirement,other) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                 {slip,Get(e,"name"),Get(e,"role"),Get(e,"company"),Get(e,"department"),Amount(e,"gross_salary"),
                  s.overtimePay,s.federalTax,s.stateTax,s.socialSecurity,s.medicare,s.healthInsurance,s.retirement401k,s.otherDeductions});
        }
        Audit("BATCH_PROCESS", "payroll", period, "Processed " + std::to_string(employees.size()) + " employees");
    });
}
Row Store::Payslip(int employee, int period) {
    if (!period) {
        auto rows = Query("SELECT p.name AS employee_name,p.role AS employee_role,p.company,"
            "COALESCE(d.name,'Unassigned') AS department,p.gross_salary AS base,p.overtime_pay AS overtime,"
            "p.gross_salary+p.overtime_pay AS gross_pay,p.net AS net_pay,p.federal_tax AS federal,"
            "p.state_tax AS state,p.social_security AS ss,p.medicare,p.health_insurance AS health,"
            "p.retirement_401k AS retirement,p.other_deductions AS other,'Current estimate (not a processed payslip)' AS period "
            "FROM payroll p LEFT JOIN departments d ON d.id=p.department_id WHERE p.id=?", {employee});
        if (rows.empty()) throw std::runtime_error("Employee no longer exists.");
        return rows[0];
    }
    auto rows = Query("SELECT s.*,x.*,pp.name AS period,pp.start_date,pp.end_date,pp.pay_date "
                      "FROM payslips s LEFT JOIN winui_payslip_details x ON x.payslip_id=s.id "
                      "LEFT JOIN pay_periods pp ON pp.id=s.pay_period_id "
                      "WHERE s.employee_id=? AND s.pay_period_id=? ORDER BY s.id DESC LIMIT 1", {employee,period});
    if (rows.empty()) throw std::runtime_error("No processed payslip exists for this employee and pay period.");
    return rows[0];
}
void Store::CopyDatabase(sqlite3* destination, sqlite3* source) {
    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main");
    if (!backup) throw std::runtime_error(sqlite3_errmsg(destination));
    int result = sqlite3_backup_step(backup, -1);
    int finish = sqlite3_backup_finish(backup);
    if (result != SQLITE_DONE || finish != SQLITE_OK)
        throw std::runtime_error("Database copy failed; no successful backup/restore was reported.");
}
void Store::Backup(std::filesystem::path const& destination) {
    if (std::filesystem::exists(destination) && std::filesystem::equivalent(path_, destination))
        throw std::runtime_error("Choose a different file from the live database.");
    sqlite3* raw = nullptr;
    auto filename = destination.u8string();
    int result = sqlite3_open_v2(filename.c_str(), &raw, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, nullptr);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> output(raw, sqlite3_close);
    if (result != SQLITE_OK) throw std::runtime_error("Cannot open the backup destination.");
    CopyDatabase(raw, db_);
}
void Store::Restore(std::filesystem::path const& source) {
    if (std::filesystem::equivalent(path_, source)) throw std::runtime_error("Select a backup, not the live database.");
    sqlite3* raw = nullptr;
    auto filename = source.u8string();
    int result = sqlite3_open_v2(filename.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> input(raw, sqlite3_close);
    if (result != SQLITE_OK) throw std::runtime_error("Cannot open the backup.");
    sqlite3_stmt* check = nullptr;
    if (sqlite3_prepare_v2(raw,"PRAGMA integrity_check",-1,&check,nullptr) != SQLITE_OK)
        throw std::runtime_error("Cannot validate the backup.");
    Statement validation(check, sqlite3_finalize);
    if (sqlite3_step(check) != SQLITE_ROW ||
        std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0))) != "ok")
        throw std::runtime_error("Backup integrity check failed.");
    validation.reset();
    // Check every column used by the application before replacing any live contents.
    for (auto const& sql : {
        "SELECT id,username,password_hash,full_name,role,created_at,last_login FROM users LIMIT 0",
        "SELECT id,name,description,manager_id,created_at FROM departments LIMIT 0",
        "SELECT id,name,type,start_date,end_date,pay_date,status,created_at FROM pay_periods LIMIT 0",
        "SELECT id,name,gross_salary,net,federal_tax,state_tax,social_security,medicare,health_insurance,"
        "retirement_401k,other_deductions,overtime_hours,overtime_rate,overtime_pay,hourly_rate,regular_hours,"
        "bio,role,company,phone,email,address,notes,department_id,pay_period_id,hire_date,status,created_at,updated_at FROM payroll LIMIT 0",
        "SELECT id,employee_id,event_type,previous_value,new_value,effective_date,notes,created_at FROM employment_history LIMIT 0",
        "SELECT id,user_id,action,table_name,record_id,details,timestamp FROM audit_log LIMIT 0",
        "SELECT id,employee_id,pay_period_id,gross_pay,net_pay,deductions_json,generated_at FROM payslips LIMIT 0"}) {
        auto s = Prepare(raw, sql, {});
    }
    auto detailTable = Prepare(raw, "SELECT name FROM sqlite_master WHERE type='table' AND name='winui_payslip_details'", {});
    bool hasDetails = sqlite3_step(detailTable.get()) == SQLITE_ROW;
    detailTable.reset();
    if (hasDetails) {
        auto columns = Prepare(raw, "SELECT payslip_id,employee_name,employee_role,company,department,base,"
            "overtime,federal,state,ss,medicare,health,retirement,other FROM winui_payslip_details LIMIT 0", {});
    }
    auto safety = path_;
    safety += ".before-restore-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".db";
    Backup(safety);
    CopyDatabase(db_, raw);
    userId_ = 0;
    username_.clear();
    Initialize();
    Audit("RESTORE", "database", 0, "Restored database; previous contents saved in a safety backup");
}
} // namespace payroll

#include "Store.h"
#include "PayrollCalculator.h"
#include "Security.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <limits>

using namespace payroll;
namespace {
int checks = 0;
void Require(bool value, char const* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class F> void Reject(F&& f, char const* message) {
    bool rejected = false;
    try { f(); } catch (std::exception const&) { rejected = true; }
    Require(rejected,message);
}
void Near(double a, double b) { Require(std::abs(a-b) < 0.011,"Unexpected monetary calculation"); }
Row Employee(std::string name) {
    return {{"name",name},{"role","Analyst"},{"company","Example"},{"status","active"},
        {"hire_date","2026-01-01"},{"gross_salary","5000"},{"hourly_rate","25"},
        {"overtime_hours","10"},{"overtime_rate","1.5"},{"regular_hours","160"},
        {"health_insurance","100"},{"retirement_401k","200"},{"other_deductions","50"},
        {"department_id","0"}};
}
}
int main() {
    auto directory = std::filesystem::temp_directory_path() /
        ("payrollix-tests-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    try {
        {
            Store s(directory / "payroll.db");
            s.Initialize();
            Require(!s.HasUsers(),"Fresh database should not have default credentials");
            Reject([&]{ s.CreateAdmin("admin","short"); },"Weak new password accepted");
            s.CreateAdmin("admin","safe-test-password");
            Require(s.HasUsers(),"Administrator not created");
            Reject([&]{ s.CreateAdmin("another","safe-test-password"); },"Second bootstrap administrator accepted");
            Require(!s.Login("admin","incorrect"),"Bad password accepted");
            Require(s.Login("admin","safe-test-password"),"Login failed");
            Require(s.UserId()>0,"User attribution missing");
            s.ChangePassword("safe-test-password","replacement-password");
            Require(!s.Login("admin","safe-test-password"),"Old password still accepted");
            Require(s.Login("admin","replacement-password"),"Changed password not accepted");
            Require(ValidDate("2024-02-29") && !ValidDate("2026-02-29"),"Leap day validation failed");
            Require(!ValidDate("2026-13-01") && !ValidDate("2026-01-00"),"Date validation failed");
            auto calculation = PayrollCalculator::calculate(5000,10,25,1.5,100,200,50);
            Near(calculation.gross,5375);
            Near(calculation.overtimePay,375);
            Near(calculation.net, calculation.gross-calculation.totalDeductions);
            auto zero = PayrollCalculator::calculate(0);
            Near(zero.net,0);
            auto high = PayrollCalculator::calculate(25000);
            Require(high.medicare>25000*0.0145,"Additional Medicare estimate missing");
            int department = s.Exec("INSERT INTO departments(name,description) VALUES('Engineering','Test')");
            auto employee = Employee("Renée \"Example\", Jr.\nPayroll");
            employee["department_id"] = std::to_string(department);
            int id = s.SaveEmployee(employee);
            Near(Amount(s.Query("SELECT net FROM payroll WHERE id=?", {id})[0],"net"),calculation.net);
            auto invalid = employee;
            invalid["gross_salary"] = "nan";
            Reject([&]{ s.SaveEmployee(invalid); },"Non-finite amount accepted");
            invalid["gross_salary"] = "-1";
            Reject([&]{ s.SaveEmployee(invalid); },"Negative salary accepted");
            invalid = employee;
            invalid["name"] = "   ";
            Reject([&]{ s.SaveEmployee(invalid); },"Blank name accepted");
            invalid = employee;
            invalid["department_id"] = "987654";
            Reject([&]{ s.SaveEmployee(invalid); },"Missing department accepted");
            employee["role"] = "Senior Analyst";
            s.SaveEmployee(employee,id);
            Require(s.Query("SELECT id FROM employment_history WHERE employee_id=?", {id}).size()==1,"Automatic history not created");
            int period = s.Exec("INSERT INTO pay_periods(name,type,start_date,end_date,pay_date) VALUES('January','Monthly','2026-01-01','2026-01-31','2026-02-01')");
            int second = s.SaveEmployee(Employee("Second Employee"));
            s.Process({id},period);
            auto snapshot = s.Payslip(id,period);
            Near(Amount(snapshot,"gross_pay"),5375);
            Require(Get(snapshot,"employee_role")=="Senior Analyst","Payslip identity snapshot missing");
            employee["role"]="Changed after payroll";
            employee["gross_salary"]="9000";
            s.SaveEmployee(employee,id);
            Near(Amount(s.Payslip(id,period),"gross_pay"),5375);
            Require(Get(s.Payslip(id,period),"employee_role")=="Senior Analyst","Historical snapshot changed after edit");
            Require(Amount(s.Payslip(id),"gross_pay")>9000,"Current estimate not recalculated");
            Reject([&]{ s.Process({second,id},period); },"Duplicate pay run accepted");
            Require(s.Query("SELECT id FROM payslips WHERE employee_id=?", {second}).empty(),"Failed batch was partially committed");
            Reject([&]{ s.DeleteEmployee(id); },"Employee with pay history deleted");
            s.Exec("UPDATE pay_periods SET status='closed' WHERE id=?", {period});
            Reject([&]{ s.Process({second},period); },"Closed period processed");
            s.Exec("UPDATE payroll SET status='inactive' WHERE id=?", {second});
            s.Exec("UPDATE pay_periods SET status='open' WHERE id=?", {period});
            Reject([&]{ s.Process({second},period); },"Inactive employee processed");
            s.Backup(directory/"backup.db");
            s.SaveEmployee(Employee("After backup"));
            Require(s.Query("SELECT id FROM payroll").size()==3,"Employee insert failed");
            s.Restore(directory/"backup.db");
            Require(s.Query("SELECT id FROM payroll").size()==2,"Restore did not replace database");
            Require(s.UserId()==0,"Restore did not sign out");
            bool foundSafety = false;
            for (auto const& entry : std::filesystem::directory_iterator(directory))
                if (entry.path().filename().string().find(".before-restore-")!=std::string::npos) foundSafety=true;
            Require(foundSafety,"Restore safety backup missing");
            Reject([&]{ s.Backup(directory/"payroll.db"); },"Live database overwritten by backup");
            Reject([&]{ s.Restore(directory/"payroll.db"); },"Live database accepted as restore source");
            {
                std::ofstream out(directory/"invalid.db"); out << "not a database";
            }
            Reject([&]{ s.Restore(directory/"invalid.db"); },"Invalid backup accepted");
            Require(s.Query("SELECT id FROM payroll").size()==2,"Invalid restore destroyed live data");
            int empty = s.SaveEmployee(Employee("Can delete"));
            s.DeleteEmployee(empty);
            Require(s.Query("SELECT id FROM payroll WHERE id=?", {empty}).empty(),"Employee delete failed");
            // Simulate a pre-WinUI database; additive initialization must preserve it.
            s.Exec("DROP TABLE winui_payslip_details");
            s.Backup(directory/"legacy.db");
            s.Restore(directory/"legacy.db");
            Require(s.Query("SELECT id FROM payroll").size()==2,"Legacy restore lost records");
            auto legacy = s.Payslip(id,period);
            Require(Get(legacy,"payslip_id").empty(),"Legacy slip incorrectly has full snapshot details");
            Require(!s.Query("SELECT id FROM audit_log WHERE user_id>0").empty(),"User-attributed audit records missing");
#ifdef _WIN32
            auto hash = security::Hash("Unicode password \xC3\xA9\xE2\x82\xAC");
            Require(security::Verify("Unicode password \xC3\xA9\xE2\x82\xAC",hash),"BCrypt round trip failed");
            Require(!security::Verify("wrong",hash),"BCrypt accepted wrong password");
            Require(hash!=security::Hash("Unicode password \xC3\xA9\xE2\x82\xAC"),"Password salts not random");
            Require(security::Verify("admin123","240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9"),"Legacy Qt SHA256 verification failed");
#endif
        }
        std::filesystem::remove_all(directory);
        std::cout << checks << " checks passed. "
#ifdef _WIN32
                  << "Includes native Windows BCrypt security tests.\n";
#else
                  << "Linux data-flow tests used a test-only password provider; BCrypt and WinUI require Windows.\n";
#endif
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "FAILED: " << error.what() << "\nTemporary test data: " << directory << '\n';
        return 1;
    }
}

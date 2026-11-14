#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>

struct sqlite3;
namespace payroll {
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;
using Value = std::variant<std::nullptr_t, int, double, std::string>;
using Params = std::vector<Value>;
std::string Get(Row const& row, std::string const& key);
double Amount(Row const& row, std::string const& key);
int Id(Row const& row, std::string const& key = "id");
std::string Decimal(double value);
bool ValidDate(std::string const& value);

class Store {
public:
    explicit Store(std::filesystem::path path);
    ~Store();
    Store(Store const&) = delete;
    Store& operator=(Store const&) = delete;
    Rows Query(std::string const& sql, Params const& params = {});
    int Exec(std::string const& sql, Params const& params = {});
    void Transaction(std::function<void()> const& work);
    void Initialize();
    void Audit(std::string const& action, std::string const& table, int id, std::string const& details);
    bool HasUsers();
    bool Login(std::string const& username, std::string const& password);
    void CreateAdmin(std::string const& username, std::string const& password);
    void ChangePassword(std::string const& oldPassword, std::string const& password);
    int SaveEmployee(Row const& fields, int id = 0);
    void DeleteEmployee(int id);
    void Process(std::vector<int> const& employees, int period);
    Row Payslip(int employee, int period = 0);
    void Backup(std::filesystem::path const& destination);
    void Restore(std::filesystem::path const& source);
    void SignOut();
    int UserId() const { return userId_; }
    std::string Username() const { return username_; }
    std::filesystem::path Path() const { return path_; }
private:
    void Open();
    void CopyDatabase(sqlite3* destination, sqlite3* source);
    sqlite3* db_ = nullptr;
    std::filesystem::path path_;
    int userId_ = 0;
    std::string username_;
};
} // namespace payroll

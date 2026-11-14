#pragma once

#include "Platform.h"
#include "Store.h"
#include <QMainWindow>
#include <QTableWidget>
#include <QWidget>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

class QVBoxLayout;
class QStatusBar;
namespace ui { class InputField; }

class AppWindow final : public QMainWindow {
public:
    AppWindow();

private:
    using Rows = payroll::Rows;
    using Row = payroll::Row;
    using GridRows = std::vector<std::vector<std::wstring>>;

    std::unique_ptr<payroll::Store> store_;
    QWidget* body_ = nullptr;
    QVBoxLayout* bodyLayout_ = nullptr;
    QStatusBar* status_ = nullptr;
    HWND hwnd_ = nullptr;
    unsigned failedLogins_ = 0;
    std::chrono::steady_clock::time_point retryAfter_{};

    void Message(std::wstring const& text, bool error = false);
    bool Confirm(std::wstring const& message);
    QWidget* Page(std::wstring const& title, std::wstring const& description = L"");
    void Display(QWidget* page);
    void Login();
    void Shell();
    void Navigate(std::wstring const& key);
    void Dashboard();
    void Employees();
    void Employee(int id = 0);
    void History(int id);
    void Payslip(int id);
    void Departments();
    void Periods();
    void Batch();
    void Reports();
    void Audit();
    void Backups();
    void Account();
    static GridRows Cells(Rows const&, std::vector<std::string> const&);
    static int SelectedId(QTableWidget const*, Rows const&);
};

#pragma once
#include "Store.h"
#include "Ui.h"
#include <memory>

class AppWindow {
public:
    AppWindow();
    void Activate() { window_.Activate(); }
private:
    using Rows = payroll::Rows;
    using Row = payroll::Row;
    using GridRows = std::vector<std::vector<std::wstring>>;
    winrt::Microsoft::UI::Xaml::Window window_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentControl body_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::InfoBar status_{nullptr};
    std::unique_ptr<payroll::Store> store_;
    HWND hwnd_ = nullptr;
    unsigned failedLogins_ = 0;
    ULONGLONG retryAfter_ = 0;
    void Message(std::wstring const& text, bool error = false);
    bool Confirm(std::wstring const& message);
    ui::StackPanel Page(std::wstring const& title, std::wstring const& description = L"");
    void Display(ui::StackPanel const& page);
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
    static int SelectedId(ui::ListView const&, Rows const&);
};

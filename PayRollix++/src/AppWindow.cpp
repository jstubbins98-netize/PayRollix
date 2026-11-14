#include "AppWindow.h"
#include "Platform.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <cmath>
#include <stdexcept>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace payroll;
namespace {
std::wstring Wide(std::string const& s) { return to_hstring(s).c_str(); }
}
AppWindow::AppWindow() {
    window_ = Window();
    window_.Title(L"Payrollix++ — Windows 11");
    auto native = window_.as<IWindowNative>();
    check_hresult(native->get_WindowHandle(&hwnd_));
    window_.AppWindow().Resize(Windows::Graphics::SizeInt32{1250,850});
    root_ = Grid();
    RowDefinition content;
    content.Height(GridLength{1,GridUnitType::Star});
    RowDefinition feedback;
    feedback.Height(GridLength{1,GridUnitType::Auto});
    root_.RowDefinitions().Append(content);
    root_.RowDefinitions().Append(feedback);
    body_ = ContentControl();
    body_.HorizontalContentAlignment(HorizontalAlignment::Stretch);
    body_.VerticalContentAlignment(VerticalAlignment::Stretch);
    status_ = InfoBar();
    status_.IsClosable(true);
    Grid::SetRow(status_,1);
    root_.Children().Append(body_);
    root_.Children().Append(status_);
    window_.Content(root_);
    ui::OnError = [this](std::wstring const& text) { Message(text,true); };
    store_ = std::make_unique<Store>(platform::DataPath());
    store_->Initialize();
    Login();
}
void AppWindow::Message(std::wstring const& text, bool error) {
    status_.Severity(error ? InfoBarSeverity::Error : InfoBarSeverity::Success);
    status_.Title(error ? L"Action could not be completed" : L"Payrollix++");
    status_.Message(text);
    status_.IsOpen(true);
}
bool AppWindow::Confirm(std::wstring const& message) {
    return MessageBoxW(hwnd_,message.c_str(),L"Confirm — Payrollix++",
                       MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2) == IDYES;
}
StackPanel AppWindow::Page(std::wstring const& title, std::wstring const& description) {
    auto panel = ui::Stack();
    panel.Margin(Thickness{28,24,28,32});
    ui::Heading(panel,title,description);
    return panel;
}
void AppWindow::Display(StackPanel const& page) { body_.Content(ui::Scroll(page)); }
AppWindow::GridRows AppWindow::Cells(Rows const& rows, std::vector<std::string> const& keys) {
    GridRows cells;
    for (auto const& row : rows) {
        std::vector<std::wstring> line;
        for (auto const& key : keys) line.push_back(Wide(Get(row,key)));
        cells.push_back(std::move(line));
    }
    return cells;
}
int AppWindow::SelectedId(ListView const& list, Rows const& rows) {
    auto index = list.SelectedIndex();
    if (index < 0 || static_cast<size_t>(index) >= rows.size())
        throw std::runtime_error("Select a record first.");
    return Id(rows[static_cast<size_t>(index)]);
}
void AppWindow::Login() {
    bool first = !store_->HasUsers();
    auto page = Page(first ? L"Set up Payrollix++" : L"Welcome back",
        first ? L"Create the first administrator, or import your existing Payrollix++ database. No default password is created."
              : L"Sign in to your local payroll workspace.");
    page.MaxWidth(650);
    page.HorizontalAlignment(HorizontalAlignment::Center);
    auto username = ui::Input(L"Username");
    PasswordBox password;
    password.Header(box_value(L"Password"));
    password.PasswordRevealMode(PasswordRevealMode::Peek);
    PasswordBox repeat;
    repeat.Header(box_value(L"Confirm password"));
    page.Children().Append(username);
    page.Children().Append(password);
    if (first) page.Children().Append(repeat);
    page.Children().Append(ui::Action(first ? L"Create administrator" : L"Sign in",
        [this,username,password,repeat,first] {
            auto user = to_string(username.Text()), pass = to_string(password.Password());
            if (first) {
                if (password.Password() != repeat.Password()) throw std::runtime_error("Passwords do not match.");
                store_->CreateAdmin(user,pass);
                Login();
                Message(L"Administrator created. Sign in to continue.");
                return;
            }
            if (GetTickCount64() < retryAfter_) throw std::runtime_error("Too many attempts. Wait 30 seconds before retrying.");
            if (!store_->Login(user,pass)) {
                if (++failedLogins_ >= 5) { retryAfter_ = GetTickCount64()+30000; failedLogins_ = 0; }
                password.Password(L"");
                throw std::runtime_error("Incorrect username or password.");
            }
            failedLogins_ = 0;
            password.Password(L"");
            status_.IsOpen(false);
            Shell();
            if (pass == "admin123") {
                Account();
                Message(L"The original default password is still in use. Change it before entering company information.",true);
            }
        },true));
    if (first) {
        page.Children().Append(ui::Text(L"Existing users: select an untouched copy of payroll.db or a full database backup. The source is not modified."));
        page.Children().Append(ui::Action(L"Import existing database",[this] {
            auto file = platform::ChooseFile(hwnd_,false,L"db",L"Import existing Payrollix++ database");
            if (file.empty()) return;
            store_->Restore(file);
            Login();
            Message(L"Database imported. Sign in with its existing credentials.");
        }));
    }
    page.Children().Append(ui::Text(L"Data stays on this Windows account. This app does not submit taxes, transfer wages, or provide certified tax calculations."));
    Display(page);
}
void AppWindow::Shell() {
    NavigationView nav;
    nav.PaneTitle(L"Payrollix++");
    nav.IsSettingsVisible(false);
    nav.IsBackButtonVisible(NavigationViewBackButtonVisible::Collapsed);
    nav.PaneDisplayMode(NavigationViewPaneDisplayMode::Left);
    nav.OpenPaneLength(240);
    std::vector<std::pair<std::wstring,std::wstring>> pages = {
        {L"Dashboard",L"dashboard"},{L"Employees",L"employees"},{L"Add employee",L"add"},
        {L"Departments",L"departments"},{L"Pay periods",L"periods"},{L"Batch payroll",L"batch"},
        {L"Reports",L"reports"},{L"Audit log",L"audit"},{L"Backup and restore",L"backups"},
        {L"Account",L"account"},{L"Sign out",L"logout"}};
    for (auto const& [label,key] : pages) {
        NavigationViewItem item;
        item.Content(box_value(hstring{label}));
        item.Tag(box_value(hstring{key}));
        nav.MenuItems().Append(item);
    }
    ContentControl host;
    host.HorizontalContentAlignment(HorizontalAlignment::Stretch);
    host.VerticalContentAlignment(VerticalAlignment::Stretch);
    nav.Content(host);
    // Keep the outer root and status bar while replacing only the content area.
    auto outer = root_.Children().GetAt(0).as<ContentControl>();
    outer.Content(nav);
    body_ = host;
    nav.ItemInvoked([this](auto const&, NavigationViewItemInvokedEventArgs const& args) {
        try {
            if (auto item = args.InvokedItemContainer())
                Navigate(unbox_value<hstring>(item.Tag()).c_str());
        } catch (std::exception const& e) { Message(Wide(e.what()),true); }
          catch (hresult_error const& e) { Message(e.message().c_str(),true); }
    });
    nav.SelectedItem(nav.MenuItems().GetAt(0));
    window_.Title(to_hstring("Payrollix++ — " + store_->Username()));
    Dashboard();
}
void AppWindow::Navigate(std::wstring const& key) {
    status_.IsOpen(false);
    if (key == L"dashboard") Dashboard();
    else if (key == L"employees") Employees();
    else if (key == L"add") Employee();
    else if (key == L"departments") Departments();
    else if (key == L"periods") Periods();
    else if (key == L"batch") Batch();
    else if (key == L"reports") Reports();
    else if (key == L"audit") Audit();
    else if (key == L"backups") Backups();
    else if (key == L"account") Account();
    else if (key == L"logout") {
        store_->SignOut();
        body_ = root_.Children().GetAt(0).as<ContentControl>();
        window_.Title(L"Payrollix++ — Windows 11");
        Login();
    }
}
void AppWindow::Dashboard() {
    auto page = Page(L"Payroll overview", L"Current employee totals, not a historical pay-run report. Amounts are USD.");
    auto rows = store_->Query("SELECT COUNT(*) AS employees,COALESCE(SUM(gross_salary+overtime_pay),0) AS gross,"
                              "COALESCE(SUM(net),0) AS net FROM payroll WHERE status='active' OR status IS NULL");
    auto const& row = rows[0];
    ui::Heading(page, Wide(Get(row,"employees")) + L" active employees");
    page.Children().Append(ui::Text(L"Gross including overtime: $" + Wide(Decimal(Amount(row,"gross"))),20));
    page.Children().Append(ui::Text(L"Estimated net payroll: $" + Wide(Decimal(Amount(row,"net"))),20));
    auto period = store_->Query("SELECT name,start_date,end_date,pay_date FROM pay_periods WHERE status='open' ORDER BY start_date DESC LIMIT 1");
    page.Children().Append(ui::Text(period.empty() ? L"No open pay period. Create one under Pay periods."
        : L"Latest open period: " + Wide(Get(period[0],"name")) + L" | Pay date: " + Wide(Get(period[0],"pay_date"))));
    page.Children().Append(ui::Action(L"Refresh", [this]{ Dashboard(); }));
    ui::Heading(page,L"Recent activity");
    auto activity = store_->Query("SELECT timestamp,action,details FROM audit_log ORDER BY id DESC LIMIT 10");
    page.Children().Append(ui::Table({L"When (UTC)",L"Action",L"Details"},Cells(activity,{"timestamp","action","details"})));
    page.Children().Append(ui::Text(L"Tax calculation limits: the inherited model assumes monthly salary, uses fixed federal brackets and a flat 5% state rate. Have payroll professionals validate it before business use."));
    Display(page);
}
void AppWindow::Account() {
    auto page = Page(L"Account",L"Signed in as " + Wide(store_->Username()) + L". Change your local application password here.");
    PasswordBox current, next, repeat;
    current.Header(box_value(L"Current password"));
    next.Header(box_value(L"New password (at least 12 characters)"));
    repeat.Header(box_value(L"Confirm new password"));
    for (auto const& item : {current,next,repeat}) page.Children().Append(item);
    page.Children().Append(ui::Action(L"Change password",[this,current,next,repeat] {
        if (next.Password() != repeat.Password()) throw std::runtime_error("Passwords do not match.");
        store_->ChangePassword(to_string(current.Password()),to_string(next.Password()));
        current.Password(L""); next.Password(L""); repeat.Password(L"");
        Message(L"Password changed.");
    },true));
    page.Children().Append(ui::Text(L"Passwords use Windows BCrypt PBKDF2-SHA256 with a random salt. Existing SHA-256 passwords are upgraded on successful sign-in. Local database access is governed by Windows folder permissions; this is not a multi-user server or a role-based approval system."));
    Display(page);
}

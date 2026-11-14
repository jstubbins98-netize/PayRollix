#include "AppWindow.h"
#include "Platform.h"
#include <stdexcept>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace payroll;
namespace {
std::wstring W(std::string const& s) { return to_hstring(s).c_str(); }
std::string Required(TextBox const& box, char const* label) {
    auto value = to_string(box.Text());
    if (value.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error(std::string(label) + " is required.");
    return value;
}
}
void AppWindow::Departments() {
    auto page = Page(L"Departments",L"Organize employees for reporting. Select a row to edit it, or clear the form to create a department.");
    auto rows = store_->Query("SELECT id,name,description FROM departments ORDER BY name");
    auto list = ui::Table({L"ID",L"Department",L"Description"},Cells(rows,{"id","name","description"}));
    list.MaxHeight(350);
    auto name = ui::Input(L"Department name"), description = ui::Input(L"Description",L"",true);
    auto selected = std::make_shared<int>(0);
    list.SelectionChanged([selected,rows,name,description](auto const& sender,auto const&) {
        auto list = sender.template as<ListView>();
        auto index = list.SelectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= rows.size()) return;
        auto const& row = rows[index];
        *selected = Id(row);
        name.Text(to_hstring(Get(row,"name")));
        description.Text(to_hstring(Get(row,"description")));
    });
    page.Children().Append(list);
    page.Children().Append(name);
    page.Children().Append(description);
    auto buttons = ui::Row();
    buttons.Children().Append(ui::Action(L"Save department",[this,selected,name,description] {
        auto title = Required(name,"Department name");
        store_->Transaction([&] {
            int id = *selected;
            if (id) store_->Exec("UPDATE departments SET name=?,description=? WHERE id=?", {title,to_string(description.Text()),id});
            else id = store_->Exec("INSERT INTO departments(name,description) VALUES(?,?)", {title,to_string(description.Text())});
            store_->Audit(*selected ? "UPDATE" : "CREATE","departments",id,"Saved department " + title);
        });
        Departments();
        Message(L"Department saved.");
    },true));
    buttons.Children().Append(ui::Action(L"New / clear",[selected,name,description,list] {
        *selected = 0; list.SelectedIndex(-1); name.Text(L""); description.Text(L"");
    }));
    buttons.Children().Append(ui::Action(L"Delete selected",[this,list,rows] {
        int id = SelectedId(list,rows);
        if (!store_->Query("SELECT id FROM payroll WHERE department_id=? LIMIT 1", {id}).empty())
            throw std::runtime_error("Reassign employees before deleting their department.");
        if (!Confirm(L"Delete this department?")) return;
        store_->Transaction([&] {
            store_->Exec("DELETE FROM departments WHERE id=?", {id});
            store_->Audit("DELETE","departments",id,"Deleted department");
        });
        Departments();
        Message(L"Department deleted.");
    }));
    page.Children().Append(buttons);
    Display(page);
}
void AppWindow::Periods() {
    auto page = Page(L"Pay periods",L"Dates label payroll snapshots; the inherited tax estimator still assumes monthly input amounts regardless of schedule.");
    auto rows = store_->Query("SELECT id,name,type,start_date,end_date,pay_date,status FROM pay_periods ORDER BY start_date DESC");
    auto list = ui::Table({L"ID",L"Name",L"Schedule",L"Start",L"End",L"Pay date",L"Status"},
        Cells(rows,{"id","name","type","start_date","end_date","pay_date","status"}));
    list.MaxHeight(330);
    page.Children().Append(list);
    auto name = ui::Input(L"Period name");
    auto type = ui::Choice(L"Schedule",{L"Weekly",L"Bi-Weekly",L"Semi-Monthly",L"Monthly"},3);
    auto start = ui::Input(L"Start date (YYYY-MM-DD)");
    auto end = ui::Input(L"End date (YYYY-MM-DD)");
    auto pay = ui::Input(L"Pay date (YYYY-MM-DD)");
    for (UIElement const& input : std::vector<UIElement>{name,type,start,end,pay}) page.Children().Append(input);
    page.Children().Append(ui::Action(L"Add period",[this,name,type,start,end,pay] {
        auto title = Required(name,"Period name"), from = to_string(start.Text()), to = to_string(end.Text()), paid = to_string(pay.Text());
        if (!ValidDate(from) || !ValidDate(to) || !ValidDate(paid) || from > to || paid < to)
            throw std::runtime_error("Use valid dates with start <= end <= pay date.");
        store_->Transaction([&] {
            int id = store_->Exec("INSERT INTO pay_periods(name,type,start_date,end_date,pay_date,status) VALUES(?,?,?,?,?,'open')",
                {title,to_string(ui::Selected(type)),from,to,paid});
            store_->Audit("CREATE","pay_periods",id,"Created period " + title);
        });
        Periods();
        Message(L"Pay period created.");
    },true));
    auto actions = ui::Row();
    for (auto const& [label,status] : std::vector<std::pair<std::wstring,std::string>>{{L"Close selected","closed"},{L"Reopen selected","open"}})
        actions.Children().Append(ui::Action(label,[this,list,rows,status] {
            int id = SelectedId(list,rows);
            store_->Transaction([&] {
                store_->Exec("UPDATE pay_periods SET status=? WHERE id=?", {status,id});
                store_->Audit("UPDATE","pay_periods",id,"Period status: " + status);
            });
            Periods();
            Message(L"Pay period status updated.");
        }));
    actions.Children().Append(ui::Action(L"Delete selected",[this,list,rows] {
        int id = SelectedId(list,rows);
        if (!store_->Query("SELECT id FROM payslips WHERE pay_period_id=? LIMIT 1", {id}).empty() ||
            !store_->Query("SELECT id FROM payroll WHERE pay_period_id=? LIMIT 1", {id}).empty())
            throw std::runtime_error("This period has payroll records. Close it instead of deleting it.");
        if (!Confirm(L"Delete this pay period?")) return;
        store_->Transaction([&] {
            store_->Exec("DELETE FROM pay_periods WHERE id=?", {id});
            store_->Audit("DELETE","pay_periods",id,"Deleted unused period");
        });
        Periods();
    }));
    page.Children().Append(actions);
    Display(page);
}
void AppWindow::Batch() {
    auto page = Page(L"Batch payroll",L"Select active employees and an open period. A batch is saved completely or not at all; duplicate employee/period runs are rejected.");
    auto periods = store_->Query("SELECT id,name FROM pay_periods WHERE status='open' ORDER BY start_date DESC");
    std::vector<std::wstring> names;
    for (auto const& period : periods) names.push_back(W(Get(period,"name")));
    auto period = ui::Choice(L"Open pay period",names);
    page.Children().Append(period);
    if (periods.empty()) page.Children().Append(ui::Text(L"No open periods. Create or reopen one under Pay periods first."));
    auto rows = store_->Query("SELECT p.id,p.name,COALESCE(d.name,'Unassigned') AS department,"
        "printf('%.2f',p.gross_salary) AS gross,printf('%.2f',p.overtime_hours) AS overtime "
        "FROM payroll p LEFT JOIN departments d ON d.id=p.department_id WHERE p.status='active' OR p.status IS NULL ORDER BY p.name");
    auto list = ui::Table({L"ID",L"Employee",L"Department",L"Monthly base ($)",L"Overtime hours"},
        Cells(rows,{"id","name","department","gross","overtime"}));
    list.SelectionMode(ListViewSelectionMode::Multiple);
    list.MaxHeight(480);
    page.Children().Append(list);
    auto actions = ui::Row();
    actions.Children().Append(ui::Action(L"Select all",[list]{ list.SelectAll(); }));
    actions.Children().Append(ui::Action(L"Clear selection",[list]{ list.SelectedItems().Clear(); }));
    actions.Children().Append(ui::Action(L"Process selected",[this,list,rows,period,periods] {
        if (period.SelectedIndex() < 0) throw std::runtime_error("Select an open pay period.");
        std::vector<int> ids;
        for (auto const& selected : list.SelectedItems()) {
            uint32_t index = 0;
            if (list.Items().IndexOf(selected,index)) ids.push_back(Id(rows.at(index)));
        }
        if (ids.empty()) throw std::runtime_error("Select at least one employee.");
        if (!Confirm(L"Process payroll for " + std::to_wstring(ids.size()) +
                     L" selected employees? This stores a permanent snapshot. Amounts are NOT prorated for non-monthly schedules.")) return;
        store_->Process(ids,Id(periods.at(period.SelectedIndex())));
        Batch();
        Message(L"Payroll processed for " + std::to_wstring(ids.size()) + L" employees. Payslips are available in the employee directory.");
    },true));
    page.Children().Append(actions);
    Display(page);
}
void AppWindow::Backups() {
    auto page = Page(L"Backup and restore",L"Use full SQLite backups to protect or move this workstation's payroll database.");
    page.Children().Append(ui::Text(L"Live database: " + store_->Path().wstring()));
    page.Children().Append(ui::Text(L"Backups and exports contain sensitive payroll information and are not encrypted by this app. Use company-approved encrypted storage and access permissions."));
    page.Children().Append(ui::Action(L"Create backup",[this] {
        auto path = platform::ChooseFile(hwnd_,true,L"db",L"Save Payrollix++ database backup");
        if (path.empty()) return;
        store_->Backup(path);
        store_->Audit("BACKUP","database",0,"Created database backup");
        Message(L"Backup saved.");
    },true));
    page.Children().Append(ui::Text(L"Restoring replaces employees, users, passwords, pay runs, and audit records with those in the backup. A safety copy of the current database is saved beside the live database first. You will be signed out and must use the restored credentials."));
    page.Children().Append(ui::Action(L"Restore from backup",[this] {
        auto path = platform::ChooseFile(hwnd_,false,L"db",L"Select Payrollix++ database backup");
        if (path.empty()) return;
        if (!Confirm(L"Replace the current database with this backup? All current users and payroll records will be replaced. A safety backup will be created first, and you will be signed out.")) return;
        store_->Restore(path);
        body_ = root_.Children().GetAt(0).as<ContentControl>();
        Login();
        Message(L"Database restored. Sign in using the backup's credentials.");
    }));
    Display(page);
}

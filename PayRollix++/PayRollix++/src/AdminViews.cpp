#include "AppWindow.h"
#include "Platform.h"
#include "Ui.h"

#include <QHBoxLayout>
#include <QTableWidget>
#include <QVBoxLayout>
#include <stdexcept>

using namespace payroll;
namespace {
std::wstring W(std::string const& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size())).toStdWString();
}
std::string S(QString const& value) { return value.toUtf8().toStdString(); }
QVBoxLayout* Layout(QWidget* page) { return qobject_cast<QVBoxLayout*>(page->layout()); }
std::string Required(ui::InputField const* input, char const* label) {
    auto value = S(input->text());
    if (value.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error(std::string(label) + " is required.");
    return value;
}
}

void AppWindow::Departments() {
    auto* page = Page(L"Departments", L"Organize employees for reporting. Select a row to edit it, or clear the form to create a department.");
    auto* layout = Layout(page);
    auto rows = store_->Query("SELECT id,name,description FROM departments ORDER BY name");
    auto* table = ui::Table({L"ID",L"Department",L"Description"},
                            Cells(rows, {"id","name","description"}), page);
    table->setMaximumHeight(350);
    auto* name = ui::Input(L"Department name");
    auto* description = ui::Input(L"Description", L"", true);
    auto selected = std::make_shared<int>(0);
    QObject::connect(table, &QTableWidget::itemSelectionChanged, this, [table, rows, name, description, selected] {
        int index = table->currentRow();
        if (index < 0 || static_cast<size_t>(index) >= rows.size()) return;
        auto const& row = rows[static_cast<size_t>(index)];
        *selected = Id(row);
        name->setText(QString::fromStdString(Get(row, "name")));
        description->setText(QString::fromStdString(Get(row, "description")));
    });
    layout->insertWidget(1, table);
    layout->insertWidget(2, name);
    layout->insertWidget(3, description);
    auto* buttons = ui::Row(page);
    auto* buttonLayout = qobject_cast<QHBoxLayout*>(buttons->layout());
    buttonLayout->addWidget(ui::Action(L"Save department", [this, selected, name, description] {
        auto title = Required(name, "Department name");
        store_->Transaction([&] {
            int id = *selected;
            if (id) store_->Exec("UPDATE departments SET name=?,description=? WHERE id=?", {title,S(description->text()),id});
            else id = store_->Exec("INSERT INTO departments(name,description) VALUES(?,?)", {title,S(description->text())});
            store_->Audit(*selected ? "UPDATE" : "CREATE", "departments", id, "Saved department " + title);
        });
        Departments();
        Message(L"Department saved.");
    }, this, true));
    buttonLayout->addWidget(ui::Action(L"New / clear", [selected, name, description, table] {
        *selected = 0;
        table->clearSelection();
        name->setText(QString{});
        description->setText(QString{});
    }, this));
    buttonLayout->addWidget(ui::Action(L"Delete selected", [this, table, rows] {
        int id = SelectedId(table, rows);
        if (!store_->Query("SELECT id FROM payroll WHERE department_id=? LIMIT 1", {id}).empty())
            throw std::runtime_error("Reassign employees before deleting their department.");
        if (!Confirm(L"Delete this department?")) return;
        store_->Transaction([&] {
            store_->Exec("DELETE FROM departments WHERE id=?", {id});
            store_->Audit("DELETE", "departments", id, "Deleted department");
        });
        Departments();
        Message(L"Department deleted.");
    }, this));
    layout->insertWidget(4, buttons);
    Display(page);
}

void AppWindow::Periods() {
    auto* page = Page(L"Pay periods", L"Dates label payroll snapshots; the inherited tax estimator still assumes monthly input amounts regardless of schedule.");
    auto* layout = Layout(page);
    auto rows = store_->Query("SELECT id,name,type,start_date,end_date,pay_date,status FROM pay_periods ORDER BY start_date DESC");
    auto* table = ui::Table({L"ID",L"Name",L"Schedule",L"Start",L"End",L"Pay date",L"Status"},
        Cells(rows, {"id","name","type","start_date","end_date","pay_date","status"}), page);
    table->setMaximumHeight(330);
    layout->insertWidget(1, table);
    auto* name = ui::Input(L"Period name");
    auto* type = ui::Choice(L"Schedule", {L"Weekly",L"Bi-Weekly",L"Semi-Monthly",L"Monthly"}, 3);
    auto* start = ui::Input(L"Start date (YYYY-MM-DD)");
    auto* end = ui::Input(L"End date (YYYY-MM-DD)");
    auto* pay = ui::Input(L"Pay date (YYYY-MM-DD)");
    for (QWidget* input : std::vector<QWidget*>{name,type,start,end,pay})
        layout->insertWidget(layout->count() - 1, input);
    layout->insertWidget(layout->count() - 1, ui::Action(L"Add period", [this,name,type,start,end,pay] {
        auto title = Required(name, "Period name");
        auto from = S(start->text()), to = S(end->text()), paid = S(pay->text());
        if (!ValidDate(from) || !ValidDate(to) || !ValidDate(paid) || from > to || paid < to)
            throw std::runtime_error("Use valid dates with start <= end <= pay date.");
        store_->Transaction([&] {
            int id = store_->Exec("INSERT INTO pay_periods(name,type,start_date,end_date,pay_date,status) VALUES(?,?,?,?,?,'open')",
                {title,S(type->currentText()),from,to,paid});
            store_->Audit("CREATE", "pay_periods", id, "Created period " + title);
        });
        Periods();
        Message(L"Pay period created.");
    }, this, true));
    auto* actions = ui::Row(page);
    auto* actionLayout = qobject_cast<QHBoxLayout*>(actions->layout());
    for (auto const& [label, status] : std::vector<std::pair<std::wstring,std::string>>{{L"Close selected","closed"},{L"Reopen selected","open"}}) {
        actionLayout->addWidget(ui::Action(label, [this,table,rows,status=status] {
            int id = SelectedId(table, rows);
            store_->Transaction([&] {
                store_->Exec("UPDATE pay_periods SET status=? WHERE id=?", {status,id});
                store_->Audit("UPDATE", "pay_periods", id, "Period status: " + status);
            });
            Periods();
            Message(L"Pay period status updated.");
        }, this));
    }
    actionLayout->addWidget(ui::Action(L"Delete selected", [this,table,rows] {
        int id = SelectedId(table, rows);
        if (!store_->Query("SELECT id FROM payslips WHERE pay_period_id=? LIMIT 1", {id}).empty() ||
            !store_->Query("SELECT id FROM payroll WHERE pay_period_id=? LIMIT 1", {id}).empty())
            throw std::runtime_error("This period has payroll records. Close it instead of deleting it.");
        if (!Confirm(L"Delete this pay period?")) return;
        store_->Transaction([&] {
            store_->Exec("DELETE FROM pay_periods WHERE id=?", {id});
            store_->Audit("DELETE", "pay_periods", id, "Deleted unused period");
        });
        Periods();
    }, this));
    layout->insertWidget(layout->count() - 1, actions);
    Display(page);
}

void AppWindow::Batch() {
    auto* page = Page(L"Batch payroll", L"Select active employees and an open period. A batch is saved completely or not at all; duplicate employee/period runs are rejected.");
    auto* layout = Layout(page);
    auto periods = store_->Query("SELECT id,name FROM pay_periods WHERE status='open' ORDER BY start_date DESC");
    std::vector<std::wstring> names;
    for (auto const& period : periods) names.push_back(W(Get(period, "name")));
    auto* period = ui::Choice(L"Open pay period", names);
    layout->insertWidget(1, period);
    if (periods.empty()) layout->insertWidget(2, ui::Text(L"No open periods. Create or reopen one under Pay periods first."));
    auto rows = store_->Query("SELECT p.id,p.name,COALESCE(d.name,'Unassigned') AS department,"
        "printf('%.2f',p.gross_salary) AS gross,printf('%.2f',p.overtime_hours) AS overtime "
        "FROM payroll p LEFT JOIN departments d ON d.id=p.department_id WHERE p.status='active' OR p.status IS NULL ORDER BY p.name");
    auto* table = ui::Table({L"ID",L"Employee",L"Department",L"Monthly base ($)",L"Overtime hours"},
        Cells(rows, {"id","name","department","gross","overtime"}), page);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setMaximumHeight(480);
    layout->insertWidget(layout->count() - 1, table);
    auto* actions = ui::Row(page);
    auto* actionLayout = qobject_cast<QHBoxLayout*>(actions->layout());
    actionLayout->addWidget(ui::Action(L"Select all", [table] { table->selectAll(); }, this));
    actionLayout->addWidget(ui::Action(L"Clear selection", [table] { table->clearSelection(); }, this));
    actionLayout->addWidget(ui::Action(L"Process selected", [this,table,rows,period,periods] {
        if (period->currentIndex() < 0) throw std::runtime_error("Select an open pay period.");
        std::vector<int> ids;
        for (auto const& index : table->selectionModel()->selectedRows()) {
            auto row = index.row();
            if (row >= 0 && static_cast<size_t>(row) < rows.size()) ids.push_back(Id(rows[static_cast<size_t>(row)]));
        }
        if (ids.empty()) throw std::runtime_error("Select at least one employee.");
        if (!Confirm(L"Process payroll for " + std::to_wstring(ids.size()) +
                     L" selected employees? This stores a permanent snapshot. Amounts are NOT prorated for non-monthly schedules.")) return;
        store_->Process(ids, Id(periods.at(static_cast<size_t>(period->currentIndex()))));
        Batch();
        Message(L"Payroll processed for " + std::to_wstring(ids.size()) + L" employees. Payslips are available in the employee directory.");
    }, this, true));
    layout->insertWidget(layout->count() - 1, actions);
    Display(page);
}

void AppWindow::Backups() {
    auto* page = Page(L"Backup and restore", L"Use full SQLite backups to protect or move this workstation's payroll database.");
    auto* layout = Layout(page);
    layout->insertWidget(1, ui::Text(L"Live database: " + store_->Path().wstring()));
    layout->insertWidget(2, ui::Text(L"Backups and exports contain sensitive payroll information and are not encrypted by this app. Use company-approved encrypted storage and access permissions."));
    layout->insertWidget(3, ui::Action(L"Create backup", [this] {
        auto path = platform::ChooseFile(hwnd_, true, L"db", L"Save Payrollix++ database backup");
        if (path.empty()) return;
        store_->Backup(path);
        store_->Audit("BACKUP", "database", 0, "Created database backup");
        Message(L"Backup saved.");
    }, this, true));
    layout->insertWidget(4, ui::Text(L"Restoring replaces employees, users, passwords, pay runs, and audit records with those in the backup. A safety copy of the current database is saved beside the live database first. You will be signed out and must use the restored credentials."));
    layout->insertWidget(5, ui::Action(L"Restore from backup", [this] {
        auto path = platform::ChooseFile(hwnd_, false, L"db", L"Select Payrollix++ database backup");
        if (path.empty()) return;
        if (!Confirm(L"Replace the current database with this backup? All current users and payroll records will be replaced. A safety backup will be created first, and you will be signed out.")) return;
        store_->Restore(path);
        setWindowTitle(QStringLiteral("Payrollix++"));
        Login();
        Message(L"Database restored. Sign in using the backup's credentials.");
    }, this));
    Display(page);
}

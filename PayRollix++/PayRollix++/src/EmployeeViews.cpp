#include "AppWindow.h"
#include "Platform.h"
#include "Ui.h"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <memory>
#include <stdexcept>

using namespace payroll;
namespace {
std::wstring W(std::string const& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size())).toStdWString();
}
std::string S(QString const& value) { return value.toUtf8().toStdString(); }
std::string Today() { return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")).toStdString(); }
QVBoxLayout* Layout(QWidget* page) { return qobject_cast<QVBoxLayout*>(page->layout()); }
}

void AppWindow::Employees() {
    auto* page = Page(L"Employee directory", L"Search employees, review payroll, and maintain employment records.");
    auto* layout = Layout(page);
    auto* search = ui::Input(L"Name, role, or company");
    auto departments = store_->Query("SELECT id,name FROM departments ORDER BY name");
    std::vector<std::wstring> names{L"All departments"};
    for (auto const& department : departments) names.push_back(W(Get(department, "name")));
    auto* dept = ui::Choice(L"Department", names);
    auto* status = ui::Choice(L"Status", {L"All statuses",L"active",L"inactive",L"terminated"});
    layout->insertWidget(1, search);
    auto* filters = ui::Row(page);
    auto* filterLayout = qobject_cast<QHBoxLayout*>(filters->layout());
    filterLayout->addWidget(dept);
    filterLayout->addWidget(status);
    layout->insertWidget(2, filters);
    auto* results = new QWidget(page);
    auto* resultsLayout = new QVBoxLayout(results);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    auto reload = [this, search, dept, status, departments, results, resultsLayout] {
        while (auto* item = resultsLayout->takeAt(0)) {
            if (auto* child = item->widget()) child->deleteLater();
            delete item;
        }
        std::string pattern = "%" + S(search->text()) + "%";
        std::string sql = "SELECT p.id,p.name,p.role,COALESCE(d.name,'Unassigned') AS department,"
            "printf('%.2f',p.gross_salary+p.overtime_pay) AS gross,printf('%.2f',p.net) AS net,"
            "COALESCE(p.status,'active') AS status FROM payroll p LEFT JOIN departments d ON d.id=p.department_id "
            "WHERE (p.name LIKE ? OR p.role LIKE ? OR p.company LIKE ?)";
        Params params{pattern, pattern, pattern};
        if (dept->currentIndex() > 0) {
            sql += " AND p.department_id=?";
            params.push_back(Id(departments.at(static_cast<size_t>(dept->currentIndex() - 1))));
        }
        if (status->currentIndex() > 0) {
            sql += " AND p.status=?";
            params.push_back(S(status->currentText()));
        }
        sql += " ORDER BY p.name";
        auto rows = store_->Query(sql, params);
        auto* table = ui::Table({L"ID",L"Name",L"Role",L"Department",L"Gross ($)",L"Net ($)",L"Status"},
                                Cells(rows, {"id","name","role","department","gross","net","status"}), results);
        table->setMaximumHeight(460);
        resultsLayout->addWidget(ui::Text(std::to_wstring(rows.size()) + L" employees"));
        resultsLayout->addWidget(table);
        auto* actions = ui::Row(results);
        auto* actionLayout = qobject_cast<QHBoxLayout*>(actions->layout());
        actionLayout->addWidget(ui::Action(L"Edit", [this, table, rows] { Employee(SelectedId(table, rows)); }, this));
        actionLayout->addWidget(ui::Action(L"History", [this, table, rows] { History(SelectedId(table, rows)); }, this));
        actionLayout->addWidget(ui::Action(L"Payslip", [this, table, rows] { Payslip(SelectedId(table, rows)); }, this));
        actionLayout->addWidget(ui::Action(L"Delete", [this, table, rows] {
            int id = SelectedId(table, rows);
            if (!Confirm(L"Delete this employee and their employment notes? This cannot be undone. Employees with processed payslips cannot be deleted.")) return;
            store_->DeleteEmployee(id);
            Employees();
            Message(L"Employee deleted.");
        }, this));
        resultsLayout->addWidget(actions);
        if (rows.empty()) resultsLayout->addWidget(ui::Text(L"No matching employees. Change the filters or add an employee."));
    };
    auto* buttons = ui::Row(page);
    auto* buttonLayout = qobject_cast<QHBoxLayout*>(buttons->layout());
    buttonLayout->addWidget(ui::Action(L"Search / refresh", reload, this, true));
    buttonLayout->addWidget(ui::Action(L"Add employee", [this] { Employee(); }, this));
    layout->insertWidget(3, buttons);
    layout->insertWidget(4, results);
    reload();
    Display(page);
}

void AppWindow::Employee(int id) {
    Row record;
    if (id) {
        auto rows = store_->Query("SELECT * FROM payroll WHERE id=?", {id});
        if (rows.empty()) throw std::runtime_error("Employee no longer exists.");
        record = rows[0];
    }
    auto* page = Page(id ? L"Edit employee" : L"Add employee",
        L"Pay inputs are monthly USD amounts. Saving recalculates estimated taxes and net pay; it does not create a pay-run snapshot.");
    auto* layout = Layout(page);
    std::map<std::string, ui::InputField*> text;
    std::vector<std::pair<std::string, std::wstring>> fields = {
        {"name",L"Full name (required)"},{"role",L"Job title / role"},{"company",L"Company"},
        {"email",L"Email"},{"phone",L"Phone"},{"address",L"Address"},{"hire_date",L"Hire date (YYYY-MM-DD)"},
        {"bio",L"Employee biography"},{"notes",L"Internal notes"}};
    layout->insertWidget(1, ui::Heading(page, L"Employee information"));
    for (auto const& [key, label] : fields) {
        auto value = Get(record, key);
        if (key == "hire_date" && value.empty()) value = Today();
        auto* input = ui::Input(label, W(value), key == "bio" || key == "notes");
        layout->insertWidget(layout->count() - 1, input);
        text.emplace(key, input);
    }
    auto departments = store_->Query("SELECT id,name FROM departments ORDER BY name");
    std::vector<std::wstring> names{L"Unassigned"};
    int selection = 0;
    for (size_t i = 0; i < departments.size(); ++i) {
        names.push_back(W(Get(departments[i], "name")));
        if (Id(departments[i]) == Id(record, "department_id")) selection = static_cast<int>(i + 1);
    }
    auto* department = ui::Choice(L"Department", names, selection);
    auto oldStatus = Get(record, "status");
    auto* status = ui::Choice(L"Employment status", {L"active",L"inactive",L"terminated"},
        oldStatus == "inactive" ? 1 : oldStatus == "terminated" ? 2 : 0);
    layout->insertWidget(layout->count() - 1, department);
    layout->insertWidget(layout->count() - 1, status);
    layout->insertWidget(layout->count() - 1, ui::Heading(page, L"Salary and overtime"));
    std::map<std::string, ui::NumberField*> numbers;
    std::vector<std::pair<std::string, std::wstring>> numeric = {
        {"gross_salary",L"Monthly base salary ($)"},{"hourly_rate",L"Hourly rate for overtime ($)"},
        {"regular_hours",L"Regular hours (reference only)"},{"overtime_hours",L"Overtime hours"},
        {"overtime_rate",L"Overtime multiplier (1 to 5)"},{"health_insurance",L"Health insurance deduction ($)"},
        {"retirement_401k",L"401(k) deduction ($)"},{"other_deductions",L"Other deductions ($)"}};
    for (auto const& [key, label] : numeric) {
        double initial = Amount(record, key);
        if (key == "overtime_rate" && initial == 0) initial = 1.5;
        auto* input = ui::Number(label, initial);
        if (key == "overtime_rate") { input->setRange(1, 5); input->setSingleStep(0.25); }
        layout->insertWidget(layout->count() - 1, input);
        numbers.emplace(key, input);
    }
    layout->insertWidget(layout->count() - 1, ui::Text(L"The inherited estimator subtracts insurance, retirement and other deductions after estimating taxes. It does not calculate pre-tax benefit treatment or withholding elections."));
    auto* actions = ui::Row(page);
    auto* actionLayout = qobject_cast<QHBoxLayout*>(actions->layout());
    actionLayout->addWidget(ui::Action(L"Calculate and save", [this, id, text, numbers, department, status, departments] {
        Row values;
        for (auto const& [key, input] : text) values[key] = S(input->text());
        for (auto const& [key, input] : numbers) {
            if (!std::isfinite(input->value())) throw std::runtime_error("Enter a valid value for " + key);
            values[key] = Decimal(input->value());
        }
        values["status"] = S(status->currentText());
        int index = department->currentIndex();
        values["department_id"] = index > 0 ? Get(departments.at(static_cast<size_t>(index - 1)), "id") : "0";
        int saved = store_->SaveEmployee(values, id);
        auto row = store_->Query("SELECT net FROM payroll WHERE id=?", {saved})[0];
        Employees();
        Message(L"Employee saved. Estimated net pay: $" + W(Decimal(Amount(row, "net"))));
    }, this, true));
    actionLayout->addWidget(ui::Action(L"Cancel", [this] { Employees(); }, this));
    if (!id) actionLayout->addWidget(ui::Action(L"Clear form", [this] { Employee(); }, this));
    layout->insertWidget(layout->count() - 1, actions);
    Display(page);
}

void AppWindow::History(int id) {
    auto employee = store_->Query("SELECT name FROM payroll WHERE id=?", {id});
    if (employee.empty()) throw std::runtime_error("Employee no longer exists.");
    auto* page = Page(L"Employment history", W(Get(employee[0], "name")) + L" — salary, role and status changes are also recorded automatically.");
    auto* layout = Layout(page);
    auto rows = store_->Query("SELECT id,event_type,previous_value,new_value,effective_date,notes "
                              "FROM employment_history WHERE employee_id=? ORDER BY effective_date DESC,id DESC", {id});
    auto* table = ui::Table({L"ID",L"Event",L"Previous",L"New",L"Effective",L"Notes"},
                            Cells(rows, {"id","event_type","previous_value","new_value","effective_date","notes"}), page);
    table->setMaximumHeight(350);
    layout->insertWidget(1, table);
    auto* type = ui::Choice(L"Event type", {L"Promotion",L"Salary Change",L"Transfer",L"Performance Review",L"Other"});
    auto* previous = ui::Input(L"Previous value");
    auto* next = ui::Input(L"New value");
    auto* date = ui::Input(L"Effective date (YYYY-MM-DD)", W(Today()));
    auto* notes = ui::Input(L"Notes", L"", true);
    for (QWidget* item : std::vector<QWidget*>{type, previous, next, date, notes})
        layout->insertWidget(layout->count() - 1, item);
    layout->insertWidget(layout->count() - 1, ui::Action(L"Record event", [this, id, type, previous, next, date, notes] {
        auto when = S(date->text());
        if (!ValidDate(when)) throw std::runtime_error("Enter a valid effective date.");
        store_->Transaction([&] {
            int event = store_->Exec("INSERT INTO employment_history(employee_id,event_type,previous_value,new_value,effective_date,notes) "
                "VALUES(?,?,?,?,?,?)", {id, S(type->currentText()), S(previous->text()), S(next->text()), when, S(notes->text())});
            store_->Audit("CREATE", "employment_history", event, "Recorded employee event");
        });
        History(id);
        Message(L"Event recorded. This note does not change the employee's salary or role; use Edit employee for those changes.");
    }, this, true));
    layout->insertWidget(layout->count() - 1, ui::Action(L"Delete selected event", [this, id, table, rows] {
        int event = SelectedId(table, rows);
        if (!Confirm(L"Delete this employment history event? The deletion will be audited.")) return;
        store_->Transaction([&] {
            store_->Exec("DELETE FROM employment_history WHERE id=? AND employee_id=?", {event, id});
            store_->Audit("DELETE", "employment_history", event, "Deleted employment event");
        });
        History(id);
    }, this));
    layout->insertWidget(layout->count() - 1, ui::Action(L"Back to employees", [this] { Employees(); }, this));
    Display(page);
}

void AppWindow::Payslip(int id) {
    auto* page = Page(L"Payslip", L"Choose a processed period for a historical snapshot, or preview the current estimate. Export contains exactly the generated preview.");
    auto* layout = Layout(page);
    auto periods = store_->Query("SELECT DISTINCT pp.id,pp.name,pp.start_date FROM pay_periods pp "
        "JOIN payslips s ON s.pay_period_id=pp.id WHERE s.employee_id=? ORDER BY pp.start_date DESC", {id});
    std::vector<std::wstring> names{L"Current estimate"};
    for (auto const& period : periods) names.push_back(W(Get(period, "name")));
    auto* period = ui::Choice(L"Pay period", names);
    auto* preview = ui::Input(L"Generated payslip", L"", true);
    preview->setReadOnly(true);
    preview->setMinimumHeight(400);
    auto contents = std::make_shared<std::wstring>();
    auto generate = [this, id, period, periods, preview, contents] {
        int selection = period->currentIndex();
        int periodId = selection > 0 ? Id(periods.at(static_cast<size_t>(selection - 1))) : 0;
        auto result = store_->Payslip(id, periodId);
        std::wstring text = L"Employee: " + W(Get(result, "employee_name")) + L"\nRole: " + W(Get(result, "employee_role")) +
            L"\nCompany: " + W(Get(result, "company")) + L"\nDepartment: " + W(Get(result, "department")) +
            L"\nPeriod: " + W(Get(result, "period")) + L"\nDates: " + W(Get(result, "start_date")) + L" to " +
            W(Get(result, "end_date")) + L"\nPay date: " + W(Get(result, "pay_date")) + L"\n\n";
        bool legacy = periodId && Get(result, "payslip_id").empty();
        if (legacy) {
            text = L"Legacy payslip — employee ID " + std::to_wstring(id) + L"\nPeriod: " + W(Get(result, "period")) +
                   L"\nPay date: " + W(Get(result, "pay_date")) +
                   L"\n\nThis record predates full snapshots. Names, benefits and overtime breakdown were not recorded.\n";
            auto json = QJsonDocument::fromJson(QByteArray::fromStdString(Get(result, "deductions_json")));
            if (json.isObject()) {
                auto object = json.object();
                for (auto const& key : {QStringLiteral("federal"),QStringLiteral("state"),QStringLiteral("ss"),QStringLiteral("medicare")}) {
                    if (object.contains(key) && object.value(key).isDouble())
                        text += key.toStdWString() + L": $" + W(Decimal(object.value(key).toDouble())) + L"\n";
                }
            }
        } else {
            for (auto const& [key, label] : std::vector<std::pair<std::string, std::wstring>>{
                {"base",L"Base salary"},{"overtime",L"Overtime pay"},{"federal",L"Federal tax"},
                {"state",L"State tax"},{"ss",L"Social Security"},{"medicare",L"Medicare"},
                {"health",L"Health insurance"},{"retirement",L"401(k)"},{"other",L"Other deductions"}})
                text += label + L": $" + W(Decimal(Amount(result, key))) + L"\n";
        }
        text += L"\nGross pay: $" + W(Decimal(Amount(result, "gross_pay"))) +
                L"\nTotal deductions: $" + W(Decimal(Amount(result, "gross_pay") - Amount(result, "net_pay"))) +
                L"\nNET PAY: $" + W(Decimal(Amount(result, "net_pay"))) + L"\n";
        *contents = text;
        preview->setText(QString::fromStdWString(text));
        store_->Audit("VIEW", "payslips", id, "Generated payslip preview");
    };
    layout->insertWidget(1, period);
    layout->insertWidget(2, ui::Action(L"Generate", generate, this, true));
    layout->insertWidget(3, preview);
    auto* actions = ui::Row(page);
    auto* actionLayout = qobject_cast<QHBoxLayout*>(actions->layout());
    actionLayout->addWidget(ui::Action(L"Export PDF", [this, id, contents] {
        platform::Print(hwnd_, L"Payrollix++ Payslip", *contents, true);
        store_->Audit("EXPORT", "payslips", id, "Opened PDF export");
    }, this));
    actionLayout->addWidget(ui::Action(L"Export HTML", [this, id, contents] {
        auto path = platform::ChooseFile(hwnd_, true, L"html", L"Export payslip");
        if (path.empty()) return;
        platform::WriteText(path, platform::Html(L"Payrollix++ Payslip", *contents));
        store_->Audit("EXPORT", "payslips", id, "Exported HTML payslip");
        Message(L"Payslip exported.");
    }, this));
    actionLayout->addWidget(ui::Action(L"Print", [this, contents] {
        platform::Print(hwnd_, L"Payrollix++ Payslip", *contents, false);
    }, this));
    layout->insertWidget(4, actions);
    layout->insertWidget(5, ui::Action(L"Back to employees", [this] { Employees(); }, this));
    generate();
    Display(page);
}

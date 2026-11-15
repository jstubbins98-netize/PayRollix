#include "AppWindow.h"
#include "ExcelExport.h"
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
void Dates(std::string const& from, std::string const& to) {
    if ((!from.empty() && !ValidDate(from)) || (!to.empty() && !ValidDate(to)) ||
        (!from.empty() && !to.empty() && from > to))
        throw std::runtime_error("Enter valid YYYY-MM-DD dates, with From no later than To, or leave dates blank.");
}
}

void AppWindow::Reports() {
    auto* page = Page(L"Reports", L"Current employee records, filtered by the last update date (UTC), not historical pay-run totals. All monetary values are USD.");
    auto* layout = Layout(page);
    auto* type = ui::Choice(L"Report type", {L"Payroll Summary",L"Department Report",L"Employee List",L"Deductions Summary"});
    auto departments = store_->Query("SELECT id,name FROM departments ORDER BY name");
    std::vector<std::wstring> names{L"All departments"};
    for (auto const& row : departments) names.push_back(W(Get(row, "name")));
    auto* dept = ui::Choice(L"Department", names);
    auto* filters = ui::Row(page);
    auto* filterLayout = qobject_cast<QHBoxLayout*>(filters->layout());
    filterLayout->addWidget(type);
    filterLayout->addWidget(dept);
    layout->insertWidget(1, filters);
    auto* from = ui::Input(L"Updated from (YYYY-MM-DD; optional)");
    auto* to = ui::Input(L"Updated to (YYYY-MM-DD; optional)");
    auto* dateFilters = ui::Row(page);
    auto* dateLayout = qobject_cast<QHBoxLayout*>(dateFilters->layout());
    dateLayout->addWidget(from);
    dateLayout->addWidget(to);
    layout->insertWidget(2, dateFilters);
    auto* output = new QWidget(page);
    auto* outputLayout = new QVBoxLayout(output);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    auto generate = [this,type,dept,departments,from,to,output,outputLayout] {
        auto start = S(from->text()), end = S(to->text());
        Dates(start, end);
        std::string where = " WHERE 1=1";
        Params params;
        if (dept->currentIndex() > 0) {
            where += " AND p.department_id=?";
            params.push_back(Id(departments.at(static_cast<size_t>(dept->currentIndex() - 1))));
        }
        if (!start.empty()) { where += " AND date(p.updated_at)>=?"; params.push_back(start); }
        if (!end.empty()) { where += " AND date(p.updated_at)<=?"; params.push_back(end); }
        std::string join = " FROM payroll p LEFT JOIN departments d ON d.id=p.department_id";
        std::string sql;
        std::vector<std::wstring> headers;
        std::vector<std::string> keys;
        std::vector<size_t> numericColumns;
        switch (type->currentIndex()) {
        case 0:
            sql = "SELECT p.name,COALESCE(d.name,'Unassigned') AS department,"
                  "printf('%.2f',p.gross_salary+p.overtime_pay) AS gross,"
                  "printf('%.2f',p.gross_salary+p.overtime_pay-p.net) AS deductions,"
                  "printf('%.2f',p.net) AS net,COALESCE(p.status,'active') AS status" + join + where + " ORDER BY p.name";
            headers = {L"Employee",L"Department",L"Gross ($)",L"Deductions ($)",L"Net ($)",L"Status"};
            keys = {"name","department","gross","deductions","net","status"};
            numericColumns = {2,3,4};
            break;
        case 1:
            sql = "SELECT COALESCE(d.name,'Unassigned') AS department,COUNT(*) AS employees,"
                  "printf('%.2f',SUM(p.gross_salary+p.overtime_pay)) AS gross,printf('%.2f',SUM(p.net)) AS net,"
                  "printf('%.2f',AVG(p.gross_salary)) AS average" + join + where + " GROUP BY d.id ORDER BY d.name";
            headers = {L"Department",L"Employees",L"Gross ($)",L"Net ($)",L"Average base ($)"};
            keys = {"department","employees","gross","net","average"};
            numericColumns = {1,2,3,4};
            break;
        case 2:
            sql = "SELECT p.name,p.role,COALESCE(d.name,'Unassigned') AS department,p.email,p.phone,p.hire_date,"
                  "COALESCE(p.status,'active') AS status" + join + where + " ORDER BY p.name";
            headers = {L"Employee",L"Role",L"Department",L"Email",L"Phone",L"Hire date",L"Status"};
            keys = {"name","role","department","email","phone","hire_date","status"};
            break;
        default:
            sql = "SELECT p.name,printf('%.2f',p.federal_tax) AS federal,printf('%.2f',p.state_tax) AS state,"
                  "printf('%.2f',p.social_security) AS ss,printf('%.2f',p.medicare) AS medicare,"
                  "printf('%.2f',p.health_insurance) AS health,printf('%.2f',p.retirement_401k) AS retirement,"
                  "printf('%.2f',p.other_deductions) AS other" + join + where + " ORDER BY p.name";
            headers = {L"Employee",L"Federal ($)",L"State ($)",L"Social Security ($)",L"Medicare ($)",L"Health ($)",L"401(k) ($)",L"Other ($)"};
            keys = {"name","federal","state","ss","medicare","health","retirement","other"};
            numericColumns = {1,2,3,4,5,6,7};
        }
        auto rows = store_->Query(sql, params);
        auto cells = Cells(rows, keys);
        auto totals = store_->Query("SELECT COUNT(*) AS n,COALESCE(SUM(p.gross_salary+p.overtime_pay),0) AS gross,"
                                    "COALESCE(SUM(p.net),0) AS net" + join + where, params)[0];
        while (auto* item = outputLayout->takeAt(0)) {
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        outputLayout->addWidget(ui::Text(W(Get(totals,"n")) + L" employees | Gross $" + W(Decimal(Amount(totals,"gross"))) +
            L" | Net $" + W(Decimal(Amount(totals,"net"))) + L" | Deductions $" + W(Decimal(Amount(totals,"gross")-Amount(totals,"net")))));
        auto* table = ui::Table(headers, cells, output);
        table->setMaximumHeight(500);
        outputLayout->addWidget(table);
        outputLayout->addWidget(ui::Text(L"Export uses the generated rows below, even if you change filters afterwards. Generate again to refresh."));
        outputLayout->addWidget(ui::Action(L"Export generated report to ODF Spreadsheet (.ods)", [this,headers,cells,numericColumns] {
            auto file = platform::ChooseFile(hwnd_, true, L"ods", L"Export report for LibreOffice Calc");
            if (file.empty()) return;
            ExportOds(file, headers, cells, numericColumns);
            store_->Audit("EXPORT", "reports", 0, "Exported generated report to ODF Spreadsheet (.ods)");
            Message(L"ODF spreadsheet exported.");
        }, this));
        outputLayout->addWidget(ui::Action(L"Export generated report to Excel (.xlsx)", [this,headers,cells,numericColumns] {
            auto file = platform::ChooseFile(hwnd_, true, L"xlsx", L"Export report to Excel");
            if (file.empty()) return;
            ExportExcel(file, headers, cells, numericColumns);
            store_->Audit("EXPORT", "reports", 0, "Exported generated report to Excel (.xlsx)");
            Message(L"Excel workbook exported.");
        }, this));
        outputLayout->addWidget(ui::Action(L"Export generated report to CSV", [this,headers,cells] {
            auto file = platform::ChooseFile(hwnd_, true, L"csv", L"Export report");
            if (file.empty()) return;
            platform::ExportCsv(file, headers, cells);
            store_->Audit("EXPORT", "reports", 0, "Exported generated report");
            Message(L"CSV exported.");
        }, this));
        store_->Audit("VIEW", "reports", 0, "Generated " + S(type->currentText()));
    };
    layout->insertWidget(3, ui::Action(L"Generate report", generate, this, true));
    layout->insertWidget(4, output);
    generate();
    Display(page);
}

void AppWindow::Audit() {
    auto* page = Page(L"Audit log", L"Local activity records with UTC timestamps and the signed-in user. This database log is not tamper-proof.");
    auto* layout = Layout(page);
    auto* action = ui::Choice(L"Action", {L"All actions",L"CREATE",L"UPDATE",L"DELETE",L"LOGIN",L"LOGOUT",L"VIEW",L"EXPORT",L"BATCH_PROCESS",L"BACKUP",L"RESTORE"});
    auto* from = ui::Input(L"From date (YYYY-MM-DD; optional)");
    auto* to = ui::Input(L"To date (YYYY-MM-DD; optional)");
    layout->insertWidget(1, action);
    auto* dates = ui::Row(page);
    auto* datesLayout = qobject_cast<QHBoxLayout*>(dates->layout());
    datesLayout->addWidget(from);
    datesLayout->addWidget(to);
    layout->insertWidget(2, dates);
    auto* output = new QWidget(page);
    auto* outputLayout = new QVBoxLayout(output);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    auto reload = [this,action,from,to,output,outputLayout] {
        auto start = S(from->text()), end = S(to->text());
        Dates(start, end);
        std::string sql = "SELECT a.id,a.timestamp,COALESCE(u.username,'System / legacy') AS username,a.action,a.table_name,"
                          "a.record_id,a.details FROM audit_log a LEFT JOIN users u ON u.id=a.user_id WHERE 1=1";
        Params params;
        if (action->currentIndex() > 0) { sql += " AND a.action=?"; params.push_back(S(action->currentText())); }
        if (!start.empty()) { sql += " AND date(a.timestamp)>=?"; params.push_back(start); }
        if (!end.empty()) { sql += " AND date(a.timestamp)<=?"; params.push_back(end); }
        sql += " ORDER BY a.id DESC";
        auto rows = store_->Query(sql, params);
        auto cells = Cells(rows, {"id","timestamp","username","action","table_name","record_id","details"});
        std::vector<std::wstring> headers{L"ID",L"When (UTC)",L"User",L"Action",L"Table",L"Record",L"Details"};
        while (auto* item = outputLayout->takeAt(0)) {
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        outputLayout->addWidget(ui::Text(std::to_wstring(rows.size()) + L" matching activity records"));
        auto* table = ui::Table(headers, cells, output);
        table->setMaximumHeight(500);
        outputLayout->addWidget(table);
        outputLayout->addWidget(ui::Action(L"Export filtered log to ODF Spreadsheet (.ods)", [this,headers,cells] {
            auto file = platform::ChooseFile(hwnd_, true, L"ods", L"Export audit log for LibreOffice Calc");
            if (file.empty()) return;
            ExportOds(file, headers, cells, {0,5});
            store_->Audit("EXPORT", "audit_log", 0, "Exported filtered audit log to ODF Spreadsheet (.ods)");
            Message(L"ODF audit spreadsheet exported.");
        }, this));
        outputLayout->addWidget(ui::Action(L"Export filtered log to Excel (.xlsx)", [this,headers,cells] {
            auto file = platform::ChooseFile(hwnd_, true, L"xlsx", L"Export audit log to Excel");
            if (file.empty()) return;
            ExportExcel(file, headers, cells, {0,5});
            store_->Audit("EXPORT", "audit_log", 0, "Exported filtered audit log to Excel (.xlsx)");
            Message(L"Excel audit workbook exported.");
        }, this));
        outputLayout->addWidget(ui::Action(L"Export filtered log to CSV", [this,headers,cells] {
            auto file = platform::ChooseFile(hwnd_, true, L"csv", L"Export audit log");
            if (file.empty()) return;
            platform::ExportCsv(file, headers, cells);
            store_->Audit("EXPORT", "audit_log", 0, "Exported filtered audit log");
            Message(L"Audit log exported.");
        }, this));
    };
    layout->insertWidget(3, ui::Action(L"Apply filters / refresh", reload, this, true));
    layout->insertWidget(4, output);
    reload();
    Display(page);
}

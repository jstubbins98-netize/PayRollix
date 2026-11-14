#include "AppWindow.h"
#include "Platform.h"
#include <winrt/Windows.Data.Json.h>
#include <cmath>
#include <stdexcept>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace payroll;
namespace {
std::wstring W(std::string const& s) { return to_hstring(s).c_str(); }
std::string Today() {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char value[11];
    snprintf(value,sizeof(value),"%04u-%02u-%02u",now.wYear,now.wMonth,now.wDay);
    return value;
}
}
void AppWindow::Employees() {
    auto page = Page(L"Employee directory", L"Search employees, review payroll, and maintain employment records.");
    auto search = ui::Input(L"Name, role, or company");
    auto departments = store_->Query("SELECT id,name FROM departments ORDER BY name");
    std::vector<std::wstring> names{L"All departments"};
    for (auto const& d : departments) names.push_back(W(Get(d,"name")));
    auto dept = ui::Choice(L"Department",names);
    auto status = ui::Choice(L"Status",{L"All statuses",L"active",L"inactive",L"terminated"});
    page.Children().Append(search);
    auto filters = ui::Row();
    filters.Children().Append(dept);
    filters.Children().Append(status);
    page.Children().Append(filters);
    ContentControl results;
    auto reload = [this,search,dept,status,departments,results] {
        auto pattern = "%" + to_string(search.Text()) + "%";
        std::string sql = "SELECT p.id,p.name,p.role,COALESCE(d.name,'Unassigned') AS department,"
            "printf('%.2f',p.gross_salary+p.overtime_pay) AS gross,printf('%.2f',p.net) AS net,"
            "COALESCE(p.status,'active') AS status FROM payroll p LEFT JOIN departments d ON d.id=p.department_id "
            "WHERE (p.name LIKE ? OR p.role LIKE ? OR p.company LIKE ?)";
        Params p{pattern,pattern,pattern};
        if (dept.SelectedIndex() > 0) { sql += " AND p.department_id=?"; p.push_back(Id(departments.at(dept.SelectedIndex()-1))); }
        if (status.SelectedIndex() > 0) { sql += " AND p.status=?"; p.push_back(to_string(ui::Selected(status))); }
        sql += " ORDER BY p.name";
        auto rows = store_->Query(sql,p);
        auto list = ui::Table({L"ID",L"Name",L"Role",L"Department",L"Gross ($)",L"Net ($)",L"Status"},
                              Cells(rows,{"id","name","role","department","gross","net","status"}));
        list.MaxHeight(460);
        auto panel = ui::Stack();
        panel.Children().Append(ui::Text(std::to_wstring(rows.size()) + L" employees"));
        panel.Children().Append(list);
        auto actions = ui::Row();
        actions.Children().Append(ui::Action(L"Edit",[this,list,rows]{ Employee(SelectedId(list,rows)); }));
        actions.Children().Append(ui::Action(L"History",[this,list,rows]{ History(SelectedId(list,rows)); }));
        actions.Children().Append(ui::Action(L"Payslip",[this,list,rows]{ Payslip(SelectedId(list,rows)); }));
        actions.Children().Append(ui::Action(L"Delete",[this,list,rows] {
            int id = SelectedId(list,rows);
            if (!Confirm(L"Delete this employee and their employment notes? This cannot be undone. Employees with processed payslips cannot be deleted.")) return;
            store_->DeleteEmployee(id);
            Employees();
            Message(L"Employee deleted.");
        }));
        panel.Children().Append(actions);
        if (rows.empty()) panel.Children().Append(ui::Text(L"No matching employees. Change the filters or add an employee."));
        results.Content(panel);
    };
    auto buttons = ui::Row();
    buttons.Children().Append(ui::Action(L"Search / refresh",reload,true));
    buttons.Children().Append(ui::Action(L"Add employee",[this]{ Employee(); }));
    page.Children().Append(buttons);
    page.Children().Append(results);
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
    auto page = Page(id ? L"Edit employee" : L"Add employee",
        L"Pay inputs are monthly USD amounts. Saving recalculates estimated taxes and net pay; it does not create a pay-run snapshot.");
    std::map<std::string,TextBox> text;
    std::vector<std::pair<std::string,std::wstring>> fields = {
        {"name",L"Full name (required)"},{"role",L"Job title / role"},{"company",L"Company"},
        {"email",L"Email"},{"phone",L"Phone"},{"address",L"Address"},{"hire_date",L"Hire date (YYYY-MM-DD)"},
        {"bio",L"Employee biography"},{"notes",L"Internal notes"}};
    ui::Heading(page,L"Employee information");
    for (auto const& [key,label] : fields) {
        auto value = Get(record,key);
        if (key == "hire_date" && value.empty()) value = Today();
        auto input = ui::Input(label,W(value),key == "bio" || key == "notes");
        page.Children().Append(input);
        text.emplace(key,input);
    }
    auto departments = store_->Query("SELECT id,name FROM departments ORDER BY name");
    std::vector<std::wstring> names{L"Unassigned"};
    int selection = 0;
    for (size_t i = 0; i < departments.size(); ++i) {
        names.push_back(W(Get(departments[i],"name")));
        if (Id(departments[i]) == Id(record,"department_id")) selection = static_cast<int>(i+1);
    }
    auto department = ui::Choice(L"Department",names,selection);
    auto oldStatus = Get(record,"status");
    auto status = ui::Choice(L"Employment status",{L"active",L"inactive",L"terminated"},
        oldStatus == "inactive" ? 1 : oldStatus == "terminated" ? 2 : 0);
    page.Children().Append(department);
    page.Children().Append(status);
    ui::Heading(page,L"Salary and overtime");
    std::map<std::string,NumberBox> numbers;
    std::vector<std::pair<std::string,std::wstring>> numeric = {
        {"gross_salary",L"Monthly base salary ($)"},{"hourly_rate",L"Hourly rate for overtime ($)"},
        {"regular_hours",L"Regular hours (reference only)"},{"overtime_hours",L"Overtime hours"},
        {"overtime_rate",L"Overtime multiplier (1 to 5)"},{"health_insurance",L"Health insurance deduction ($)"},
        {"retirement_401k",L"401(k) deduction ($)"},{"other_deductions",L"Other deductions ($)"}};
    for (auto const& [key,label] : numeric) {
        double initial = Amount(record,key);
        if (key == "overtime_rate" && initial == 0) initial = 1.5;
        auto input = ui::Number(label,initial);
        if (key == "overtime_rate") { input.Minimum(1); input.Maximum(5); input.SmallChange(0.25); }
        page.Children().Append(input);
        numbers.emplace(key,input);
    }
    page.Children().Append(ui::Text(L"The inherited estimator subtracts insurance, retirement and other deductions after estimating taxes. It does not calculate pre-tax benefit treatment or withholding elections."));
    auto actions = ui::Row();
    actions.Children().Append(ui::Action(L"Calculate and save",[this,id,text,numbers,department,status,departments] {
        Row fields;
        for (auto const& [key,input] : text) fields[key] = to_string(input.Text());
        for (auto const& [key,input] : numbers) {
            if (!std::isfinite(input.Value())) throw std::runtime_error("Enter a valid value for " + key);
            fields[key] = Decimal(input.Value());
        }
        fields["status"] = to_string(ui::Selected(status));
        int index = department.SelectedIndex();
        fields["department_id"] = index > 0 ? Get(departments.at(index-1),"id") : "0";
        int saved = store_->SaveEmployee(fields,id);
        auto row = store_->Query("SELECT net FROM payroll WHERE id=?", {saved})[0];
        Employees();
        Message(L"Employee saved. Estimated net pay: $" + W(Decimal(Amount(row,"net"))));
    },true));
    actions.Children().Append(ui::Action(L"Cancel",[this]{ Employees(); }));
    if (!id) actions.Children().Append(ui::Action(L"Clear form",[this]{ Employee(); }));
    page.Children().Append(actions);
    Display(page);
}
void AppWindow::History(int id) {
    auto employee = store_->Query("SELECT name FROM payroll WHERE id=?", {id});
    if (employee.empty()) throw std::runtime_error("Employee no longer exists.");
    auto page = Page(L"Employment history",W(Get(employee[0],"name")) + L" — salary, role and status changes are also recorded automatically.");
    auto rows = store_->Query("SELECT id,event_type,previous_value,new_value,effective_date,notes "
                              "FROM employment_history WHERE employee_id=? ORDER BY effective_date DESC,id DESC", {id});
    auto list = ui::Table({L"ID",L"Event",L"Previous",L"New",L"Effective",L"Notes"},
                          Cells(rows,{"id","event_type","previous_value","new_value","effective_date","notes"}));
    list.MaxHeight(350);
    page.Children().Append(list);
    auto type = ui::Choice(L"Event type",{L"Promotion",L"Salary Change",L"Transfer",L"Performance Review",L"Other"});
    auto previous = ui::Input(L"Previous value"), next = ui::Input(L"New value");
    auto date = ui::Input(L"Effective date (YYYY-MM-DD)",W(Today()));
    auto notes = ui::Input(L"Notes",L"",true);
    for (UIElement const& item : std::vector<UIElement>{type,previous,next,date,notes}) page.Children().Append(item);
    page.Children().Append(ui::Action(L"Record event",[this,id,type,previous,next,date,notes] {
        auto when = to_string(date.Text());
        if (!ValidDate(when)) throw std::runtime_error("Enter a valid effective date.");
        store_->Transaction([&] {
            int event = store_->Exec("INSERT INTO employment_history(employee_id,event_type,previous_value,new_value,effective_date,notes) "
                "VALUES(?,?,?,?,?,?)", {id,to_string(ui::Selected(type)),to_string(previous.Text()),to_string(next.Text()),when,to_string(notes.Text())});
            store_->Audit("CREATE","employment_history",event,"Recorded employee event");
        });
        History(id);
        Message(L"Event recorded. This note does not change the employee's salary or role; use Edit employee for those changes.");
    },true));
    page.Children().Append(ui::Action(L"Delete selected event",[this,id,list,rows] {
        int event = SelectedId(list,rows);
        if (!Confirm(L"Delete this employment history event? The deletion will be audited.")) return;
        store_->Transaction([&] {
            store_->Exec("DELETE FROM employment_history WHERE id=? AND employee_id=?", {event,id});
            store_->Audit("DELETE","employment_history",event,"Deleted employment event");
        });
        History(id);
    }));
    page.Children().Append(ui::Action(L"Back to employees",[this]{ Employees(); }));
    Display(page);
}
void AppWindow::Payslip(int id) {
    auto page = Page(L"Payslip",L"Choose a processed period for a historical snapshot, or preview the current estimate. Export contains exactly the generated preview.");
    auto periods = store_->Query("SELECT DISTINCT pp.id,pp.name,pp.start_date FROM pay_periods pp "
        "JOIN payslips s ON s.pay_period_id=pp.id WHERE s.employee_id=? ORDER BY pp.start_date DESC", {id});
    std::vector<std::wstring> names{L"Current estimate"};
    for (auto const& p : periods) names.push_back(W(Get(p,"name")));
    auto period = ui::Choice(L"Pay period",names);
    auto preview = ui::Input(L"Generated payslip",L"",true);
    preview.IsReadOnly(true);
    preview.MinHeight(400);
    auto contents = std::make_shared<std::wstring>();
    auto generate = [this,id,period,periods,preview,contents] {
        int selection = period.SelectedIndex();
        int periodId = selection > 0 ? Id(periods.at(selection-1)) : 0;
        auto r = store_->Payslip(id,periodId);
        std::wstring text = L"Employee: " + W(Get(r,"employee_name")) + L"\nRole: " + W(Get(r,"employee_role")) +
            L"\nCompany: " + W(Get(r,"company")) + L"\nDepartment: " + W(Get(r,"department")) +
            L"\nPeriod: " + W(Get(r,"period")) + L"\nDates: " + W(Get(r,"start_date")) + L" to " +
            W(Get(r,"end_date")) + L"\nPay date: " + W(Get(r,"pay_date")) + L"\n\n";
        bool legacy = periodId && Get(r,"payslip_id").empty();
        if (legacy) {
            text = L"Legacy payslip — employee ID " + std::to_wstring(id) + L"\nPeriod: " + W(Get(r,"period")) +
                   L"\nPay date: " + W(Get(r,"pay_date")) +
                   L"\n\nThis record predates full snapshots. Names, benefits and overtime breakdown were not recorded.\n";
            // Only render facts actually saved in the legacy snapshot.
            Windows::Data::Json::JsonObject json;
            if (Windows::Data::Json::JsonObject::TryParse(to_hstring(Get(r,"deductions_json")),json)) {
                for (auto key : {L"federal",L"state",L"ss",L"medicare"})
                    if (json.HasKey(key)) text += std::wstring(key) + L": $" + W(Decimal(json.GetNamedNumber(key))) + L"\n";
            }
        } else {
            for (auto const& [key,label] : std::vector<std::pair<std::string,std::wstring>>{
                {"base",L"Base salary"},{"overtime",L"Overtime pay"},{"federal",L"Federal tax"},
                {"state",L"State tax"},{"ss",L"Social Security"},{"medicare",L"Medicare"},
                {"health",L"Health insurance"},{"retirement",L"401(k)"},{"other",L"Other deductions"}})
                text += label + L": $" + W(Decimal(Amount(r,key))) + L"\n";
        }
        text += L"\nGross pay: $" + W(Decimal(Amount(r,"gross_pay"))) +
                L"\nTotal deductions: $" + W(Decimal(Amount(r,"gross_pay")-Amount(r,"net_pay"))) +
                L"\nNET PAY: $" + W(Decimal(Amount(r,"net_pay"))) + L"\n";
        *contents = text;
        preview.Text(text);
        store_->Audit("VIEW","payslips",id,"Generated payslip preview");
    };
    page.Children().Append(period);
    page.Children().Append(ui::Action(L"Generate",generate,true));
    page.Children().Append(preview);
    auto actions = ui::Row();
    actions.Children().Append(ui::Action(L"Export PDF",[this,id,contents] {
        platform::Print(hwnd_,L"Payrollix++ Payslip",*contents,true);
        store_->Audit("EXPORT","payslips",id,"Opened PDF export");
    }));
    actions.Children().Append(ui::Action(L"Export HTML",[this,id,contents] {
        auto path = platform::ChooseFile(hwnd_,true,L"html",L"Export payslip");
        if (path.empty()) return;
        platform::WriteText(path,platform::Html(L"Payrollix++ Payslip",*contents));
        store_->Audit("EXPORT","payslips",id,"Exported HTML payslip");
        Message(L"Payslip exported.");
    }));
    actions.Children().Append(ui::Action(L"Print",[this,contents] { platform::Print(hwnd_,L"Payrollix++ Payslip",*contents,false); }));
    page.Children().Append(actions);
    page.Children().Append(ui::Action(L"Back to employees",[this]{ Employees(); }));
    generate();
    Display(page);
}

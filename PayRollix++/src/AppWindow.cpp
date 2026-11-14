#include "AppWindow.h"
#include "Platform.h"
#include "Ui.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

using namespace payroll;
namespace {
QString Q(std::wstring const& value) { return QString::fromStdWString(value); }
std::wstring W(std::string const& value) { return QString::fromUtf8(value.data(), static_cast<int>(value.size())).toStdWString(); }
std::string S(QString const& value) { return value.toUtf8().toStdString(); }
QWidget* PasswordField(QString const& label, QLineEdit*& edit) {
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto* title = new QLabel(label, container);
    title->setObjectName("fieldLabel");
    edit = new QLineEdit(container);
    edit->setEchoMode(QLineEdit::Password);
    edit->setMinimumHeight(40);
    layout->addWidget(title);
    layout->addWidget(edit);
    return container;
}
}

AppWindow::AppWindow() {
    setWindowTitle(QStringLiteral("Payrollix++"));
    resize(1280, 860);
    setMinimumSize(900, 620);
    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget { background:#f3f5f2; color:#22312e; font-family:'Segoe UI'; font-size:13px; }"
        "QStatusBar { background:#e5ebe6; color:#334540; border-top:1px solid #cbd5ce; }"
        "QLabel#pageTitle { color:#203c36; font-size:25px; font-weight:650; }"
        "QLabel#pageDescription { color:#64736d; font-size:13px; }"
        "QLabel#fieldLabel { color:#41554f; font-weight:600; }"
        "QPushButton { color:#29433d; background:#e7ece8; border:1px solid #c8d3cc; border-radius:5px; padding:7px 14px; }"
        "QPushButton:hover { background:#dce6df; } QPushButton:pressed { background:#cddbd2; }"
        "QPushButton#primaryAction { background:#315f53; color:#fffdf8; border-color:#315f53; font-weight:650; }"
        "QPushButton#primaryAction:hover { background:#264f45; }"
        "QLineEdit,QPlainTextEdit,QDoubleSpinBox,QComboBox { background:#fbfcf9; border:1px solid #cbd5ce; border-radius:4px; padding:7px; selection-background-color:#7d9d8d; }"
        "QLineEdit:focus,QPlainTextEdit:focus,QDoubleSpinBox:focus,QComboBox:focus { border:1px solid #527e6e; }"
        "QTableWidget { background:#fbfcf9; alternate-background-color:#f0f4f0; border:1px solid #ccd6cf; gridline-color:#e2e8e3; }"
        "QHeaderView::section { background:#e5ebe6; color:#30453e; border:0; border-bottom:1px solid #ccd6cf; padding:8px; font-weight:650; }"
        "QListWidget { background:#e8eee9; border:0; }"
    ));
    status_ = statusBar();
    status_->setSizeGripEnabled(false);
    ui::OnError = [this](std::wstring const& text) { Message(text, true); };
    hwnd_ = reinterpret_cast<HWND>(winId());
    store_ = std::make_unique<Store>(platform::DataPath());
    store_->Initialize();
    Login();
}

void AppWindow::Message(std::wstring const& text, bool error) {
    if (!status_) return;
    status_->setStyleSheet(error
        ? QStringLiteral("QStatusBar { background:#f7e6df; color:#803d2d; border-top:1px solid #dfb9aa; }")
        : QStringLiteral("QStatusBar { background:#e5ebe6; color:#334540; border-top:1px solid #cbd5ce; }"));
    status_->showMessage(Q(text), error ? 12000 : 6500);
}

bool AppWindow::Confirm(std::wstring const& message) {
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Confirm — Payrollix++"), Q(message),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

QWidget* AppWindow::Page(std::wstring const& title, std::wstring const& description) {
    auto* page = new QWidget;
    page->setObjectName("page");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 32);
    layout->setSpacing(14);
    layout->addWidget(ui::Heading(page, title, description));
    layout->addStretch(1);
    return page;
}

void AppWindow::Display(QWidget* page) {
    if (!bodyLayout_ || !page) return;
    while (auto* item = bodyLayout_->takeAt(0)) {
        if (auto* widget = item->widget()) widget->deleteLater();
        delete item;
    }
    auto* scroll = new QScrollArea(body_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    bodyLayout_->addWidget(scroll);
}

AppWindow::GridRows AppWindow::Cells(Rows const& rows, std::vector<std::string> const& keys) {
    GridRows cells;
    cells.reserve(rows.size());
    for (auto const& row : rows) {
        std::vector<std::wstring> line;
        line.reserve(keys.size());
        for (auto const& key : keys) line.push_back(W(Get(row, key)));
        cells.push_back(std::move(line));
    }
    return cells;
}

int AppWindow::SelectedId(QTableWidget const* table, Rows const& rows) {
    int index = table ? table->currentRow() : -1;
    if (index < 0 || static_cast<size_t>(index) >= rows.size())
        throw std::runtime_error("Select a record first.");
    return Id(rows[static_cast<size_t>(index)]);
}

void AppWindow::Login() {
    bool first = !store_->HasUsers();
    auto* page = Page(first ? L"Set up Payrollix++" : L"Welcome back",
        first ? L"Create the first administrator, or import your existing Payrollix++ database. No default password is created."
              : L"Sign in to your local payroll workspace.");
    page->setMaximumWidth(720);
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    auto* username = ui::Input(L"Username");
    QLineEdit *password = nullptr, *repeat = nullptr;
    auto* passField = PasswordField(QStringLiteral("Password"), password);
    QWidget* repeatField = first ? PasswordField(QStringLiteral("Confirm password"), repeat) : nullptr;
    layout->insertWidget(1, username);
    layout->insertWidget(2, passField);
    if (first) layout->insertWidget(3, repeatField);
    int position = first ? 4 : 3;
    layout->insertWidget(position++, ui::Action(first ? L"Create administrator" : L"Sign in",
        [this, username, password, repeat, first] {
            std::string user = S(username->text()), pass = S(password->text());
            if (first) {
                if (password->text() != repeat->text())
                    throw std::runtime_error("Passwords do not match.");
                store_->CreateAdmin(user, pass);
                Login();
                Message(L"Administrator created. Sign in to continue.");
                return;
            }
            if (std::chrono::steady_clock::now() < retryAfter_)
                throw std::runtime_error("Too many attempts. Wait 30 seconds before retrying.");
            if (!store_->Login(user, pass)) {
                if (++failedLogins_ >= 5) {
                    retryAfter_ = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                    failedLogins_ = 0;
                }
                password->clear();
                throw std::runtime_error("Incorrect username or password.");
            }
            failedLogins_ = 0;
            password->clear();
            Shell();
            if (pass == "admin123") {
                Account();
                Message(L"The original default password is still in use. Change it before entering company information.", true);
            }
        }, this, true));
    if (first) {
        layout->insertWidget(position++, ui::Text(L"Existing users: select an untouched copy of payroll.db or a full database backup. The source is not modified."));
        layout->insertWidget(position++, ui::Action(L"Import existing database", [this] {
            auto file = platform::ChooseFile(hwnd_, false, L"db",
                                              L"Import existing Payrollix++ database");
            if (file.empty()) return;
            store_->Restore(file);
            Login();
            Message(L"Database imported. Sign in with its existing credentials.");
        }, this));
    }
    layout->insertWidget(position, ui::Text(L"Data stays on this Windows account. This app does not submit taxes, transfer wages, or provide certified tax calculations."));
    auto* root = new QWidget;
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(root);
    body_ = root;
    bodyLayout_ = rootLayout;
    Display(page);
}

void AppWindow::Shell() {
    auto* root = new QWidget;
    auto* layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* sidebar = new QWidget(root);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(230);
    sidebar->setStyleSheet(QStringLiteral("#sidebar { background:#e8eee9; border-right:1px solid #d1dbd4; }"));
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(18, 24, 18, 20);
    sideLayout->setSpacing(7);
    auto* brand = ui::Text(L"Payrollix++", 20, sidebar);
    brand->setStyleSheet(QStringLiteral("color:#244a40; font-weight:700;"));
    sideLayout->addWidget(brand);
    auto* userLabel = ui::Text(L"LOCAL WORKSPACE  /  " + W(store_->Username()), 10, sidebar);
    userLabel->setStyleSheet(QStringLiteral("color:#77867e; font-weight:650;"));
    sideLayout->addWidget(userLabel);
    sideLayout->addSpacing(18);
    std::vector<std::pair<std::wstring, std::wstring>> pages = {
        {L"Dashboard",L"dashboard"},{L"Employees",L"employees"},{L"Add employee",L"add"},
        {L"Departments",L"departments"},{L"Pay periods",L"periods"},{L"Batch payroll",L"batch"},
        {L"Reports",L"reports"},{L"Audit log",L"audit"},{L"Backup and restore",L"backups"},
        {L"Account",L"account"},{L"Sign out",L"logout"}};
    for (auto const& entry : pages) {
        auto label = entry.first;
        auto key = entry.second;
        sideLayout->addWidget(ui::Action(label, [this, key] { Navigate(key); }, this));
    }
    sideLayout->addStretch(1);
    sideLayout->addWidget(ui::Text(L"Local payroll records\nAmounts shown in USD", 11, sidebar));
    auto* content = new QWidget(root);
    content->setObjectName("workspaceContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(sidebar);
    layout->addWidget(content, 1);
    setCentralWidget(root);
    body_ = content;
    bodyLayout_ = contentLayout;
    setWindowTitle(QStringLiteral("Payrollix++ — %1").arg(QString::fromStdString(store_->Username())));
    Dashboard();
}

void AppWindow::Navigate(std::wstring const& key) {
    if (status_) status_->clearMessage();
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
        setWindowTitle(QStringLiteral("Payrollix++"));
        Login();
    }
}

void AppWindow::Dashboard() {
    auto* page = Page(L"Payroll overview", L"Current employee totals, not a historical pay-run report. Amounts are USD.");
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    auto rows = store_->Query("SELECT COUNT(*) AS employees,COALESCE(SUM(gross_salary+overtime_pay),0) AS gross,"
                              "COALESCE(SUM(net),0) AS net FROM payroll WHERE status='active' OR status IS NULL");
    auto const& row = rows[0];
    layout->insertWidget(1, ui::Heading(page, W(Get(row, "employees")) + L" active employees"));
    layout->insertWidget(2, ui::Text(L"Gross including overtime: $" + W(Decimal(Amount(row, "gross"))), 20));
    layout->insertWidget(3, ui::Text(L"Estimated net payroll: $" + W(Decimal(Amount(row, "net"))), 20));
    auto period = store_->Query("SELECT name,start_date,end_date,pay_date FROM pay_periods WHERE status='open' ORDER BY start_date DESC LIMIT 1");
    layout->insertWidget(4, ui::Text(period.empty() ? L"No open pay period. Create one under Pay periods."
        : L"Latest open period: " + W(Get(period[0], "name")) + L" | Pay date: " + W(Get(period[0], "pay_date"))));
    layout->insertWidget(5, ui::Action(L"Refresh", [this] { Dashboard(); }, this, true));
    layout->insertWidget(6, ui::Heading(page, L"Recent activity"));
    auto activity = store_->Query("SELECT timestamp,action,details FROM audit_log ORDER BY id DESC LIMIT 10");
    layout->insertWidget(7, ui::Table({L"When (UTC)",L"Action",L"Details"},
                                       Cells(activity, {"timestamp","action","details"}), page));
    layout->insertWidget(8, ui::Text(L"Tax calculation limits: the inherited model assumes monthly salary, uses fixed federal brackets and a flat 5% state rate. Have payroll professionals validate it before business use."));
    Display(page);
}

void AppWindow::Account() {
    auto* page = Page(L"Account", L"Signed in as " + W(store_->Username()) + L". Change your local application password here.");
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    QLineEdit *current = nullptr, *next = nullptr, *repeat = nullptr;
    layout->insertWidget(1, PasswordField(QStringLiteral("Current password"), current));
    layout->insertWidget(2, PasswordField(QStringLiteral("New password (at least 12 characters)"), next));
    layout->insertWidget(3, PasswordField(QStringLiteral("Confirm new password"), repeat));
    layout->insertWidget(4, ui::Action(L"Change password", [this, current, next, repeat] {
        if (next->text() != repeat->text()) throw std::runtime_error("Passwords do not match.");
        store_->ChangePassword(S(current->text()), S(next->text()));
        current->clear(); next->clear(); repeat->clear();
        Message(L"Password changed.");
    }, this, true));
    layout->insertWidget(5, ui::Text(L"Passwords use Windows BCrypt PBKDF2-SHA256 with a random salt. Existing SHA-256 passwords are upgraded on successful sign-in. Local database access is governed by Windows folder permissions; this is not a multi-user server or a role-based approval system."));
    Display(page);
}

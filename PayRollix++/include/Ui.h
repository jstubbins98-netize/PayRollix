#pragma once

#include <QComboBox>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QDoubleSpinBox>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <exception>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

inline std::function<void(std::wstring const&)> OnError;

inline QString Q(std::wstring const& value) { return QString::fromStdWString(value); }
inline QString Q(std::string const& value) { return QString::fromUtf8(value.data(), static_cast<int>(value.size())); }
inline QString Q(char const* value) { return QString::fromUtf8(value); }
inline std::string Utf8(QString const& value) { return value.toUtf8().toStdString(); }
inline std::wstring Wide(QString const& value) { return value.toStdWString(); }

inline void ReportError(std::wstring const& text) noexcept {
    try {
        if (OnError) OnError(text);
        else QMessageBox::critical(nullptr, QStringLiteral("Payrollix++"), Q(text));
    } catch (...) {}
}
inline void ReportError(std::exception const& error) noexcept {
    try { ReportError(QString::fromUtf8(error.what()).toStdWString()); }
    catch (...) { ReportError(L"An unexpected error occurred."); }
}

class InputField final : public QWidget {
public:
    explicit InputField(std::wstring const& label, std::wstring const& value = L"",
                        bool multiline = false, QWidget* parent = nullptr)
        : QWidget(parent), editor_(new QPlainTextEdit(this)) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        auto* title = new QLabel(Q(label), this);
        title->setObjectName("fieldLabel");
        layout->addWidget(title);
        layout->addWidget(editor_);
        editor_->setPlainText(Q(value));
        editor_->setTabChangesFocus(true);
        if (multiline) {
            editor_->setMinimumHeight(92);
            editor_->setMaximumHeight(180);
        } else {
            editor_->setFixedHeight(40);
            editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }
    QString text() const { return editor_->toPlainText(); }
    void setText(QString const& value) { editor_->setPlainText(value); }
    void setReadOnly(bool value) { editor_->setReadOnly(value); }
    void setMinimumHeight(int value) { editor_->setMinimumHeight(value); QWidget::setMinimumHeight(value + 24); }
    void setPlaceholderText(QString const& value) { editor_->setPlaceholderText(value); }
private:
    QPlainTextEdit* editor_;
};

inline QWidget* Row(QWidget* parent = nullptr) {
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(10);
    return widget;
}
inline void Add(QWidget* parent, QWidget* child) {
    if (auto* layout = qobject_cast<QBoxLayout*>(parent ? parent->layout() : nullptr))
        layout->addWidget(child);
}
inline QLabel* Text(std::wstring const& text, int size = 14, QWidget* parent = nullptr) {
    auto* label = new QLabel(Q(text), parent);
    label->setWordWrap(true);
    if (size >= 20) {
        QFont font = label->font();
        font.setPointSize(size);
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
    }
    return label;
}
inline QWidget* Heading(QWidget* parent, std::wstring const& title,
                        std::wstring const& description = L"") {
    auto* block = new QWidget(parent);
    auto* layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(5);
    auto* titleLabel = Text(title, 24, block);
    titleLabel->setObjectName("pageTitle");
    layout->addWidget(titleLabel);
    if (!description.empty()) {
        auto* detail = Text(description, 13, block);
        detail->setObjectName("pageDescription");
        layout->addWidget(detail);
    }
    return block;
}
inline InputField* Input(std::wstring const& label, std::wstring const& value = L"",
                          bool multiline = false, QWidget* parent = nullptr) {
    return new InputField(label, value, multiline, parent);
}
class NumberField final : public QWidget {
public:
    explicit NumberField(std::wstring const& label, double value = 0, QWidget* parent = nullptr)
        : QWidget(parent), spin_(new QDoubleSpinBox(this)) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        auto* title = new QLabel(Q(label), this);
        title->setObjectName("fieldLabel");
        spin_->setRange(0.0, 1000000000.0);
        spin_->setDecimals(2);
        spin_->setSingleStep(1.0);
        spin_->setValue(value);
        spin_->setMinimumHeight(40);
        layout->addWidget(title);
        layout->addWidget(spin_);
    }
    double value() const { return spin_->value(); }
    void setRange(double minimum, double maximum) { spin_->setRange(minimum, maximum); }
    void setSingleStep(double value) { spin_->setSingleStep(value); }
private:
    QDoubleSpinBox* spin_;
};
inline NumberField* Number(std::wstring const& label, double value = 0, QWidget* parent = nullptr) {
    return new NumberField(label, value, parent);
}

class ChoiceField final : public QWidget {
public:
    explicit ChoiceField(std::wstring const& label, std::vector<std::wstring> const& options,
                         int selected = 0, QWidget* parent = nullptr)
        : QWidget(parent), combo_(new QComboBox(this)) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        auto* title = new QLabel(Q(label), this);
        title->setObjectName("fieldLabel");
        for (auto const& option : options) combo_->addItem(Q(option));
        if (options.empty()) combo_->setCurrentIndex(-1);
        else combo_->setCurrentIndex(std::clamp(selected, 0, static_cast<int>(options.size()) - 1));
        combo_->setMinimumHeight(40);
        layout->addWidget(title);
        layout->addWidget(combo_);
    }
    int currentIndex() const { return combo_->currentIndex(); }
    void setCurrentIndex(int index) { combo_->setCurrentIndex(index); }
    QString currentText() const { return combo_->currentText(); }
private:
    QComboBox* combo_;
};
inline ChoiceField* Choice(std::wstring const& label, std::vector<std::wstring> const& options,
                            int selected = 0, QWidget* parent = nullptr) {
    return new ChoiceField(label, options, selected, parent);
}
inline QPushButton* Action(std::wstring const& label, std::function<void()> callback,
                           QWidget* context, bool primary = false) {
    auto* button = new QPushButton(Q(label), context);
    button->setObjectName(primary ? "primaryAction" : "actionButton");
    button->setMinimumHeight(38);
    QObject::connect(button, &QPushButton::clicked, context,
        [callback = std::move(callback)] {
            try { if (callback) callback(); }
            catch (std::exception const& error) { ReportError(error); }
            catch (...) { ReportError(L"An unexpected error occurred."); }
        });
    return button;
}
inline QTableWidget* Table(std::vector<std::wstring> const& headers,
                           std::vector<std::vector<std::wstring>> const& rows,
                           QWidget* parent = nullptr) {
    auto* table = new QTableWidget(static_cast<int>(rows.size()), static_cast<int>(headers.size()), parent);
    table->setObjectName("recordsTable");
    table->setHorizontalHeaderLabels([&] {
        QStringList labels;
        for (auto const& header : headers) labels.push_back(Q(header));
        return labels;
    }());
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->setMinimumHeight(120);
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        for (int col = 0; col < static_cast<int>(headers.size()); ++col) {
            auto* item = new QTableWidgetItem(col < static_cast<int>(rows[row].size()) ? Q(rows[row][col]) : QString{});
            item->setToolTip(item->text());
            table->setItem(row, col, item);
        }
        table->setRowHeight(row, 34);
    }
    return table;
}
inline QString Value(InputField const* input) { return input ? input->text() : QString{}; }
inline std::wstring Selected(ChoiceField const* choice) {
    return choice && choice->currentIndex() >= 0 ? choice->currentText().toStdWString() : std::wstring{};
}

} // namespace ui

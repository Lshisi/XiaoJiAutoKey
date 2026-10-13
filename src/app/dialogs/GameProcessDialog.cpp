#include "dialogs/GameProcessDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentComboBox.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <set>

namespace {

QStringList RunningProcessNames() {
    QStringList names;
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return names;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (::Process32FirstW(snapshot, &entry)) {
        std::set<QString> seen;
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
            if (!name.isEmpty() && name.size() <= 64 && seen.insert(name).second) {
                names.push_back(name);
            }
        } while (::Process32NextW(snapshot, &entry));
    }
    ::CloseHandle(snapshot);
    names.sort();
    return names;
}

}  // namespace

GameProcessDialog::GameProcessDialog(const QStringList& initialProcesses, QWidget* parent)
    : QDialog(parent), processes_(initialProcesses) {
    setWindowTitle(QString::fromWCharArray(L"\u6e38\u620f\u8fdb\u7a0b\u7ba1\u7406"));
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(420, 460);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(windowTitle(), this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    root->addLayout(titleRow);

    auto* descLabel = new QLabel(QString::fromWCharArray(
        L"\u5df2\u5185\u7f6e\u8bc6\u522b\u5251\u7f513\u65d7\u8230\u7aef / \u65e0\u754c / "
        L"\u7f18\u8d77\u7b49 JX3Client \u7cfb\u5ba2\u6237\u7aef\u3002"
        L"\u5982\u9700\u989d\u5916\u8bc6\u522b\uff08\u5982\u6a21\u62df\u5668\u6216\u542f\u52a8\u5668\uff09\uff0c"
        L"\u5728\u6b64\u6dfb\u52a0\u8fdb\u7a0b\u540d\uff0c\u652f\u6301 name* \u524d\u7f00\u901a\u914d\u3002"), this);
    descLabel->setFont(Fluent::Typography::caption());
    descLabel->setWordWrap(true);
    QPalette descPalette = descLabel->palette();
    descPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    descLabel->setPalette(descPalette);
    root->addWidget(descLabel);

    processCombo_ = new FluentComboBox(this);
    processCombo_->setFixedHeight(28);
    processCombo_->setSearchable(true);
    root->addWidget(processCombo_);

    auto* addRow = new QHBoxLayout();
    addRow->setSpacing(8);
    processEdit_ = new FluentLineEdit(this);
    processEdit_->setPlaceholderText(QString::fromWCharArray(L"\u624b\u52a8\u8f93\u5165\u8fdb\u7a0b\u540d\uff0c\u5982 mumu*"));
    addRow->addWidget(processEdit_, 1);
    auto* addBtn = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u6dfb\u52a0"), this);
    addBtn->setFixedWidth(72);
    addRow->addWidget(addBtn);
    root->addLayout(addRow);

    listWidget_ = new QListWidget(this);
    listWidget_->setMinimumHeight(180);
    listWidget_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    refreshListTheme();
    root->addWidget(listWidget_, 1);

    root->addStretch();

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancelButton = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u53d6\u6d88"), this);
    okButton_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u4fdd\u5b58"), this);
    cancelButton->setFixedWidth(78);
    okButton_->setFixedWidth(78);
    buttons->addWidget(cancelButton);
    buttons->addSpacing(8);
    buttons->addWidget(okButton_);
    root->addLayout(buttons);

    for (const auto& name : processes_) {
        appendRow(name);
    }

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        addProcess(processEdit_->text().trimmed().toLower());
    });
    connect(processEdit_, &FluentLineEdit::returnPressed, this, [this]() {
        addProcess(processEdit_->text().trimmed().toLower());
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(okButton_, &QPushButton::clicked, this, &QDialog::accept);
}

QStringList GameProcessDialog::processes() const {
    QStringList result;
    for (int i = 0; i < listWidget_->count(); ++i) {
        const QString name = listWidget_->item(i)->data(Qt::UserRole).toString().trimmed();
        if (!name.isEmpty()) {
            result.push_back(name);
        }
    }
    return result;
}

void GameProcessDialog::addProcess(const QString& name) {
    if (name.isEmpty() || name.size() > 260) {
        return;
    }
    for (int i = 0; i < listWidget_->count(); ++i) {
        if (listWidget_->item(i)->data(Qt::UserRole).toString() == name) {
            return;
        }
    }
    appendRow(name);
    processEdit_->clear();
}

void GameProcessDialog::appendRow(const QString& name) {
    // 进程名放在 UserRole，条目本身不设 text，避免 itemWidget 的行标签与
    // 委托默认绘制的文本叠在一起。
    auto* item = new QListWidgetItem();
    item->setData(Qt::UserRole, name);
    item->setSizeHint(QSize(0, 32));
    listWidget_->addItem(item);

    auto* row = new QWidget();
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(8, 0, 6, 0);
    rowLayout->setSpacing(6);

    auto* nameLabel = new QLabel(name, row);
    nameLabel->setFont(Fluent::Typography::body());
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(nameLabel, 1);

    auto* removeButton = new FluentButton(FluentButton::Icon, QString(QChar(0x00D7)), row);
    removeButton->setFixedSize(22, 22);
    removeButton->setAccessibleName(QString::fromWCharArray(L"\u5220\u9664 %1").arg(name));
    connect(removeButton, &QPushButton::clicked, this, [this, row]() {
        for (int i = 0; i < listWidget_->count(); ++i) {
            if (listWidget_->itemWidget(listWidget_->item(i)) == row) {
                delete listWidget_->takeItem(i);
                return;
            }
        }
    });
    rowLayout->addWidget(removeButton, 0, Qt::AlignVCenter);

    listWidget_->setItemWidget(item, row);
}

void GameProcessDialog::refreshListTheme() {
    if (!listWidget_) {
        return;
    }
    listWidget_->setStyleSheet(QString(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 6px; "
        "color: %3; padding: 4px; }"
        "QListWidget::item { border-radius: 4px; }"
        "QListWidget::item:hover { background: %4; }"
        "QListWidget::item:selected { background: %5; }")
        .arg(Fluent::Colors::cardBg().name(),
            Fluent::Colors::border().name(QColor::HexArgb),
            Fluent::Colors::textPrimary().name(),
            Fluent::Colors::hoverBg().name(QColor::HexArgb),
            Fluent::Colors::selectedBg().name(QColor::HexArgb)));

    for (int i = 0; i < listWidget_->count(); ++i) {
        QWidget* row = listWidget_->itemWidget(listWidget_->item(i));
        if (!row) {
            continue;
        }
        if (auto* label = row->findChild<QLabel*>()) {
            QPalette labelPalette = label->palette();
            labelPalette.setColor(QPalette::WindowText, Fluent::Colors::textPrimary());
            label->setPalette(labelPalette);
        }
    }
}

void GameProcessDialog::refreshRunningProcesses() {
    const QStringList running = RunningProcessNames();
    processCombo_->blockSignals(true);
    processCombo_->clear();
    processCombo_->addItem(QString::fromWCharArray(L"\u4ece\u8fd0\u884c\u4e2d\u7684\u8fdb\u7a0b\u9009\u62e9..."));
    processCombo_->addItems(running);
    processCombo_->setCurrentIndex(0);
    processCombo_->blockSignals(false);
    disconnect(processCombo_, nullptr, nullptr, nullptr);
    connect(processCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index <= 0) return;
        addProcess(processCombo_->itemText(index).toLower());
    });
}

void GameProcessDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void GameProcessDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshRunningProcesses();
    refreshListTheme();
}

void GameProcessDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void GameProcessDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void GameProcessDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

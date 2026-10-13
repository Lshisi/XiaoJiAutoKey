#include "dialogs/ShareImportDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentContextMenu.h"
#include "ui/styles/DesignTokens.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTextOption>
#include <QVBoxLayout>

namespace {

QString PayloadSummary(const SharePayload& payload) {
    QStringList parts;
    if (!payload.profiles.empty()) {
        parts.push_back(QString::fromWCharArray(L"%1 \u4e2a\u65b9\u6848")
            .arg(payload.profiles.size()));
    }
    if (!payload.macros.empty()) {
        parts.push_back(QString::fromWCharArray(L"%1 \u4e2a\u5b8f")
            .arg(payload.macros.size()));
    }
    QString summary = QString::fromWCharArray(L"\u5c06\u5bfc\u5165 ") + parts.join(QString::fromWCharArray(L"\uff0c"));
    QStringList names;
    for (const auto& profile : payload.profiles) {
        names.push_back(QString::fromStdWString(profile.name));
        if (names.size() >= 4) break;
    }
    for (const auto& macro : payload.macros) {
        names.push_back(QString::fromStdWString(macro.name));
        if (names.size() >= 4) break;
    }
    if (!names.empty()) {
        summary += QString::fromWCharArray(L"\uff1a") + names.join(QString::fromWCharArray(L"\u3001"));
        if (payload.profiles.size() + payload.macros.size() > names.size()) {
            summary += QString::fromWCharArray(L" \u7b49");
        }
    }
    if (!payload.profiles.empty() && !payload.faction.empty()) {
        summary += QString::fromWCharArray(
            L"\n\u5bfc\u5165\u540e\u5c06\u540c\u6b65\u5207\u6362\u95e8\u6d3e\u4e3b\u9898\u4e3a %1\u3002")
            .arg(QString::fromStdString(payload.faction));
    }
    return summary;
}

}  // namespace

ShareImportDialog::ShareImportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QString::fromWCharArray(L"\u5bfc\u5165\u914d\u7f6e"));
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(560, 460);

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

    textEdit_ = new QPlainTextEdit(this);
    textEdit_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(textEdit_, &QPlainTextEdit::customContextMenuRequested, this,
        [this](const QPoint& pos) {
            FluentContextMenu::popupForPlainTextEdit(textEdit_,
                textEdit_->viewport()->mapToGlobal(pos));
        });
    textEdit_->setWordWrapMode(QTextOption::WrapAnywhere);
    textEdit_->setStyleSheet(QString(
        "QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 6px;"
        " color: %3; padding: 8px; font-family: Consolas, 'Microsoft YaHei UI'; selection-background-color: %4; }")
        .arg(Fluent::Colors::cardBg().name(),
            Fluent::Colors::border().name(QColor::HexArgb),
            Fluent::Colors::textPrimary().name(),
            Fluent::Colors::selectedBg().name(QColor::HexArgb))
        + Fluent::Styles::scrollBarQss(Fluent::Styles::scrollBarHandle(),
            Fluent::Styles::scrollBarHandleHover()));
    textEdit_->setPlaceholderText(QString::fromWCharArray(
        L"\u7c98\u8d34\u5206\u4eab\u7801\uff08XJK1:\u2026\uff09\u6216\u6587\u672c\u914d\u65b9\uff0c"
        L"\u5185\u5bb9\u8bc6\u522b\u540e\u5373\u53ef\u5bfc\u5165"));
    root->addWidget(textEdit_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setFont(Fluent::Typography::caption());
    statusLabel_->setWordWrap(true);
    QPalette statusPalette = statusLabel_->palette();
    statusPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    statusLabel_->setPalette(statusPalette);
    root->addWidget(statusLabel_);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    pasteBtn_ = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u7c98\u8d34"), this);
    pasteBtn_->setFixedWidth(78);
    importBtn_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u5bfc\u5165"), this);
    importBtn_->setFixedWidth(78);
    importBtn_->setEnabled(false);
    buttons->addWidget(pasteBtn_);
    buttons->addWidget(importBtn_);
    root->addLayout(buttons);

    // 内容变化时即时解析，识别成功即可导入，无需单独点预览。
    connect(textEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        previewImport();
    });
    connect(pasteBtn_, &QPushButton::clicked, this, [this]() {
        textEdit_->setPlainText(QApplication::clipboard()->text());
    });
    connect(importBtn_, &QPushButton::clicked, this, [this]() {
        if (hasImport_) {
            accept();
        }
    });
}

void ShareImportDialog::setText(const QString& text) {
    if (!textEdit_) {
        return;
    }
    textEdit_->setPlainText(text);
    if (hasImport_ && importBtn_) {
        importBtn_->setFocus();
    }
}

void ShareImportDialog::previewImport() {
    hasImport_ = false;
    importBtn_->setEnabled(false);

    const std::string text = textEdit_->toPlainText().trimmed().toStdString();
    if (text.empty()) {
        statusLabel_->clear();
        return;
    }
    auto payload = ShareCodec::DecodeShareCode(text);
    if (!payload.has_value()) {
        payload = ShareCodec::DecodeTextRecipe(text);
    }
    if (!payload.has_value()) {
        statusLabel_->setText(QString::fromWCharArray(
            L"\u89e3\u6790\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u5185\u5bb9\u662f\u5426\u5b8c\u6574"));
        return;
    }

    importPayload_ = std::move(*payload);
    hasImport_ = true;
    importBtn_->setEnabled(true);
    statusLabel_->setText(PayloadSummary(importPayload_));
}

void ShareImportDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void ShareImportDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ShareImportDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void ShareImportDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

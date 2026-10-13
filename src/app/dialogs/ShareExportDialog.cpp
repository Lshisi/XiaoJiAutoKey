#include "dialogs/ShareExportDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentComboBox.h"
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

// 概述导出内容，方便确认导出的是哪份方案 / 哪条宏。
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
    if (parts.empty()) {
        return {};
    }
    QString summary = parts.join(QString::fromWCharArray(L"\uff0c"));
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
    return summary;
}

}  // namespace

ShareExportDialog::ShareExportDialog(const QString& title, const SharePayload& payload, QWidget* parent)
    : QDialog(parent), payload_(payload) {
    setWindowTitle(title);
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(560, 400);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(title, this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    root->addLayout(titleRow);

    auto* formatRow = new QHBoxLayout();
    formatRow->setSpacing(8);
    auto* formatLabel = new QLabel(QString::fromWCharArray(L"\u683c\u5f0f:"), this);
    formatLabel->setFont(Fluent::Typography::body());
    formatRow->addWidget(formatLabel);
    formatCombo_ = new FluentComboBox(this);
    formatCombo_->setFixedHeight(28);
    formatCombo_->addItem(QString::fromWCharArray(
        L"\u5206\u4eab\u7801\uff08XJK1:\u2026\uff0c\u7d27\u51d1\u9002\u5408\u53d1\u9001\uff09"));
    formatCombo_->addItem(QString::fromWCharArray(
        L"\u6587\u672c\u914d\u65b9\uff08\u53ef\u8bfb\uff0c\u8d34\u5427\u8bba\u575b\u76f4\u63a5\u8d34\u6b63\u6587\uff09"));
    formatRow->addWidget(formatCombo_, 1);
    root->addLayout(formatRow);

    textEdit_ = new QPlainTextEdit(this);
    textEdit_->setReadOnly(true);
    textEdit_->setWordWrapMode(QTextOption::WrapAnywhere);
    textEdit_->setStyleSheet(QString(
        "QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 6px;"
        " color: %3; padding: 8px; font-family: Consolas, 'Microsoft YaHei UI'; selection-background-color: %4; }")
        .arg(Fluent::Colors::cardBg().name(),
            Fluent::Colors::border().name(QColor::HexArgb),
            Fluent::Colors::textPrimary().name(),
            Fluent::Colors::selectedBg().name(QColor::HexArgb)));
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
    copyBtn_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u590d\u5236"), this);
    copyBtn_->setFixedWidth(78);
    buttons->addWidget(copyBtn_);
    root->addLayout(buttons);

    summary_ = PayloadSummary(payload);
    refreshExport();

    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshExport();
    });
    connect(copyBtn_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(textEdit_->toPlainText());
        statusLabel_->setText(QString::fromWCharArray(L"\u5df2\u590d\u5236\u5230\u526a\u8d34\u677f"));
    });
}

void ShareExportDialog::refreshExport() {
    const QString content = formatCombo_->currentIndex() == 1
        ? QString::fromStdString(ShareCodec::EncodeTextRecipe(payload_))
        : QString::fromStdString(ShareCodec::EncodeShareCode(payload_));
    textEdit_->setPlainText(content);
    QString status = summary_;
    if (!status.isEmpty()) {
        status += QString::fromWCharArray(L" \u00b7 ");
    }
    status += QString::fromWCharArray(L"\u957f\u5ea6: %1 \u5b57\u7b26").arg(content.size());
    statusLabel_->setText(status);
}

void ShareExportDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void ShareExportDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ShareExportDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void ShareExportDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

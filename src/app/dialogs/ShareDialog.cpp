#include "dialogs/ShareDialog.h"

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
#include <QShowEvent>
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

ShareDialog::ShareDialog(Mode initialMode, const ExportData& exportData, QWidget* parent)
    : QDialog(parent), exportData_(exportData) {
    setWindowTitle(QString::fromWCharArray(L"\u914d\u7f6e\u5206\u4eab"));
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(560, 560);

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

    auto* modeRow = new QHBoxLayout();
    modeRow->setSpacing(8);
    auto* modeLabel = new QLabel(QString::fromWCharArray(L"\u6a21\u5f0f:"), this);
    modeLabel->setFont(Fluent::Typography::body());
    modeRow->addWidget(modeLabel);
    modeCombo_ = new FluentComboBox(this);
    modeCombo_->setFixedHeight(28);
    modeCombo_->addItem(QString::fromWCharArray(L"\u5bfc\u51fa\u6309\u952e\u65b9\u6848"));
    modeCombo_->addItem(QString::fromWCharArray(L"\u5bfc\u51fa\u5b8f"));
    modeCombo_->addItem(QString::fromWCharArray(L"\u5bfc\u5165\u914d\u7f6e"));
    modeRow->addWidget(modeCombo_, 1);
    root->addLayout(modeRow);

    auto* optionRow = new QHBoxLayout();
    optionRow->setSpacing(8);
    scopeCombo_ = new FluentComboBox(this);
    scopeCombo_->setFixedHeight(28);
    scopeCombo_->addItem(QString::fromWCharArray(L"\u5f53\u524d\u65b9\u6848"));
    scopeCombo_->addItem(QString::fromWCharArray(L"\u5f53\u524d\u5b8f"));
    scopeCombo_->addItem(QString::fromWCharArray(L"\u5168\u90e8\u5185\u5bb9"));
    optionRow->addWidget(scopeCombo_, 1);
    formatCombo_ = new FluentComboBox(this);
    formatCombo_->setFixedHeight(28);
    formatCombo_->addItem(QString::fromWCharArray(L"\u5206\u4eab\u7801"));
    formatCombo_->addItem(QString::fromWCharArray(L"\u6587\u672c\u914d\u65b9"));
    optionRow->addWidget(formatCombo_, 1);
    root->addLayout(optionRow);

    textEdit_ = new QPlainTextEdit(this);
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
    statusLabel_->setText(QString::fromWCharArray(L"\u957f\u5ea6: 0 \u5b57\u7b26"));
    root->addWidget(statusLabel_);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    copyBtn_ = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u590d\u5236"), this);
    copyBtn_->setFixedWidth(78);
    previewBtn_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u9884\u89c8"), this);
    previewBtn_->setFixedWidth(78);
    importBtn_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u5bfc\u5165"), this);
    importBtn_->setFixedWidth(78);
    buttons->addWidget(copyBtn_);
    buttons->addWidget(previewBtn_);
    buttons->addWidget(importBtn_);
    root->addLayout(buttons);

    switch (initialMode) {
    case Mode::ExportMacro:
        modeCombo_->setCurrentIndex(1);
        break;
    case Mode::Import:
        modeCombo_->setCurrentIndex(2);
        break;
    case Mode::ExportProfile:
    default:
        modeCombo_->setCurrentIndex(0);
        break;
    }

    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshMode();
    });
    connect(scopeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshExport();
    });
    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshExport();
    });
    connect(copyBtn_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(textEdit_->toPlainText());
        statusLabel_->setText(QString::fromWCharArray(L"\u5df2\u590d\u5236\u5230\u526a\u8d34\u677f"));
    });
    connect(previewBtn_, &QPushButton::clicked, this, [this]() {
        previewImport();
    });
    connect(importBtn_, &QPushButton::clicked, this, [this]() {
        if (!hasImport_) {
            previewImport();
        }
        if (hasImport_) {
            accept();
        }
    });

    refreshMode();
}

void ShareDialog::refreshMode() {
    importing_ = modeCombo_->currentIndex() == 2;
    scopeCombo_->setVisible(!importing_);
    formatCombo_->setVisible(!importing_);
    copyBtn_->setVisible(!importing_);
    previewBtn_->setVisible(importing_);
    importBtn_->setVisible(importing_);
    importBtn_->setEnabled(false);
    hasImport_ = false;
    if (importing_) {
        textEdit_->setReadOnly(false);
        textEdit_->clear();
        textEdit_->setPlaceholderText(QString::fromWCharArray(
            L"\u7c98\u8d34\u5206\u4eab\u7801\uff08XJK1:\u2026\uff09\u6216\u6587\u672c\u914d\u65b9\uff0c"
            L"\u7136\u540e\u70b9\u51fb\u9884\u89c8"));
        statusLabel_->setText(QString());
    } else {
        textEdit_->setReadOnly(true);
        textEdit_->setPlaceholderText(QString());
        refreshExport();
    }
}

void ShareDialog::refreshExport() {
    const SharePayload* payload = &exportData_.everything;
    if (scopeCombo_->currentIndex() == 0) {
        payload = &exportData_.currentProfile;
    } else if (scopeCombo_->currentIndex() == 1) {
        payload = &exportData_.currentMacro;
    }

    QString content;
    if (formatCombo_->currentIndex() == 0) {
        content = QString::fromStdString(ShareCodec::EncodeShareCode(*payload));
    } else {
        content = QString::fromStdString(ShareCodec::EncodeTextRecipe(*payload));
    }
    textEdit_->setPlainText(content);
    statusLabel_->setText(QString::fromWCharArray(L"\u957f\u5ea6: %1 \u5b57\u7b26")
        .arg(content.size()));
}

void ShareDialog::previewImport() {
    hasImport_ = false;
    importBtn_->setEnabled(false);

    const std::string text = textEdit_->toPlainText().trimmed().toStdString();
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

void ShareDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void ShareDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
}

void ShareDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ShareDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void ShareDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

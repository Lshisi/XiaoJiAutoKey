#include "dialogs/PauseKeyDialog.h"

#include "ui/components/FluentButton.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PauseKeyDialog::PauseKeyDialog(QWidget* parent) : KeyCaptureDialogBase(parent) {
    setWindowTitle(QString::fromWCharArray(L"\u6dfb\u52a0\u6682\u505c\u6309\u952e"));
    setFixedSize(320, 140);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setStyleSheet("QDialog { background: #FFFFFF; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto* keyRow = new QHBoxLayout();
    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), this);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(50);
    keyRow->addWidget(keyLabel);
    auto* keyEdit = new FluentLineEdit(this);
    keyEdit->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    keyRow->addWidget(keyEdit);
    layout->addLayout(keyRow);
    setCaptureEditor(keyEdit);

    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), this);
    cancelBtn->setFixedWidth(80);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), this);
    okBtn->setFixedWidth(80);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (!capturedKey().has_value() || !capturedKey()->IsValid()) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u3002"));
            return;
        }
        accept();
    });
}

PauseKeyEditResult PauseKeyDialog::editResult() const {
    return {*capturedKey()};
}

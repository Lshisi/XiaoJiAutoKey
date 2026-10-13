#include "dialogs/ConfigNameDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

ConfigNameDialog::ConfigNameDialog(const QString& title,
    const QString& label,
    const QString& initialName,
    QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(360, 196);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(title, this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleLabel->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    root->addLayout(titleRow);

    auto* descLabel = new QLabel(label, this);
    descLabel->setFont(Fluent::Typography::caption());
    descLabel->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textSecondary().name()));
    root->addWidget(descLabel);

    nameEdit_ = new FluentLineEdit(initialName, this);
    root->addWidget(nameEdit_);

    root->addStretch();

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancelButton = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), this);
    okButton_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), this);
    cancelButton->setFixedWidth(78);
    okButton_->setFixedWidth(78);
    buttons->addWidget(cancelButton);
    buttons->addSpacing(8);
    buttons->addWidget(okButton_);
    root->addLayout(buttons);

    auto updateOkState = [this]() {
        okButton_->setEnabled(!nameEdit_->text().trimmed().isEmpty());
    };
    connect(nameEdit_, &QLineEdit::textChanged, this, updateOkState);
    connect(nameEdit_, &QLineEdit::returnPressed, this, [this]() {
        if (!nameEdit_->text().trimmed().isEmpty()) accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(okButton_, &QPushButton::clicked, this, &QDialog::accept);
    updateOkState();
}

QString ConfigNameDialog::nameText() const {
    return nameEdit_ ? nameEdit_->text().trimmed() : QString();
}

bool ConfigNameDialog::getName(QWidget* parent,
    const QString& title,
    const QString& label,
    const QString& initialName,
    QString& name) {
    ConfigNameDialog dialog(title, label, initialName, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    name = dialog.nameText();
    return !name.isEmpty();
}

void ConfigNameDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void ConfigNameDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (nameEdit_) {
        nameEdit_->setFocus();
        nameEdit_->selectAll();
    }
}

void ConfigNameDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ConfigNameDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void ConfigNameDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

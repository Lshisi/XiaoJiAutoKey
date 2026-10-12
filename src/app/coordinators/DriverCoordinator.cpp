#include "coordinators/DriverCoordinator.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/pages/DriverPage.h"
#include "ui/styles/DesignTokens.h"

#include <QCloseEvent>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QProgressBar>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>
#include <QPointer>
#include <utility>

namespace {
struct DriverUninstallResult {
    bool ok = false;
    std::wstring message;
};

QString driverName(DriverMode mode) {
    return mode == DriverMode::Hid ? QStringLiteral("HID") : QStringLiteral("DD");
}

class DriverUninstallProgressDialog final : public QDialog {
public:
    explicit DriverUninstallProgressDialog(DriverMode mode, QWidget* parent)
        : QDialog(parent) {
        const QString name = driverName(mode);
        setWindowTitle(name + QString::fromWCharArray(L" \u5378\u8f7d"));
        DialogChrome::apply(this);
        setModal(true);
        setWindowModality(Qt::ApplicationModal);
        setFixedSize(420, 168);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 20, 24, 20);
        root->setSpacing(12);

        auto* titleLabel = new QLabel(QString::fromWCharArray(L"\u6b63\u5728\u540e\u53f0\u5378\u8f7d ") + name, this);
        titleLabel->setFont(Fluent::Typography::cardTitle());
        titleLabel->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
        root->addWidget(titleLabel);

        messageLabel_ = new QLabel(QString::fromWCharArray(L"\u8bf7\u52ff\u5173\u95ed\u7a0b\u5e8f\uff0c\u5378\u8f7d\u5b8c\u6210\u540e\u5c06\u5c1d\u8bd5\u91cd\u542f\u7535\u8111\u3002"), this);
        messageLabel_->setFont(Fluent::Typography::body());
        messageLabel_->setWordWrap(true);
        messageLabel_->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textSecondary().name()));
        root->addWidget(messageLabel_);

        progressBar_ = new QProgressBar(this);
        progressBar_->setRange(0, 100);
        progressBar_->setValue(6);
        progressBar_->setTextVisible(true);
        progressBar_->setFixedHeight(22);
        progressBar_->setStyleSheet(QString(
            "QProgressBar { border: 1px solid %1; border-radius: 6px; background: %2; color: %3; text-align: center; }"
            "QProgressBar::chunk { border-radius: 5px; background: %4; }")
            .arg(Fluent::Colors::border().name(QColor::HexArgb),
                 Fluent::Colors::sidebarBg().name(),
                 Fluent::Colors::textPrimary().name(),
                 Fluent::Colors::primary().name()));
        root->addWidget(progressBar_);

        connect(&timer_, &QTimer::timeout, this, [this]() {
            if (!progressBar_) return;
            const int current = progressBar_->value();
            const int next = current < 55 ? current + 3 : (current < 86 ? current + 1 : current);
            progressBar_->setValue(qMin(next, 90));
        });
        timer_.start(260);
    }

    void markFinished() {
        timer_.stop();
        if (progressBar_) progressBar_->setValue(100);
        if (messageLabel_) {
            messageLabel_->setText(QString::fromWCharArray(L"\u5378\u8f7d\u5b8c\u6210\uff0c\u6b63\u5728\u5904\u7406\u540e\u7eed\u72b6\u6001..."));
        }
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        if (event) event->ignore();
    }

    void reject() override {}

    void keyPressEvent(QKeyEvent* event) override {
        if (event && event->key() == Qt::Key_Escape) {
            event->ignore();
            return;
        }
        QDialog::keyPressEvent(event);
    }

    void paintEvent(QPaintEvent* event) override {
        DialogChrome::paint(this, event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (DialogChrome::mousePress(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
        QDialog::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (DialogChrome::mouseMove(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
        QDialog::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (DialogChrome::mouseRelease(event, dragging_)) return;
        QDialog::mouseReleaseEvent(event);
    }

private:
    QLabel* messageLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTimer timer_;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
}

DriverCoordinator::DriverCoordinator(QObject* parent) : QObject(parent) {}

void DriverCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page || !context_.config) return;

    connect(context_.page, &DriverPage::driverModeChanged, this, [this](DriverMode mode) {
        if (!context_.config || !context_.switchDriverMode) return;
        const auto result = context_.switchDriverMode(mode);
        if (result == SwitchResult::Failed) {
            AppMessageDialog::warning(
                context_.parentWidget,
                QString::fromWCharArray(L"\u9a71\u52a8\u5207\u6362\u5931\u8d25"),
                QString::fromWCharArray(L"\u65e0\u6cd5\u5207\u6362\u5230\u6240\u9009\u9a71\u52a8\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u914d\u7f6e\u3002"));
        }
        if (context_.refreshStatus) context_.refreshStatus();
    });

    connect(context_.page, &DriverPage::refreshRequested, this, [this]() {
        if (context_.refreshStatus) context_.refreshStatus();
    });

    connect(context_.page, &DriverPage::uninstallDriverRequested, this, [this](DriverMode mode) {
        if (mode == DriverMode::Usb) {
            return;
        }
        if (!context_.runtimeService) return;
        const QString name = driverName(mode);
        const bool confirmed = ConfigConfirmDialog::confirm(
            context_.parentWidget,
            QString::fromWCharArray(L"\u5378\u8f7d ") + name + QString::fromWCharArray(L" \u9a71\u52a8"),
            QString::fromWCharArray(L"\u5378\u8f7d\u540e ") + name + QString::fromWCharArray(L" \u5c06\u4e0d\u53ef\u7528\uff0c\u5e76\u4f1a\u5728\u5378\u8f7d\u5b8c\u6210\u540e\u91cd\u542f\u7535\u8111\u3002\u5378\u8f7d\u8fc7\u7a0b\u4e2d\u8bf7\u52ff\u5173\u95ed\u7a0b\u5e8f\u3002\u662f\u5426\u7ee7\u7eed\uff1f"),
            QString::fromWCharArray(L"\u5378\u8f7d"));
        if (!confirmed) return;

        if (context_.prepareDriverUninstall) context_.prepareDriverUninstall(mode);
        if (context_.page) context_.page->setBusy(true);
        if (context_.setDialogOpen) context_.setDialogOpen(true);

        auto* progressDialog = new DriverUninstallProgressDialog(mode, context_.parentWidget);
        const QPointer<DriverUninstallProgressDialog> progressDialogPtr(progressDialog);
        const auto result = std::make_shared<DriverUninstallResult>();
        auto* thread = QThread::create([runtimeService = context_.runtimeService, result, mode]() {
            result->ok = mode == DriverMode::Hid
                ? runtimeService->UninstallEmbeddedHidDriver(&result->message)
                : runtimeService->UninstallEmbeddedInfDriver(&result->message);
        });
        thread->setParent(this);

        connect(thread, &QThread::finished, this, [this, progressDialog, thread, result, mode, name]() {
            thread->deleteLater();

            if (context_.finalizeDriverUninstall) context_.finalizeDriverUninstall(mode);
            if (context_.page) context_.page->setBusy(false);
            if (context_.refreshStatus) context_.refreshStatus();

            if (progressDialog) {
                progressDialog->markFinished();
                progressDialog->accept();
                progressDialog->deleteLater();
            }
            if (context_.setDialogOpen) context_.setDialogOpen(false);

            if (result->ok) {
                const bool rebootStarted = context_.rebootComputer && context_.rebootComputer();
                if (!rebootStarted) {
                    AppMessageDialog::warning(
                        context_.parentWidget,
                        QString::fromWCharArray(L"\u91cd\u542f\u5931\u8d25"),
                        QString::fromWCharArray(L"\u65e0\u6cd5\u81ea\u52a8\u53d1\u8d77\u7cfb\u7edf\u91cd\u542f\uff0c\u8bf7\u624b\u52a8\u91cd\u542f\u7535\u8111\u3002"));
                }
            } else {
                AppMessageDialog::warning(
                    context_.parentWidget,
                    name + QString::fromWCharArray(L" \u5378\u8f7d\u9700\u8981\u624b\u52a8\u5904\u7406"),
                    name + QString::fromWCharArray(L" \u9a71\u52a8\u5df2\u8fdb\u5165\u5378\u8f7d\u6d41\u7a0b\uff0c\u4f46 Windows \u4ecd\u663e\u793a\u6709\u8bbe\u5907\u6216\u670d\u52a1\u5904\u4e8e\u5f85\u91ca\u653e\u72b6\u6001\u3002\n\n\u8bf7\u624b\u52a8\u91cd\u542f\u7535\u8111\u540e\u518d\u7ee7\u7eed\u4f7f\u7528\u3002\n\n\u5378\u8f7d\u7ed3\u679c\uff1a\n") +
                        QString::fromStdWString(result->message));
            }
        }, Qt::QueuedConnection);

        thread->start();
        progressDialog->exec();
        if (progressDialogPtr && context_.setDialogOpen) context_.setDialogOpen(false);
    });
}

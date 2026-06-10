#include "coordinators/CloseCoordinator.h"

#include "dialogs/CloseConfirmDialog.h"
#include "input/GlobalInputHook.h"
#include "view/MainWindowView.h"

#include <QCloseEvent>
#include <QDialog>
#include <QMainWindow>

CloseCoordinator::CloseCoordinator(QObject* parent) : QObject(parent) {}

void CloseCoordinator::bind(Context context) {
    context_ = std::move(context);
}

void CloseCoordinator::handleClose(QCloseEvent* event) {
    if (!context_.config || !context_.view || !context_.inputHook) {
        event->accept();
        return;
    }

    if (context_.saveConfig) context_.saveConfig();
    if (context_.config->closeAction == CloseAction::Minimize) {
        if (auto* window = qobject_cast<QMainWindow*>(context_.parent)) {
            context_.view->initializeTray(window);
        }
        if (context_.hideWindow) context_.hideWindow();
        event->ignore();
        return;
    }
    if (context_.config->closeAction == CloseAction::Exit) {
        context_.view->hideTrayIcon();
        event->accept();
        return;
    }

    CloseConfirmDialog dialog(context_.parent);
    context_.inputHook->setDialogOpen(true);
    if (dialog.exec() == QDialog::Rejected) {
        context_.inputHook->setDialogOpen(false);
        event->ignore();
        return;
    }
    context_.inputHook->setDialogOpen(false);

    const auto decision = dialog.decision();
    if (decision.remember) {
        context_.config->closeAction = decision.exit ? CloseAction::Exit : CloseAction::Minimize;
        if (context_.saveConfig) context_.saveConfig();
    }

    if (decision.exit) {
        context_.view->hideTrayIcon();
        event->accept();
    } else {
        if (auto* window = qobject_cast<QMainWindow*>(context_.parent)) {
            context_.view->initializeTray(window);
        }
        if (context_.hideWindow) context_.hideWindow();
        event->ignore();
    }
}

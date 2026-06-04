#include "coordinators/ProfileCoordinator.h"

#include "controllers/MacroController.h"
#include "controllers/ProfileController.h"
#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "ui/pages/KeyPage.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

ProfileCoordinator::ProfileCoordinator(QObject* parent) : QObject(parent) {}

void ProfileCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page) return;

    connect(context_.page, &KeyPage::profileSwitched, this, &ProfileCoordinator::onProfileSwitched);
    connect(context_.page, &KeyPage::profileAddRequested, this, &ProfileCoordinator::onProfileAdd);
    connect(context_.page, &KeyPage::profileRenameRequested, this, &ProfileCoordinator::onProfileRename);
    connect(context_.page, &KeyPage::profileDeleteRequested, this, &ProfileCoordinator::onProfileDelete);
}

void ProfileCoordinator::switchToProfile(std::uint64_t profileId) {
    if (!context_.config || !context_.profileController || !context_.macroController) return;
    if (profileId == context_.config->activeProfileId) return;

    stopEnginesForProfileChange();

    auto result = context_.profileController->switchProfile(profileId);
    if (!result.ok) {
        QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u914d\u7f6e\u5207\u6362\u5931\u8d25"), result.message);
        return;
    }
    resetActiveMacro();

    if (context_.applyBindings) context_.applyBindings();
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void ProfileCoordinator::onProfileSwitched(std::uint64_t profileId) {
    switchToProfile(profileId);
}

void ProfileCoordinator::onProfileAdd() {
    if (!context_.inputHook || !context_.profileController || !context_.macroController) return;

    context_.inputHook->setDialogOpen(true);
    bool ok = false;
    QString name = QInputDialog::getText(context_.parent,
        QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u914d\u7f6e\u540d\u79f0:"),
        QLineEdit::Normal, QString(), &ok);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    stopEnginesForProfileChange();

    std::uint64_t createdProfileId = 0;
    auto result = context_.profileController->addProfile(name, createdProfileId);
    if (!result.ok) {
        QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e\u5931\u8d25"), result.message);
        return;
    }

    resetActiveMacro();
    if (context_.applyBindings) context_.applyBindings();
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void ProfileCoordinator::onProfileRename() {
    if (!context_.inputHook || !context_.profileController) return;
    auto* profile = context_.profileController->activeProfile();
    if (!profile) return;

    context_.inputHook->setDialogOpen(true);
    bool ok = false;
    QString name = QInputDialog::getText(context_.parent,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u914d\u7f6e"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QLineEdit::Normal, QString::fromStdWString(profile->name), &ok);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    auto result = context_.profileController->renameActiveProfile(name);
    if (!result.ok) {
        QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u91cd\u547d\u540d\u5931\u8d25"), result.message);
        return;
    }
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshProfileCombo) context_.refreshProfileCombo();
}

void ProfileCoordinator::onProfileDelete() {
    if (!context_.config || !context_.inputHook || !context_.profileController || !context_.macroController) return;
    if (context_.config->profiles.size() <= 1) {
        QMessageBox::information(context_.parent,
            QString::fromWCharArray(L"\u65e0\u6cd5\u5220\u9664"),
            QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u914d\u7f6e\u65b9\u6848\u3002"));
        return;
    }

    auto* profile = context_.profileController->activeProfile();
    if (!profile) return;

    context_.inputHook->setDialogOpen(true);
    auto answer = QMessageBox::question(context_.parent,
        QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u914d\u7f6e\u201c") +
            QString::fromStdWString(profile->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    context_.inputHook->setDialogOpen(false);
    if (answer != QMessageBox::Yes) return;

    std::uint64_t newActiveProfileId = 0;
    auto result = context_.profileController->deleteActiveProfile(newActiveProfileId);
    if (!result.ok) {
        QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e\u5931\u8d25"), result.message);
        return;
    }
    resetActiveMacro();

    if (context_.applyBindings) context_.applyBindings();
    if (context_.applyMacros) context_.applyMacros();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void ProfileCoordinator::resetActiveMacro() {
    if (context_.macroController) context_.macroController->resetActiveMacro();
}

void ProfileCoordinator::stopEnginesForProfileChange() {
    if (context_.repeaterEngine && context_.repeaterEngine->IsToolRunning() && context_.setRepeatRunning) {
        context_.setRepeatRunning(false);
    }
    if (context_.macroEngine) context_.macroEngine->StopAll();
}

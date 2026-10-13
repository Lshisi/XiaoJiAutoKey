#include "coordinators/ProfileCoordinator.h"

#include "config/ActiveConfigValidator.h"
#include "controllers/MacroController.h"
#include "controllers/ProfileController.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/ConfigNameDialog.h"
#include "input/GlobalInputHook.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "ui/pages/KeyPage.h"

ProfileCoordinator::ProfileCoordinator(QObject* parent) : QObject(parent) {}

void ProfileCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page) return;

    connect(context_.page, &KeyPage::profileSwitched, this, &ProfileCoordinator::onProfileSwitched);
    connect(context_.page, &KeyPage::profileAddRequested, this, &ProfileCoordinator::onProfileAdd);
    connect(context_.page, &KeyPage::profileRenameRequested, this, &ProfileCoordinator::onProfileRename);
    connect(context_.page, &KeyPage::profileDeleteRequested, this, &ProfileCoordinator::onProfileDelete);
    connect(context_.page, &KeyPage::profileEnabledToggled, this, &ProfileCoordinator::onProfileEnabledToggled);
}

void ProfileCoordinator::switchToProfile(std::uint64_t profileId) {
    if (!context_.config || !context_.profileController || !context_.macroController) return;
    if (profileId == context_.config->activeProfileId) return;

    const auto* targetProfile = context_.profileController->profileById(profileId);
    auto validation = ActiveConfigValidator::validateActiveHotkeys(
        targetProfile,
        context_.macroController->activeMacro());
    if (!validation.ok) {
        if (context_.refreshProfileCombo) context_.refreshProfileCombo();
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u6309\u952e\u51b2\u7a81"), validation.message);
        return;
    }

    stopEnginesForProfileChange();

    auto result = context_.profileController->switchProfile(profileId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u914d\u7f6e\u5207\u6362\u5931\u8d25"), result.message);
        return;
    }
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void ProfileCoordinator::onProfileSwitched(std::uint64_t profileId) {
    switchToProfile(profileId);
}

void ProfileCoordinator::onProfileAdd() {
    if (!context_.inputHook || !context_.profileController || !context_.macroController) return;

    context_.inputHook->setDialogOpen(true);
    QString name;
    const bool ok = ConfigNameDialog::getName(context_.parent,
        QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e"),
        QString::fromWCharArray(L"\u8bf7\u8f93\u5165\u914d\u7f6e\u540d\u79f0:"),
        QString(),
        name);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    stopEnginesForProfileChange();

    std::uint64_t createdProfileId = 0;
    auto result = context_.profileController->addProfile(name, createdProfileId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e\u5931\u8d25"), result.message);
        return;
    }

    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void ProfileCoordinator::onProfileRename() {
    if (!context_.inputHook || !context_.profileController) return;
    auto* profile = context_.profileController->activeProfile();
    if (!profile) return;

    context_.inputHook->setDialogOpen(true);
    QString name;
    const bool ok = ConfigNameDialog::getName(context_.parent,
        QString::fromWCharArray(L"\u91cd\u547d\u540d\u914d\u7f6e"),
        QString::fromWCharArray(L"\u65b0\u540d\u79f0:"),
        QString::fromStdWString(profile->name),
        name);
    context_.inputHook->setDialogOpen(false);
    if (!ok || name.trimmed().isEmpty()) return;

    auto result = context_.profileController->renameActiveProfile(name);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u91cd\u547d\u540d\u5931\u8d25"), result.message);
        return;
    }
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshProfileCombo) context_.refreshProfileCombo();
}

void ProfileCoordinator::onProfileDelete() {
    if (!context_.config || !context_.inputHook || !context_.profileController || !context_.macroController) return;
    if (context_.config->profiles.size() <= 1) {
        AppMessageDialog::information(context_.parent,
            QString::fromWCharArray(L"\u65e0\u6cd5\u5220\u9664"),
            QString::fromWCharArray(L"\u81f3\u5c11\u9700\u8981\u4fdd\u7559\u4e00\u4e2a\u914d\u7f6e\u65b9\u6848\u3002"));
        return;
    }

    auto* profile = context_.profileController->activeProfile();
    if (!profile) return;
    const std::uint64_t deletedProfileId = profile->id;

    context_.inputHook->setDialogOpen(true);
    const bool confirmed = ConfigConfirmDialog::confirm(context_.parent,
        QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e"),
        QString::fromWCharArray(L"\u786e\u5b9a\u8981\u5220\u9664\u914d\u7f6e\u201c") +
            QString::fromStdWString(profile->name) +
            QString::fromWCharArray(L"\u201d\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"),
        QString::fromWCharArray(L"\u5220\u9664"));
    context_.inputHook->setDialogOpen(false);
    if (!confirmed) return;

    std::uint64_t newActiveProfileId = 0;
    auto result = context_.profileController->deleteActiveProfile(newActiveProfileId);
    if (!result.ok) {
        AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e\u5931\u8d25"), result.message);
        return;
    }

    if (context_.repeaterEngine) {
        context_.repeaterEngine->ForgetProfileRuntimeState(deletedProfileId);
    }
    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    if (context_.refreshAll) context_.refreshAll();
}

void ProfileCoordinator::onProfileEnabledToggled(bool enabled) {
    if (!context_.profileController) return;
    auto* profile = context_.profileController->activeProfile();
    if (!profile || profile->enabled == enabled) return;

    auto result = context_.profileController->setActiveProfileEnabled(enabled);
    if (!result.ok) {
        context_.page->setProfileEnabled(profile->enabled);
        if (context_.refreshProfileCombo) context_.refreshProfileCombo();
        return;
    }

    if (enabled) {
        auto validation = ActiveConfigValidator::validateActiveHotkeys(
            profile,
            context_.macroController ? context_.macroController->activeMacro() : nullptr);
        if (!validation.ok) {
            context_.profileController->setActiveProfileEnabled(false);
            context_.page->setProfileEnabled(false);
            if (context_.refreshProfileCombo) context_.refreshProfileCombo();
            AppMessageDialog::warning(context_.parent,
                QString::fromWCharArray(L"\u65e0\u6cd5\u542f\u7528\u914d\u7f6e"), validation.message);
            return;
        }
    } else if (context_.repeaterEngine && context_.repeaterEngine->IsToolRunning() && context_.setRepeatRunning) {
        context_.setRepeatRunning(false);
    }

    if (context_.applyBindings) context_.applyBindings();
    if (context_.saveConfig) context_.saveConfig();
    context_.page->setProfileEnabled(enabled);
    if (context_.refreshProfileCombo) context_.refreshProfileCombo();
}

void ProfileCoordinator::stopEnginesForProfileChange() {
    if (context_.repeaterEngine && context_.repeaterEngine->IsToolRunning() && context_.setRepeatRunning) {
        context_.setRepeatRunning(false);
    }
    if (context_.macroEngine) {
        context_.macroEngine->StopAll();
    }
}

#pragma once

#include "AppTypes.h"

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <vector>

class DraggableTable;
class FluentButton;
class FluentLineEdit;
class FluentSwitch;
class FluentComboBox;

class MacroPage : public QWidget {
    Q_OBJECT
public:
    explicit MacroPage(QWidget* parent = nullptr);

    void setMacros(const std::vector<MacroConfig>& macros);
    void setActiveMacro(std::uint64_t macroId);
    void setSteps(const std::vector<MacroStep>& steps);
    void setRunning(bool running);
    void setHotkeyText(const QString& text);
    void setBeepEnabled(bool enabled);
    void setStatusOverlayEnabled(bool enabled);
    void setPauseKeys(const std::vector<KeyDescriptor>& keys);
    void setMode(MacroTriggerMode mode);
    void setLoopCount(int count);
    void updateThemeColors();

    // Profile (macro) management
    void setMacroProfiles(const std::vector<std::pair<std::uint64_t, QString>>& macros, std::uint64_t activeId);

signals:
    // Step management
    void addStepRequested();
    void editStepRequested(int stepIndex);
    void deleteStepRequested(int stepIndex);
    void stepMoveRequested(int fromIndex, int toIndex);

    // Macro profile management
    void macroSwitched(quint64 macroId);
    void macroAddRequested();
    void macroRenameRequested();
    void macroDeleteRequested();

    // Control
    void startStopClicked();
    void hotkeyEditClicked();
    void hotkeyClearClicked();
    void beepToggled(bool enabled);
    void statusOverlayToggled(bool enabled);
    void pauseKeyAddClicked();
    void pauseKeyDeleteClicked(int index);
    void soundSettingsClicked();
    void modeChanged(int modeIndex);
    void loopCountChanged(int count);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshTable();
    void updateStartBtnText();
    void refreshPauseKeyList();
    void applyPageTheme();

    DraggableTable* table_;
    FluentButton* addBtn_;
    FluentButton* startBtn_;
    FluentLineEdit* hotkeyEdit_;
    FluentButton* hotkeyClearBtn_ = nullptr;
    QCheckBox* beepCheck_;
    QCheckBox* statusOverlayCheck_ = nullptr;
    FluentButton* soundSettingsBtn_ = nullptr;
    FluentComboBox* modeCombo_;
    FluentLineEdit* loopEdit_;
    QLabel* loopLabel_;
    QLabel* statusLabel_ = nullptr;
    std::vector<KeyDescriptor> pauseKeys_;
    QWidget* pauseKeyListWidget_ = nullptr;
    QHBoxLayout* pauseKeyListLayout_ = nullptr;

    // Macro profile UI
    FluentComboBox* macroCombo_;
    FluentButton* macroAddBtn_;
    FluentButton* macroRenameBtn_;
    FluentButton* macroDeleteBtn_;

    std::vector<MacroStep> steps_;
    bool running_ = false;
    QString hotkeyName_;
};

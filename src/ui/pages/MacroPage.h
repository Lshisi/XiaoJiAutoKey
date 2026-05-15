#pragma once

#include "../../include/AppTypes.h"

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <vector>

class FluentTable;
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
    void stepEnableToggled(int stepIndex, bool enabled);

    // Macro profile management
    void macroSwitched(quint64 macroId);
    void macroAddRequested();
    void macroRenameRequested();
    void macroDeleteRequested();

    // Control
    void startStopClicked();
    void hotkeyEditClicked();
    void modeChanged(int modeIndex);
    void loopCountChanged(int count);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshTable();

    FluentTable* table_;
    FluentButton* addBtn_;
    FluentButton* startBtn_;
    FluentButton* stopBtn_;
    FluentLineEdit* hotkeyEdit_;
    FluentComboBox* modeCombo_;
    FluentLineEdit* loopEdit_;
    QLabel* loopLabel_;

    // Macro profile UI
    FluentComboBox* macroCombo_;
    FluentButton* macroAddBtn_;
    FluentButton* macroRenameBtn_;
    FluentButton* macroDeleteBtn_;

    std::vector<MacroStep> steps_;
    bool running_ = false;
    QString hotkeyName_;
};

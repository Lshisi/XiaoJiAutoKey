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

class KeyPage : public QWidget {
    Q_OBJECT
public:
    explicit KeyPage(QWidget* parent = nullptr);

    void setBindings(const std::vector<KeyBinding>& bindings);
    void setRunning(bool running);
    void setHotkeyText(const QString& text);
    void setBeepEnabled(bool enabled);
    void setMixedMode(bool enabled);
    FluentLineEdit* hotkeyEditWidget() const { return hotkeyEdit_; }

    // Profile management
    void setProfiles(const std::vector<std::pair<std::uint64_t, QString>>& profiles, std::uint64_t activeId);

signals:
    void addRequested();
    void editRequested(std::uint64_t id);
    void deleteRequested(std::uint64_t id);
    void enableToggled(std::uint64_t id, bool enabled);
    void startStopClicked();
    void hotkeyEditClicked();
    void beepToggled(bool enabled);
    void mixedModeToggled(bool enabled);

    // Profile signals
    void profileSwitched(quint64 profileId);
    void profileAddRequested();
    void profileRenameRequested();
    void profileDeleteRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshTable();
    void updateStartBtnText();

    FluentTable* table_;
    FluentButton* addBtn_;
    FluentButton* startBtn_;
    FluentButton* stopBtn_;
    FluentLineEdit* hotkeyEdit_;
    QCheckBox* beepCheck_;
    QCheckBox* mixedModeCheck_;
    QLabel* statusLabel_;
    std::vector<KeyBinding> bindings_;
    QString hotkeyName_;
    bool running_ = false;

    // Profile UI
    FluentComboBox* profileCombo_;
    FluentButton* profileAddBtn_;
    FluentButton* profileRenameBtn_;
    FluentButton* profileDeleteBtn_;
};

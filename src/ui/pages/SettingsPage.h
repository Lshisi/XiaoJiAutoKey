#pragma once

#include "../../include/AppTypes.h"

#include <QWidget>
#include <QLabel>

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void setConfig(const AppConfig& config);
    void setHotkeyText(const QString& text);
    void setConfigPath(const QString& path);

signals:
    void hotkeyEditClicked();
    void beepToggled(bool enabled);
    void closeActionChanged(CloseAction action);
    void openConfigFolder();
    void changeConfigPath();
    void resetConfigPath();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLabel* configPathLabel_;
};

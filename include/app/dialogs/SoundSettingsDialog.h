#pragma once

#include <QDialog>
#include <QPoint>
#include <string>

class SoundService;
class FluentLineEdit;
class QLabel;

struct SoundSettingsResult {
    std::wstring soundOn;
    std::wstring soundOff;
    int volume = 100;
};

class SoundSettingsDialog : public QDialog {
    Q_OBJECT

public:
    SoundSettingsDialog(
        SoundService& soundService,
        bool macroMode,
        SoundSettingsResult initial,
        QWidget* parent = nullptr);

    [[nodiscard]] SoundSettingsResult settings() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void addSoundRow(const QString& labelText, QString* pathValue, bool running);

    SoundService& soundService_;
    bool macroMode_ = false;
    QString soundOnPath_;
    QString soundOffPath_;
    int soundVolume_ = 100;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};

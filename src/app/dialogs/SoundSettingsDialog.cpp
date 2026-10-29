#include "dialogs/SoundSettingsDialog.h"
#include "dialogs/AppMessageDialog.h"
#include "services/SoundService.h"
#include <QFileDialog>
#include <algorithm>
#include <filesystem>
SoundSettingsDialog::SoundSettingsDialog(SoundService& service, bool macroMode, SoundSettingsResult initial, QWidget* parent)
    : QmlDialog(parent), soundService_(service), macroMode_(macroMode) {
    setWindowTitle(macroMode ? tr("循环音效设置") : tr("按键音效设置"));
    present("DialogSound.qml", {{"soundOn", QString::fromStdWString(initial.soundOn)},
        {"soundOff", QString::fromStdWString(initial.soundOff)}, {"volume", std::clamp(initial.volume, 0, 100)},
        {"actions", QVariantList{button("accept", tr("保存")), button("cancel", tr("取消"), false)}}}, QSize(430, 260), true);
}
SoundSettingsResult SoundSettingsDialog::settings() const {
    return {value("soundOn").toString().toStdWString(), value("soundOff").toString().toStdWString(), value("volume").toInt()};
}
void SoundSettingsDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "volume") setValue(field, std::clamp(argument.toInt(), 0, 100));
}
void SoundSettingsDialog::handleAction(const QString& action, const QVariant& argument) {
    const bool running = argument.toBool();
    const QString field = running ? QStringLiteral("soundOn") : QStringLiteral("soundOff");
    if (action == "chooseSound") {
        const QString path = QFileDialog::getOpenFileName(this, running ? tr("选择开启音效") : tr("选择关闭音效"),
            {}, tr("音频文件 (*.mp3 *.wav *.wma *.mid *.midi)"));
        if (path.isEmpty()) return;
        if (!soundService_.isValidAudioFile(path)) {
            AppMessageDialog::warning(this, tr("格式不支持"), tr("请选择 mp3、wav、wma、mid 格式的音频文件。"));
            return;
        }
        setValue(field, path);
    } else if (action == "resetSound") setValue(field, QString());
    else if (action == "previewSound") {
        std::filesystem::path path = value(field).toString().toStdWString();
        if (path.empty() || !std::filesystem::exists(path))
            path = soundService_.resolveToggleSoundPath(running, macroMode_, {}, {});
        soundService_.playFile(path, value("volume").toInt(), L"autokey_preview_sound");
    } else if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}

#include "dialogs/SoundSettingsDialog.h"

#include "FactionTheme.h"
#include "services/SoundService.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/styles/DesignTokens.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <filesystem>

SoundSettingsDialog::SoundSettingsDialog(
    SoundService& soundService,
    bool macroMode,
    SoundSettingsResult initial,
    QWidget* parent)
    : QDialog(parent),
      soundService_(soundService),
      macroMode_(macroMode),
      soundOnPath_(QString::fromStdWString(initial.soundOn)),
      soundOffPath_(QString::fromStdWString(initial.soundOff)),
      soundVolume_(std::clamp(initial.volume, 0, 100)) {
    setWindowTitle(macroMode_
        ? QString::fromWCharArray(L"\u5b8f\u97f3\u6548\u8bbe\u7f6e")
        : QString::fromWCharArray(L"\u6309\u952e\u97f3\u6548\u8bbe\u7f6e"));
    setFixedSize(430, 260);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    const QString accentColor = FactionTheme::instance().primaryColor().name();
    setStyleSheet(QString(
        "QDialog { background: #FFFFFF; }"
        "QLabel { color: #202020; }"
        "QSlider::groove:horizontal { height: 4px; background: rgba(0,0,0,0.10); border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: %1; }"
        "QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }"
    ).arg(accentColor));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto* titleRow = new QHBoxLayout();
    auto* titleIcon = new QLabel(QString(QChar(0xE189)), this);
    titleIcon->setFont(QFont("Segoe MDL2 Assets", 18));
    titleIcon->setFixedWidth(28);
    titleRow->addWidget(titleIcon);
    auto* title = new QLabel(windowTitle(), this);
    title->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(title);
    titleRow->addStretch();
    auto* closeBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE711)), this);
    closeBtn->setFont(QFont("Segoe MDL2 Assets", 10));
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip(QString::fromWCharArray(L"\u5173\u95ed"));
    titleRow->addWidget(closeBtn);
    layout->addLayout(titleRow);

    addSoundRow(QString::fromWCharArray(L"\u6309\u952e\u5f00\u542f"), &soundOnPath_, true);
    addSoundRow(QString::fromWCharArray(L"\u6309\u952e\u5173\u95ed"), &soundOffPath_, false);

    auto* volumeRow = new QHBoxLayout();
    volumeRow->setSpacing(8);
    auto* volumeLabel = new QLabel(QString::fromWCharArray(L"\u97f3\u91cf"), this);
    volumeLabel->setFont(Fluent::Typography::body());
    volumeLabel->setFixedWidth(76);
    volumeRow->addWidget(volumeLabel);
    auto* volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(soundVolume_);
    volumeRow->addWidget(volumeSlider, 1);
    auto* volumeValueLabel = new QLabel(QString::number(soundVolume_) + "%", this);
    volumeValueLabel->setFont(Fluent::Typography::caption());
    volumeValueLabel->setFixedWidth(42);
    volumeValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    volumeRow->addWidget(volumeValueLabel);
    layout->addLayout(volumeRow);

    auto* desc = new QLabel(QString::fromWCharArray(L"\u652f\u6301 mp3\u3001wav\u3001wma\u3001mid \u683c\u5f0f\uff1b\u6e05\u7a7a\u8def\u5f84\u540e\u4f7f\u7528\u5185\u7f6e\u9ed8\u8ba4\u97f3\u6548\u3002"), this);
    desc->setFont(Fluent::Typography::caption());
    desc->setWordWrap(true);
    QPalette descPal = desc->palette();
    descPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    desc->setPalette(descPal);
    layout->addWidget(desc);

    layout->addStretch();

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), this);
    cancelBtn->setFixedWidth(82);
    auto* saveBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u4fdd\u5b58"), this);
    saveBtn->setFixedWidth(94);
    buttonRow->addWidget(cancelBtn);
    buttonRow->addSpacing(8);
    buttonRow->addWidget(saveBtn);
    layout->addLayout(buttonRow);

    connect(volumeSlider, &QSlider::valueChanged, this, [this, volumeValueLabel](int value) {
        soundVolume_ = value;
        volumeValueLabel->setText(QString::number(value) + "%");
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
}

SoundSettingsResult SoundSettingsDialog::settings() const {
    return {soundOnPath_.toStdWString(), soundOffPath_.toStdWString(), soundVolume_};
}

void SoundSettingsDialog::addSoundRow(const QString& labelText, QString* pathValue, bool running) {
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    auto* row = new QHBoxLayout();
    row->setSpacing(8);

    auto* label = new QLabel(labelText, this);
    label->setFont(Fluent::Typography::body());
    label->setFixedWidth(76);
    row->addWidget(label);

    auto* pathEdit = new FluentLineEdit(this);
    pathEdit->setReadOnly(true);
    pathEdit->setPlaceholderText(QString::fromWCharArray(L"\u9ed8\u8ba4\u5185\u7f6e\u97f3\u6548"));
    pathEdit->setText(*pathValue);
    pathEdit->setCursor(Qt::PointingHandCursor);
    pathEdit->setToolTip(QString::fromWCharArray(L"\u70b9\u51fb\u9009\u62e9\u97f3\u9891"));
    row->addWidget(pathEdit, 1);

    auto* previewBtn = new FluentButton(FluentButton::Primary, QString(QChar(0xE768)), this);
    previewBtn->setFont(QFont("Segoe MDL2 Assets", 11));
    previewBtn->setFixedSize(32, 30);
    previewBtn->setToolTip(QString::fromWCharArray(L"\u8bd5\u542c"));
    row->addWidget(previewBtn);

    auto* resetBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), this);
    resetBtn->setFont(QFont("Segoe MDL2 Assets", 12));
    resetBtn->setFixedSize(30, 30);
    resetBtn->setToolTip(QString::fromWCharArray(L"\u6062\u590d\u9ed8\u8ba4"));
    row->addWidget(resetBtn);

    connect(pathEdit, &FluentLineEdit::clicked, this, [this, pathValue, pathEdit, running]() {
        const QString caption = running
            ? (macroMode_ ? QString::fromWCharArray(L"\u9009\u62e9\u5b8f\u5f00\u542f\u97f3\u6548") : QString::fromWCharArray(L"\u9009\u62e9\u6309\u952e\u5f00\u542f\u97f3\u6548"))
            : (macroMode_ ? QString::fromWCharArray(L"\u9009\u62e9\u5b8f\u5173\u95ed\u97f3\u6548") : QString::fromWCharArray(L"\u9009\u62e9\u6309\u952e\u5173\u95ed\u97f3\u6548"));
        QString filePath = QFileDialog::getOpenFileName(this,
            caption,
            QString(),
            QString::fromWCharArray(L"\u97f3\u9891\u6587\u4ef6 (*.mp3 *.wav *.wma *.mid *.midi)"));
        if (filePath.isEmpty()) return;
        if (!soundService_.isValidAudioFile(filePath)) {
            QMessageBox::warning(this,
                QString::fromWCharArray(L"\u683c\u5f0f\u4e0d\u652f\u6301"),
                QString::fromWCharArray(L"\u8bf7\u9009\u62e9 mp3\u3001wav\u3001wma\u3001mid \u683c\u5f0f\u7684\u97f3\u9891\u6587\u4ef6\u3002"));
            return;
        }
        *pathValue = filePath;
        pathEdit->setText(filePath);
    });

    connect(previewBtn, &QPushButton::clicked, this, [this, pathValue, running]() {
        std::filesystem::path previewPath;
        if (!pathValue->isEmpty()) previewPath = pathValue->toStdWString();
        if (previewPath.empty() || !std::filesystem::exists(previewPath)) {
            previewPath = soundService_.resolveToggleSoundPath(running, macroMode_, {}, {});
        }
        soundService_.playFile(previewPath, soundVolume_, L"autokey_preview_sound");
    });

    connect(resetBtn, &QPushButton::clicked, this, [pathValue, pathEdit]() {
        pathValue->clear();
        pathEdit->clear();
    });

    layout->addLayout(row);
}

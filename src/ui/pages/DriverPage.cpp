#include "DriverPage.h"

#include "../components/FluentButton.h"
#include "../components/FluentCard.h"
#include "../components/FluentComboBox.h"
#include "../components/FluentSwitch.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(Fluent::Typography::cardTitle());
    return label;
}

QWidget* makeDivider(QWidget* parent) {
    auto* divider = new QWidget(parent);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    return divider;
}

QPalette mutedPalette(QWidget* widget) {
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    return pal;
}

QString compactDriverDetails(const QString& details) {
    QString dd32;
    QString dd64;
    QString infKeyboardService;
    QString infMouseService;
    QString infKeyboardDevice;
    QString infMouseDevice;
    QString hidDll;
    QString hidService;
    QString hidSys;
    QString hidRootDevice;
    QString hidChildDevice;
    QString ch552Device;
    QString usbConnection;
    QString usbFirmware;
    QString nativePackage;
    QString nativeService;
    QString nativeSys;
    QString nativeDevice;
    QString nativeConnection;
    QString nativeVersion;
    const QStringList lines = details.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("DD32.dll:"))) dd32 = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("DD64.dll:"))) dd64 = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u952e\u76d8\u670d\u52a1:"))) infKeyboardService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u9f20\u6807\u670d\u52a1:"))) infMouseService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u952e\u76d8\u8bbe\u5907:"))) infKeyboardDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u9f20\u6807\u8bbe\u5907:"))) infMouseDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("HID DLL:"))) hidDll = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"HID \u670d\u52a1:"))) hidService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("HID SYS:"))) hidSys = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"HID Root \u8bbe\u5907:"))) hidRootDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"HID Child \u8bbe\u5907:"))) hidChildDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"USB \u8bbe\u5907:"))) ch552Device = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"USB \u901a\u4fe1:"))) usbConnection = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"USB \u56fa\u4ef6:"))) usbFirmware = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"\u5c0f\u53fd HID \u53d1\u5e03\u5305:"))) nativePackage = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"\u5c0f\u53fd HID \u670d\u52a1:"))) nativeService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"\u5c0f\u53fd HID SYS:"))) nativeSys = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"\u5c0f\u53fd HID \u8bbe\u5907:"))) nativeDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"\u5c0f\u53fd HID \u901a\u4fe1:"))) nativeConnection = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"\u5c0f\u53fd HID \u7248\u672c:"))) nativeVersion = trimmed.section(QLatin1Char(':'), 1).trimmed();
    }

    QStringList sections;
    const auto pairOk = [](const QString& a, const QString& b, const QString& okText) {
        return !a.isEmpty() && !b.isEmpty() && a == okText && b == okText;
    };
    const QString found = QString::fromWCharArray(L"\u5df2\u627e\u5230");
    const QString installed = QString::fromWCharArray(L"\u5df2\u5b89\u88c5");
    const QString notInstalled = QString::fromWCharArray(L"\u672a\u5b89\u88c5");
    const QString pendingRestart = QString::fromWCharArray(L"\u7b49\u5f85\u91cd\u542f");
    const QString ok = QStringLiteral("OK");

    QStringList dd;
    if (!dd32.isEmpty() || !dd64.isEmpty()) {
        dd.push_back(QString::fromWCharArray(L"\u8fd0\u884c\u6587\u4ef6: ") +
            (pairOk(dd32, dd64, found) ? QString::fromWCharArray(L"\u5df2\u5c31\u7eea") : QString::fromWCharArray(L"\u4e0d\u5b8c\u6574")));
    }
    if (!infKeyboardService.isEmpty() || !infMouseService.isEmpty()) {
        dd.push_back(QString::fromWCharArray(L"INF \u670d\u52a1: ") +
            (pairOk(infKeyboardService, infMouseService, installed) ? QString::fromWCharArray(L"\u5df2\u5b89\u88c5") : QString::fromWCharArray(L"\u4e0d\u5b8c\u6574")));
    }
    if (!infKeyboardDevice.isEmpty() || !infMouseDevice.isEmpty()) {
        dd.push_back(QString::fromWCharArray(L"INF \u8bbe\u5907: ") +
            (pairOk(infKeyboardDevice, infMouseDevice, ok) ? QStringLiteral("OK") : QString::fromWCharArray(L"\u5f02\u5e38")));
    }

    QStringList hid;
    if (!hidDll.isEmpty()) {
        hid.push_back(QStringLiteral("DLL: ") + hidDll);
    }
    if (!hidService.isEmpty() || !hidSys.isEmpty()) {
        const bool hidServiceSysInstalled = hidService == installed && hidSys == installed;
        const bool hidServiceSysMissing = hidService == notInstalled && hidSys == notInstalled;
        const bool hidServicePendingRestart = hidService == pendingRestart && hidSys == notInstalled;
        hid.push_back(QString::fromWCharArray(L"\u670d\u52a1/SYS: ") +
            (hidServiceSysInstalled ? QString::fromWCharArray(L"\u5df2\u5b89\u88c5") :
                (hidServicePendingRestart ? QString::fromWCharArray(L"\u7b49\u5f85\u91cd\u542f") :
                    (hidServiceSysMissing ? QString::fromWCharArray(L"\u672a\u5b89\u88c5") : QString::fromWCharArray(L"\u4e0d\u5b8c\u6574")))));
    }
    if (!hidRootDevice.isEmpty() || !hidChildDevice.isEmpty()) {
        const bool hidDevicesOk = hidRootDevice == ok && hidChildDevice == ok;
        const bool hidDevicesMissing = hidRootDevice == notInstalled && hidChildDevice == notInstalled;
        const bool hidDevicesPendingRestart = hidRootDevice == pendingRestart && hidChildDevice == pendingRestart;
        hid.push_back(QString::fromWCharArray(L"\u8bbe\u5907: ") +
            (hidDevicesOk ? QStringLiteral("OK") :
                (hidDevicesPendingRestart ? QString::fromWCharArray(L"\u7b49\u5f85\u91cd\u542f") :
                    (hidDevicesMissing ? QString::fromWCharArray(L"\u672a\u5b89\u88c5") : QString::fromWCharArray(L"\u5f02\u5e38")))));
    }

    if (!dd.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QStringLiteral("DD"));
        sections.append(dd);
    }
    if (!hid.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QStringLiteral("HID"));
        sections.append(hid);
    }
    QStringList native;
    if (!nativePackage.isEmpty()) native.push_back(QString::fromWCharArray(L"\u53d1\u5e03\u5305: ") + nativePackage);
    if (!nativeService.isEmpty() || !nativeSys.isEmpty()) {
        native.push_back(QString::fromWCharArray(L"\u670d\u52a1/SYS: ") + nativeService + QStringLiteral(" / ") + nativeSys);
    }
    if (!nativeDevice.isEmpty()) native.push_back(QString::fromWCharArray(L"\u8bbe\u5907: ") + nativeDevice);
    if (!nativeConnection.isEmpty()) native.push_back(QString::fromWCharArray(L"\u901a\u4fe1: ") + nativeConnection);
    if (!nativeVersion.isEmpty()) native.push_back(QString::fromWCharArray(L"\u7248\u672c: ") + nativeVersion);
    if (!native.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QString::fromWCharArray(L"\u5c0f\u53fd HID"));
        sections.append(native);
    }
    QStringList usb;
    if (!ch552Device.isEmpty()) usb.push_back(QString::fromWCharArray(L"\u8bbe\u5907: ") + ch552Device);
    if (!usbConnection.isEmpty()) usb.push_back(QString::fromWCharArray(L"\u901a\u4fe1: ") + usbConnection);
    if (!usbFirmware.isEmpty()) usb.push_back(QString::fromWCharArray(L"\u56fa\u4ef6: ") + usbFirmware);
    if (!usb.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QStringLiteral("USB"));
        sections.append(usb);
    }
    return sections.isEmpty()
        ? QString::fromWCharArray(L"\u6682\u65e0\u8be6\u7ec6\u72b6\u6001")
        : sections.join(QLatin1Char('\n'));
}

}

DriverPage::DriverPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("driverPageRoot"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setAutoFillBackground(false);
    if (auto* viewport = scrollArea->viewport()) {
        viewport->setAutoFillBackground(false);
        viewport->setAttribute(Qt::WA_StyledBackground, true);
        viewport->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.12)"),
                Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.2)")));

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");
    auto* outerLayout = new QVBoxLayout(scrollContent);
    outerLayout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap,
        Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap);
    outerLayout->setSpacing(0);

    auto* panel = new FluentCard(scrollContent);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (auto* panelRootLayout = qobject_cast<QVBoxLayout*>(panel->layout())) {
        panelRootLayout->setContentsMargins(
            Fluent::Spacing::cardPadding,
            Fluent::Spacing::cardPadding,
            Fluent::Spacing::cardPadding,
            Fluent::Spacing::cardPadding);
        panelRootLayout->setAlignment(Qt::AlignTop);
    }
    auto* panelLayout = panel->contentLayout();
    panelLayout->setSpacing(8);

    auto* pageTitle = new QLabel(QString::fromWCharArray(L"\u9a71\u52a8\u7ba1\u7406"), panel);
    pageTitle->setFont(Fluent::Typography::pageTitle());
    panelLayout->addWidget(pageTitle);

    auto* pageDesc = new QLabel(QString::fromWCharArray(L"\u7ba1\u7406 DD\u3001HID\u3001\u5c0f\u53fd HID \u548c USB \u786c\u4ef6\u8f93\u5165\uff0c\u67e5\u770b\u72b6\u6001\u5e76\u5207\u6362\u8fd0\u884c\u6a21\u5f0f\u3002"), panel);
    pageDesc->setFont(Fluent::Typography::body());
    pageDesc->setWordWrap(true);
    pageDesc->setPalette(mutedPalette(pageDesc));
    panelLayout->addWidget(pageDesc);
    panelLayout->addWidget(makeDivider(panel));

    panelLayout->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u8fd0\u884c\u6a21\u5f0f"), panel));
    auto* modeRow = new QHBoxLayout();
    auto* modeLabel = new QLabel(QString::fromWCharArray(L"\u4f7f\u7528\u9a71\u52a8"), panel);
    modeLabel->setFont(Fluent::Typography::body());
    modeRow->addWidget(modeLabel);
    modeRow->addStretch();
    driverModeCombo_ = new FluentComboBox(panel);
    driverModeCombo_->addItem(QStringLiteral("DD"), static_cast<int>(DriverMode::Legacy));
    driverModeCombo_->addItem(QStringLiteral("HID"), static_cast<int>(DriverMode::Hid));
    driverModeCombo_->addItem(QStringLiteral("USB"), static_cast<int>(DriverMode::Usb));
    driverModeCombo_->addItem(QString::fromWCharArray(L"\u5c0f\u53fd HID"), static_cast<int>(DriverMode::Native));
    driverModeCombo_->setFixedSize(190, 28);
    modeRow->addWidget(driverModeCombo_);
    panelLayout->addLayout(modeRow);
    panelLayout->addWidget(makeDivider(panel));

    backgroundAutoRunSection_ = new QWidget(panel);
    auto* agentRow = new QHBoxLayout(backgroundAutoRunSection_);
    agentRow->setContentsMargins(0, 0, 0, 0);
    agentRow->setSpacing(8);
    auto* agentLabel = new QLabel(QString::fromWCharArray(L"USB \u540e\u53f0\u8fd0\u884c"), backgroundAutoRunSection_);
    agentLabel->setFont(Fluent::Typography::body());
    agentRow->addWidget(agentLabel);
    agentRow->addStretch();
    agentStatusValue_ = new QLabel(QString::fromWCharArray(L"\u5f85\u68c0\u67e5"), backgroundAutoRunSection_);
    agentStatusValue_->setFont(Fluent::Typography::caption());
    agentStatusValue_->setWordWrap(true);
    agentRow->addWidget(agentStatusValue_);
    repairAgentButton_ = new FluentButton(
        FluentButton::Secondary, QString::fromWCharArray(L"\u4fee\u590d\u540e\u53f0"), backgroundAutoRunSection_);
    repairAgentButton_->setFixedWidth(91);
    repairAgentButton_->hide();
    agentRow->addWidget(repairAgentButton_);
    backgroundAutoRunSwitch_ = new FluentSwitch(backgroundAutoRunSection_);
    agentRow->addWidget(backgroundAutoRunSwitch_);
    backgroundAutoRunSection_->hide();
    panelLayout->addWidget(backgroundAutoRunSection_);
    backgroundAutoRunDivider_ = makeDivider(panel);
    backgroundAutoRunDivider_->hide();
    panelLayout->addWidget(backgroundAutoRunDivider_);

    auto* statusTitleRow = new QHBoxLayout();
    statusTitleRow->setContentsMargins(0, 0, 0, 0);
    statusTitleRow->setSpacing(8);
    statusTitleRow->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u5f53\u524d\u72b6\u6001"), panel));
    statusTitleRow->addStretch();
    uninstallDdButton_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u5378\u8f7d DD"), panel);
    uninstallDdButton_->setFixedWidth(91);
    uninstallDdButton_->hide();
    statusTitleRow->addWidget(uninstallDdButton_);
    uninstallHidButton_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u5378\u8f7d HID"), panel);
    uninstallHidButton_->setFixedWidth(91);
    uninstallHidButton_->hide();
    statusTitleRow->addWidget(uninstallHidButton_);
    uninstallNativeButton_ = new FluentButton(
        FluentButton::Primary, QString::fromWCharArray(L"\u5378\u8f7d\u5c0f\u53fd"), panel);
    uninstallNativeButton_->setFixedWidth(91);
    uninstallNativeButton_->hide();
    statusTitleRow->addWidget(uninstallNativeButton_);
    updateUsbButton_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u66f4\u65b0\u9a71\u52a8"), panel);
    updateUsbButton_->setFixedWidth(91);
    updateUsbButton_->hide();
    statusTitleRow->addWidget(updateUsbButton_);
    panelLayout->addLayout(statusTitleRow);

    auto* summaryRow = new QHBoxLayout();
    summaryRow->setContentsMargins(0, 0, 0, 0);
    auto* summaryLabel = new QLabel(QString::fromWCharArray(L"\u6982\u8981"), panel);
    summaryLabel->setFont(Fluent::Typography::body());
    summaryRow->addWidget(summaryLabel);
    summaryRow->addStretch();
    statusSummaryValue_ = new QLabel(QString::fromWCharArray(L"\u5f85\u68c0\u67e5"), panel);
    statusSummaryValue_->setFont(Fluent::Typography::bodyStrong());
    statusSummaryValue_->setWordWrap(true);
    statusSummaryValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    summaryRow->addWidget(statusSummaryValue_, 1);
    panelLayout->addLayout(summaryRow);

    summaryPanel_ = new QWidget(panel);
    summaryPanel_->setObjectName(QStringLiteral("driverSummaryPanel"));
    summaryPanel_->setAttribute(Qt::WA_StyledBackground, true);
    summaryPanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* summaryPanelLayout = new QVBoxLayout(summaryPanel_);
    summaryPanelLayout->setContentsMargins(10, 8, 10, 8);
    summaryPanelLayout->setSpacing(0);
    summaryDetailValue_ = new QTextEdit(summaryPanel_);
    summaryDetailValue_->setReadOnly(true);
    summaryDetailValue_->setFrameShape(QFrame::NoFrame);
    summaryDetailValue_->document()->setDocumentMargin(0);
    summaryDetailValue_->setMinimumHeight(194);
    summaryDetailValue_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    summaryDetailValue_->setFont(Fluent::Typography::caption());
    summaryDetailValue_->setPlainText(QString::fromWCharArray(L"\u6682\u65e0\u8be6\u7ec6\u72b6\u6001"));
    summaryDetailValue_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    summaryDetailValue_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    summaryPanelLayout->addWidget(summaryDetailValue_);
    panelLayout->addWidget(summaryPanel_, 1);

    outerLayout->addWidget(panel, 1);
    scrollArea->setWidget(scrollContent);
    if (auto* viewport = scrollArea->viewport()) {
        viewport->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    pageLayout->addWidget(scrollArea);

    connect(driverModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (!driverModeCombo_ || index < 0) return;
        emit driverModeChanged(static_cast<DriverMode>(driverModeCombo_->itemData(index).toInt()));
    });
    connect(uninstallDdButton_, &QPushButton::clicked, this, [this]() {
        emit uninstallDriverRequested(DriverMode::Legacy);
    });
    connect(uninstallHidButton_, &QPushButton::clicked, this, [this]() {
        emit uninstallDriverRequested(DriverMode::Hid);
    });
    connect(uninstallNativeButton_, &QPushButton::clicked, this, [this]() {
        emit uninstallDriverRequested(DriverMode::Native);
    });
    connect(updateUsbButton_, &QPushButton::clicked, this, [this]() {
        emit updateUsbDriverRequested();
    });
    connect(repairAgentButton_, &QPushButton::clicked, this, &DriverPage::repairAgentRequested);
    connect(backgroundAutoRunSwitch_, &FluentSwitch::toggled,
        this, &DriverPage::backgroundAutoRunToggled);

    applyTheme();
}

void DriverPage::updateThemeColors() {
    applyTheme();
    if (driverModeCombo_) {
        driverModeCombo_->refreshThemeStyle();
    }
    if (uninstallDdButton_) uninstallDdButton_->refreshMetrics();
    if (uninstallHidButton_) uninstallHidButton_->refreshMetrics();
    if (uninstallNativeButton_) uninstallNativeButton_->refreshMetrics();
    if (updateUsbButton_) updateUsbButton_->refreshMetrics();
    if (repairAgentButton_) repairAgentButton_->refreshMetrics();
    update();
}

void DriverPage::setDriverMode(DriverMode mode) {
    if (!driverModeCombo_) return;
    const int target = static_cast<int>(mode);
    for (int i = 0; i < driverModeCombo_->count(); ++i) {
        if (driverModeCombo_->itemData(i).toInt() == target) {
            driverModeCombo_->blockSignals(true);
            driverModeCombo_->setCurrentIndex(i);
            driverModeCombo_->blockSignals(false);
            break;
        }
    }
}

void DriverPage::setStatusSummary(const QString& summary) {
    if (statusSummaryValue_) statusSummaryValue_->setText(summary);
}

void DriverPage::setDetailText(const QString& details) {
    if (summaryDetailValue_) summaryDetailValue_->setPlainText(compactDriverDetails(details));
}

void DriverPage::setDriverActions(bool ddInstalled, bool hidInstalled, bool nativeInstalled,
                                  bool usbUpdateAvailable) {
    if (uninstallDdButton_) uninstallDdButton_->setVisible(ddInstalled);
    if (uninstallHidButton_) uninstallHidButton_->setVisible(hidInstalled);
    if (uninstallNativeButton_) uninstallNativeButton_->setVisible(nativeInstalled);
    if (updateUsbButton_) updateUsbButton_->setVisible(usbUpdateAvailable);
}

void DriverPage::setBusy(bool busy) {
    busy_ = busy;
    if (driverModeCombo_) driverModeCombo_->setEnabled(!busy);
    if (uninstallDdButton_) uninstallDdButton_->setEnabled(!busy);
    if (uninstallHidButton_) uninstallHidButton_->setEnabled(!busy);
    if (uninstallNativeButton_) uninstallNativeButton_->setEnabled(!busy);
    if (updateUsbButton_) updateUsbButton_->setEnabled(!busy);
    if (repairAgentButton_) repairAgentButton_->setEnabled(!busy);
    if (backgroundAutoRunSwitch_) backgroundAutoRunSwitch_->setEnabled(!busy && !backgroundAutoRunBusy_);
}

void DriverPage::setBackgroundAutoRun(bool enabled) {
    if (!backgroundAutoRunSwitch_) return;
    backgroundAutoRunSwitch_->blockSignals(true);
    backgroundAutoRunSwitch_->setChecked(enabled);
    backgroundAutoRunSwitch_->blockSignals(false);
}

void DriverPage::setBackgroundAutoRunAvailable(bool available) {
    if (backgroundAutoRunSection_) backgroundAutoRunSection_->setVisible(available);
    if (backgroundAutoRunDivider_) backgroundAutoRunDivider_->setVisible(available);
}

void DriverPage::setBackgroundAutoRunBusy(bool busy, bool enabling) {
    backgroundAutoRunBusy_ = busy;
    if (backgroundAutoRunSwitch_) backgroundAutoRunSwitch_->setEnabled(!busy_ && !busy);
    if (busy && agentStatusValue_) {
        agentStatusValue_->setText(enabling
            ? QString::fromWCharArray(L"\u5f00\u542f\u4e2d...")
            : QString::fromWCharArray(L"\u5173\u95ed\u4e2d..."));
    }
    if (busy && repairAgentButton_) repairAgentButton_->hide();
}

void DriverPage::setAgentState(const QString& status, bool repairAvailable) {
    if (backgroundAutoRunBusy_) return;
    if (agentStatusValue_) agentStatusValue_->setText(status);
    if (repairAgentButton_) repairAgentButton_->setVisible(repairAvailable);
}

void DriverPage::applyTheme() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString(
        "QWidget#driverPageRoot { background: %1; }"
        "QWidget#driverSummaryPanel { background-color: %2; border: 1px solid %3; border-radius: 8px; }"
        "QTextEdit { background: transparent; border: none; color: %4; padding: 0; }"
        "QTextEdit QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QTextEdit QScrollBar::handle:vertical { background: %5; border-radius: 3px; min-height: 22px; }"
        "QTextEdit QScrollBar::handle:vertical:hover { background: %6; }"
        "QTextEdit QScrollBar::add-line:vertical, QTextEdit QScrollBar::sub-line:vertical { height: 0; }"
        "QTextEdit QScrollBar::add-page:vertical, QTextEdit QScrollBar::sub-page:vertical { background: none; }")
        .arg(palette.surfaceTint.name(),
            palette.accentSubtle.name(),
            palette.accentBorder.name(QColor::HexArgb),
            Fluent::Colors::textSecondary().name(),
            Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.16)"),
            Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.24)")));
}

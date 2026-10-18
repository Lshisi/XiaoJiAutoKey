#include "dialogs/DriverSelfCheckDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
constexpr int kDialogWidth = 460;
constexpr int kDialogHeight = 468;
}  // namespace

DriverSelfCheckDialog::DriverSelfCheckDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QString::fromWCharArray(L"\u9a71\u52a8\u81ea\u68c0"));
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(kDialogWidth, kDialogHeight);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(10);

    // ── 标题行 ──
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(QString::fromWCharArray(L"\u9a71\u52a8\u81ea\u68c0"), this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleLabel->setStyleSheet(
        QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    closeButton_ = DialogChrome::createCloseButton(this);
    titleRow->addWidget(closeButton_);
    root->addLayout(titleRow);

    // ── 结论标题（红 / 黄 / 绿 / 中性）──
    headingLabel_ = new QLabel(this);
    headingLabel_->setFont(Fluent::Typography::cardTitle());
    headingLabel_->setWordWrap(true);
    root->addWidget(headingLabel_);

    // ── 详情（可滚动富文本）──
    bodyScroll_ = new QScrollArea(this);
    bodyScroll_->setObjectName(QStringLiteral("selfCheckBodyScroll"));
    bodyScroll_->setWidgetResizable(true);
    bodyScroll_->setFrameShape(QFrame::NoFrame);
    bodyScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bodyLabel_ = new QLabel(bodyScroll_);
    bodyLabel_->setObjectName(QStringLiteral("selfCheckBodyContent"));
    bodyLabel_->setTextFormat(Qt::RichText);
    bodyLabel_->setWordWrap(true);
    bodyLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    bodyLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    bodyScroll_->setWidget(bodyLabel_);
    root->addWidget(bodyScroll_, 1);

    // ── 执行阶段文案与进度 ──
    statusLabel_ = new QLabel(this);
    statusLabel_->setFont(Fluent::Typography::body());
    statusLabel_->setWordWrap(true);
    statusLabel_->hide();
    root->addWidget(statusLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(6);
    progressBar_->hide();
    root->addWidget(progressBar_);

    // ── 动态按钮行 ──
    buttonsRow_ = new QHBoxLayout();
    buttonsRow_->setSpacing(8);
    buttonsRow_->addStretch();
    root->addLayout(buttonsRow_);

    refreshVisuals();
}

void DriverSelfCheckDialog::beginCheck() {
    severity_ = Severity::Info;
    severityColor_ = Fluent::Colors::textSecondary();
    headingLabel_->setText(QString::fromWCharArray(L"\u6b63\u5728\u68c0\u6d4b\u9a71\u52a8\u73af\u5883..."));
    bodyLabel_->setText(QString::fromWCharArray(
        L"\u6b63\u5728\u68c0\u67e5\u9a71\u52a8\u8fd0\u884c\u72b6\u6001\u4e0e Windows \u5185\u6838\u9694\u79bb\u8bbe\u7f6e\uff0c\u8bf7\u7a0d\u5019\u3002"));
    statusLabel_->hide();
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->show();
    rebuildActionButtons({});
    setBusy(true);
    refreshVisuals();
}

void DriverSelfCheckDialog::setConclusion(const Conclusion& conclusion) {
    severity_ = conclusion.severity;
    switch (conclusion.severity) {
    case Severity::Ok:
        severityColor_ = Fluent::Colors::success();
        break;
    case Severity::Warning:
        severityColor_ = Fluent::Colors::warning();
        break;
    case Severity::Danger:
        severityColor_ = Fluent::Colors::danger();
        break;
    case Severity::Info:
    default:
        severityColor_ = FactionTheme::instance().palette().accent;
        break;
    }
    headingLabel_->setText(conclusion.heading);
    bodyLabel_->setText(conclusion.bodyHtml);
    statusLabel_->hide();
    progressBar_->hide();
    rebuildActionButtons(conclusion.actions);
    setBusy(false);
    refreshVisuals();
}

void DriverSelfCheckDialog::beginExecute(const QString& title) {
    severity_ = Severity::Info;
    severityColor_ = FactionTheme::instance().palette().accent;
    headingLabel_->setText(title);
    bodyLabel_->clear();
    statusLabel_->setText(QString::fromWCharArray(L"\u6b63\u5728\u51c6\u5907..."));
    statusLabel_->show();
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->show();
    rebuildActionButtons({});
    setBusy(true);
    refreshVisuals();
}

void DriverSelfCheckDialog::setExecuteMessage(const QString& message) {
    if (statusLabel_) statusLabel_->setText(message);
}

void DriverSelfCheckDialog::finishExecute(bool ok, const QString& resultHtml,
    const QVector<QPair<Action, QString>>& actions) {
    severity_ = ok ? Severity::Ok : Severity::Danger;
    severityColor_ = ok ? Fluent::Colors::success() : Fluent::Colors::danger();
    headingLabel_->setText(ok
        ? QString::fromWCharArray(L"\u64cd\u4f5c\u5df2\u5b8c\u6210")
        : QString::fromWCharArray(L"\u64cd\u4f5c\u672a\u5b8c\u6210"));
    bodyLabel_->setText(resultHtml);
    statusLabel_->hide();
    progressBar_->hide();
    rebuildActionButtons(actions);
    setBusy(false);
    refreshVisuals();
}

void DriverSelfCheckDialog::rebuildActionButtons(
    const QVector<QPair<Action, QString>>& actions) {
    qDeleteAll(actionButtons_);
    actionButtons_.clear();
    for (int i = 0; i < actions.size(); ++i) {
        auto* button = new FluentButton(
            i == 0 ? FluentButton::Primary : FluentButton::Secondary, actions[i].second, this);
        button->setFixedHeight(28);
        button->setMinimumWidth(96);
        const Action action = actions[i].first;
        connect(button, &QPushButton::clicked, this, [this, action]() {
            emit actionRequested(static_cast<int>(action));
        });
        buttonsRow_->addWidget(button);
        actionButtons_.push_back(button);
    }
}

void DriverSelfCheckDialog::setBusy(bool busy) {
    busy_ = busy;
    if (closeButton_) closeButton_->setEnabled(!busy);
}

void DriverSelfCheckDialog::refreshVisuals() {
    headingLabel_->setStyleSheet(
        QString("QLabel { color: %1; background: transparent; border: none; }")
            .arg(severityColor_.isValid()
                     ? severityColor_.name(QColor::HexArgb)
                     : Fluent::Colors::textPrimary().name(QColor::HexArgb)));
    setStyleSheet(QString(
        "QScrollArea#selfCheckBodyScroll { background: %1; border: 1px solid %2; border-radius: 6px; }"
        "QLabel#selfCheckBodyContent { color: %3; background: transparent; border: none; }")
        .arg(Fluent::Colors::cardBg().name(),
            Fluent::Colors::border().name(QColor::HexArgb),
            Fluent::Colors::textPrimary().name(QColor::HexArgb)));
    if (auto* viewport = bodyScroll_->viewport()) {
        viewport->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    // 滚动条懒创建懒上样式：直接把样式设到滚动条控件本身，并在 showEvent 再刷一次。
    if (auto* scrollBar = bodyScroll_->verticalScrollBar()) {
        scrollBar->setStyleSheet(Fluent::Styles::scrollBarQss(
            Fluent::Styles::scrollBarHandle(), Fluent::Styles::scrollBarHandle()));
    }
    statusLabel_->setStyleSheet(
        QString("QLabel { color: %1; background: transparent; border: none; }")
            .arg(Fluent::Colors::textSecondary().name(QColor::HexArgb)));
    progressBar_->setStyleSheet(
        QString("QProgressBar { background: %1; border: none; border-radius: 3px; }"
                "QProgressBar::chunk { background: %2; border-radius: 3px; }")
            .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.14)")
                                             : QStringLiteral("rgba(0,0,0,0.10)"),
                FactionTheme::instance().palette().accent.name()));
}

void DriverSelfCheckDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void DriverSelfCheckDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshVisuals();
}

void DriverSelfCheckDialog::keyPressEvent(QKeyEvent* event) {
    // 检测/执行中不允许 Esc 关闭，避免中途打断后台流程。
    if (busy_ && event->key() == Qt::Key_Escape) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void DriverSelfCheckDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void DriverSelfCheckDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void DriverSelfCheckDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

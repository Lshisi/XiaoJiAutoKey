#include "dialogs/UpdateDialog.h"

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

constexpr int kDialogWidth = 440;
constexpr int kDialogHeight = 404;

QString formatMegabytes(qint64 bytes) {
    return QString::number(static_cast<double>(bytes) / 1024.0 / 1024.0, 'f', 1)
        + QStringLiteral(" MB");
}

// 把 Gitee 发行版描述（轻量 Markdown）转成 QLabel 富文本：## 标题加粗，列表用圆点。
QString formatNotes(const QString& notes) {
    if (notes.trimmed().isEmpty()) {
        return QString::fromWCharArray(
            L"\u6682\u65e0\u66f4\u65b0\u65e5\u5fd7\u3002");
    }
    QStringList lines;
    for (QString line : notes.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        line = line.toHtmlEscaped();
        if (line.startsWith(QStringLiteral("### "))) {
            line = line.mid(4);
        } else if (line.startsWith(QStringLiteral("## "))) {
            line = QStringLiteral("<b>") + line.mid(3) + QStringLiteral("</b>");
        } else if (line.startsWith(QStringLiteral("# "))) {
            line = QStringLiteral("<b>") + line.mid(2) + QStringLiteral("</b>");
        }
        if (line.startsWith(QStringLiteral("- "))) {
            line = QStringLiteral("\u2022 ") + line.mid(2);
        }
        lines.push_back(line);
    }
    return lines.join(QStringLiteral("<br>"));
}

}  // namespace

UpdateDialog::UpdateDialog(const QString& version, const QString& notes, bool hasAsset,
    QWidget* parent)
    : QDialog(parent),
      version_(version),
      hasAsset_(hasAsset) {
    setWindowTitle(QString::fromWCharArray(L"\u8f6f\u4ef6\u66f4\u65b0"));
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(kDialogWidth, kDialogHeight);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(10);

    // ── 标题行 ──
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(QString::fromWCharArray(L"\u8f6f\u4ef6\u66f4\u65b0"), this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleLabel->setStyleSheet(
        QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    closeButton_ = DialogChrome::createCloseButton(this);
    titleRow->addWidget(closeButton_);
    root->addLayout(titleRow);

    // ── 发现新版本标题 ──
    headingLabel_ = new QLabel(
        QString::fromWCharArray(L"\u53d1\u73b0\u65b0\u7248\u672c V") + version_, this);
    headingLabel_->setFont(Fluent::Typography::cardTitle());
    root->addWidget(headingLabel_);

    // ── 更新日志（可滚动）──
    notesScroll_ = new QScrollArea(this);
    notesScroll_->setObjectName(QStringLiteral("updateNotesScroll"));
    notesScroll_->setWidgetResizable(true);
    notesScroll_->setFrameShape(QFrame::NoFrame);
    notesScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    notesLabel_ = new QLabel(notesScroll_);
    notesLabel_->setObjectName(QStringLiteral("updateNotesContent"));
    notesLabel_->setTextFormat(Qt::RichText);
    notesLabel_->setText(formatNotes(notes));
    notesLabel_->setWordWrap(true);
    notesLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    notesLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    notesScroll_->setWidget(notesLabel_);
    root->addWidget(notesScroll_, 1);

    // ── 状态与进度 ──
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

    progressLabel_ = new QLabel(this);
    progressLabel_->setFont(Fluent::Typography::caption());
    progressLabel_->hide();
    root->addWidget(progressLabel_);

    root->addStretch();

    // ── 按钮行 ──
    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    cancelButton_ = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u53d6\u6d88"), this);
    cancelButton_->setFixedHeight(28);
    cancelButton_->setFixedWidth(88);
    buttons->addWidget(cancelButton_);
    buttons->addSpacing(8);
    actionButton_ = new FluentButton(FluentButton::Primary,
        hasAsset_ ? QString::fromWCharArray(L"\u786e\u8ba4\u66f4\u65b0")
                  : QString::fromWCharArray(L"\u524d\u5f80\u4e0b\u8f7d"),
        this);
    actionButton_->setFixedHeight(28);
    actionButton_->setMinimumWidth(112);
    buttons->addWidget(actionButton_);
    root->addLayout(buttons);

    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
    connect(actionButton_, &QPushButton::clicked, this, [this]() {
        if (busy_) {
            return;
        }
        if (actionButton_->text() == QString::fromWCharArray(L"\u7acb\u5373\u91cd\u542f\u66f4\u65b0")) {
            emit restartRequested();
        } else {
            emit confirmRequested();
        }
    });

    refreshVisuals();
}

void UpdateDialog::beginDownload() {
    busy_ = true;
    cancelButton_->hide();
    actionButton_->hide();
    if (closeButton_) {
        closeButton_->setEnabled(false);
    }
    statusLabel_->setText(QString::fromWCharArray(L"\u6b63\u5728\u4e0b\u8f7d\u66f4\u65b0\u5305..."));
    statusLabel_->show();
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->show();
    progressLabel_->clear();
    progressLabel_->show();
    refreshVisuals();
}

void UpdateDialog::setDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        progressBar_->setRange(0, 100);
        progressBar_->setValue(static_cast<int>(received * 100 / total));
        progressLabel_->setText(formatMegabytes(received) + QStringLiteral(" / ")
            + formatMegabytes(total));
    } else {
        progressBar_->setRange(0, 0);
        progressLabel_->setText(formatMegabytes(received));
    }
}

void UpdateDialog::setStagedReady() {
    busy_ = false;
    if (closeButton_) {
        closeButton_->setEnabled(true);
    }
    progressBar_->hide();
    progressLabel_->hide();
    statusLabel_->setText(QString::fromWCharArray(
        L"\u66f4\u65b0\u5305\u5df2\u5c31\u7eea\uff0c\u91cd\u542f\u540e\u81ea\u52a8\u5b8c\u6210\u5b89\u88c5\u3002"));
    statusLabel_->show();
    cancelButton_->setText(QString::fromWCharArray(L"\u7a0d\u540e"));
    cancelButton_->show();
    actionButton_->setText(QString::fromWCharArray(L"\u7acb\u5373\u91cd\u542f\u66f4\u65b0"));
    actionButton_->show();
    refreshVisuals();
}

void UpdateDialog::setDownloadError(const QString& message) {
    busy_ = false;
    if (closeButton_) {
        closeButton_->setEnabled(true);
    }
    progressBar_->hide();
    progressLabel_->hide();
    statusLabel_->setText(message);
    statusLabel_->show();
    cancelButton_->setText(QString::fromWCharArray(L"\u53d6\u6d88"));
    cancelButton_->show();
    actionButton_->setText(QString::fromWCharArray(L"\u91cd\u8bd5"));
    actionButton_->show();
    refreshVisuals();
}

void UpdateDialog::refreshVisuals() {
    const QColor accent = FactionTheme::instance().palette().accent;
    headingLabel_->setStyleSheet(
        QString("QLabel { color: %1; background: transparent; border: none; }")
            .arg(accent.name(QColor::HexArgb)));
    setStyleSheet(QString(
        "QScrollArea#updateNotesScroll { background: %1; border: 1px solid %2; border-radius: 6px; }"
        "QLabel#updateNotesContent { color: %3; background: transparent; border: none; }")
        .arg(Fluent::Colors::cardBg().name(),
            Fluent::Colors::border().name(QColor::HexArgb),
            Fluent::Colors::textPrimary().name(QColor::HexArgb)));
    if (auto* viewport = notesScroll_->viewport()) {
        viewport->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    // 滚动条是懒创建、懒上样式的：挂在祖先上的样式表在首次显示时可能来不及
    // 作用到它（表现为系统原生滚动条）。直接把样式设到滚动条控件本身，
    // 并在 showEvent 里再刷新一次，保证打开即生效。
    const QString scrollHandleColor = Fluent::Theme::isDarkMode()
        ? QStringLiteral("rgba(255,255,255,0.18)")
        : QStringLiteral("rgba(0,0,0,0.12)");
    if (auto* scrollBar = notesScroll_->verticalScrollBar()) {
        scrollBar->setStyleSheet(QString(
            "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }"
            "QScrollBar::handle:vertical:hover { background: %1; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }"
            "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { width: 0; height: 0; background: none; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(scrollHandleColor));
    }
    // 下载中显示中性提示，失败显示错误色，其余用主题强调色。
    QColor statusColor = accent;
    if (!progressBar_->isHidden()) {
        statusColor = Fluent::Colors::textSecondary();
    } else if (!busy_ && statusLabel_->text() != QString::fromWCharArray(
                       L"\u66f4\u65b0\u5305\u5df2\u5c31\u7eea\uff0c\u91cd\u542f\u540e\u81ea\u52a8\u5b8c\u6210\u5b89\u88c5\u3002")
               && !statusLabel_->text().isEmpty()) {
        statusColor = Fluent::Colors::danger();
    }
    statusLabel_->setStyleSheet(
        QString("QLabel { color: %1; background: transparent; border: none; }")
            .arg(statusColor.name(QColor::HexArgb)));
    progressBar_->setStyleSheet(
        QString("QProgressBar { background: %1; border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background: %2; border-radius: 3px; }")
            .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.14)")
                                             : QStringLiteral("rgba(0,0,0,0.10)"),
                accent.name()));
    progressLabel_->setStyleSheet(
        QString("QLabel { color: %1; background: transparent; border: none; }")
            .arg(Fluent::Colors::textSecondary().name(QColor::HexArgb)));
}

void UpdateDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void UpdateDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    // 弹窗显示后再刷新一次主题与滚动条样式，覆盖懒创建控件的首帧样式。
    refreshVisuals();
}

void UpdateDialog::keyPressEvent(QKeyEvent* event) {
    // 下载中不允许 Esc 关闭，避免误触中断更新流程。
    if (busy_ && event->key() == Qt::Key_Escape) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void UpdateDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void UpdateDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void UpdateDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

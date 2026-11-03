#pragma once
#include "dialogs/QmlDialog.h"
#include <QPair>
#include <QVector>
class DriverSelfCheckDialog : public QmlDialog {
    Q_OBJECT
public:
    enum class Action { None = 0, OpenDiagnostics, OpenSecurityCenter };
    Q_ENUM(Action)
    enum class Severity { Info = 0, Ok, Warning, Danger };
    struct Conclusion {
        Severity severity = Severity::Info;
        QString heading;
        QString bodyHtml;
        QVector<QPair<Action, QString>> actions;
    };
    explicit DriverSelfCheckDialog(QWidget* parent = nullptr);
    void beginCheck();
    void setConclusion(const Conclusion& conclusion);
signals:
    void actionRequested(int action);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
private:
    bool busy_ = false;
    QVector<QPair<Action, QString>> actions_;
};

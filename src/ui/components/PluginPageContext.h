#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QElapsedTimer>

class PluginPageContext final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pluginId READ pluginId CONSTANT)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap runtime READ runtime NOTIFY runtimeChanged)
public:
    PluginPageContext(QString id, QStringList permissions, QObject* parent = nullptr);
    QString pluginId() const { return id_; }
    QVariantMap theme() const;
    QVariantMap state() const { return state_; }
    QVariantMap settings() const { return settings_; }
    QVariantMap runtime() const { return runtime_; }
    void setRuntimeState(const QVariantMap& state);
    void setSettings(const QVariantMap& settings);
    void setRuntime(const QVariantMap& runtime);
    void refreshTheme() { emit themeChanged(); }
    void deactivate();
    void activate() { active_ = true; }
    Q_INVOKABLE bool saveSetting(const QString& key, const QVariant& value);
    Q_INVOKABLE void navigate(const QString& pageId);
    Q_INVOKABLE void requestEngine(bool macro, bool running);
    Q_INVOKABLE void log(const QString& message);
    Q_INVOKABLE void showMessage(const QString& title, const QString& message);
    Q_INVOKABLE void requestConfirmation(const QString& requestId, const QString& title, const QString& message);
    Q_INVOKABLE QString request(const QString& command, const QVariantMap& payload = {});
    Q_INVOKABLE void reply(const QString& requestId, bool ok, const QVariantMap& result = {}, const QString& error = {});
    Q_INVOKABLE void publish(const QString& topic, const QVariantMap& payload = {});
    Q_INVOKABLE bool setBackgroundRunning(bool running);
    Q_INVOKABLE bool showWindow(const QString& windowId);
    Q_INVOKABLE bool hideWindow(const QString& windowId);
    Q_INVOKABLE bool closeWindow(const QString& windowId);
    Q_INVOKABLE bool configureWindow(const QString& windowId, const QVariantMap& options);
    Q_INVOKABLE bool startWindowMove(const QString& windowId);
signals:
    void themeChanged();
    void stateChanged();
    void settingsChanged();
    void settingEdited(const QString& key, const QVariant& value);
    void navigationRequested(const QString& pageId);
    void engineRequested(bool macro, bool running);
    void logRequested(const QString& message);
    void messageRequested(const QString& title, const QString& message);
    void confirmationRequested(const QString& requestId, const QString& title, const QString& message);
    void confirmationFinished(const QString& requestId, bool accepted);
    void runtimeChanged();
    void lifecycleEvent(const QString& phase, const QString& reason);
    void requestFinished(const QString& requestId, bool ok, const QVariantMap& result, const QString& error);
    void commandReceived(const QString& requestId, const QString& command, const QVariantMap& payload);
    void eventReceived(const QString& topic, const QVariantMap& payload);
    void requestIssued(const QString& requestId, const QString& command, const QVariantMap& payload);
    void replyIssued(const QString& requestId, bool ok, const QVariantMap& result, const QString& error);
    void eventPublished(const QString& topic, const QVariantMap& payload);
    void backgroundRequested(bool running);
    void windowRequested(const QString& operation, const QString& windowId, const QVariantMap& options);
private:
    bool allowMessage();
    QString id_;
    QStringList permissions_;
    QVariantMap state_;
    QVariantMap settings_;
    QVariantMap runtime_;
    bool active_ = true;
    QElapsedTimer messageClock_;
    int messageCount_ = 0;
};

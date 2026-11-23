#include "PreviewController.h"
#include "ui/components/QmlIconProvider.h"
#include <QUrl>
#include <QTimer>

PreviewController::~PreviewController() {
    pluginCenter_.setPageContextProvider({});
    if (petRuntime_) {
        disconnect(petRuntime_.get(), &Plugins::PluginRuntime::changed, this, &PreviewController::refreshPreviewPlugins);
        petRuntime_->shutdown(QStringLiteral("exit")); petRuntime_.reset();
    }
    petContext_.deactivate();
}

void PreviewController::initializePreviewPlugins(bool enabled) {
    previewPlugins_ = {
        QVariantMap{{"id", "xiaoji.driver.xiaoji"}, {"name", tr("小叽驱动")}, {"author", tr("小叽官方")}, {"version", "1.0.0"}, {"description", tr("默认输入驱动，支持键盘、鼠标与滚轮。")}, {"builtin", true}, {"official", true}, {"enabled", true}, {"hasDriver", true}, {"hasPage", false}, {"source", tr("内置")}, {"installationSource", tr("随程序提供（示例）")}, {"permissions", QStringList{"driver.provide", "device.identify"}}}
    };
    if (enabled) previewPlugins_.append(QVariantMap{{"id", "example.status-page"}, {"name", tr("运行状态面板")},
        {"author", tr("SDK 示例")}, {"version", "1.0.0"}, {"description", tr("读取运行状态，保存插件自己的显示标题。")},
        {"source", tr("本地")}, {"enabled", true}, {"hasPage", true}, {"hasDriver", false}, {"reviewed", false},
        {"permissions", QStringList{"ui.page", "engine.read", "settings.own"}}});
    if (enabled) previewPlugins_.append(QVariantMap{{"id", "example.desktop-pet"}, {"name", tr("桌宠示例")},
        {"author", tr("SDK 示例")}, {"version", "1.0.0"}, {"description", tr("设置页、独立后台和透明桌面窗口共用正式插件接口。")},
        {"source", tr("本地")}, {"enabled", true}, {"hasPage", true}, {"hasDriver", false}, {"hasBackground", true}, {"hasWindow", true},
        {"backgroundEnabled", false}, {"permissions", QStringList{"ui.page", "ui.window", "runtime.background", "messaging.own", "settings.own"}}});
    if (enabled) previewPlugins_.append(QVariantMap{{"id", "example.broken-page"}, {"name", tr("异常插件（示例）")},
        {"author", tr("SDK 示例")}, {"version", "1.0.0"}, {"description", tr("清单或文件异常时仍保留管理条目。")},
        {"source", tr("本地")}, {"enabled", false}, {"invalid", true}, {"blocked", true},
        {"installationSource", tr("本地示例插件.zip")}, {"error", tr("插件入口文件缺失，请重新安装或卸载此插件。")}});
    syncPreviewPet();
    refreshPreviewPlugins();
}

void PreviewController::syncPreviewPet() {
    bool enabled = false;
    bool resume = false;
    for (const auto& row : previewPlugins_) if (row.toMap().value("id").toString() == QStringLiteral("example.desktop-pet")) {
        enabled = row.toMap().value("enabled").toBool(); resume = row.toMap().value("backgroundEnabled").toBool();
    }
    if (!enabled) {
        if (petRuntime_) {
            disconnect(petRuntime_.get(), &Plugins::PluginRuntime::changed, this, &PreviewController::refreshPreviewPlugins);
            petRuntime_->shutdown(QStringLiteral("disabled")); petRuntime_.reset();
        }
        petContext_.deactivate();
        return;
    }
    if (petRuntime_) return;
    petContext_.activate();
    Plugins::Manifest manifest;
    manifest.id = QStringLiteral("example.desktop-pet"); manifest.name = tr("桌宠示例"); manifest.version = QStringLiteral("1.0.0");
    manifest.directory = QStringLiteral(":/plugin-examples/desktop-pet"); manifest.backgroundFile = QStringLiteral("Background.qml");
    manifest.permissions = {QStringLiteral("ui.page"), QStringLiteral("ui.window"), QStringLiteral("runtime.background"), QStringLiteral("messaging.own"), QStringLiteral("settings.own")};
    manifest.windows.push_back({QStringLiteral("pet"), tr("桌宠"), QStringLiteral("Pet.qml"), 220, 200, true, true, true});
    manifest.commands.push_back({QStringLiteral("snapshot"), tr("查询桌宠状态"), QStringLiteral("background")});
    manifest.commands.push_back({QStringLiteral("poke"), tr("和桌宠打招呼"), QStringLiteral("background")});
    petRuntime_ = std::make_unique<Plugins::PluginRuntime>(manifest, &petContext_, previewPetWindowState_, this);
    connect(petRuntime_.get(), &Plugins::PluginRuntime::changed, this, &PreviewController::refreshPreviewPlugins);
    connect(petRuntime_.get(), &Plugins::PluginRuntime::windowStateSaved, this, [this](const QVariantMap& state) { previewPetWindowState_ = state; });
    connect(petRuntime_.get(), &Plugins::PluginRuntime::backgroundRequested, this, [this](bool running) {
        handlePluginAction(QStringLiteral("background"), QStringLiteral("example.desktop-pet"), running);
    });
    connect(petRuntime_.get(), &Plugins::PluginRuntime::backgroundFailed, this, [this](const QString&) {
        for (auto& item : previewPlugins_) {
            auto row = item.toMap();
            if (row.value("id").toString() == QStringLiteral("example.desktop-pet")) { row.insert("backgroundEnabled", false); item = row; }
        }
        refreshPreviewPlugins();
    });
    if (resume) QTimer::singleShot(0, petRuntime_.get(), [this] { if (petRuntime_) petRuntime_->startBackground(QStringLiteral("restore")); });
}

void PreviewController::refreshPreviewPlugins() {
    QVariantList pages, providers;
    const QStringList ids{QStringLiteral("xiaoji.driver.xiaoji"), QStringLiteral("xiaoji.driver.dd"), QStringLiteral("xiaoji.driver.usb")};
    bool selectedAvailable = false;
    for (const auto& item : previewPlugins_) {
        const auto row = item.toMap();
        if (row.value("id").toString() == ids[static_cast<int>(driverMode_)] && row.value("enabled").toBool()
            && !row.value("blocked").toBool() && !row.value("newInstall").toBool()) selectedAvailable = true;
    }
    if (!selectedAvailable) for (const auto& item : previewPlugins_) {
        const auto row = item.toMap();
        const auto index = ids.indexOf(row.value("id").toString());
        if (index >= 0 && row.value("enabled").toBool() && !row.value("blocked").toBool() && !row.value("newInstall").toBool()) {
            driverMode_ = static_cast<DriverMode>(index); refreshDriver(); break;
        }
    }
    for (auto& value : previewPlugins_) {
        auto row = value.toMap();
        row.insert("minHostVersion", QStringLiteral("1.1.8"));
        row.insert("latestVersion", QStringLiteral("1.0.1"));
        row.insert("latestReviewed", false);
        if (row.value("builtin").toBool()) {
            row.insert("enabled", true);
            row.insert("bundledVersion", QStringLiteral("1.0.0"));
            row.insert("canRestoreBundled", !row.value("pending").toBool() && row.value("version").toString() != QStringLiteral("1.0.0"));
            row.insert("updateAvailable", demoData_ && !row.value("pending").toBool() && row.value("version").toString() != QStringLiteral("1.0.1"));
        }
        if (row.value("updateAvailable").toBool()) row.insert("notes", tr("示例更新：优化连接状态。目标版本未审核，可自行选择是否更新。"));
        row.insert("currentDriver", row.value("id").toString() == ids[static_cast<int>(driverMode_)]);
        row.insert("canDisable", !row.value("builtin").toBool());
        row.insert("canRemove", !row.value("builtin").toBool());
        row.insert("canEnable", !row.value("removalPendingRestart").toBool());
        row.insert("disableReason", row.value("builtin").toBool() ? tr("小叽驱动是内置基础插件，始终启用，不能停用或卸载插件。") : QString());
        row.insert("hasSystemDriverUninstall", row.value("id").toString() == ids[0] || row.value("id").toString() == ids[1]);
        if (row.value("id").toString() == QStringLiteral("example.desktop-pet") && petRuntime_) {
            const auto runtime = petRuntime_->presentation();
            row.insert("runtimeStatus", runtime.value("status")); row.insert("backgroundRunning", runtime.value("backgroundRunning"));
            row.insert("runtimeError", runtime.value("error")); row.insert("windows", runtime.value("windows")); row.insert("commands", runtime.value("commands"));
        } else if (row.value("id").toString() == QStringLiteral("example.desktop-pet")) {
            row.insert("runtimeStatus", QStringLiteral("stopped")); row.insert("backgroundRunning", false);
            row.insert("runtimeError", QString()); row.insert("windows", QVariantList{}); row.insert("commands", QVariantList{});
        }
        value = row;
        if (!row.value("enabled").toBool() || row.value("blocked").toBool() || row.value("newInstall").toBool()) continue;
        if (row.value("hasPage").toBool()) {
            const bool pet = row.value("id").toString() == QStringLiteral("example.desktop-pet");
            pages.append(QVariantMap{{"id", QStringLiteral("plugin:") + row.value("id").toString() + (pet ? QStringLiteral("/settings") : QStringLiteral("/main"))},
                {"pluginId", row.value("id")}, {"title", pet ? tr("桌宠设置") : tr("运行面板")},
                {"icon", QmlIconProvider::pageIconName(pet ? QStringLiteral(":/plugin-examples/desktop-pet/icons/pet.svg")
                    : QStringLiteral(":/plugin-examples/status/icons/status.svg"))},
                {"source", QUrl(pet ? QStringLiteral("qrc:/plugin-examples/desktop-pet/Main.qml") : QStringLiteral("qrc:/plugin-examples/status/Main.qml"))}});
        }
        if (row.value("hasDriver").toBool()) providers.append(QVariantMap{{"id", row.value("id").toString() + QStringLiteral("/default")}, {"name", row.value("name")}});
    }
    QVariantList discover{
        QVariantMap{{"id", "xiaoji.driver.dd"}, {"name", tr("DD 驱动")}, {"author", tr("小叽官方")},
            {"version", "1.0.0"}, {"description", tr("按需下载，通过 DD 运行库提供输入输出。")}, {"official", true},
            {"hasDriver", true}, {"compatible", true}, {"reviewed", true}, {"source", tr("官方")}},
        QVariantMap{{"id", "xiaoji.driver.usb"}, {"name", tr("USB 驱动")}, {"author", tr("小叽官方")},
            {"version", "1.0.0"}, {"description", tr("按需下载，通过 CH552 USB 设备输出。")}, {"official", true},
            {"hasDriver", true}, {"compatible", true}, {"reviewed", false}, {"source", tr("官方")}},
        QVariantMap{{"id", "example.usb-tool"}, {"name", tr("设备工具（示例）")},
        {"author", tr("社区开发者")}, {"version", "1.0.0"}, {"description", tr("公开发布的未审核页面插件。")},
        {"official", false}, {"source", tr("社区")}, {"minHostVersion", "1.1.8"}, {"hasPage", true}, {"hasDriver", false}, {"reviewed", false}, {"compatible", true}}};
    for (auto& item : discover) {
        auto row = item.toMap();
        bool installed = false;
        for (const auto& plugin : previewPlugins_) if (plugin.toMap().value("id") == row.value("id")) installed = true;
        row.insert("installed", installed);
        row.insert("minHostVersion", QStringLiteral("1.1.8"));
        if (row.value("official").toBool()) row.insert("repository", QStringLiteral("https://github.com/Lshisi/XiaoJiAutoKey"));
        item = row;
    }
    pluginCenter_.setRegistryConfigured(true);
    pluginCenter_.setSnapshot(previewPlugins_, demoData_ ? discover : QVariantList{}, pages);
    driverPage_.setProviders(providers, ids[static_cast<int>(driverMode_)] + QStringLiteral("/default"));
    refreshDriver();
    pluginContext_.activate();
    pluginContext_.setRuntimeState({{"driver", ids[static_cast<int>(driverMode_)]}, {"repeatRunning", keys_.model()->state().value("running")}, {"macroRunning", macros_.model()->state().value("running")}});
    if (currentPage_.startsWith(QStringLiteral("plugin:")) && pluginCenter_.page(currentPage_).isEmpty()) navigate(QStringLiteral("plugins"));
}

void PreviewController::handlePluginAction(const QString& action, const QString& id, const QVariant& value) {
    if (action == QStringLiteral("dialog")) return;
    if (action == QStringLiteral("refresh")) {
        if (pluginCenter_.state().value("catalogRefreshing").toBool()) return;
        pluginCenter_.setCatalogRefreshState(true);
        pluginCenter_.setStatus(tr("正在刷新插件列表…"), true);
        QTimer::singleShot(350, this, [this] {
            refreshPreviewPlugins();
            pluginCenter_.setCatalogRefreshState(false);
            pluginCenter_.setStatus(tr("插件列表已刷新（示例），未发起网络请求。"));
        });
        return;
    }
    if (id == QStringLiteral("example.desktop-pet") && petRuntime_) {
        if (action == QStringLiteral("background")) {
            if (value.toBool()) petRuntime_->startBackground(); else petRuntime_->stopBackground();
            for (auto& item : previewPlugins_) { auto row = item.toMap(); if (row.value("id").toString() == id) { row.insert("backgroundEnabled", value.toBool()); item = row; } }
            refreshPreviewPlugins(); return;
        }
        if (action == QStringLiteral("window") || action == QStringLiteral("window-close") || action == QStringLiteral("window-interactive")) {
            petRuntime_->windowAction(action == QStringLiteral("window") ? QStringLiteral("show") : action == QStringLiteral("window-close") ? QStringLiteral("close") : QStringLiteral("configure"), value.toString(),
                action == QStringLiteral("window-interactive") ? QVariantMap{{"clickThrough", false}} : QVariantMap{});
            return;
        }
        if (action == QStringLiteral("command")) { petContext_.request(value.toString(), {}); return; }
    }
    if (action == QStringLiteral("enable") || action == QStringLiteral("update")) {
        for (auto& item : previewPlugins_) {
            auto row = item.toMap();
            if (row.value("id").toString() != id) continue;
            if (action == QStringLiteral("enable")) {
                if ((!value.toBool() && !row.value("canDisable").toBool()) || (value.toBool() && (row.value("blocked").toBool() || !row.value("canEnable").toBool()))) {
                    pluginCenter_.setStatus(row.value("blocked").toBool() ? row.value("error").toString() : row.value("disableReason").toString());
                    refreshPreviewPlugins(); return;
                }
                row.insert("enabled", value.toBool());
            } else { row.insert("pending", true); row.insert("pendingVersion", row.value("latestVersion")); row.insert("updateAvailable", false); }
            item = row;
        }
    } else if (action == QStringLiteral("open")) {
        navigate(QStringLiteral("plugin:") + id + (id == QStringLiteral("example.desktop-pet") ? QStringLiteral("/settings") : QStringLiteral("/main")));
        return;
    } else if (action == QStringLiteral("remove")) {
        for (const auto& item : previewPlugins_) {
            const auto row = item.toMap();
            if (row.value("id").toString() != id) continue;
            if (!row.value("canRemove").toBool()) { pluginCenter_.setStatus(row.value("disableReason").toString()); return; }
            const bool hasUninstall = row.value("hasSystemDriverUninstall").toBool();
            dialog_.present("DialogMessage.qml", tr("卸载插件"),
                {{"message", tr("移除 %1，保留插件设置。此处只修改预览数据。").arg(row.value("name").toString())},
                    {"optionText", hasUninstall ? tr("同时卸载此插件使用的系统驱动") : QString()}, {"optionChecked", hasUninstall}},
                [this, id](const QVariantMap& fields) {
                    if (fields.value("optionChecked").toBool() && id == QStringLiteral("xiaoji.driver.dd")) ddInstalled_ = false;
                    for (qsizetype i = previewPlugins_.size(); i > 0; --i)
                        if (previewPlugins_[i - 1].toMap().value("id").toString() == id) previewPlugins_.removeAt(i - 1);
                    syncPreviewPet(); refreshPreviewPlugins();
                    pluginCenter_.setStatus(tr("示例插件已卸载，仅修改内存数据。"));
                }, 430, true);
            return;
        }
    } else if (action == QStringLiteral("uninstall-system")) {
        dialog_.present("DialogMessage.qml", tr("卸载系统驱动"),
            {{"message", tr("卸载此插件使用的系统驱动，保留插件本身。此处仅展示预览状态。")}},
            [this, id](const QVariantMap&) {
                if (id == QStringLiteral("xiaoji.driver.xiaoji")) xiaojiInstalled_ = false;
                else if (id == QStringLiteral("xiaoji.driver.dd")) ddInstalled_ = false;
                refreshPreviewPlugins();
            }, 420, true);
        return;
    } else if (action == QStringLiteral("restore-bundled")) {
        for (auto& item : previewPlugins_) {
            auto row = item.toMap();
            if (row.value("id").toString() != id || !row.value("canRestoreBundled").toBool()) continue;
            row.insert("pending", true); row.insert("pendingVersion", row.value("bundledVersion")); item = row;
        }
        pluginCenter_.setStatus(tr("已准备恢复示例随包版本，仅在内存中演示待重启状态。"));
    } else if (action == QStringLiteral("restart")) {
        for (auto& item : previewPlugins_) { auto row = item.toMap(); if (row.value("pending").toBool()) { row.insert("pending", false); row.insert("newInstall", false); row.insert("version", row.value("pendingVersion")); } item = row; }
        pluginCenter_.setStatus(tr("示例更新已应用，没有启动或重启真实程序。"));
    } else if (action == QStringLiteral("import") || action == QStringLiteral("install")) {
        const auto pluginId = action == QStringLiteral("import") ? QStringLiteral("example.status-page") : id;
        bool present = false;
        for (const auto& item : previewPlugins_) if (item.toMap().value("id").toString() == pluginId) present = true;
        if (!present && action == QStringLiteral("install")) {
            for (const auto& item : pluginCenter_.state().value("discover").toList()) {
                auto row = item.toMap();
                if (row.value("id").toString() != pluginId) continue;
                row.insert("enabled", true);
                row.insert("pending", row.value("hasDriver").toBool());
                row.insert("newInstall", row.value("hasDriver").toBool());
                row.insert("pendingVersion", row.value("version"));
                row.insert("installationSource", tr("插件中心下载（示例）"));
                previewPlugins_.append(row); break;
            }
        } else if (!present) previewPlugins_.append(QVariantMap{{"id", "example.status-page"}, {"name", tr("运行状态面板")}, {"author", tr("SDK 示例")},
            {"version", "1.0.0"}, {"description", tr("读取运行状态，保存插件自己的显示标题。")}, {"source", tr("本地")},
            {"installationSource", tr("本地示例插件.zip")}, {"enabled", true}, {"hasPage", true}, {"reviewed", false}});
        pluginCenter_.setStatus(tr("仅导入或安装内存中的示例插件，不读取文件或联网。"));
    } else pluginCenter_.setStatus(tr("此处仅展示插件中心的示例状态，不执行联网或驱动操作。"));
    syncPreviewPet();
    refreshPreviewPlugins();
}

#include "PreviewController.h"
#include "ui/components/QmlIconProvider.h"
#include <QUrl>
#include <QTimer>

namespace {
bool hasPreviewBuiltin(const QVariantList& plugins, const QString& excluded) {
    for (const auto& value : plugins) {
        const auto row = value.toMap();
        if (row.value("id").toString() != excluded && row.value("builtin").toBool()
            && row.value("installed", true).toBool() && row.value("enabled").toBool()
            && row.value("hasDriver").toBool() && !row.value("blocked").toBool()
            && !row.value("invalid").toBool() && !row.value("newInstall").toBool()
            && !row.value("removalPendingRestart").toBool()) return true;
    }
    return false;
}
}

PreviewController::~PreviewController() {
    pluginCenter_.setPageContextProvider({});
    if (petRuntime_) {
        disconnect(petRuntime_.get(), &Plugins::PluginRuntime::changed, this, &PreviewController::refreshPreviewPlugins);
        petRuntime_->shutdown(QStringLiteral("exit")); petRuntime_.reset();
    }
    petContext_.deactivate();
    calendarContext_.deactivate();
}

void PreviewController::initializePreviewPlugins(bool enabled) {
    previewPlugins_ = {
        QVariantMap{{"id", "xiaoji.driver.xiaoji"}, {"name", tr("小叽驱动")}, {"author", tr("小叽官方")}, {"version", "1.0.0"}, {"description", tr("默认输入驱动，支持键盘、鼠标与滚轮。")}, {"builtin", true}, {"official", true}, {"enabled", true}, {"hasDriver", true}, {"hasPage", false}, {"source", tr("内置")}, {"installationSource", tr("随程序提供（示例）")}, {"permissions", QStringList{"driver.provide", "device.identify"}}},
        QVariantMap{{"id", "xiaoji.driver.dd"}, {"name", tr("DD 驱动")}, {"author", tr("小叽官方")}, {"version", "1.0.0"}, {"description", tr("支持键盘、鼠标移动与滚轮输出。")}, {"builtin", true}, {"official", true}, {"enabled", true}, {"hasDriver", true}, {"hasPage", false}, {"source", tr("内置")}, {"installationSource", tr("随程序提供（示例）")}, {"permissions", QStringList{"driver.provide", "device.identify"}}}
    };
    calendarContext_.activate();
    calendarContext_.setSettings({{"demoData", enabled}});
    previewPlugins_.append(QVariantMap{{"id", "xiaoji.page.calendar"}, {"name", tr("活动日历")},
        {"author", tr("小叽官方")}, {"version", "1.0.0"}, {"description", tr("查看每日大战、战场、门派活动和本周安排。")},
        {"source", tr("官方")}, {"official", true}, {"enabled", true}, {"hasPage", true}, {"hasDriver", false},
        {"permissions", QStringList{"ui.page"}}});
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
        {"source", tr("插件中心")}, {"enabled", false}, {"invalid", true}, {"blocked", true},
        {"integrityError", true}, {"canRepair", true}, {"repairRequiresRestart", false},
        {"installationSource", tr("插件中心下载（示例）")}, {"error", tr("插件文件已变化或缺失，已停止加载，请重新安装该版本。")}});
    if (enabled) previewPlugins_.append(QVariantMap{{"id", "example.self-described"}, {"name", tr("本地工具（示例）")},
        {"author", tr("小叽官方")}, {"version", "1.0.0"}, {"enabled", false}, {"source", tr("本地导入")},
        {"description", tr("作者文字由插件自行填写，显示名称不会授予官方身份。")}});
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
    QVariantList pages, providers, installedPlugins, builtinPlugins;
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
        const bool calendar = row.value("id").toString() == QStringLiteral("xiaoji.page.calendar");
        const bool installed = row.value("installed", true).toBool();
        row.insert("installed", installed);
        if (!installed) row.insert("enabled", false);
        const bool bundled = row.value("builtin").toBool() && row.value("version").toString() == QStringLiteral("1.0.0");
        row.insert("bundled", bundled);
        const bool publisherVerified = !bundled && !row.value("integrityError").toBool()
            && (row.value("official").toBool() || row.value("publisherVerified").toBool());
        row.insert("publisherVerified", publisherVerified);
        const bool dd = row.value("id").toString() == QStringLiteral("xiaoji.driver.dd");
        const auto publisher = row.value("publisher").toString();
        const auto repository = row.value("repository").toString();
        row.insert("publisher", publisherVerified ? publisher.isEmpty() ? dd ? QStringLiteral("gitee:fourteen-fourteen") : QStringLiteral("github:lshisi") : publisher : QString());
        row.insert("repository", publisherVerified ? repository.isEmpty() ? dd ? QStringLiteral("https://gitee.com/fourteen-fourteen/xiao-ji-auto-key")
            : QStringLiteral("https://github.com/Lshisi/XiaoJiAutoKey") : repository : QString());
        if (bundled) row.insert("source", tr("随程序提供"));
        else if (row.value("source").toString() == tr("本地")) row.insert("source", tr("本地导入"));
        else if (row.value("source").toString() == tr("官方") || row.value("source").toString() == tr("社区")
            || row.value("source").toString() == tr("随程序提供")) row.insert("source", tr("插件中心"));
        if (bundled || row.value("integrityError").toBool()) row.insert("reviewed", false);
        row.insert("minHostVersion", calendar ? QStringLiteral("1.1.9") : QStringLiteral("1.1.8"));
        row.insert("latestVersion", calendar ? QStringLiteral("1.0.0") : QStringLiteral("1.0.1"));
        row.insert("latestReviewed", false);
        if (row.value("builtin").toBool()) {
            row.insert("bundledVersion", QStringLiteral("1.0.0"));
            row.insert("canRestoreBundled", !row.value("pending").toBool() && (!installed || row.value("version").toString() != QStringLiteral("1.0.0")));
            row.insert("updateAvailable", installed && demoData_ && !row.value("pending").toBool() && row.value("version").toString() != QStringLiteral("1.0.1"));
        }
        if (row.value("updateAvailable").toBool()) row.insert("notes", tr("示例更新：优化连接状态。目标版本未审核，可自行选择是否更新。"));
        row.insert("currentDriver", row.value("id").toString() == ids[static_cast<int>(driverMode_)]);
        const bool lastBuiltin = row.value("builtin").toBool() && !hasPreviewBuiltin(previewPlugins_, row.value("id").toString());
        row.insert("canDisable", installed && !lastBuiltin);
        row.insert("canRemove", installed && !lastBuiltin);
        row.insert("canEnable", !row.value("removalPendingRestart").toBool());
        row.insert("disableReason", installed && lastBuiltin ? tr("至少保留一个可用的内置驱动，请先安装并启用另一种内置驱动。") : QString());
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
        if (row.value("builtin").toBool()) builtinPlugins.append(row);
        if (installed) installedPlugins.append(row);
        if (!installed || !row.value("enabled").toBool() || row.value("blocked").toBool() || row.value("newInstall").toBool()) continue;
        if (row.value("hasPage").toBool()) {
            const bool pet = row.value("id").toString() == QStringLiteral("example.desktop-pet");
            pages.append(QVariantMap{{"id", QStringLiteral("plugin:") + row.value("id").toString() + (pet ? QStringLiteral("/settings") : QStringLiteral("/main"))},
                {"pluginId", row.value("id")}, {"title", calendar ? tr("活动日历") : pet ? tr("桌宠设置") : tr("运行面板")},
                {"icon", QmlIconProvider::pageIconName(calendar ? QStringLiteral(":/plugin-examples/calendar/icons/calendar.svg")
                    : pet ? QStringLiteral(":/plugin-examples/desktop-pet/icons/pet.svg")
                    : QStringLiteral(":/plugin-examples/status/icons/status.svg"))},
                {"source", QUrl(calendar ? QStringLiteral("qrc:/plugin-examples/CalendarPreview.qml")
                    : pet ? QStringLiteral("qrc:/plugin-examples/desktop-pet/Main.qml") : QStringLiteral("qrc:/plugin-examples/status/Main.qml"))}});
        }
        if (row.value("hasDriver").toBool()) providers.append(QVariantMap{{"id", row.value("id").toString() + QStringLiteral("/default")}, {"name", row.value("name")}});
    }
    QVariantList discover{
        QVariantMap{{"id", "xiaoji.driver.usb"}, {"name", tr("USB 驱动")}, {"author", tr("小叽官方")},
            {"version", "1.0.0"}, {"description", tr("按需下载，通过 CH552 USB 设备输出。")}, {"official", true},
            {"hasDriver", true}, {"compatible", true}, {"reviewed", false}, {"source", tr("官方")}},
        QVariantMap{{"id", "example.usb-tool"}, {"name", tr("设备工具（示例）")},
        {"author", tr("社区开发者")}, {"version", "1.0.0"}, {"description", tr("公开发布的未审核页面插件。")},
        {"official", false}, {"source", tr("社区")}, {"minHostVersion", "1.1.8"}, {"hasPage", true}, {"hasDriver", false}, {"reviewed", false}, {"compatible", true}}};
    for (auto& item : discover) {
        auto row = item.toMap();
        bool installed = false;
        for (const auto& plugin : previewPlugins_)
            if (plugin.toMap().value("id") == row.value("id") && plugin.toMap().value("installed", true).toBool()) installed = true;
        row.insert("installed", installed);
        row.insert("minHostVersion", QStringLiteral("1.1.8"));
        row.insert("publisherVerified", true);
        row.insert("bundled", false);
        row.insert("source", tr("插件中心"));
        const bool dd = row.value("id").toString() == QStringLiteral("xiaoji.driver.dd");
        row.insert("publisher", row.value("official").toBool() ? dd ? QStringLiteral("gitee:fourteen-fourteen") : QStringLiteral("github:lshisi") : QStringLiteral("github:example"));
        row.insert("repository", row.value("official").toBool() ? dd ? QStringLiteral("https://gitee.com/fourteen-fourteen/xiao-ji-auto-key")
            : QStringLiteral("https://github.com/Lshisi/XiaoJiAutoKey") : QStringLiteral("https://github.com/example/xiaoji-usb-tool"));
        item = row;
    }
    pluginCenter_.setRegistryConfigured(true);
    pluginCenter_.setSnapshot(installedPlugins, demoData_ ? discover : QVariantList{}, pages, builtinPlugins);
    driverPage_.setProviders(providers, ids[static_cast<int>(driverMode_)] + QStringLiteral("/default"));
    refreshDriver();
    pluginContext_.activate();
    pluginContext_.setRuntimeState({{"driver", ids[static_cast<int>(driverMode_)]}, {"repeatRunning", keys_.model()->state().value("running")}, {"macroRunning", macros_.model()->state().value("running")}});
    if (currentPage_.startsWith(QStringLiteral("plugin:")) && pluginCenter_.page(currentPage_).isEmpty()) navigate(QStringLiteral("plugins"));
}

void PreviewController::handlePluginAction(const QString& action, const QString& id, const QVariant& value) {
    if (action == QStringLiteral("dialog")) return;
    if (action == QStringLiteral("repository")) {
        pluginCenter_.setStatus(tr("预览只展示发布仓库，不打开外部网页。")); return;
    }
    if (action == QStringLiteral("repair-plugin")) {
        dialog_.present("DialogMessage.qml", tr("重新安装插件"),
            {{"message", tr("重新下载原版插件并保留现有设置。预览只修改内存，不读取插件包或联网。")}},
            [this, id](const QVariantMap&) {
                for (auto& item : previewPlugins_) {
                    auto row = item.toMap();
                    if (row.value("id").toString() != id) continue;
                    row.insert("error", QString()); row.insert("invalid", false); row.insert("blocked", false);
                    row.insert("integrityError", false); row.insert("canRepair", false); row.insert("enabled", true);
                    row.insert("publisherVerified", true); row.insert("publisher", QStringLiteral("github:example"));
                    row.insert("repository", QStringLiteral("https://github.com/example/xiaoji-status-page"));
                    item = row;
                }
                refreshPreviewPlugins();
                pluginCenter_.setStatus(tr("示例插件已恢复认证状态，设置保留；仅修改内存。"));
            }, 430, true);
        return;
    }
    if (action == QStringLiteral("refresh")) {
        if (pluginCenter_.state().value("catalogRefreshing").toBool()) return;
        pluginCenter_.setCatalogRefreshState(true);
        pluginCenter_.setStatus(tr("正在刷新插件列表…"), true);
        QTimer::singleShot(350, this, [this] {
            refreshPreviewPlugins();
            pluginCenter_.setCatalogRefreshState(false);
            pluginCenter_.setStatus({});
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
                    for (qsizetype i = previewPlugins_.size(); i > 0; --i) {
                        auto row = previewPlugins_[i - 1].toMap();
                        if (row.value("id").toString() != id) continue;
                        if (row.value("builtin").toBool() && !hasPreviewBuiltin(previewPlugins_, id)) {
                            pluginCenter_.setStatus(tr("至少保留一个可用的内置驱动。")); return;
                        }
                        if (fields.value("optionChecked").toBool()) {
                            if (id == QStringLiteral("xiaoji.driver.dd")) ddInstalled_ = false;
                            else if (id == QStringLiteral("xiaoji.driver.xiaoji")) xiaojiInstalled_ = false;
                        }
                        if (row.value("builtin").toBool()) {
                            row.insert("installed", false); row.insert("enabled", false); row.insert("pending", false);
                            previewPlugins_[i - 1] = row;
                        } else previewPlugins_.removeAt(i - 1);
                    }
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
            if (!row.value("installed", true).toBool()) {
                row.insert("installed", true); row.insert("enabled", true); row.insert("newInstall", true);
            }
            row.insert("pending", true); row.insert("pendingVersion", row.value("bundledVersion")); item = row;
        }
        pluginCenter_.setStatus(tr("已准备恢复示例随包版本，仅在内存中演示待重启状态。"));
    } else if (action == QStringLiteral("restart")) {
        for (auto& item : previewPlugins_) { auto row = item.toMap(); if (row.value("pending").toBool()) { row.insert("pending", false); row.insert("newInstall", false); row.insert("version", row.value("pendingVersion")); } item = row; }
        pluginCenter_.setStatus(tr("示例更新已应用，没有启动或重启真实程序。"));
    } else if (action == QStringLiteral("import") || action == QStringLiteral("install")) {
        if (action == QStringLiteral("import")) for (const auto& item : previewPlugins_) {
            const auto existing = item.toMap();
            if (!existing.value("publisherVerified").toBool() || existing.value("official").toBool()) continue;
            const auto pluginId = existing.value("id").toString();
            dialog_.present("DialogMessage.qml", tr("更换插件发布来源"),
                {{"message", tr("将把 %1 从 %2 更换为发布者未验证的本地包。相同插件 ID 不代表相同发布者，原有设置将保留。此处仅演示确认界面。")
                    .arg(existing.value("name").toString(), existing.value("publisher").toString())}},
                [this, pluginId](const QVariantMap&) {
                    for (auto& item : previewPlugins_) {
                        auto row = item.toMap();
                        if (row.value("id").toString() != pluginId) continue;
                        row.insert("publisherVerified", false); row.insert("publisher", QString()); row.insert("repository", QString());
                        row.insert("reviewed", false); row.insert("source", tr("本地导入")); item = row;
                    }
                    refreshPreviewPlugins(); pluginCenter_.setStatus(tr("示例插件已更换为未验证的本地来源，仅修改内存。"));
                }, 430, true);
            return;
        }
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

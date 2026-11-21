#include "services/XiaojiRuntimeService.h"

#include "xiaoji_native.h"

#include <QByteArray>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#include <Windows.h>
#include <algorithm>
#include <utility>

namespace {
bool Execute(const std::filesystem::path& program, const QStringList& args,
    const std::filesystem::path& work, std::wstring* message, bool utf8 = false, int timeoutMs = 120000) {
    QProcess process;
    process.setProgram(QString::fromStdWString(program.wstring()));
    process.setArguments(args);
    process.setWorkingDirectory(QString::fromStdWString(work.wstring()));
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(5000)) {
        if (message) *message = L"无法启动小叽驱动管理组件：" + process.errorString().toStdWString();
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        if (message) *message = L"小叽驱动管理操作超时，请检查系统设备状态后再操作。";
        return false;
    }
    const auto output = process.readAll();
    if (message) *message = (utf8 ? QString::fromUtf8(output) : QString::fromLocal8Bit(output)).toStdWString();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString Marker(const std::wstring& output, const QString& prefix) {
    for (const auto& line : QString::fromStdWString(output).split(QLatin1Char('\n'))) {
        if (line.startsWith(prefix)) return line.mid(prefix.size()).trimmed();
    }
    return {};
}
}

XiaojiRuntimeService::XiaojiRuntimeService(std::filesystem::path exeDir)
    : exeDir_(std::move(exeDir)) {}

std::filesystem::path XiaojiRuntimeService::ManagerPath() const {
    return exeDir_ / L"driver" / L"xiaoji" / L"xiaoji.exe";
}

std::filesystem::path XiaojiRuntimeService::InstallerPath() const {
    return exeDir_ / L"driver" / L"xiaoji" / L"xiaoji-setup.exe";
}

std::filesystem::path XiaojiRuntimeService::BundledInfPath() const {
    return exeDir_ / L"driver" / L"xiaoji" / L"package" / L"xiaoji.inf";
}

XiaojiRuntimeService::Status XiaojiRuntimeService::Inspect() const {
    const auto devices = xiaoji::InspectDevices();
    return {QFileInfo(QString::fromStdWString(ManagerPath().wstring())).isFile(),
        QFileInfo(QString::fromStdWString(InstallerPath().wstring())).isFile(),
        devices.keyboardPresent, devices.mousePresent,
        devices.keyboardReady, devices.mouseReady, devices.profileReady, devices.boundPackagePresent,
        devices.configurationPresent, devices.error};
}

bool XiaojiRuntimeService::Run(const std::wstring& command,
    const std::filesystem::path& argument, std::wstring* message) const {
    QStringList args{QString::fromStdWString(command)};
    if (!argument.empty()) args.push_back(QString::fromStdWString(argument.wstring()));
    return Execute(ManagerPath(), args, exeDir_, message);
}

bool XiaojiRuntimeService::Install(const std::filesystem::path& signedInf, std::wstring* message) const {
    if (_wcsicmp(signedInf.filename().c_str(), L"xiaoji.inf") != 0) {
        if (message) *message = L"驱动安装文件必须为 xiaoji.inf。";
        return false;
    }
    if (!Run(L"verify", signedInf, message) || !Run(L"install", signedInf, message)) return false;
    const auto status = Inspect();
    if (status.keyboardReady && status.mouseReady && status.profileReady) return true;
    if (message) *message += L"\n安装返回后键盘、鼠标或输入配置尚未就绪。\n" + status.error;
    return false;
}

bool XiaojiRuntimeService::ReadTrustToken(std::wstring* token, std::wstring* message) const {
    std::wstring output;
    if (!Execute(InstallerPath(), {QStringLiteral("--trust-status")}, exeDir_, &output, true, 30000)) {
        if (message) *message = output;
        return false;
    }
    const auto prefix = QStringLiteral("XIAOJI_TRUST_TOKEN=");
    if (QString::fromStdWString(output).indexOf(prefix) < 0) {
        if (message) *message = L"本机签名组件未返回有效的证书状态。";
        return false;
    }
    const auto value = Marker(output, prefix);
    if (!value.isEmpty() && value != QStringLiteral("new")) {
        if (value.size() != 40 || std::any_of(value.begin(), value.end(), [](QChar ch) {
            return !((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || (ch.toLower() >= QLatin1Char('a') && ch.toLower() <= QLatin1Char('f')));
        })) {
            if (message) *message = L"本机签名组件返回的证书指纹无效。";
            return false;
        }
    }
    if (token) *token = value.toStdWString();
    return true;
}

bool XiaojiRuntimeService::InstallLocal(const std::wstring& approvedToken, std::wstring* message) const {
    QStringList args{QStringLiteral("--install")};
    if (!approvedToken.empty()) args << QStringLiteral("--approve-local-trust") << QString::fromStdWString(approvedToken);
    std::wstring output;
    if (!Execute(InstallerPath(), args, exeDir_, &output, true)) {
        if (message) *message = output;
        return false;
    }
    const auto encoded = Marker(output, QStringLiteral("XIAOJI_SIGNED_INF=")).toLatin1();
    const auto decoded = QByteArray::fromBase64(encoded);
    const auto inf = std::filesystem::path(QString::fromUtf8(decoded).toStdWString());
    if (encoded.isEmpty() || decoded.toBase64() != encoded || !inf.is_absolute()
        || _wcsicmp(inf.filename().c_str(), L"xiaoji.inf") != 0) {
        if (message) *message = L"本机签名组件未返回有效的已签名驱动路径。\n" + output;
        return false;
    }
    std::wstring installed;
    const bool ok = Install(inf, &installed);
    if (message) *message = output + L"\n" + installed;
    return ok;
}

bool XiaojiRuntimeService::Repair(std::wstring* message) const {
    if (!Run(L"repair", {}, message)) return false;
    const auto status = Inspect();
    if (status.keyboardReady && status.mouseReady && status.profileReady) return true;
    if (message) *message += L"\n驱动输入配置仍未就绪。\n" + status.error;
    return false;
}

bool XiaojiRuntimeService::Uninstall(std::wstring* message) const {
    if (!Run(L"uninstall", {}, message)) return false;
    const auto status = Inspect();
    if (!status.keyboardPresent && !status.mousePresent && !status.configurationPresent) return true;
    if (message) *message += L"\n系统仍存在小叽设备或卸载记录，卸载尚未完成。请重启后通过保留的卸载按钮重试。\n" + status.error;
    return false;
}

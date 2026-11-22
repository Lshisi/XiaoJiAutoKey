#include "core/plugins/DeviceDiscovery.h"
#include <QRegularExpression>
#include <Windows.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <vector>

namespace Plugins {
namespace {
QString property(HDEVINFO devices, SP_DEVINFO_DATA& item, DWORD field) {
    DWORD bytes = 0;
    SetupDiGetDeviceRegistryPropertyW(devices, &item, field, nullptr, nullptr, 0, &bytes);
    if (!bytes || bytes > 65536) return {};
    std::vector<BYTE> buffer(bytes + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(devices, &item, field, nullptr, buffer.data(), bytes, nullptr)) return {};
    auto text = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.data()), static_cast<qsizetype>(bytes / sizeof(wchar_t)));
    text.replace(QChar::Null, QLatin1Char(' '));
    return text;
}
}
QString detectSerialPort(const QStringList& identityTokens) {
    if (identityTokens.isEmpty()) return {};
    const HDEVINFO devices = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return {};
    struct Close { HDEVINFO value; ~Close() { SetupDiDestroyDeviceInfoList(value); } } close{devices};
    static const QRegularExpression port(QStringLiteral("\\b(COM[0-9]+)\\b"), QRegularExpression::CaseInsensitiveOption);
    for (DWORD index = 0; ; ++index) {
        SP_DEVINFO_DATA item{}; item.cbSize = sizeof(item);
        if (!SetupDiEnumDeviceInfo(devices, index, &item)) break;
        const auto identity = property(devices, item, SPDRP_HARDWAREID).toLower();
        bool matched = false;
        for (const auto& token : identityTokens) if (token.size() >= 5 && identity.contains(token.toLower())) { matched = true; break; }
        if (!matched) continue;
        const auto match = port.match(property(devices, item, SPDRP_FRIENDLYNAME));
        if (match.hasMatch()) return match.captured(1).toUpper();
    }
    return {};
}
}

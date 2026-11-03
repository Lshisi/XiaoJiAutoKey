#pragma once

#include "AppTypes.h"
#include "services/CodeIntegrityInspector.h"
#include "services/DdRuntimeService.h"

#include <QString>

struct DriverRuntimeDiagnosticState {
    bool keyProfileEnabled = false;
    bool macroEnabled = false;
    bool keyEngineRunning = false;
    bool macroEngineRunning = false;
    bool gameGateEnabled = false;
    bool gameGatePaused = false;
};

struct DriverDiagnosticInput {
    DriverMode mode = DriverMode::Legacy;
    bool driverReady = false;
    bool uninstalledByUser = false;
    bool guardsInspected = false;
    DdRuntimeService::RuntimeStatus runtime;
    CodeIntegrityInspector::GuardStatus guards;
    DriverRuntimeDiagnosticState state;
};

struct DriverDiagnosticReport {
    DriverDiagnosticInput input;
    QString generatedAt;
    QString applicationVersion;
    QString operatingSystem;
    QString processArchitecture;
    QString nativeArchitecture;
};

[[nodiscard]] DriverDiagnosticReport BuildDriverDiagnosticReport(const DriverDiagnosticInput& input);
[[nodiscard]] QString FormatDriverDiagnosticReport(const DriverDiagnosticReport& report);

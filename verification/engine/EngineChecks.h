#pragma once

#include "AppTypes.h"
#include "ConfigStore.h"
#include "InputDriverRouter.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include <QJsonArray>
#include <QJsonObject>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace checks {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace path = std::filesystem;
struct Captured {
    long long us;
    std::string scenario;
    KeyDescriptor key;
    bool down;
    int wheel;
    unsigned flags;
};
struct SourceSample { long long us; std::string scenario; bool repeat; bool macro; };
struct Result {
    std::string id, description, status, detail;
    int assertions = 0;
    QJsonObject metadata;
    QJsonArray steps;
    long long durationUs = 0;
    std::string cleanup;
};
extern path::path fixtures, outputDir, runtimeDir;
extern std::vector<unsigned> modifiers;
extern std::mutex captureMutex;
extern std::vector<Captured> events;
extern std::vector<SourceSample> samples;
extern std::vector<Result> results;
extern QJsonArray sourceStages;
extern std::atomic<int> abortCode;
extern InputDriverRouter* router;
extern InputDriver* backend;
extern RepeaterEngine* repeat;
extern MacroEngine* macro;
extern AppConfig config;
extern Result* result;
extern std::string driverName, suite, caseFilter, tagFilter, current;
extern bool listOnly;
extern unsigned seed;

long long Stamp();
KeyDescriptor K(unsigned key, unsigned mods = 0);
KeyDescriptor M(unsigned key, unsigned mods = 0);
void Need(bool condition, const std::string& message);
void Capture(KeyDescriptor key, bool down, int wheel = 0, unsigned flags = 0);
void Pump(int ms);
bool Until(const std::function<bool()>& predicate, int ms = 1000);
long long Mark();
int Count(KeyDescriptor key, bool down, long long after = 0);
int Wheel(int delta, long long after = 0);
void Output(KeyDescriptor key, int ms = 380, int minimum = 2);
void Quiet(KeyDescriptor key, int ms = 220);
void Physical(KeyDescriptor key, bool down);
void Tap(KeyDescriptor key);
void Reset();
void NoStuck();
void Case(std::string id, std::string description, const std::function<void()>& function,
    QJsonObject metadata = {});
void Unverified(std::string id, std::string description, std::string detail);
ProfileConfig Profile(const std::string& id);
MacroConfig Macro(const std::string& id);
void Setup(const ProfileConfig& p, bool running = true);
void StartMacro(const MacroConfig& m);
void Sources(int expected, int ms = 900, bool heartbeat = true);
void CheckK1();
void CheckK2();
void CheckAllK3();
void CheckK4();
void CheckK5();
void CheckRelated();
void CheckMacros();
void ConfigCases();
void RunCatalog(const path::path& file);
KeyDescriptor ParseKey(const QJsonValue& value);
ProfileConfig ParseProfile(const QJsonObject& object);
MacroConfig ParseMacro(const QJsonObject& object);
void AllowCapture(KeyDescriptor key);
void AllowWheel();
}

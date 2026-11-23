#include "EngineChecks.h"
#include "RecordingDriver.h"
#include <QCommandLineParser>
#include <QFile>
#include <QDateTime>
#include <QStringList>
#include <QSaveFile>
#include <set>
#include "ConfigStore.h"
#include "DdDriver.h"
#include "InputDriverRouter.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "XiaojiDriver.h"
#include "coordinators/EngineHookCoordinator.h"
#include "dialogs/AppMessageDialog.h"
#include "input/GlobalInputHook.h"
#include "input/RawInputManager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <shlobj.h>
#include <mmsystem.h>

// Valid-config paths only: fail closed rather than displaying a modal dialog.
void AppMessageDialog::warning(QWidget*, const QString&, const QString& text) {
    throw std::runtime_error("Unexpected modal validation: " + text.toStdString());
}

namespace checks {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace path = std::filesystem;
path::path fixtures, runtimeDir;
std::vector<unsigned> modifiers = {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL,
    VK_RCONTROL, VK_LMENU, VK_RMENU};
std::mutex captureMutex;
std::vector<Captured> events;
std::vector<SourceSample> samples;
std::vector<Result> results;
QJsonArray sourceStages;
std::string current = "startup";
std::atomic<int> abortCode{0};
std::atomic<bool> guardActive{false};
std::atomic<DWORD> guardThreadId{0};
std::thread guardThread;
std::mutex guardInitMutex;
std::condition_variable guardInitCv;
bool guardInitialized = false, guardReady = false;
Clock::time_point started = Clock::now();
InputDriverRouter* router = nullptr;
InputDriver* backend = nullptr;
RepeaterEngine* repeat = nullptr;
MacroEngine* macro = nullptr;
AppConfig config;
Result* result = nullptr;
std::string driverName, suite, caseFilter, tagFilter;
bool listOnly = false;
unsigned seed = 117;
std::string commit, blockedReason;
bool sourceDirty = false;
int timeoutMs = 900000;
Clock::time_point caseStarted;
int caseTimeoutMs = 45000;
bool cleaning = false;
std::set<std::pair<int, unsigned>> allowedKeys;
bool wheelAllowed = false, contaminated = false;
HWND expectedForeground = nullptr;
void SaveReport();
bool Selected(const std::string& id, const QJsonObject& metadata);
path::path outputDir;

long long Stamp() { return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-started).count(); }
KeyDescriptor K(unsigned key, unsigned mods) { return {InputDevice::Keyboard, key, static_cast<std::uint8_t>(mods)}; }
KeyDescriptor M(unsigned key, unsigned mods) { return {InputDevice::Mouse, key, static_cast<std::uint8_t>(mods)}; }
void Need(bool condition, const std::string& message) {
    if (result) ++result->assertions;
    if (!condition) throw std::runtime_error(message);
}
bool GuardedKeyboard(unsigned code) {
    return code != VK_ESCAPE;
}
void AllowCapture(KeyDescriptor key) {
    std::lock_guard lock(captureMutex);
    for (auto component : ExpandKeyComponents(key))
        allowedKeys.emplace(static_cast<int>(component.device), component.code);
}
void AllowWheel() {
    std::lock_guard lock(captureMutex);
    wheelAllowed = true;
}
void Capture(KeyDescriptor key, bool down, int wheel, unsigned flags) {
    std::lock_guard lock(captureMutex);
    if (driverName != "recording" &&
        (wheel ? !wheelAllowed : !allowedKeys.count({static_cast<int>(key.device), key.code})))
        contaminated = true;
    events.push_back({Stamp(), current, key, down, wheel, flags});
}
LRESULT CALLBACK KeyboardCapture(int code, WPARAM message, LPARAM data) {
    if (code >= 0 && guardActive.load()) {
        auto& key = *reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if (key.vkCode == VK_ESCAPE) abortCode = 1;
        if (GuardedKeyboard(key.vkCode)) {
            Capture(KeyboardHookKeyDescriptor(key.vkCode,key.scanCode,key.flags),
                message == WM_KEYDOWN || message == WM_SYSKEYDOWN,0,key.flags);
            return 1;
        }
    }
    return CallNextHookEx(nullptr,code,message,data);
}
LRESULT CALLBACK MouseCapture(int code, WPARAM message, LPARAM data) {
    if (code >= 0 && guardActive.load()) {
        auto& info = *reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        unsigned key = 0;
        bool down = false;
        switch (message) {
        case WM_LBUTTONDOWN: key=VK_LBUTTON; down=true; break;
        case WM_LBUTTONUP: key=VK_LBUTTON; break;
        case WM_RBUTTONDOWN: key=VK_RBUTTON; down=true; break;
        case WM_RBUTTONUP: key=VK_RBUTTON; break;
        case WM_MBUTTONDOWN: key=VK_MBUTTON; down=true; break;
        case WM_MBUTTONUP: key=VK_MBUTTON; break;
        case WM_XBUTTONDOWN: down=true; [[fallthrough]];
        case WM_XBUTTONUP: key=HIWORD(info.mouseData)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2; break;
        case WM_MOUSEWHEEL:
            Capture({},false,static_cast<short>(HIWORD(info.mouseData)),info.flags);
            return 1;
        default: break;
        }
        if (key) { Capture(M(key),down,0,info.flags); return 1; }
    }
    return CallNextHookEx(nullptr,code,message,data);
}
void StartGuard() {
    guardActive = true;
    guardThread = std::thread([] {
        MSG msg{};
        PeekMessageW(&msg,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
        guardThreadId = GetCurrentThreadId();
        auto keyboard=SetWindowsHookExW(WH_KEYBOARD_LL,KeyboardCapture,GetModuleHandleW(nullptr),0);
        auto mouse=SetWindowsHookExW(WH_MOUSE_LL,MouseCapture,GetModuleHandleW(nullptr),0);
        {
            std::lock_guard lock(guardInitMutex);
            guardReady=keyboard && mouse;
            guardInitialized=true;
        }
        guardInitCv.notify_one();
        if (keyboard && mouse) while (GetMessageW(&msg,nullptr,0,0)>0) DispatchMessageW(&msg);
        if (keyboard) UnhookWindowsHookEx(keyboard);
        if (mouse) UnhookWindowsHookEx(mouse);
        guardThreadId=0;
    });
    std::unique_lock lock(guardInitMutex);
    guardInitCv.wait(lock,[]{return guardInitialized;});
    Need(guardReady,"Safety capture hooks unavailable");
}
void StopGuard() {
    if (guardThreadId.load()) PostThreadMessageW(guardThreadId,WM_QUIT,0,0);
    if (guardThread.joinable()) guardThread.join();
    guardActive=false;
}
void Pump(int ms) {
    auto until=Clock::now()+std::chrono::milliseconds(ms);
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents,1);
        if (router) samples.push_back({Stamp(),current,
            router->ForwardedDownSince(K('A'),InputSource::Repeater).has_value(),
            router->ForwardedDownSince(K('A'),InputSource::Macro).has_value()});
        if (!cleaning) {
            if (expectedForeground && GetForegroundWindow()!=expectedForeground) {
                {std::lock_guard lock(captureMutex);contaminated=true;}
                throw std::runtime_error("Foreground changed during a foreground-sensitive engine scenario");
            }
            if (path::exists(outputDir / "cancel.flag")) abortCode=1;
            if (abortCode.load()) throw std::runtime_error("Run aborted by Escape or cancellation");
            if (Clock::now()-started>std::chrono::milliseconds(timeoutMs)
                || (result && Clock::now()-caseStarted>std::chrono::milliseconds(caseTimeoutMs))) {
                abortCode=2;
                throw std::runtime_error("Safety timeout");
            }
        }
        std::this_thread::sleep_for(1ms);
    } while (Clock::now()<until);
}
bool Until(const std::function<bool()>& predicate,int ms) {
    auto deadline=Clock::now()+std::chrono::milliseconds(ms);
    do { if (predicate()) return true; Pump(2); } while (Clock::now()<deadline);
    return predicate();
}
long long Mark() { return Stamp(); }
int Count(KeyDescriptor key,bool down,long long after) {
    std::lock_guard lock(captureMutex);
    return static_cast<int>(std::count_if(events.begin(),events.end(),[&](const auto& event){
        return event.scenario==current && event.us>=after && !event.wheel
            && SamePrimaryKey(event.key,key) && event.down==down;
    }));
}
int Wheel(int delta,long long after) {
    std::lock_guard lock(captureMutex);
    return static_cast<int>(std::count_if(events.begin(),events.end(),[&](const auto& event){
        return event.scenario==current && event.us>=after && event.wheel==delta;
    }));
}
void Output(KeyDescriptor key,int ms,int minimum) {
    auto at=Mark(); Pump(ms);
    Need(Count(key,true,at)>=minimum,"Missing real driver down events for "+ToUtf8(KeyDescriptorToStorage(key)));
    Need(Count(key,false,at)>=minimum-1,"Missing real driver releases for "+ToUtf8(KeyDescriptorToStorage(key)));
}
void Quiet(KeyDescriptor key,int ms) {
    auto at=Mark(); Pump(ms);
    Need(Count(key,true,at)==0,"Unexpected output after release/pause: "+ToUtf8(KeyDescriptorToStorage(key)));
}
void Physical(KeyDescriptor key,bool down) {
    AllowCapture(key);
    router->NotePhysicalKey(key,down);
    if (down) repeat->OnPhysicalDown(key); else repeat->OnPhysicalUp(key);
}
void Tap(KeyDescriptor key) { Physical(key,true); Physical(key,false); }
void Reset() {
    cleaning=true;
    repeat->SetToolRunning(false);
    macro->StopAll();
    macro->UpdateMacros({});
    repeat->SetSuspended(false);
    macro->SetSuspended(false);
    repeat->UpdateBindings(0,{},false);
    repeat->SetPauseKeys({});
    repeat->SetSequenceSwitchKey({});
    for (auto code:modifiers) router->NotePhysicalKey(K(code),false);
    repeat->SetMixedMode(false);
    repeat->SetWasdProtection(false);
    repeat->SetSelfInjectedKeyboardVisible(false);
    repeat->SetSelfInjectedMouseVisible(false);
    Pump(20);
    cleaning=false;
}
ProfileConfig Profile(const std::string& id) {
    auto prefix="["+id+"]";
    auto it=std::find_if(config.profiles.begin(),config.profiles.end(),[&](auto& p){return ToUtf8(p.name).starts_with(prefix);});
    Need(it!=config.profiles.end(),"Fixture profile missing: "+id);
    return *it;
}
MacroConfig Macro(const std::string& id) {
    auto prefix="["+id+"]";
    auto it=std::find_if(config.macros.begin(),config.macros.end(),[&](auto& p){return ToUtf8(p.name).starts_with(prefix);});
    Need(it!=config.macros.end(),"Fixture macro missing: "+id);
    return *it;
}
void Setup(const ProfileConfig& p,bool running) {
    for (const auto& binding:p.bindings) AllowCapture(binding.key);
    for (const auto& key:p.pauseKeys) AllowCapture(key);
    if (p.sequenceSwitchKey) AllowCapture(*p.sequenceSwitchKey);
    if (p.controlHotkey) AllowCapture(*p.controlHotkey);
    repeat->ForgetProfileRuntimeState(p.id);
    repeat->SetSelfInjectedKeyboardVisible(false);
    repeat->SetSelfInjectedMouseVisible(false);
    repeat->UpdateBindings(p.id,p.bindings,p.rememberSequencePosition);
    repeat->SetMixedMode(p.mixedMode);
    repeat->SetWasdProtection(p.wasdProtection);
    repeat->SetPauseKeys(p.pauseKeys);
    repeat->SetSequenceSwitchKey(p.sequenceSwitchKey);
    repeat->RefreshLongPressForeground(GetForegroundWindow(),true);
    repeat->SetToolRunning(running);
}
void StartMacro(const MacroConfig& m) {
    for (const auto& step:m.steps) {
        if (step.action==MacroAction::Wheel) AllowWheel();
        else if (step.key.IsValid()) AllowCapture(step.key);
    }
    for (const auto& key:m.pauseKeys) AllowCapture(key);
    if (m.triggerKey) AllowCapture(*m.triggerKey);
    macro->UpdateMacros({m});
    macro->OnTriggerDown(*m.triggerKey);
    if (m.triggerMode==MacroTriggerMode::Toggle) macro->OnTriggerUp(*m.triggerKey);
    Need(macro->HasActiveMacro(),"Macro failed to start: "+ToUtf8(macro->GetStatus(m.id).error));
}
void Sources(int expected,int ms,bool heartbeat) {
    auto at=Mark(); Pump(ms);
    auto r=std::count_if(samples.begin(),samples.end(),[&](auto& x){return x.scenario==current && x.us>=at && x.repeat;});
    auto m=std::count_if(samples.begin(),samples.end(),[&](auto& x){return x.scenario==current && x.us>=at && x.macro;});
    auto a=Count(K('A'),true,at);
    sourceStages.append(QJsonObject{{"scenario",QString::fromStdString(current)},
        {"afterUs",static_cast<qint64>(at)},{"durationMs",ms},{"expectedSource",expected},
        {"repeatSamples",static_cast<qint64>(r)},{"macroSamples",static_cast<qint64>(m)},
        {"actualADowns",a},{"actualBDowns",Count(K('B'),true,at)}});
    Need((expected==1?r>0:r==0) && (expected==2?m>0:m==0),
        "Wrong A source; expected="+std::to_string(expected)+" repeaterSamples="+std::to_string(r)+" macroSamples="+std::to_string(m)+" capturedADowns="+std::to_string(a));
    Need(expected==0?a==0:expected==1?a>=std::max(2,ms/225):a>=1 && a<=2,
        "Observed A cadence contradicts owner samples: capturedADowns="+std::to_string(a));
    if (heartbeat) Need(Count(K('B'),true,at)>0,"Macro B heartbeat missing");
}
void NoStuck() {
    std::lock_guard lock(captureMutex);
    std::vector<KeyDescriptor> keys;
    for (const auto& e:events) if (e.scenario==current && !e.wheel) {
        auto found=std::find_if(keys.begin(),keys.end(),[&](auto& k){return SamePrimaryKey(k,e.key);});
        if (e.down && found==keys.end()) keys.push_back(e.key);
        if (!e.down && found!=keys.end()) keys.erase(found);
    }
    Need(keys.empty(),"Captured output remains down after cleanup");
}
bool Selected(const std::string& id,const QJsonObject& metadata) {
    if (metadata["group"]=="infrastructure") return !listOnly;
    if (!caseFilter.empty() && id.find(caseFilter)==std::string::npos) return false;
    if (!tagFilter.empty() && !metadata["tags"].toArray().contains(QString::fromStdString(tagFilter))) return false;
    return suite=="all" || suite==metadata["group"].toString().toStdString()
        || (suite.size()==2 && suite[0]=='K' && id.starts_with(suite))
        || (suite=="related" && id.starts_with("G"))
        || (suite=="macro" && id.starts_with("M"));
}
void Case(std::string id,std::string description,const std::function<void()>& function,QJsonObject metadata) {
    if (metadata.isEmpty()) metadata={{"group","regression"},{"tags",QJsonArray{"regression"}}};
    if (!Selected(id,metadata)) return;
    results.push_back({id,description,listOnly?"LISTED":abortCode.load()?"ABORTED":blockedReason.empty()?"RUNNING":"BLOCKED",blockedReason,0,metadata});
    result=&results.back();
    if (listOnly || abortCode.load() || !blockedReason.empty()) { result=nullptr; return; }
    caseStarted=Clock::now();
    caseTimeoutMs=metadata["timeoutMs"].toInt(45000);
    expectedForeground=metadata["requiresStableForeground"].toBool()?GetForegroundWindow():nullptr;
    if (metadata["requiresStableForeground"].toBool() && !expectedForeground) {
        result->status="BLOCKED"; result->detail="A stable foreground window is required; no window is opened by verification";
        result=nullptr; return;
    }
    {std::lock_guard lock(captureMutex);current=id;contaminated=false;allowedKeys.clear();wheelAllowed=false;}
    // Legacy functions have a fixed fixture vocabulary; data scenarios narrow this in Setup.
    if (metadata["group"]=="regression") {
        for (auto key:modifiers) AllowCapture(K(key));
        for (const auto& p:config.profiles) for (const auto& b:p.bindings) AllowCapture(b.key);
        for (auto key:{K(VK_F8),K(VK_F9),K('P'),K('R'),K('D'),M(VK_MBUTTON)}) AllowCapture(key);
        AllowWheel();
    }
    SaveReport();
    try { function(); Reset(); NoStuck(); result->status="PASS"; result->cleanup="released"; }
    catch (const std::exception& e) {
        result->status=abortCode.load()?"ABORTED":"FAIL";
        result->detail=e.what();
        try { Reset(); NoStuck(); result->cleanup="released after failure"; }
        catch (...) { result->cleanup="release unconfirmed"; }
        router->ReleaseAll(); backend->ReleaseAll();
    }
    {std::lock_guard lock(captureMutex);
        if (contaminated && result->status!="ABORTED") {
            result->status="CONTAMINATED";
            result->detail="Unexpected captured input; source device is unknown. "+result->detail;
            abortCode=3;
        }
    }
    result->durationUs=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-caseStarted).count();
    std::cout<<result->status<<" "<<id<<" ("<<result->assertions<<") "<<result->detail<<std::endl;
    result=nullptr;
    expectedForeground=nullptr;
    SaveReport();
}
void Unverified(std::string id,std::string description,std::string detail) {
    QJsonObject metadata{{"group",id.starts_with("ENV-")?"external":"regression"},{"tags",QJsonArray{"external"}}};
    if (!Selected(id,metadata)) return;
    results.push_back({id,description,listOnly?"LISTED":"NOT_VERIFIED",detail,0,metadata});
}
void ProbeCapture() {
    AllowCapture(K(VK_F24)); AllowCapture(K(VK_F8));
    AllowCapture(M(VK_RBUTTON)); AllowCapture(M(VK_MBUTTON));
    for (unsigned code:modifiers) Need(!(GetAsyncKeyState(code)&0x8000),"Release physical modifiers before running");
    Need(!(GetAsyncKeyState(VK_RBUTTON)&0x8000) && !(GetAsyncKeyState(VK_MBUTTON)&0x8000),"Release mouse buttons before running");
    INPUT input[2]{};
    input[0].type=input[1].type=INPUT_KEYBOARD;
    input[0].ki.wVk=input[1].ki.wVk=VK_F24;
    input[1].ki.dwFlags=KEYEVENTF_KEYUP;
    auto at=Mark();
    Need(SendInput(2,input,sizeof(INPUT))==2,"Safety hook SendInput probe failed");
    Need(Until([&]{return Count(K(VK_F24),true,at)==1 && Count(K(VK_F24),false,at)==1;}),"Safety hook did not capture SendInput");
    for (auto key:{K(VK_F8),M(VK_RBUTTON),M(VK_MBUTTON)}) {
        at=Mark();
        Need(backend->SendDown(key),"Driver probe down failed");
        Need(Until([&]{return Count(key,true,at)>0;}),"No actual driver event observed; refusing full injection suite");
        Need(!(GetAsyncKeyState(static_cast<int>(key.code))&0x8000),"Safety interception failed; key reached Windows key state");
        Need(backend->SendUp(key),"Driver probe release failed");
        Need(Until([&]{return Count(key,false,at)>0;}),"Driver release not captured");
    }
}

void SaveReport() {
    path::create_directories(outputDir);
    QJsonArray rows;
    QJsonObject counts,coverage;
    for (const auto& row:results) {
        rows.append(QJsonObject{{"id",QString::fromStdString(row.id)},
            {"description",QString::fromStdString(row.description)},{"status",QString::fromStdString(row.status)},
            {"detail",QString::fromStdString(row.detail)},{"assertions",row.assertions},
            {"metadata",row.metadata},{"steps",row.steps},{"durationUs",static_cast<qint64>(row.durationUs)},
            {"cleanup",QString::fromStdString(row.cleanup)}});
        const auto status=QString::fromStdString(row.status);
        counts[status]=counts[status].toInt()+1;
        auto dimensions=row.metadata["modes"].toArray();
        dimensions.append("group:"+row.metadata["group"].toString());
        for (const auto& dimension:dimensions) {
            auto key=dimension.toString(); auto tally=coverage[key].toObject();
            tally[status]=tally[status].toInt()+1; coverage[key]=tally;
        }
    }
    QString overall=listOnly?"LIST_ONLY":"PASS";
    for (auto state:{"NOT_VERIFIED","BLOCKED","FAIL","CONTAMINATED","ABORTED","RUNNING","NO_MATCH"})
        if (counts.value(state).toInt()) overall=state;
    std::vector<Captured> captured;
    {std::lock_guard lock(captureMutex);captured=events;}
    QJsonObject document{{"schemaVersion",1},{"commit",QString::fromStdString(commit)},
        {"sourceDirty",sourceDirty},{"driver",QString::fromStdString(driverName)},
        {"suite",QString::fromStdString(suite)},{"seed",static_cast<qint64>(seed)},
        {"generatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"build",QString(__DATE__ " " __TIME__)},{"command",QJsonArray::fromStringList(QCoreApplication::arguments())},
        {"overall",overall},{"counts",counts},{"coverage",coverage},{"results",rows},{"sourceStages",sourceStages},
        {"capturedEvents",static_cast<qint64>(captured.size())},
        {"scope",driverName=="recording"?"Production engine/router, recording output only; no system input sent":"Production engine/router/driver; output intercepted at low-level hooks, not game acceptance"},
        {"limitations",QJsonArray{"No game acceptance or anti-cheat compatibility claim", "Raw-signal cases bypass WM_INPUT parsing/device filtering",
            "No full application self-input-filtering end-to-end claim", "Hardware hooks cannot distinguish unexpected physical input using the same keys",
            "SetSuspended is not foreground detection", "Scheduling uses real time, not a virtual clock"}}};
    const auto bytes=QJsonDocument(document).toJson(QJsonDocument::Indented);
    QString reportError;
    const auto save=[&] {
        QSaveFile json(QString::fromStdWString((outputDir/"results.json").wstring()));
        if (!json.open(QIODevice::WriteOnly) || json.write(bytes)!=bytes.size() || !json.commit()) {
            reportError=json.errorString(); return false;
        }
        return true;
    };
    // Windows readers/scanners can briefly prevent atomic replacement; retry only report I/O, never a scenario.
    bool saved=false;
    for (int attempt=0;attempt<10 && !(saved=save());++attempt) std::this_thread::sleep_for(50ms);
    if (!saved) throw std::runtime_error("Cannot save report: "+reportError.toStdString());
    static std::size_t eventsWritten=0,samplesWritten=0;
    static bool csvInitialized=false;
    std::ofstream csv(outputDir/"events.csv",std::ios::app);
    std::ofstream source(outputDir/"owners.csv",std::ios::app);
    if (!csvInitialized) {
        csv<<"microseconds,scenario,device,vk,down,wheel,flags\n";
        source<<"microseconds,scenario,A_repeater_forwarded,A_macro_forwarded\n";
    }
    for (auto i=eventsWritten;i<captured.size();++i) {
        const auto& e=captured[i];
        csv<<e.us<<','<<e.scenario<<','<<(e.key.device==InputDevice::Mouse?"mouse":"keyboard")
            <<','<<e.key.code<<','<<e.down<<','<<e.wheel<<','<<e.flags<<'\n';
    }
    for (auto i=samplesWritten;i<samples.size();++i) {
        const auto& s=samples[i];
        source<<s.us<<','<<s.scenario<<','<<s.repeat<<','<<s.macro<<'\n';
    }
    csv.flush(); source.flush();
    if (!csv || !source) throw std::runtime_error("Cannot append captured events/ownership report");
    eventsWritten=captured.size(); samplesWritten=samples.size(); csvInitialized=true;
}
bool DdServicesRunning() {
    auto manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if (!manager) return false;
    bool ready=true;
    for (auto name:{L"dd.key.96688",L"dd.mou.96688"}) {
        auto service=OpenServiceW(manager,name,SERVICE_QUERY_STATUS);
        SERVICE_STATUS status{};
        ready=service && QueryServiceStatus(service,&status) && status.dwCurrentState==SERVICE_RUNNING && ready;
        if (service) CloseServiceHandle(service);
    }
    CloseServiceHandle(manager); return ready;
}
}

int main(int argc,char** argv) {
    using namespace checks;
    SetConsoleOutputCP(CP_UTF8);
    QCoreApplication app(argc,argv);
    QCommandLineParser args; args.addHelpOption();
    for (auto name:{"driver","suite","fixtures","catalog","runtime","output","commit","case","tag","seed","timeout-ms"})
        args.addOption(QCommandLineOption(name,QString("Verification ")+name,"value"));
    for (auto name:{"allow-real-input","list","source-dirty","catalog-only"}) args.addOption(QCommandLineOption(name));
    args.process(app);
    driverName=args.isSet("driver")?args.value("driver").toStdString():"recording";
    suite=args.isSet("suite")?args.value("suite").toStdString():"all";
    const std::set<std::string> suites={"all","regression","single","composite","complex","validation","stress","config","external","K1","K2","K3","K4","K5","related","macro"};
    if (!suites.count(suite) || (driverName!="recording" && driverName!="dd" && driverName!="xiaoji")) {
        std::cerr<<"Unknown suite or driver\n"; return 2;
    }
    listOnly=args.isSet("list");
    if (!listOnly && driverName!="recording" && !args.isSet("allow-real-input")) {
        std::cerr<<"Real drivers require --allow-real-input\n"; return 2;
    }
    fixtures=path::path(args.value("fixtures").toStdWString());
    runtimeDir=path::path(args.value("runtime").toStdWString());
    outputDir=path::path(args.value("output").toStdWString());
    if (outputDir.empty() || path::exists(outputDir/"results.json")) {
        std::cerr<<"Provide a fresh --output directory\n"; return 2;
    }
    commit=args.value("commit").toStdString(); sourceDirty=args.isSet("source-dirty");
    caseFilter=args.value("case").toStdString(); tagFilter=args.value("tag").toStdString();
    bool numeric=true;
    if (args.isSet("seed")) seed=args.value("seed").toUInt(&numeric);
    if (!numeric) return 2;
    if (args.isSet("timeout-ms")) timeoutMs=args.value("timeout-ms").toInt(&numeric);
    if (!numeric || timeoutMs<1000 || timeoutMs>7200000) return 2;
    path::create_directories(outputDir);
    HANDLE lock=nullptr;
    if (!listOnly) {
        lock=CreateMutexW(nullptr,FALSE,L"Local\\XiaojiEngineVerification");
        if (!lock || GetLastError()==ERROR_ALREADY_EXISTS) {
            std::cerr<<"Another verification run is active\n"; if (lock) CloseHandle(lock); return 2;
        }
    }
    RecordingDriver recorded;
    std::unique_ptr<DdDriver> dd;
    std::unique_ptr<XiaojiDriver> xiaoji;
    backend=&recorded;
    InputDriverRouter selectedRouter; selectedRouter.SetDriver(backend); router=&selectedRouter;
    RepeaterEngine repeater(selectedRouter); MacroEngine macros(selectedRouter); repeat=&repeater; macro=&macros;
    bool highResolutionTimer=false;
    int exitCode=0;
    try {
        if (!listOnly && driverName!="recording") {
            if (!IsUserAnAdmin()) blockedReason="Real-driver verification requires an administrator terminal; no elevation is attempted";
            else if (driverName=="dd") {
                if (!DdServicesRunning()) blockedReason="Existing DD keyboard/mouse services must already be running; installation is forbidden";
                else {
                    dd=std::make_unique<DdDriver>(); backend=dd.get();
                    if (!dd->LoadLegacyFromDirectory(runtimeDir.wstring())) blockedReason=ToUtf8(dd->GetLastError());
                }
            } else {
                xiaoji=std::make_unique<XiaojiDriver>(); backend=xiaoji.get();
                if (!xiaoji->Connect()) blockedReason=ToUtf8(xiaoji->GetLastError());
            }
            selectedRouter.SetDriver(backend);
        }
        if (!listOnly && blockedReason.empty()) {
            highResolutionTimer=timeBeginPeriod(1)==TIMERR_NOERROR;
            if (!highResolutionTimer) throw std::runtime_error("Cannot request 1ms timer resolution");
            if (driverName!="recording") {
                StartGuard();
                Case("capture-probe","Real output captured and suppressed",ProbeCapture,{{"group","infrastructure"}});
                if (results.back().status!="PASS") throw std::runtime_error("Capture probe failed; no scenario injection allowed");
            }
            repeater.StartThread(); macros.StartThread();
        }
        Case("driver-connect",driverName=="recording"?"Recording output adapter is ready":"Production driver connection",
            []{Need(backend->IsReady(),"Selected output adapter is not ready");},{{"group","infrastructure"}});
        if (!args.isSet("catalog-only")) {
            config=ConfigStore(fixtures/"regressions.json").Load();
            if (config.profiles.size()!=35 || config.macros.size()!=6) throw std::runtime_error("Regression fixture missing or invalid");
            CheckK1(); CheckK2(); CheckAllK3(); CheckK4(); CheckK5(); CheckRelated(); CheckMacros(); ConfigCases();
        }
        RunCatalog(path::path(args.value("catalog").toStdWString()));
        if (!args.isSet("catalog-only")) {
            Unverified("ENV-WM-INPUT","WM_INPUT ingress and device filtering","Not implemented; coordinator signal replay is not this coverage");
            Unverified("ENV-SELF-FILTER","Full application injected-input feedback loop","Needs a separate application-context ingress harness");
            Unverified("ENV-FOREGROUND","Real game process recognition and focus switching","Direct SetSuspended scenarios do not validate foreground recognition");
            Unverified("ENV-GAME","Target game martial-assist response","Engine timing/output is not game acceptance");
        }
    } catch (const std::exception& e) {
        std::cerr<<"FATAL "<<e.what()<<'\n';
        results.push_back({"infrastructure","Execution could not complete","ABORTED",e.what()}); exitCode=2;
    }
    result=nullptr;
    repeater.StopThread(); macros.StopThread(); selectedRouter.ReleaseAll(); backend->ReleaseAll();
    if (guardActive.load()) { std::this_thread::sleep_for(60ms); StopGuard(); }
    if (highResolutionTimer) timeEndPeriod(1);
    if (std::none_of(results.begin(),results.end(),[](const auto& r){return r.metadata["group"]!="infrastructure";})) {
        results.push_back({"selection","No scenario matched the requested filters","NO_MATCH",caseFilter+" / "+tagFilter});
        if (!exitCode) exitCode=6;
    }
    try { SaveReport(); } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; exitCode=2; }
    if (lock) CloseHandle(lock);
    if (exitCode) return exitCode;
    for (auto [status,code]:{std::pair{"ABORTED",2},{"CONTAMINATED",5},{"FAIL",1},{"BLOCKED",3},{"NOT_VERIFIED",4}})
        if (std::any_of(results.begin(),results.end(),[&](const auto& r){return r.status==status;})) return code;
    std::cout<<"Report: "<<outputDir.string()<<'\n'; return 0;
}

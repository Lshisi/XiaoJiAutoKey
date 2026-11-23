#include "EngineChecks.h"
#include "MacroActions.h"
#include "RepeatModeRules.h"
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QSaveFile>
#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

namespace checks {
namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Scenario definition: " + message);
}
void KnownFields(const QJsonObject& object, std::initializer_list<const char*> allowed) {
    for (auto it=object.begin();it!=object.end();++it)
        Require(std::any_of(allowed.begin(),allowed.end(),[&](const auto* key){return it.key()==key;}),
            "unknown field: "+it.key().toStdString());
}
template<class T> T Named(const QJsonValue& value, const std::map<QString,T>& names) {
    auto it=names.find(value.toString());
    Require(it!=names.end(), "unknown enum: " + value.toString().toStdString());
    return it->second;
}
KeyBinding Binding(const QJsonObject& o) {
    KeyBinding b;
    b.id=static_cast<std::uint64_t>(o["id"].toInteger(1));
    b.key=ParseKey(o["key"]);
    b.mode=Named<RepeatMode>(o["mode"], {{"toggle",RepeatMode::Toggle},{"hold",RepeatMode::Hold},
        {"independent",RepeatMode::Independent},{"sequence",RepeatMode::Sequence},{"longPress",RepeatMode::LongPress}});
    b.delayMs=o["delayMs"].toInt(90);
    Require(b.delayMs>=1 && b.delayMs<=10000,"delayMs outside 1..10000");
    b.enabled=o["enabled"].toBool(true);
    b.suppressMixedMode=o["suppressMixedMode"].toBool();
    b.preventAccidentalStop=o["preventAccidentalStop"].toBool();
    b.martialAssist=o["martialAssist"].toBool();
    return b;
}
bool CapturedHeld(KeyDescriptor key) {
    std::lock_guard lock(captureMutex);
    bool held=false;
    for (const auto& e:events)
        if (e.scenario==result->id && !e.wheel && SamePrimaryKey(key,e.key)) held=e.down;
    return held;
}
bool StateMatches(const QJsonObject& expected) {
    KnownFields(expected,{"running","paused","priority","independent","sequence","holds","macro"});
    const auto s=repeat->GetStatus();
    const auto has=[&](const char* key){return expected.contains(key);};
    if (has("running") && s.running!=expected["running"].toBool()) return false;
    if (has("paused") && s.paused!=expected["paused"].toBool()) return false;
    if (has("priority") && s.holdPriorityActive!=expected["priority"].toBool()) return false;
    if (has("independent")) {
        if (expected["independent"].isNull()) { if (s.independentKey) return false; }
        else if (s.independentKey!=ParseKey(expected["independent"])) return false;
    }
    if (has("sequence")) {
        if (expected["sequence"].isNull()) { if (s.sequenceTaskId) return false; }
        else if (s.sequenceTaskId!=static_cast<std::uint64_t>(expected["sequence"].toInteger())) return false;
    }
    if (has("holds") && s.holdKeys.size()!=static_cast<std::size_t>(expected["holds"].toInt())) return false;
    if (has("macro")) {
        const auto m=expected["macro"].toObject();
        KnownFields(m,{"id","running","paused","step","error"});
        Require(!m.empty(),"macro state assertion cannot be empty");
        Require(m.size()>1 || !m.contains("id"),"macro state must assert more than id");
        const auto status=macro->GetStatus(static_cast<std::uint64_t>(m["id"].toInteger(1)));
        if (m.contains("running") && status.running!=m["running"].toBool()) return false;
        if (m.contains("paused") && status.paused!=m["paused"].toBool()) return false;
        if (m.contains("step") && status.currentStep!=m["step"].toInt()) return false;
        if (m.contains("error") && (!status.error.empty())!=m["error"].toBool()) return false;
    }
    return true;
}
QJsonObject Snapshot() {
    const auto s=repeat->GetStatus();
    return {{"running",s.running},{"paused",s.paused},{"priority",s.holdPriorityActive},
        {"independent",s.independentKey?QJsonValue(QString::fromStdWString(KeyDescriptorToStorage(*s.independentKey))):QJsonValue(QJsonValue::Null)},
        {"sequence",s.sequenceTaskId?QJsonValue(static_cast<qint64>(*s.sequenceTaskId)):QJsonValue(QJsonValue::Null)},
        {"macroActive",macro->HasActiveMacro()}};
}
void Feed(KeyDescriptor key, bool down, const QString& target) {
    AllowCapture(key);
    router->NotePhysicalKey(key,down);
    Require(target=="repeat" || target=="macro" || target=="both","invalid input target");
    if (target!="macro") { if (down) repeat->OnPhysicalDown(key); else repeat->OnPhysicalUp(key); }
    if (target!="repeat") { if (down) macro->OnTriggerDown(key); else macro->OnTriggerUp(key); }
}
void Observe(const QJsonObject& step, long long since) {
    auto outputs=step["outputs"].toArray();
    Require(!outputs.empty(),"output assertion cannot be empty");
    for (const auto& value:outputs) {
        const auto e=value.toObject();
        KnownFields(e,{"wheel","min","max","key","minDown","maxDown","minUp","maxUp","held"});
        if (e.contains("wheel")) {
            const auto n=Wheel(e["wheel"].toInt(),since);
            Need(n>=e["min"].toInt(1) && n<=e["max"].toInt(100000),"Wheel count: actual="+std::to_string(n));
            continue;
        }
        const auto key=ParseKey(e["key"]);
        Require(e.size()>1,"output assertion has no expectation");
        const int downs=Count(key,true,since),ups=Count(key,false,since);
        const auto name=ToUtf8(KeyDescriptorToStorage(key));
        Need(downs>=e["minDown"].toInt(0) && downs<=e["maxDown"].toInt(100000),
            "Down count for "+name+": actual="+std::to_string(downs)+" expected="+QJsonDocument(e).toJson(QJsonDocument::Compact).toStdString());
        Need(ups>=e["minUp"].toInt(0) && ups<=e["maxUp"].toInt(100000),"Up count for "+name+": actual="+std::to_string(ups));
        if (e.contains("held")) Need(CapturedHeld(key)==e["held"].toBool(),"Captured held-state differs for "+name);
    }
}
void RandomWalk(const QJsonObject& step) {
    const int count=step["count"].toInt(100);
    Require(count>=1 && count<=2000,"random count outside 1..2000");
    std::mt19937 random(seed);
    bool paused=false,suspended=false,hold=false,independent=false;
    std::uint64_t sequence=4;
    for (int i=0;i<count;++i) {
        const auto choice=random()%7;
        QJsonObject action{{"index",i},{"atUs",static_cast<qint64>(Stamp())}};
        if (choice==0) {
            paused=!paused; Feed(K('P'),paused,"repeat"); action["op"]=paused?"pause-down":"pause-up";
        } else if (choice==1) {
            suspended=!suspended; repeat->SetSuspended(suspended); action["op"]=suspended?"suspend":"resume";
        } else if (choice==2) {
            hold=!hold; Feed(K('B'),hold,"repeat"); action["op"]=hold?"hold-down":"hold-up";
        } else if (choice==3) {
            Feed(K('C'),true,"repeat"); Feed(K('C'),false,"repeat"); independent=!independent; action["op"]="independent-tap";
        } else if (choice==4) {
            Feed(K('R'),true,"repeat"); Feed(K('R'),true,"repeat"); Feed(K('R'),false,"repeat");
            sequence=sequence==4?5:4; action["op"]="sequence-with-duplicate-down";
        } else if (choice==5) {
            repeat->SetToolRunning(false); repeat->SetSuspended(false); repeat->SetToolRunning(true);
            paused=false; suspended=false; hold=false; independent=false; sequence=4;
            action["op"]="stop-restart";
        } else { action["op"]="wait"; }
        // Store the generated sequence before evaluating it, including the failing operation.
        result->steps.append(action);
        Pump(12);
        Need(repeat->GetStatus().independentKey==(independent?std::optional(K('C')):std::nullopt),"Random independent model differs at "+std::to_string(i));
        Need(repeat->GetStatus().sequenceTaskId==sequence,"Random sequence model differs at "+std::to_string(i));
        Need(repeat->GetStatus().paused==paused,"Random pause model differs at "+std::to_string(i));
        if (i%10==0) {
            const auto at=Mark(); Pump(180);
            Need((Count(K('A'),true,at)>0)==(!paused && !suspended && !hold),"Random background output model differs at "+std::to_string(i));
            Need((Count(K('B'),true,at)>0)==(hold && !suspended),"Random hold output model differs at "+std::to_string(i));
        }
    }
}
void Execute(const QJsonArray& steps, ProfileConfig& profile, int depth=0) {
    Require(depth<4,"nested repeat depth exceeded");
    long long mark=Mark();
    for (const auto& value:steps) {
        Require(value.isObject(),"step must be an object");
        const auto step=value.toObject();
        const auto op=step["op"].toString();
        auto trace=step; trace["atUs"]=static_cast<qint64>(Stamp());
        const int traceIndex=result->steps.size(); result->steps.append(trace);
        if (op=="start") repeat->SetToolRunning(true);
        else if (op=="stop") repeat->SetToolRunning(false);
        else if (op=="macroStop") macro->StopAll();
        else if (op=="down" || op=="up" || op=="tap") {
            const auto key=ParseKey(step["key"]); const auto target=step["target"].toString("repeat");
            if (op!="up") Feed(key,true,target);
            if (op!="down") Feed(key,false,target);
        } else if (op=="suspend") {
            const auto target=step["target"].toString("both");
            Require(target=="both" || target=="repeat" || target=="macro","invalid suspend target");
            if (target!="macro") repeat->SetSuspended(step["value"].toBool());
            if (target!="repeat") macro->SetSuspended(step["value"].toBool());
        } else if (op=="wait") {
            const int ms=step["ms"].toInt(-1); Require(ms>=0 && ms<=10000,"wait outside 0..10000"); Pump(ms);
        } else if (op=="mark") mark=Mark();
        else if (op=="foreground") repeat->RefreshLongPressForeground(GetForegroundWindow(),step["allowed"].toBool(true));
        else if (op=="source") {
            const auto key=ParseKey(step["key"]); const auto expected=step["source"].toString();
            Require(expected=="repeat" || expected=="macro" || expected=="none","invalid source");
            const auto ms=step["ms"].toInt(900); Require(ms>=1 && ms<=10000,"invalid source observation window");
            Pump(25); auto at=Mark(); int r=0,m=0;
            const auto until=Clock::now()+std::chrono::milliseconds(ms);
            while (Clock::now()<until) {
                r+=router->ForwardedDownSince(key,InputSource::Repeater).has_value();
                m+=router->ForwardedDownSince(key,InputSource::Macro).has_value(); Pump(2);
            }
            trace["repeatSamples"]=r; trace["macroSamples"]=m;
            Need((expected=="repeat"?r>0:r==0) && (expected=="macro"?m>0:m==0),"Observed source differs");
            const auto n=Count(key,true,at);
            // A retained LongPress can have zero new downs: require it to be held instead.
            Need(expected=="none"?n==0:n>0 || CapturedHeld(key),"Captured output contradicts source samples");
            if (step.contains("heartbeat")) Need(Count(ParseKey(step["heartbeat"]),true,at)>0,"Other-source heartbeat missing");
        }
        else if (op=="observe" || op=="counts") {
            if (op=="observe") {
                const int ms=step["ms"].toInt(280),settle=step["settleMs"].toInt(25);
                Require(ms>=1 && ms<=10000 && settle>=0 && settle<=2000,"invalid observation window");
                Pump(settle); mark=Mark(); Pump(ms);
            }
            Observe(step,step["from"]=="start"?0:mark);
        } else if (op=="state") {
            const auto e=step["expect"].toObject(); Require(!e.empty(),"state assertion cannot be empty");
            const int ms=step["withinMs"].toInt(500); Require(ms>=0 && ms<=10000,"invalid state timeout");
            Need(Until([&]{return StateMatches(e);},ms),"State mismatch: expected="+QJsonDocument(e).toJson(QJsonDocument::Compact).toStdString()
                +" actual="+QJsonDocument(Snapshot()).toJson(QJsonDocument::Compact).toStdString());
        } else if (op=="bindings") {
            profile=ParseProfile(step["profile"].toObject());
            for (const auto& b:profile.bindings) AllowCapture(b.key);
            repeat->UpdateBindings(profile.id,profile.bindings,profile.rememberSequencePosition);
        } else if (op=="setup") {
            repeat->SetToolRunning(false); profile=ParseProfile(step["profile"].toObject());
            Setup(profile,step["running"].toBool(true));
        } else if (op=="mixed") repeat->SetMixedMode(step["value"].toBool());
        else if (op=="validatePair") {
            auto a=Binding(step["a"].toObject()),b=Binding(step["b"].toObject());
            auto expected=Named<RepeatBindingConflict>(step["conflict"],{{"none",RepeatBindingConflict::None},
                {"duplicate",RepeatBindingConflict::Duplicate},{"sharedTrigger",RepeatBindingConflict::SharedTrigger},
                {"heldTrigger",RepeatBindingConflict::HeldTrigger},{"modifierTrigger",RepeatBindingConflict::ModifierTrigger},
                {"sharedPrimary",RepeatBindingConflict::SharedPrimary},{"pulsedModifier",RepeatBindingConflict::PulsedModifier},
                {"modifierState",RepeatBindingConflict::ModifierState}});
            const bool mixed=step["mixed"].toBool();
            Need(CheckRepeatBindingPair(a,b,mixed)==expected,"Conflict classification mismatch");
            Need(CheckRepeatBindingPair(b,a,mixed)==expected,"Conflict classification is not symmetric");
            if (step.contains("together")) Need(CanRepeatBindingsRunTogether(a,b,mixed)==step["together"].toBool(),"Concurrency rule differs");
        } else if (op=="validateMacro") {
            auto m=ParseMacro(step["macro"].toObject());
            Need(ValidateMacroSteps(m.steps).empty()==step["valid"].toBool(),"Macro validation differs");
        } else if (op=="repeat") {
            const int n=step["count"].toInt(); Require(n>0 && n<=500,"repeat count outside 1..500");
            for (int i=0;i<n;++i) Execute(step["steps"].toArray(),profile,depth+1);
        } else if (op=="randomWalk") RandomWalk(step);
        else throw std::runtime_error("Unknown scenario operation: "+op.toStdString());
        trace["afterUs"]=static_cast<qint64>(Stamp()); trace["observed"]=Snapshot();
        result->steps.replace(traceIndex,trace);
    }
}
}

KeyDescriptor ParseKey(const QJsonValue& value) {
    auto key=StorageToKeyDescriptor(value.toString().toStdWString());
    Require(key && key->IsValid(),"invalid key: "+value.toString().toStdString());
    return *key;
}
ProfileConfig ParseProfile(const QJsonObject& o) {
    ProfileConfig p; p.id=static_cast<std::uint64_t>(o["id"].toInteger(10000));
    p.controlHotkey=M(VK_MBUTTON); p.sequenceSwitchKey=K('R');
    p.mixedMode=o["mixedMode"].toBool(); p.wasdProtection=o["wasdProtection"].toBool();
    p.rememberSequencePosition=o["rememberSequencePosition"].toBool();
    if (o.contains("sequenceSwitchKey")) p.sequenceSwitchKey=ParseKey(o["sequenceSwitchKey"]);
    for (const auto& key:o["pauseKeys"].toArray()) p.pauseKeys.push_back(ParseKey(key));
    std::set<std::uint64_t> ids;
    for (const auto& b:o["bindings"].toArray()) {
        auto binding=Binding(b.toObject()); Require(ids.insert(binding.id).second,"duplicate binding id");
        p.bindings.push_back(binding);
    }
    return p;
}
MacroConfig ParseMacro(const QJsonObject& o) {
    MacroConfig m; m.id=static_cast<std::uint64_t>(o["id"].toInteger(1));
    m.enabled=o["enabled"].toBool(true); m.loopCount=o["loopCount"].toInt();
    m.triggerKey=o.contains("triggerKey")?ParseKey(o["triggerKey"]):K(VK_F9);
    m.triggerMode=Named<MacroTriggerMode>(o["triggerMode"].toString("toggle"),{{"toggle",MacroTriggerMode::Toggle},{"hold",MacroTriggerMode::Hold}});
    for (const auto& key:o["pauseKeys"].toArray()) m.pauseKeys.push_back(ParseKey(key));
    std::uint64_t id=0;
    for (const auto& value:o["steps"].toArray()) {
        const auto s=value.toObject(); MacroStep step; step.id=++id;
        step.action=Named<MacroAction>(s["action"],{{"click",MacroAction::Click},{"hold",MacroAction::Hold},
            {"down",MacroAction::Down},{"up",MacroAction::Up},{"wait",MacroAction::Wait},{"wheel",MacroAction::Wheel}});
        if (MacroActionNeedsKey(step.action) && !s["key"].toString().isEmpty()) step.key=ParseKey(s["key"]);
        step.enabled=s["enabled"].toBool(true); step.durationMs=s["durationMs"].toInt(120);
        step.delayAfterMs=s["delayAfterMs"].toInt(90); step.wheelDelta=s["wheelDelta"].toInt(120);
        m.steps.push_back(step);
    }
    return m;
}
void RunCatalog(const path::path& file) {
    QFile input(QString::fromStdWString(file.wstring())); Require(input.open(QIODevice::ReadOnly),"cannot open catalog");
    QJsonParseError error; auto document=QJsonDocument::fromJson(input.readAll(),&error);
    Require(error.error==QJsonParseError::NoError && document.isObject(),"invalid catalog JSON");
    Require(document.object()["schemaVersion"].toInt()==1,"unsupported catalog schema");
    const auto cases=document.object()["cases"].toArray(); Require(!cases.empty(),"catalog is empty");
    std::set<std::string> ids;
    for (const auto& value:cases) {
        const auto definition=value.toObject(); const auto id=definition["id"].toString().toStdString();
        Require(!id.empty() && ids.insert(id).second,"empty/duplicate case id");
        Require(!definition["steps"].toArray().empty(),"case has no steps: "+id);
        QJsonObject metadata{{"group",definition["group"]},{"tags",definition["tags"]},
            {"modes",definition["modes"]},{"timeoutMs",definition["timeoutMs"].toInt(45000)},
            {"requiresStableForeground",definition["requiresStableForeground"]},{"seed",static_cast<qint64>(seed)}};
        Case(id,definition["description"].toString().toStdString(),[&] {
            auto p=ParseProfile(definition["profile"].toObject());
            Setup(p,definition["running"].toBool(false));
            std::vector<MacroConfig> macros;
            for (const auto& m:definition["macros"].toArray()) {
                auto parsed=ParseMacro(m.toObject());
                for (const auto& s:parsed.steps) { if (s.action==MacroAction::Wheel) AllowWheel(); else if (s.key.IsValid()) AllowCapture(s.key); }
                macros.push_back(parsed);
            }
            macro->UpdateMacros(macros);
            Execute(definition["steps"].toArray(),p);
            Need(result->assertions>0,"Scenario must evaluate explicit expectations");
        },metadata);
        if (!results.empty() && results.back().id==id && results.back().status!="PASS" && results.back().status!="LISTED") {
            const auto hash=QCryptographicHash::hash(QByteArray::fromStdString(id),QCryptographicHash::Sha256).toHex().left(12);
            const auto name="repro-"+hash+".json";
            QSaveFile replay(QString::fromStdWString(outputDir.wstring())+"/"+QString::fromLatin1(name));
            Require(replay.open(QIODevice::WriteOnly),"cannot preserve reproducer");
            replay.write(QJsonDocument(QJsonObject{{"schemaVersion",1},{"seed",static_cast<qint64>(seed)},
                {"cases",QJsonArray{definition}}}).toJson(QJsonDocument::Indented));
            Require(replay.commit(),"cannot commit reproducer");
            results.back().metadata["reproducer"]=QString::fromLatin1(name);
        }
    }
}
}

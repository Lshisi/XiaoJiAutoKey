#include "EngineChecks.h"
#include "coordinators/EngineHookCoordinator.h"
#include "input/GlobalInputHook.h"
#include "input/RawInputManager.h"
#include <algorithm>
#include <QFile>
#include <QJsonDocument>
#include <fstream>

namespace checks {
void ConfigCases() {
    for (auto kind:{"legacy","modern"}) {
        Case(std::string("C1-")+kind,"Sound config migration, priority, save and reload",[&] {
            const auto audio=outputDir/"sound-paths";
            path::create_directories(audio);
            for (auto name:{"repeat-on.wav","repeat-off.wav","macro-on.wav","macro-off.wav"})
                std::ofstream(audio/name,std::ios::binary)<<"RIFF";
            QFile input(QString::fromStdWString((fixtures/(std::string("sounds-")+kind+".json")).wstring()));
            Need(input.open(QIODevice::ReadOnly),"Cannot read sound fixture");
            auto text=input.readAll();
            text.replace("@AUDIO@",QString::fromStdWString(audio.wstring()).replace('\\','/').toUtf8());
            const auto file=outputDir/(std::string("C1-")+kind+".json");
            std::ofstream(file,std::ios::binary).write(text.data(),text.size());
            ConfigStore store(file); auto c=store.Load();
            const bool legacy=std::string(kind)=="legacy";
            for (const auto& d:SoundEventDefinitions()) {
                const int volume=d.group==SoundGroup::Repeat?(legacy?0:35)
                    :d.group==SoundGroup::Macro?(legacy?25:45):(legacy?25:55);
                Need(c.sounds[SoundEventIndex(d.event)].volume==volume,"Sound volume conversion differs");
            }
            for (auto [event,name]:{std::pair{SoundEvent::RepeatOn,"repeat-on.wav"},
                {SoundEvent::RepeatOff,"repeat-off.wav"},{SoundEvent::MacroOn,"macro-on.wav"},{SoundEvent::MacroOff,"macro-off.wav"}})
                Need(path::equivalent(c.sounds[SoundEventIndex(event)].customPath,audio/name),"Sound path changed");
            std::wstring error;
            Need(store.Save(c,error,2000),"Save failed: "+ToUtf8(error));
            auto restored=store.Load();
            for (std::size_t i=0;i<c.sounds.size();++i)
                Need(c.sounds[i].volume==restored.sounds[i].volume && c.sounds[i].customPath==restored.sounds[i].customPath
                    && c.sounds[i].enabled==restored.sounds[i].enabled,"Sound settings changed on reload");
            QFile saved(QString::fromStdWString(file.wstring())); Need(saved.open(QIODevice::ReadOnly),"Cannot reload saved JSON");
            auto object=QJsonDocument::fromJson(saved.readAll()).object();
            Need(object.contains("sounds"),"Missing sounds after save");
            for (auto field:{"customSoundOn","customSoundOff","macroSoundOn","macroSoundOff","soundVolume","macroSoundVolume"})
                Need(!object.contains(field),"Legacy field written again");
        },{{"group","config"},{"tags",QJsonArray{"config","sounds","migration"}}});
    }
}
void CheckK1() {
    for (int index=1;index<=9;++index) {
        auto id="K1-0"+std::to_string(index);
        Case(id,"Sequence release, repeat debounce, disabled segment and restart memory",[&] {
            auto p=Profile(id); Setup(p);
            Need(p.sequenceSwitchKey.has_value(),"No sequence key");
            auto key=*p.sequenceSwitchKey;
            auto first=p.bindings[0].id,second=p.bindings[1].id;
            Need(repeat->GetStatus().sequenceTaskId==first,"Wrong initial segment");
            for (int stroke=0;stroke<6;++stroke) {
                if (key.modifiers) Physical(K(VK_LSHIFT),true);
                Physical(key,true);
                auto expected=stroke%2?first:second;
                Need(repeat->GetStatus().sequenceTaskId==expected,"Sequence did not advance");
                for (int duplicate=0;duplicate<5;++duplicate) Physical(key,true);
                Need(repeat->GetStatus().sequenceTaskId==expected,"Repeated down advanced more than once");
                auto out=p.bindings[stroke%2?0:1].key;
                auto at=Mark();
                Need(Until([&]{return Count(out,true,at)>0;},400),"Selected sequence has no real output");
                if (key.modifiers && stroke%2) {
                    Physical(K(VK_LSHIFT),false);
                    Physical(K(key.code),false);
                } else {
                    Physical(key,false);
                    if (key.modifiers) Physical(K(VK_LSHIFT),false);
                }
            }
            Tap(key);
            Need(repeat->GetStatus().sequenceTaskId==second,"Extra complete stroke failed");
            repeat->SetToolRunning(false); repeat->SetToolRunning(true);
            Need(repeat->GetStatus().sequenceTaskId==(p.rememberSequencePosition?second:first),"Restart memory differs from config");
            Need(Count(K('3'),true)==0,"Disabled segment produced output");
        });
    }
}
void CheckK2() {
    for (int index:{1,2}) for (auto modifier:{VK_LSHIFT,VK_LCONTROL,VK_LMENU}) {
        auto scene="K2-0"+std::to_string(index);
        Case(scene+"-mod"+std::to_string(modifier),"Hold release after modifier snapshot changes",[&] {
            auto p=Profile(scene); Setup(p);
            if (index==2) Output(K('B'));
            Physical(K('A'),true); Output(K('A'));
            if (index==2) { Need(repeat->GetStatus().holdPriorityActive,"Hold priority absent"); Quiet(K('B')); }
            Physical(K(modifier),true);
            Physical(K('A',ModifierMaskFromKey(K(modifier))),false);
            Need(repeat->GetStatus().holdKeys.empty(),"Hold key remained selected after primary release");
            Need(!repeat->GetStatus().holdPriorityActive,"Hold priority remained active");
            Quiet(K('A'));
            Physical(K(modifier),false);
            if (index==2) Output(K('B'));
            Physical(K('A'),true); Output(K('A')); Physical(K('A'),false); Quiet(K('A'));
        });
    }
    for (bool modifierFirst:{false,true}) Case(std::string("K2-03-")+(modifierFirst?"modifier-first":"primary-first"),"Required combo component release",[&] {
        Setup(Profile("K2-03"));
        for (int cycle=0;cycle<3;++cycle) {
            Physical(K(VK_LSHIFT),true); Physical(K('A',kKeyModifierShift),true); Output(K('A'));
            Physical(modifierFirst?K(VK_LSHIFT):K('A',kKeyModifierShift),false);
            Need(repeat->GetStatus().holdKeys.empty(),"Combo hold not released"); Quiet(K('A'));
            Physical(modifierFirst?K('A'):K(VK_LSHIFT),false);
        }
    });
    for (int index:{4,5}) for (auto modifier:{VK_LSHIFT,VK_LCONTROL,VK_LMENU}) {
        auto scene="K2-0"+std::to_string(index);
        Case(scene+"-mod"+std::to_string(modifier),"Independent release, switch and accidental-stop protection",[&] {
            Setup(Profile(scene));
            Physical(K('A'),true); Output(K('A'));
            Physical(K(modifier),true); Physical(K('A',ModifierMaskFromKey(K(modifier))),false); Physical(K(modifier),false);
            Need(repeat->GetStatus().independentKey==K('A'),"Physical up lost independent selection");
            Tap(K('A'));
            if (index==4) { Need(!repeat->GetStatus().independentKey,"Next A swallowed instead of stopping"); Quiet(K('A')); Tap(K('A')); }
            else Need(repeat->GetStatus().independentKey==K('A'),"Accidental-stop protection changed selection");
            Output(K('A')); Tap(K('B'));
            Need(repeat->GetStatus().independentKey==K('B'),"Independent B switch failed");
            Quiet(K('A')); Output(K('B'));
        });
    }
    Case("K2-06","Mouse primary release after Shift changes",[&] {
        Setup(Profile("K2-06")); Physical(M(VK_RBUTTON),true); Output(M(VK_RBUTTON));
        Physical(K(VK_LSHIFT),true); Physical(M(VK_RBUTTON,kKeyModifierShift),false);
        Need(repeat->GetStatus().holdKeys.empty(),"Mouse hold remained selected"); Quiet(M(VK_RBUTTON));
        Physical(K(VK_LSHIFT),false);
    });
}
void CheckK3(bool actualHook,KeyDescriptor control,unsigned extra,bool requiredModifierFirst=false) {
    std::string id=std::string("K3-")+(actualHook?"real-hook-":"raw-signal-")
        +ToUtf8(KeyDescriptorToStorage(control))+"-"+std::to_string(extra)+(requiredModifierFirst?"-modifier-first":"-primary-first");
    if (actualHook && driverName == "recording") {
        Unverified(id, "Production global hook", "Requires an explicitly selected real driver; recording is not OS input");
        return;
    }
    Case(id,actualHook?"Production global hook with real driver events":"Production coordinator via public RawInputManager signal",[&] {
        auto p=Profile("K3-01"); p.controlHotkey=control; Setup(p,false);
        GlobalInputHook hook;
        RawInputManager raw;
        EngineHookCoordinator coordinator;
        int toggles=0;
        EngineHookCoordinator::Context context;
        context.hook=&hook;
        context.rawInput=actualHook?nullptr:&raw;
        context.repeaterEngine=repeat;
        context.macroEngine=macro;
        context.notePhysicalKey=[](const auto& k,bool down){router->NotePhysicalKey(k,down);};
        context.controlHotkey=[&]{return std::optional(control);};
        context.captureActive=[] {return false;};
        context.repeatRunning=[] {return repeat->IsToolRunning();};
        context.canUseDdDriver=[] {return true;};
        context.toggleRepeatRunning=[&] {
            ++toggles; repeat->SetToolRunning(!repeat->IsToolRunning());
            hook.setRepeatRunning(repeat->IsToolRunning());
        };
        coordinator.bind(std::move(context));
        coordinator.refreshState();
        if (actualHook) { Need(coordinator.start(),"Production hook registration failed"); Pump(350); }
        auto event=[&](KeyDescriptor key,bool down) {
            if (actualHook) {
                key.modifiers=0;
                Need(down?backend->SendDown(key):backend->SendUp(key),"Real control-key injection failed");
                Pump(20);
            } else if (key.device==InputDevice::Mouse) raw.mouseChanged(key,down);
            else raw.keyboardChanged(key,down);
        };
        try {
            for (int cycle=0;cycle<3;++cycle) {
                if (control.modifiers) event(K(VK_LCONTROL),true);
                event(control,true);
                Need(toggles==cycle*2+1 && repeat->IsToolRunning(),"First complete stroke did not start");
                for (int duplicate=0;duplicate<4;++duplicate) event(control,true);
                Need(toggles==cycle*2+1,"Repeated down toggled multiple times");
                if (!control.modifiers) event(K(extra),true);
                if (control.modifiers && requiredModifierFirst) {
                    event(K(VK_LCONTROL),false); event(K(control.code),false);
                } else {
                    auto up=control;
                    if (!control.modifiers) up.modifiers=ModifierMaskFromKey(K(extra));
                    event(up,false);
                    event(K(control.modifiers?VK_LCONTROL:extra),false);
                }
                if (control.modifiers) event(K(VK_LCONTROL),true);
                event(control,true);
                Need(toggles==cycle*2+2 && !repeat->IsToolRunning(),"Second complete stroke was swallowed");
                event(control,false);
                if (control.modifiers) event(K(VK_LCONTROL),false);
            }
        } catch (...) { hook.stop(); throw; }
        hook.stop();
        Pump(25);
    });
}
void CheckAllK3() {
    for (bool real:{false,true}) {
        for (auto code:{VK_LSHIFT,VK_LCONTROL,VK_LMENU}) {
            CheckK3(real,M(VK_MBUTTON),code);
            CheckK3(real,K(VK_F8),code);
        }
        CheckK3(real,K(VK_F8,kKeyModifierCtrl),VK_LCONTROL,false);
        CheckK3(real,K(VK_F8,kKeyModifierCtrl),VK_LCONTROL,true);
    }
}
void CheckK4() {
    for (int index=1;index<=6;++index) {
        auto id="K4-0"+std::to_string(index);
        Case(id,"Standalone held modifier produces alternating real driver pulses",[&] {
            auto p=Profile(id); Setup(p);
            auto key=p.bindings[0].key;
            auto at=Mark(); Physical(key,true); Output(key,530,3);
            {
                std::lock_guard lock(captureMutex);
                bool lastDown=false;
                for (const auto& e:events) if (e.scenario==current && e.us>=at && SamePrimaryKey(e.key,key)) {
                    Need(e.down!=lastDown,"Modifier output is not alternating down/up"); lastDown=e.down;
                }
            }
            Physical(key,false);
            Need(repeat->GetStatus().holdKeys.empty(),"Modifier physical record not cleared"); Quiet(key);
            Physical(key,true); Output(key,360,2); Physical(key,false); Quiet(key);
        });
    }
}
void CheckK5() {
    for (auto scene:{"K5-01","K5-02","K5-03"}) for (bool macroFirst:{false,true}) {
        Case(std::string(scene)+(macroFirst?"-macro-first":"-repeat-first"),"Ownership retained through pause/resume and transferred on stop",[&] {
            auto p=Profile(scene); Setup(p,false);
            auto m=Macro("K5-M");
            if (macroFirst) { StartMacro(m); Pump(30); }
            repeat->SetToolRunning(true);
            if (std::string(scene)=="K5-02") Tap(K('A'));
            if (!macroFirst) { Pump(30); StartMacro(m); }
            Sources(macroFirst?2:1);
            Physical(K('P'),true);
            Need(repeat->GetStatus().paused,"Pause status absent");
            Sources(macroFirst?2:0);
            Physical(K('P'),false);
            Sources(macroFirst?2:1);
            if (macroFirst) { macro->StopAll(); Sources(1,450,false); }
            else { repeat->SetToolRunning(false); Sources(2); }
        });
    }
    for (auto scene:{"K5-02","K5-03"}) Case(std::string(scene)+"-deselect","Source registration released by independent deselect / segment change",[&] {
        Setup(Profile(scene));
        if (std::string(scene)=="K5-02") Tap(K('A'));
        StartMacro(Macro("K5-M")); Sources(1);
        Tap(K(std::string(scene)=="K5-02"?'D':'R'));
        Sources(2); Output(K('D'));
    });
    Case("K5-04","Hold priority does not lose earlier ownership",[&] {
        Setup(Profile("K5-04")); StartMacro(Macro("K5-M")); Sources(1);
        for (int cycle=0;cycle<2;++cycle) {
            Physical(K('C'),true); Need(repeat->GetStatus().holdPriorityActive,"Priority missing");
            Sources(0); Need(Count(K('C'),true)>0,"Priority key has no output");
            Physical(K('C'),false); Sources(1);
        }
    });
    for (bool macroFirst:{false,true}) Case(std::string("K5-05-")+(macroFirst?"macro-first":"repeat-first"),"Direct SetSuspended gate preserves ownership, not a real foreground test",[&] {
        Setup(Profile("K5-05"),false);
        if (macroFirst) StartMacro(Macro("K5-M"));
        repeat->SetToolRunning(true);
        if (!macroFirst) StartMacro(Macro("K5-M"));
        Sources(macroFirst?2:1);
        repeat->SetSuspended(true); macro->SetSuspended(true);
        Sources(0,400,false);
        repeat->SetSuspended(false); macro->SetSuspended(false);
        Sources(macroFirst?2:1);
    });
    Case("K5-N","Different primary keys run concurrently",[&] {
        Setup(Profile("K5-01")); StartMacro(Macro("K5-N"));
        auto at=Mark(); Pump(1000);
        Need(Count(K('A'),true,at)>=3 && Count(K('C'),true,at)>0 && Count(K('D'),true,at)>0,"Independent sources failed to run concurrently");
    });
}
void CheckRelated() {
    Case("G1","Combo prefix remains held while primary pulses",[&] {
        Setup(Profile("G1")); Output(K('A'),600,3);
        Need(Count(K(VK_LSHIFT),true)==1 && Count(K(VK_LSHIFT),false)==0,"Combo prefix was pulsed instead of held");
        Physical(K('P'),true); Quiet(K('A'));
        Need(Count(K(VK_LSHIFT),false)==1,"Pause did not release combo prefix");
        Physical(K('P'),false); Output(K('A'));
        Need(Count(K(VK_LSHIFT),true)==2,"Combo prefix not restored");
    });
    Case("G2","Long-press modifier holds, pauses, suspends and releases",[&] {
        Setup(Profile("G2")); Pump(350);
        Need(Count(K(VK_LCONTROL),true)==1 && Count(K(VK_LCONTROL),false)==0,"Long-press modifier did not remain down");
        Physical(K('P'),true); Pump(40);
        Need(Count(K(VK_LCONTROL),false)==1,"Pause did not release long press");
        Physical(K('P'),false); Pump(100);
        Need(Count(K(VK_LCONTROL),true)==2,"Long press did not resume");
        repeat->SetSuspended(true); Pump(40);
        Need(Count(K(VK_LCONTROL),false)==2,"Suspension did not release long press");
        repeat->SetSuspended(false); Pump(100);
        Need(Count(K(VK_LCONTROL),true)==3,"Long press did not restore after suspension");
    });
    Case("G3","Ending one shared combo does not release the other prefix",[&] {
        auto p=Profile("G3"); Setup(p); Pump(150);
        Need(Count(K('1'),true)==1 && Count(K('2'),true)==1 && Count(K(VK_LCONTROL),true)==1,"Shared long-press outputs missing");
        p.bindings[0].enabled=false;
        repeat->UpdateBindings(p.id,p.bindings,p.rememberSequencePosition); Pump(80);
        Need(Count(K('1'),false)==1 && Count(K('2'),false)==0 && Count(K(VK_LCONTROL),false)==0,"Ending first key broke remaining shared prefix");
        Physical(K('P'),true); Pump(40);
        Need(Count(K('2'),false)==1 && Count(K(VK_LCONTROL),false)==1,"Pause did not release remaining combo");
        Physical(K('P'),false); Pump(100);
        Need(Count(K('2'),true)==2 && Count(K(VK_LCONTROL),true)==2,"Remaining combo did not resume");
    });
    Case("G4","Multiple holds and toggle run concurrently without priority",[&] {
        Setup(Profile("G4")); Physical(K('A'),true); Physical(K('B'),true);
        auto at=Mark(); Pump(500);
        for (auto key:{'A','B','C'}) Need(Count(K(key),true,at)>=2,"Concurrent key missing");
        Physical(K('A'),false); Quiet(K('A')); Output(K('B'));
        Physical(K('P'),true); Quiet(K('C')); Output(K('B'));
        Physical(K('P'),false); Output(K('C')); Physical(K('B'),false);
    });
    Case("G5","Participating and excluded hold-priority items",[&] {
        Setup(Profile("G5")); Physical(K('B'),true); Output(K('C'));
        Need(!repeat->GetStatus().holdPriorityActive,"Excluded hold incorrectly pauses toggle");
        Physical(K('A'),true); Need(repeat->GetStatus().holdPriorityActive,"Participating hold does not pause toggle");
        Quiet(K('C')); Output(K('A')); Output(K('B'));
        Physical(K('A'),false); Need(!repeat->GetStatus().holdPriorityActive,"Priority did not clear while excluded B remains held");
        Output(K('C')); Output(K('B')); Physical(K('B'),false);
    });
    Unverified("G6","Martial-assist timing and game behavior","Requires the actual target game; not inferred from engine-level results");
}
void CheckMacros() {
    Case("M1","Finite three loops, repeat-trigger debounce and pause/resume",[&] {
        auto m=Macro("M1"); StartMacro(m);
        Need(Until([&]{return Count(K('1'),false)>0;}),"First finite click missing");
        macro->OnTriggerDown(K(VK_F9));
        for (int i=0;i<8;++i) macro->OnTriggerDown(K(VK_F9));
        Need(!macro->HasActiveMacro(),"A new complete trigger should stop toggle macro");
        macro->OnTriggerUp(K(VK_F9));
        auto at=Mark();
        macro->OnTriggerDown(K(VK_F9));
        for (int i=0;i<8;++i) macro->OnTriggerDown(K(VK_F9));
        Need(macro->HasActiveMacro(),"Auto-repeat incorrectly stopped finite macro");
        macro->OnTriggerUp(K(VK_F9)); Pump(50);
        macro->OnTriggerDown(K('P'));
        Need(macro->GetStatus(m.id).paused,"Macro did not pause");
        auto paused=Count(K('1'),true,at); Pump(550);
        Need(Count(K('1'),true,at)==paused,"Macro output during pause");
        macro->OnTriggerUp(K('P'));
        Need(Until([&]{return !macro->HasActiveMacro();},2500),"Finite macro did not finish");
        Need(Count(K('1'),true,at)==3 && Count(K('1'),false,at)==3,"Finite macro emitted wrong click count");
    });
    for (auto modifier:{VK_LSHIFT,VK_LCONTROL,VK_LMENU}) Case("M2-mod"+std::to_string(modifier),"Hold-trigger primary up with extra modifier",[&] {
        auto m=Macro("M2"); StartMacro(m); Output(K('1'));
        macro->OnTriggerDown(K(modifier));
        macro->OnTriggerUp(K(VK_F9,ModifierMaskFromKey(K(modifier))));
        Need(!macro->HasActiveMacro(),"Hold trigger did not end on primary up"); Quiet(K('1'));
        macro->OnTriggerUp(K(modifier));
        macro->OnTriggerDown(K(VK_F9)); Need(macro->HasActiveMacro(),"Hold trigger restart swallowed");
        Output(K('1')); macro->OnTriggerUp(K(VK_F9));
    });
    for (bool modifierFirst:{false,true}) Case(std::string("M3-")+(modifierFirst?"modifier-first":"primary-first"),"Combo hold trigger releases on either required component",[&] {
        auto m=Macro("M3"); macro->UpdateMacros({m});
        for (int cycle=0;cycle<2;++cycle) {
            macro->OnTriggerDown(K(VK_LCONTROL)); macro->OnTriggerDown(K(VK_F9,kKeyModifierCtrl));
            Need(macro->HasActiveMacro(),"Combo macro did not start"); Output(K('1'));
            macro->OnTriggerUp(modifierFirst?K(VK_LCONTROL):K(VK_F9,kKeyModifierCtrl));
            Need(!macro->HasActiveMacro(),"Combo hold did not stop on required release"); Quiet(K('1'));
            macro->OnTriggerUp(modifierFirst?K(VK_F9):K(VK_LCONTROL));
        }
    });
    Case("M4-all-actions","All real-output actions with hold/wait pause and final release",[&] {
        auto m=Macro("M4"); m.loopCount=1; StartMacro(m);
        Need(Until([&]{return macro->GetStatus(m.id).currentStep==1;},1000),"Hold step not reached");
        Pump(60); macro->OnTriggerDown(K('P')); Pump(150);
        Need(macro->GetStatus(m.id).paused && macro->GetStatus(m.id).currentStep==1,"Hold pause lost step");
        Need(Count(K('2'),false)==1,"Hold pause did not release key");
        macro->OnTriggerUp(K('P'));
        Need(Until([&]{return Count(K('2'),true)==2;}),"Hold did not resume");
        Need(Until([&]{return macro->GetStatus(m.id).currentStep==5;},2200),"Wait step not reached");
        macro->OnTriggerDown(K('P')); Pump(200);
        Need(macro->GetStatus(m.id).paused && macro->GetStatus(m.id).currentStep==5,"Wait pause lost step");
        macro->OnTriggerUp(K('P'));
        Need(Until([&]{return !macro->HasActiveMacro();},3000),"Action macro did not finish");
        Need(Count(K('1'),true)==1 && Count(K('3'),true)==1,"Click replayed or missing");
        Need(Count(K(VK_LCONTROL),true)==1 && Count(K(VK_LCONTROL),false)==1,"Down/up action imbalance");
        Need(Wheel(120)==1 && Wheel(-120)==1,"Real wheel output missing or duplicated");
    });
    Case("M4-stop-held","Stopping macro releases currently held output",[&] {
        auto m=Macro("M4"); StartMacro(m);
        Need(Until([&]{return Count(K('2'),true)>0;},1000),"Hold action not reached");
        macro->StopAll(); Pump(30);
        Need(!macro->HasActiveMacro() && Count(K('2'),false)==1,"Stop leaked held action"); Quiet(K('2'));
    });
}
}

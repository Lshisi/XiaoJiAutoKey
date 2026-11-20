#include "SoundSettings.h"

#include <QCoreApplication>

namespace {
constexpr std::array<SoundEventDefinition, kSoundEventCount> kDefinitions{{
    {SoundEvent::RepeatOn, "repeatOn", QT_TRANSLATE_NOOP("SoundSettings", "按键开启"),
        QT_TRANSLATE_NOOP("SoundSettings", "启动按键连发工具时提示。"), L"按键开启.wav", SoundGroup::Repeat, 80},
    {SoundEvent::RepeatOff, "repeatOff", QT_TRANSLATE_NOOP("SoundSettings", "按键关闭"),
        QT_TRANSLATE_NOOP("SoundSettings", "停止按键连发工具时提示。"), L"按键关闭.wav", SoundGroup::Repeat, 80},
    {SoundEvent::IndependentOn, "independentOn", QT_TRANSLATE_NOOP("SoundSettings", "独立开启"),
        QT_TRANSLATE_NOOP("SoundSettings", "首次选中一个独立模式按键时提示。"), L"独立开启.wav", SoundGroup::Repeat, 65},
    {SoundEvent::IndependentOff, "independentOff", QT_TRANSLATE_NOOP("SoundSettings", "独立关闭"),
        QT_TRANSLATE_NOOP("SoundSettings", "再次按当前独立键，停止该键连发时提示；开启防误停后不触发。"), L"独立关闭.wav", SoundGroup::Repeat, 65},
    {SoundEvent::IndependentSwitch, "independentSwitch", QT_TRANSLATE_NOOP("SoundSettings", "独立换键"),
        QT_TRANSLATE_NOOP("SoundSettings", "从一个独立键切换到另一个独立键时提示。"), L"按键切换.wav", SoundGroup::Repeat, 60},
    {SoundEvent::SequenceSwitch, "sequenceSwitch", QT_TRANSLATE_NOOP("SoundSettings", "切换下一段"),
        QT_TRANSLATE_NOOP("SoundSettings", "按切换键进入另一个可用段时提示；当前段未改变时不提示。"), L"按键切换.wav", SoundGroup::Repeat, 60},
    {SoundEvent::RepeatPause, "repeatPause", QT_TRANSLATE_NOOP("SoundSettings", "按键暂停"),
        QT_TRANSLATE_NOOP("SoundSettings", "运行中按住暂停键时提示；多个暂停键只在首次进入暂停时提示。"), L"按键暂停.wav", SoundGroup::Repeat, 60},
    {SoundEvent::RepeatResume, "repeatResume", QT_TRANSLATE_NOOP("SoundSettings", "按键恢复"),
        QT_TRANSLATE_NOOP("SoundSettings", "松开最后一个暂停键，解除按键暂停时提示。"), L"按键恢复.wav", SoundGroup::Repeat, 60},
    {SoundEvent::MacroOn, "macroOn", QT_TRANSLATE_NOOP("SoundSettings", "循环开启"),
        QT_TRANSLATE_NOOP("SoundSettings", "循环连发成功开始运行时提示。"), L"循环开启.wav", SoundGroup::Macro, 80},
    {SoundEvent::MacroOff, "macroOff", QT_TRANSLATE_NOOP("SoundSettings", "循环关闭"),
        QT_TRANSLATE_NOOP("SoundSettings", "循环连发停止时提示，沿用现有关闭提示。"), L"循环关闭.wav", SoundGroup::Macro, 80},
    {SoundEvent::MacroPause, "macroPause", QT_TRANSLATE_NOOP("SoundSettings", "循环暂停"),
        QT_TRANSLATE_NOOP("SoundSettings", "循环运行中按住暂停键时提示。"), L"循环暂停.wav", SoundGroup::Macro, 60},
    {SoundEvent::MacroResume, "macroResume", QT_TRANSLATE_NOOP("SoundSettings", "循环恢复"),
        QT_TRANSLATE_NOOP("SoundSettings", "松开最后一个循环暂停键，解除暂停时提示。"), L"循环恢复.wav", SoundGroup::Macro, 60},
    {SoundEvent::RecordingOn, "recordingOn", QT_TRANSLATE_NOOP("SoundSettings", "录制开始"),
        QT_TRANSLATE_NOOP("SoundSettings", "倒计时结束，正式开始录制输入时提示。"), L"录制开始.wav", SoundGroup::Recording, 80},
    {SoundEvent::RecordingOff, "recordingOff", QT_TRANSLATE_NOOP("SoundSettings", "录制结束"),
        QT_TRANSLATE_NOOP("SoundSettings", "停止录制、达到录制容量或取消正在进行的录制时提示。"), L"录制结束.wav", SoundGroup::Recording, 80},
}};
}

const std::array<SoundEventDefinition, kSoundEventCount>& SoundEventDefinitions() { return kDefinitions; }

const SoundEventDefinition& SoundDefinition(SoundEvent event) { return kDefinitions[SoundEventIndex(event)]; }

const SoundEventDefinition* FindSoundDefinition(std::string_view id) {
    for (const auto& definition : kDefinitions) {
        if (id == definition.id) return &definition;
    }
    return nullptr;
}

SoundSettings DefaultSoundSettings() {
    SoundSettings settings;
    for (const auto& definition : kDefinitions) {
        settings[SoundEventIndex(definition.event)].volume = definition.defaultVolume;
    }
    return settings;
}

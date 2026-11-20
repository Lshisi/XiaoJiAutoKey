#include "SoundSettings.h"

#include <QCoreApplication>

namespace {
constexpr std::array<SoundEventDefinition, kSoundEventCount> kDefinitions{{
    {SoundEvent::RepeatOn, "repeatOn", QT_TRANSLATE_NOOP("SoundSettings", "按键开启"),
        QT_TRANSLATE_NOOP("SoundSettings", "点击“启动”或按启停快捷键，开始按键连发时播放。"), L"按键开启.wav", SoundGroup::Repeat, 80},
    {SoundEvent::RepeatOff, "repeatOff", QT_TRANSLATE_NOOP("SoundSettings", "按键关闭"),
        QT_TRANSLATE_NOOP("SoundSettings", "点击“停止”或按启停快捷键，结束按键连发时播放。"), L"按键关闭.wav", SoundGroup::Repeat, 80},
    {SoundEvent::IndependentOn, "independentOn", QT_TRANSLATE_NOOP("SoundSettings", "独立开启"),
        QT_TRANSLATE_NOOP("SoundSettings", "先启动工具，再按一下独立模式的按键，开始连点时播放。"), L"独立开启.wav", SoundGroup::Repeat, 65},
    {SoundEvent::IndependentOff, "independentOff", QT_TRANSLATE_NOOP("SoundSettings", "独立关闭"),
        QT_TRANSLATE_NOOP("SoundSettings", "再按一下正在连点的独立按键，让它停止时播放。开启“防误停”后，再按同一个键不会停止，也就不会播放这项声音。"), L"独立关闭.wav", SoundGroup::Repeat, 65},
    {SoundEvent::IndependentSwitch, "independentSwitch", QT_TRANSLATE_NOOP("SoundSettings", "独立换键"),
        QT_TRANSLATE_NOOP("SoundSettings", "独立模式下，例如正在连点 1，按 2 改为连点 2 时播放。默认与“切换模式换键”声音相同，可以分别设置。"), L"按键切换.wav", SoundGroup::Repeat, 60},
    {SoundEvent::SequenceSwitch, "sequenceSwitch", QT_TRANSLATE_NOOP("SoundSettings", "切换模式换键"),
        QT_TRANSLATE_NOOP("SoundSettings", "切换模式下，按列表下方的“切换键”换到下一个按键时播放。默认与“独立换键”声音相同，可以分别设置。"), L"按键切换.wav", SoundGroup::Repeat, 60},
    {SoundEvent::RepeatPause, "repeatPause", QT_TRANSLATE_NOOP("SoundSettings", "按键暂停"),
        QT_TRANSLATE_NOOP("SoundSettings", "运行时按住“暂停键”，暂时停下按键连发时播放。"), L"按键暂停.wav", SoundGroup::Repeat, 60},
    {SoundEvent::RepeatResume, "repeatResume", QT_TRANSLATE_NOOP("SoundSettings", "按键恢复"),
        QT_TRANSLATE_NOOP("SoundSettings", "松开“暂停键”，继续按键连发时播放。按住了多个暂停键时，要全部松开才会继续。"), L"按键恢复.wav", SoundGroup::Repeat, 60},
    {SoundEvent::MacroOn, "macroOn", QT_TRANSLATE_NOOP("SoundSettings", "循环开启"),
        QT_TRANSLATE_NOOP("SoundSettings", "开始执行循环列表里的按键时播放。"), L"循环开启.wav", SoundGroup::Macro, 80},
    {SoundEvent::MacroOff, "macroOff", QT_TRANSLATE_NOOP("SoundSettings", "循环关闭"),
        QT_TRANSLATE_NOOP("SoundSettings", "停止循环，或设定的循环次数全部完成时播放。"), L"循环关闭.wav", SoundGroup::Macro, 80},
    {SoundEvent::MacroPause, "macroPause", QT_TRANSLATE_NOOP("SoundSettings", "循环暂停"),
        QT_TRANSLATE_NOOP("SoundSettings", "循环运行时按住“暂停键”，暂时停下来时播放。"), L"循环暂停.wav", SoundGroup::Macro, 60},
    {SoundEvent::MacroResume, "macroResume", QT_TRANSLATE_NOOP("SoundSettings", "循环恢复"),
        QT_TRANSLATE_NOOP("SoundSettings", "松开循环的“暂停键”，继续执行时播放。按住了多个暂停键时，要全部松开才会继续。"), L"循环恢复.wav", SoundGroup::Macro, 60},
    {SoundEvent::RecordingOn, "recordingOn", QT_TRANSLATE_NOOP("SoundSettings", "录制开始"),
        QT_TRANSLATE_NOOP("SoundSettings", "准备时间结束，真正开始记录你的键盘和鼠标操作时播放。"), L"录制开始.wav", SoundGroup::Recording, 80},
    {SoundEvent::RecordingOff, "recordingOff", QT_TRANSLATE_NOOP("SoundSettings", "录制结束"),
        QT_TRANSLATE_NOOP("SoundSettings", "结束正在进行的录制时播放，例如按 F10 或点击“停止录制”。"), L"录制结束.wav", SoundGroup::Recording, 80},
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

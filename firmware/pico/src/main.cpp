#include "bsp/board.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kVersion = "xiaoji-pico-cpp-0.1";
constexpr uint32_t kNumpadEnterKeyCode = 0x0100000D;

enum class Device : uint32_t {
    Keyboard = 0,
    Mouse = 1,
};

struct HidKey {
    uint8_t modifier = 0;
    uint8_t keycode = 0;
};

uint8_t keyboard_modifiers = 0;
std::array<uint8_t, 6> keyboard_keys{};
uint8_t mouse_buttons = 0;
uint16_t consumer_usage = 0;

char command_buffer[96]{};
uint8_t command_length = 0;

void cdc_write_line(const char* text) {
    tud_cdc_write_str(text);
    tud_cdc_write_str("\r\n");
    tud_cdc_write_flush();
}

bool is_modifier(uint8_t keycode) {
    return keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT;
}

uint8_t modifier_bit(uint8_t keycode) {
    return static_cast<uint8_t>(1u << (keycode - HID_KEY_CONTROL_LEFT));
}

bool contains_key(uint8_t keycode) {
    for (const auto key : keyboard_keys) {
        if (key == keycode) {
            return true;
        }
    }
    return false;
}

void add_key(uint8_t keycode) {
    if (keycode == 0 || contains_key(keycode)) {
        return;
    }
    for (auto& key : keyboard_keys) {
        if (key == 0) {
            key = keycode;
            return;
        }
    }
}

bool can_add_key(uint8_t keycode) {
    if (keycode == 0 || contains_key(keycode)) {
        return true;
    }
    for (const auto key : keyboard_keys) {
        if (key == 0) {
            return true;
        }
    }
    return false;
}

void remove_key(uint8_t keycode) {
    for (auto& key : keyboard_keys) {
        if (key == keycode) {
            key = 0;
        }
    }
}

bool send_keyboard_report() {
    if (!tud_hid_n_ready(0)) {
        return false;
    }
    return tud_hid_n_keyboard_report(0, 1, keyboard_modifiers, keyboard_keys.data());
}

bool send_mouse_report() {
    if (!tud_hid_n_ready(1)) {
        return false;
    }
    return tud_hid_n_mouse_report(1, 2, mouse_buttons, 0, 0, 0, 0);
}

bool send_consumer_report() {
    if (!tud_hid_n_ready(2)) {
        return false;
    }
    return tud_hid_n_report(2, 3, &consumer_usage, sizeof(consumer_usage));
}

void release_all() {
    keyboard_modifiers = 0;
    keyboard_keys.fill(0);
    mouse_buttons = 0;
    consumer_usage = 0;
    send_keyboard_report();
    send_mouse_report();
    send_consumer_report();
    board_led_write(false);
}

HidKey vk_to_hid(uint32_t vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return {0, static_cast<uint8_t>(HID_KEY_A + (vk - 'A'))};
    }
    if (vk >= '0' && vk <= '9') {
        static constexpr uint8_t digits[] = {
            HID_KEY_0, HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_4,
            HID_KEY_5, HID_KEY_6, HID_KEY_7, HID_KEY_8, HID_KEY_9
        };
        return {0, digits[vk - '0']};
    }
    if (vk >= 0x70 && vk <= 0x87) {
        return {0, static_cast<uint8_t>(HID_KEY_F1 + (vk - 0x70))};
    }
    if (vk >= 0x60 && vk <= 0x69) {
        return {0, static_cast<uint8_t>(HID_KEY_KEYPAD_0 + (vk - 0x60))};
    }

    switch (vk) {
    case 0x03: return {0, HID_KEY_CANCEL};
    case 0x08: return {0, HID_KEY_BACKSPACE};
    case 0x09: return {0, HID_KEY_TAB};
    case 0x0C: return {0, HID_KEY_CLEAR};
    case 0x0D: return {0, HID_KEY_ENTER};
    case 0x10:
    case 0xA0: return {0, HID_KEY_SHIFT_LEFT};
    case 0xA1: return {0, HID_KEY_SHIFT_RIGHT};
    case 0x11:
    case 0xA2: return {0, HID_KEY_CONTROL_LEFT};
    case 0xA3: return {0, HID_KEY_CONTROL_RIGHT};
    case 0x12:
    case 0xA4: return {0, HID_KEY_ALT_LEFT};
    case 0xA5: return {0, HID_KEY_ALT_RIGHT};
    case 0x1B: return {0, HID_KEY_ESCAPE};
    case 0x15: return {0, HID_KEY_LANG1};
    case 0x19: return {0, HID_KEY_LANG2};
    case 0x1C: return {0, HID_KEY_KANJI1};
    case 0x1D: return {0, HID_KEY_KANJI2};
    case 0x1E: return {0, HID_KEY_KANJI3};
    case 0x1F: return {0, HID_KEY_KANJI4};
    case 0x20: return {0, HID_KEY_SPACE};
    case 0x21: return {0, HID_KEY_PAGE_UP};
    case 0x22: return {0, HID_KEY_PAGE_DOWN};
    case 0x23: return {0, HID_KEY_END};
    case 0x24: return {0, HID_KEY_HOME};
    case 0x25: return {0, HID_KEY_ARROW_LEFT};
    case 0x26: return {0, HID_KEY_ARROW_UP};
    case 0x27: return {0, HID_KEY_ARROW_RIGHT};
    case 0x28: return {0, HID_KEY_ARROW_DOWN};
    case 0x29: return {0, HID_KEY_SELECT};
    case 0x2A: return {0, HID_KEY_PRINT_SCREEN};
    case 0x2B: return {0, HID_KEY_EXECUTE};
    case 0x2C: return {0, HID_KEY_PRINT_SCREEN};
    case 0x2D: return {0, HID_KEY_INSERT};
    case 0x2E: return {0, HID_KEY_DELETE};
    case 0x2F: return {0, HID_KEY_HELP};
    case 0x5B: return {0, HID_KEY_GUI_LEFT};
    case 0x5C: return {0, HID_KEY_GUI_RIGHT};
    case 0x5D: return {0, HID_KEY_APPLICATION};
    case 0x5F: return {0, HID_KEY_POWER};
    case 0x6A: return {0, HID_KEY_KEYPAD_MULTIPLY};
    case 0x6B: return {0, HID_KEY_KEYPAD_ADD};
    case 0x6C: return {0, HID_KEY_SEPARATOR};
    case 0x6D: return {0, HID_KEY_KEYPAD_SUBTRACT};
    case 0x6E: return {0, HID_KEY_KEYPAD_DECIMAL};
    case 0x6F: return {0, HID_KEY_KEYPAD_DIVIDE};
    case 0x90: return {0, HID_KEY_NUM_LOCK};
    case 0x91: return {0, HID_KEY_SCROLL_LOCK};
    case 0x13: return {0, HID_KEY_PAUSE};
    case 0x14: return {0, HID_KEY_CAPS_LOCK};
    case 0xBA: return {0, HID_KEY_SEMICOLON};
    case 0xBB: return {0, HID_KEY_EQUAL};
    case 0xBC: return {0, HID_KEY_COMMA};
    case 0xBD: return {0, HID_KEY_MINUS};
    case 0xBE: return {0, HID_KEY_PERIOD};
    case 0xBF: return {0, HID_KEY_SLASH};
    case 0xC0: return {0, HID_KEY_GRAVE};
    case 0xDB: return {0, HID_KEY_BRACKET_LEFT};
    case 0xDC: return {0, HID_KEY_BACKSLASH};
    case 0xDD: return {0, HID_KEY_BRACKET_RIGHT};
    case 0xDE: return {0, HID_KEY_APOSTROPHE};
    case 0xDF: return {0, HID_KEY_EUROPE_2};
    case 0xE1: return {0, HID_KEY_KANJI1};
    case 0xE2: return {0, HID_KEY_EUROPE_1};
    case 0xE3: return {0, HID_KEY_HELP};
    case 0xE4: return {0, HID_KEY_KEYPAD_00};
    case 0xF6: return {0, HID_KEY_SYSREQ_ATTENTION};
    case 0xF7: return {0, HID_KEY_CRSEL_PROPS};
    case 0xF8: return {0, HID_KEY_EXSEL};
    case 0xF9: return {0, HID_KEY_OUT};
    case 0xFA: return {0, HID_KEY_AGAIN};
    case 0xFB: return {0, HID_KEY_OPER};
    case 0xFD: return {0, HID_KEY_PRIOR};
    case 0xFE: return {0, HID_KEY_CLEAR};
    case kNumpadEnterKeyCode: return {0, HID_KEY_KEYPAD_ENTER};
    default: return {};
    }
}

uint16_t vk_to_consumer(uint32_t vk) {
    switch (vk) {
    case 0x5F: return HID_USAGE_CONSUMER_SLEEP;
    case 0xA6: return HID_USAGE_CONSUMER_AC_BACK;
    case 0xA7: return HID_USAGE_CONSUMER_AC_FORWARD;
    case 0xA8: return HID_USAGE_CONSUMER_AC_REFRESH;
    case 0xA9: return HID_USAGE_CONSUMER_AC_STOP;
    case 0xAA: return HID_USAGE_CONSUMER_AC_SEARCH;
    case 0xAB: return HID_USAGE_CONSUMER_AC_BOOKMARKS;
    case 0xAC: return HID_USAGE_CONSUMER_AC_HOME;
    case 0xAD: return HID_USAGE_CONSUMER_MUTE;
    case 0xAE: return HID_USAGE_CONSUMER_VOLUME_DECREMENT;
    case 0xAF: return HID_USAGE_CONSUMER_VOLUME_INCREMENT;
    case 0xB0: return HID_USAGE_CONSUMER_SCAN_NEXT_TRACK;
    case 0xB1: return HID_USAGE_CONSUMER_SCAN_PREVIOUS_TRACK;
    case 0xB2: return HID_USAGE_CONSUMER_STOP;
    case 0xB3: return HID_USAGE_CONSUMER_PLAY_PAUSE;
    case 0xB4: return HID_USAGE_CONSUMER_AL_EMAIL_READER;
    case 0xB5: return HID_USAGE_CONSUMER_AL_AUDIO_PLAYER;
    case 0xB6: return HID_USAGE_CONSUMER_AL_CALCULATOR;
    case 0xB7: return HID_USAGE_CONSUMER_AL_LOCAL_MACHINE_BROWSER;
    default: return 0;
    }
}

uint8_t mouse_vk_to_button(uint32_t vk) {
    switch (vk) {
    case 0x01: return MOUSE_BUTTON_LEFT;
    case 0x02: return MOUSE_BUTTON_RIGHT;
    case 0x04: return MOUSE_BUTTON_MIDDLE;
    case 0x05: return MOUSE_BUTTON_BACKWARD;
    case 0x06: return MOUSE_BUTTON_FORWARD;
    default: return 0;
    }
}

bool apply_keyboard(uint32_t vk, bool down) {
    const HidKey hid = vk_to_hid(vk);
    if (hid.keycode != 0) {
        if (!is_modifier(hid.keycode) && down && !can_add_key(hid.keycode)) {
            cdc_write_line("ERR KEY_ROLLOVER_FULL");
            return false;
        }
        if (is_modifier(hid.keycode)) {
            if (down) {
                keyboard_modifiers = static_cast<uint8_t>(keyboard_modifiers | modifier_bit(hid.keycode));
            } else {
                keyboard_modifiers = static_cast<uint8_t>(keyboard_modifiers & ~modifier_bit(hid.keycode));
            }
        } else if (down) {
            add_key(hid.keycode);
        } else {
            remove_key(hid.keycode);
        }
        board_led_write(down);
        return send_keyboard_report();
    }

    const uint16_t usage = vk_to_consumer(vk);
    if (usage == 0) {
        return false;
    }
    if (down) {
        consumer_usage = usage;
    } else if (consumer_usage == usage) {
        consumer_usage = 0;
    } else {
        consumer_usage = 0;
    }
    board_led_write(down);
    return send_consumer_report();
}

bool apply_mouse(uint32_t vk, bool down) {
    const uint8_t button = mouse_vk_to_button(vk);
    if (button == 0) {
        return false;
    }
    if (down) {
        mouse_buttons = static_cast<uint8_t>(mouse_buttons | button);
    } else {
        mouse_buttons = static_cast<uint8_t>(mouse_buttons & ~button);
    }
    board_led_write(down);
    return send_mouse_report();
}

char* next_token(char*& cursor) {
    while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (!*cursor) {
        return nullptr;
    }
    char* start = cursor;
    while (*cursor && !std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor) {
        *cursor++ = '\0';
    }
    return start;
}

bool parse_u32(const char* token, uint32_t* value) {
    if (!token || !value) {
        return false;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(token, &end, 0);
    if (end == token || *end != '\0') {
        return false;
    }
    *value = static_cast<uint32_t>(parsed);
    return true;
}

void handle_command(char* line) {
    char* cursor = line;
    char* command = next_token(cursor);
    if (!command) {
        return;
    }

    for (char* p = command; *p; ++p) {
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    }

    if (std::strcmp(command, "PING") == 0) {
        cdc_write_line("OK PONG");
        return;
    }
    if (std::strcmp(command, "VER") == 0) {
        tud_cdc_write_str("OK ");
        cdc_write_line(kVersion);
        return;
    }
    if (std::strcmp(command, "R") == 0) {
        release_all();
        cdc_write_line("OK RELEASE_ALL");
        return;
    }
    if (std::strcmp(command, "B") == 0) {
        release_all();
        cdc_write_line("OK BOOTSEL");
        sleep_ms(50);
        reset_usb_boot(0, 0);
        return;
    }

    const bool down = std::strcmp(command, "D") == 0;
    const bool up = std::strcmp(command, "U") == 0;
    if (!down && !up) {
        cdc_write_line("ERR UNKNOWN_COMMAND");
        return;
    }

    uint32_t device = 0;
    uint32_t code = 0;
    if (!parse_u32(next_token(cursor), &device) ||
        !parse_u32(next_token(cursor), &code)) {
        cdc_write_line("ERR BAD_ARGUMENTS");
        return;
    }

    const bool ok = device == static_cast<uint32_t>(Device::Keyboard)
        ? apply_keyboard(code, down)
        : (device == static_cast<uint32_t>(Device::Mouse) ? apply_mouse(code, down) : false);

    cdc_write_line(ok ? "OK" : "ERR UNSUPPORTED_KEY");
}

void poll_cdc() {
    while (tud_cdc_available()) {
        const char ch = static_cast<char>(tud_cdc_read_char());
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            command_buffer[command_length] = '\0';
            handle_command(command_buffer);
            command_length = 0;
            continue;
        }
        if (command_length + 1 < sizeof(command_buffer)) {
            command_buffer[command_length++] = ch;
        } else {
            command_length = 0;
            release_all();
            cdc_write_line("ERR LINE_TOO_LONG");
        }
    }
}

}  // namespace

int main() {
    board_init();
    tusb_init();
    release_all();

    while (true) {
        tud_task();
        poll_cdc();
    }
}

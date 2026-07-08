#include "pico/unique_id.h"
#include "tusb.h"

#include <cstdint>
#include <cstring>

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID_KEYBOARD,
    ITF_NUM_HID_MOUSE,
    ITF_NUM_HID_CONSUMER,
    ITF_NUM_TOTAL
};

enum {
    EPNUM_CDC_NOTIF = 0x81,
    EPNUM_CDC_OUT = 0x02,
    EPNUM_CDC_IN = 0x82,
    EPNUM_HID_KEYBOARD = 0x83,
    EPNUM_HID_MOUSE = 0x84,
    EPNUM_HID_CONSUMER = 0x85,
};

enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE = 2,
    REPORT_ID_CONSUMER = 3,
};

constexpr uint8_t STRING_INDEX_SERIAL = 3;

static tusb_desc_device_t const desc_device = {
    sizeof(tusb_desc_device_t),
    TUSB_DESC_DEVICE,
    0x0200,
    TUSB_CLASS_MISC,
    MISC_SUBCLASS_COMMON,
    MISC_PROTOCOL_IAD,
    CFG_TUD_ENDPOINT0_SIZE,
    0x1209,
    0x584A,
    0x0100,
    0x01,
    0x02,
    0x03,
    0x01,
};

uint8_t const* tud_descriptor_device_cb() {
    return reinterpret_cast<uint8_t const*>(&desc_device);
}

static uint8_t const desc_hid_keyboard_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))
};

static uint8_t const desc_hid_mouse_report[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE))
};

static uint8_t const desc_hid_consumer_report[] = {
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER))
};

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    if (instance == 0) {
        return desc_hid_keyboard_report;
    }
    if (instance == 1) {
        return desc_hid_mouse_report;
    }
    return desc_hid_consumer_report;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD, 5, HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_hid_keyboard_report), EPNUM_HID_KEYBOARD, CFG_TUD_HID_EP_BUFSIZE, 1),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_MOUSE, 6, HID_ITF_PROTOCOL_MOUSE,
        sizeof(desc_hid_mouse_report), EPNUM_HID_MOUSE, CFG_TUD_HID_EP_BUFSIZE, 1),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_CONSUMER, 7, HID_ITF_PROTOCOL_NONE,
        sizeof(desc_hid_consumer_report), EPNUM_HID_CONSUMER, CFG_TUD_HID_EP_BUFSIZE, 1),
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

static char const* const string_desc_arr[] = {
    reinterpret_cast<const char*>(u"\u0409"),
    "XiaoJi",
    "XiaoJi MX Keyboard",
    nullptr,
    "XiaoJi Control",
    "XiaoJi Keyboard",
    "XiaoJi Mouse",
    "XiaoJi Media",
};

static uint16_t desc_str[32];

static const char* serial_string() {
    static char serial[sizeof("XJAK-") + 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES]{};
    static bool initialized = false;
    if (!initialized) {
        std::memcpy(serial, "XJAK-", 5);
        pico_get_unique_board_id_string(
            serial + 5,
            static_cast<uint>(sizeof(serial) - 5));
        initialized = true;
    }
    return serial;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t chr_count = 0;
    if (index == 0) {
        std::memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return nullptr;
        }
        const char* str = index == STRING_INDEX_SERIAL
            ? serial_string()
            : string_desc_arr[index];
        if (str == nullptr) {
            return nullptr;
        }
        chr_count = static_cast<uint8_t>(std::strlen(str));
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (uint8_t i = 0; i < chr_count; ++i) {
            desc_str[1 + i] = str[i];
        }
    }

    desc_str[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
    hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
    hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

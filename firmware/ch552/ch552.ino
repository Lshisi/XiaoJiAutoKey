#include <Arduino.h>

#include "src/usb/USBCDC.h"
#include "src/usb/USBHIDBridge.h"
#include "src/usb/USBhandler.h"

#define XIAOJI_UPDATE_TOKEN 714021UL
#define XIAOJI_NUMPAD_ENTER 0x0100000DUL

static __xdata char commandBuffer[48];
static __data uint8_t commandLength;
static __data uint8_t keyboardModifiers;
static __xdata uint8_t keyboardKeys[6];
static __data uint8_t mouseButtons;
static __data uint16_t consumerUsage;
static __data uint8_t previousUsbConfig;

static void cdc_write_line(const __code char *text) {
  while (*text) {
    USBSerial_write(*text++);
  }
  USBSerial_write('\r');
  USBSerial_write('\n');
  USBSerial_flush();
}

static void clear_input_state(void) {
  __data uint8_t i;
  keyboardModifiers = 0;
  for (i = 0; i < 6; ++i) {
    keyboardKeys[i] = 0;
  }
  mouseButtons = 0;
  consumerUsage = 0;
}

static uint8_t release_all(void) {
  clear_input_state();
  if (!HID_SendKeyboard(keyboardModifiers, keyboardKeys)) {
    return 0;
  }
  if (!HID_SendMouse(mouseButtons)) {
    return 0;
  }
  return HID_SendConsumer(consumerUsage);
}

static uint8_t is_modifier(uint8_t usage) {
  return usage >= 0xe0 && usage <= 0xe7;
}

static uint8_t contains_key(uint8_t usage) {
  __data uint8_t i;
  for (i = 0; i < 6; ++i) {
    if (keyboardKeys[i] == usage) {
      return 1;
    }
  }
  return 0;
}

static uint8_t add_key(uint8_t usage) {
  __data uint8_t i;
  if (contains_key(usage)) {
    return 1;
  }
  for (i = 0; i < 6; ++i) {
    if (keyboardKeys[i] == 0) {
      keyboardKeys[i] = usage;
      return 1;
    }
  }
  return 0;
}

static void remove_key(uint8_t usage) {
  __data uint8_t i;
  for (i = 0; i < 6; ++i) {
    if (keyboardKeys[i] == usage) {
      keyboardKeys[i] = 0;
    }
  }
}

static uint8_t vk_to_hid(uint32_t vk) {
  if (vk >= 'A' && vk <= 'Z') {
    return (uint8_t)(0x04 + vk - 'A');
  }
  if (vk >= '1' && vk <= '9') {
    return (uint8_t)(0x1e + vk - '1');
  }
  if (vk == '0') {
    return 0x27;
  }
  if (vk >= 0x70 && vk <= 0x7b) {
    return (uint8_t)(0x3a + vk - 0x70);
  }
  if (vk >= 0x7c && vk <= 0x87) {
    return (uint8_t)(0x68 + vk - 0x7c);
  }
  if (vk == 0x60) {
    return 0x62;
  }
  if (vk >= 0x61 && vk <= 0x69) {
    return (uint8_t)(0x59 + vk - 0x61);
  }

  switch (vk) {
  case 0x03: return 0x9b; // Cancel
  case 0x08: return 0x2a; // Backspace
  case 0x09: return 0x2b; // Tab
  case 0x0c: return 0x9c; // Clear
  case 0x0d: return 0x28; // Enter
  case 0x10:
  case 0xa0: return 0xe1; // Left Shift
  case 0xa1: return 0xe5; // Right Shift
  case 0x11:
  case 0xa2: return 0xe0; // Left Control
  case 0xa3: return 0xe4; // Right Control
  case 0x12:
  case 0xa4: return 0xe2; // Left Alt
  case 0xa5: return 0xe6; // Right Alt
  case 0x1b: return 0x29; // Escape
  case 0x15: return 0x90; // LANG1
  case 0x19: return 0x91; // LANG2
  case 0x1c: return 0x87; // International1
  case 0x1d: return 0x88; // International2
  case 0x1e: return 0x89; // International3
  case 0x1f: return 0x8a; // International4
  case 0x20: return 0x2c; // Space
  case 0x21: return 0x4b; // Page Up
  case 0x22: return 0x4e; // Page Down
  case 0x23: return 0x4d; // End
  case 0x24: return 0x4a; // Home
  case 0x25: return 0x50; // Left
  case 0x26: return 0x52; // Up
  case 0x27: return 0x4f; // Right
  case 0x28: return 0x51; // Down
  case 0x29: return 0x77; // Select
  case 0x2a: return 0x46; // Print
  case 0x2b: return 0x74; // Execute
  case 0x2c: return 0x46; // Print Screen
  case 0x2d: return 0x49; // Insert
  case 0x2e: return 0x4c; // Delete
  case 0x2f: return 0x75; // Help
  case 0x5b: return 0xe3; // Left GUI
  case 0x5c: return 0xe7; // Right GUI
  case 0x5d: return 0x65; // Application
  case 0x5f: return 0x66; // Power
  case 0x6a: return 0x55; // Keypad multiply
  case 0x6b: return 0x57; // Keypad add
  case 0x6c: return 0x9f; // Separator
  case 0x6d: return 0x56; // Keypad subtract
  case 0x6e: return 0x63; // Keypad decimal
  case 0x6f: return 0x54; // Keypad divide
  case 0x90: return 0x53; // Num Lock
  case 0x91: return 0x47; // Scroll Lock
  case 0x13: return 0x48; // Pause
  case 0x14: return 0x39; // Caps Lock
  case 0xba: return 0x33;
  case 0xbb: return 0x2e;
  case 0xbc: return 0x36;
  case 0xbd: return 0x2d;
  case 0xbe: return 0x37;
  case 0xbf: return 0x38;
  case 0xc0: return 0x35;
  case 0xdb: return 0x2f;
  case 0xdc: return 0x31;
  case 0xdd: return 0x30;
  case 0xde: return 0x34;
  case 0xdf: return 0x64;
  case 0xe1: return 0x87;
  case 0xe2: return 0x32;
  case 0xe3: return 0x75;
  case 0xe4: return 0xb0;
  case 0xf6: return 0x9a;
  case 0xf7: return 0xa3;
  case 0xf8: return 0xa4;
  case 0xf9: return 0xa0;
  case 0xfa: return 0x79;
  case 0xfb: return 0xa1;
  case 0xfd: return 0x9d;
  case 0xfe: return 0x9c;
  case XIAOJI_NUMPAD_ENTER: return 0x58;
  default: return 0;
  }
}

static uint16_t vk_to_consumer(uint32_t vk) {
  switch (vk) {
  case 0x5f: return 0x0032;
  case 0xa6: return 0x0224;
  case 0xa7: return 0x0225;
  case 0xa8: return 0x0227;
  case 0xa9: return 0x0226;
  case 0xaa: return 0x0221;
  case 0xab: return 0x022a;
  case 0xac: return 0x0223;
  case 0xad: return 0x00e2;
  case 0xae: return 0x00ea;
  case 0xaf: return 0x00e9;
  case 0xb0: return 0x00b5;
  case 0xb1: return 0x00b6;
  case 0xb2: return 0x00b7;
  case 0xb3: return 0x00cd;
  case 0xb4: return 0x018a;
  case 0xb5: return 0x01c7;
  case 0xb6: return 0x0192;
  case 0xb7: return 0x0194;
  default: return 0;
  }
}

static uint8_t apply_keyboard(uint32_t vk, uint8_t down) {
  __data uint8_t usage = vk_to_hid(vk);
  __data uint16_t consumer;

  if (usage != 0) {
    if (is_modifier(usage)) {
      __data uint8_t bit = (uint8_t)(1u << (usage - 0xe0));
      if (down) {
        keyboardModifiers |= bit;
      } else {
        keyboardModifiers &= (uint8_t)~bit;
      }
    } else if (down) {
      if (!add_key(usage)) {
        return 0;
      }
    } else {
      remove_key(usage);
    }
    return HID_SendKeyboard(keyboardModifiers, keyboardKeys);
  }

  consumer = vk_to_consumer(vk);
  if (consumer == 0) {
    return 0;
  }
  consumerUsage = down ? consumer : 0;
  return HID_SendConsumer(consumerUsage);
}

static uint8_t apply_mouse(uint32_t vk, uint8_t down) {
  __data uint8_t button;
  switch (vk) {
  case 0x01: button = 0x01; break;
  case 0x02: button = 0x02; break;
  case 0x04: button = 0x04; break;
  case 0x05: button = 0x08; break;
  case 0x06: button = 0x10; break;
  default: return 0;
  }
  if (down) {
    mouseButtons |= button;
  } else {
    mouseButtons &= (uint8_t)~button;
  }
  return HID_SendMouse(mouseButtons);
}

static __xdata char *next_token(__xdata char **cursor) {
  __xdata char *start;
  while (**cursor == ' ' || **cursor == '\t') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return 0;
  }
  start = *cursor;
  while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t') {
    ++(*cursor);
  }
  if (**cursor != '\0') {
    **cursor = '\0';
    ++(*cursor);
  }
  return start;
}

static uint8_t parse_u32(__xdata char *text, uint32_t *value) {
  __data uint32_t result = 0;
  __data uint8_t digit;
  if (text == 0 || *text == '\0') {
    return 0;
  }
  while (*text) {
    if (*text < '0' || *text > '9') {
      return 0;
    }
    digit = (uint8_t)(*text++ - '0');
    if (result > 429496729UL ||
        (result == 429496729UL && digit > 5)) {
      return 0;
    }
    result = result * 10 + digit;
  }
  *value = result;
  return 1;
}

static uint8_t token_is(__xdata const char *token, const __code char *expected) {
  while (*token && *expected && *token == *expected) {
    ++token;
    ++expected;
  }
  return *token == '\0' && *expected == '\0';
}

static void enter_bootloader(void) {
  release_all();
  cdc_write_line("OK UPDATE_BOOTLOADER");
  delay(20);
  USB_CTRL = 0;
  EA = 0;
  TMOD = 0;
  delayMicroseconds(50000);
  delayMicroseconds(50000);
  __asm__("lcall #0x3800");
  while (1) {
  }
}

static void handle_command(__xdata char *line) {
  __xdata char *cursor = line;
  __xdata char *command = next_token(&cursor);
  __data uint8_t i;
  __data uint8_t down;
  __data uint32_t device;
  __data uint32_t code;

  if (command == 0) {
    return;
  }
  for (i = 0; command[i]; ++i) {
    if (command[i] >= 'a' && command[i] <= 'z') {
      command[i] = (char)(command[i] - 'a' + 'A');
    }
  }

  if (token_is(command, "PING")) {
    cdc_write_line("OK PONG");
    return;
  }
  if (token_is(command, "VER")) {
    cdc_write_line("OK XiaoJi USB V1.1.2");
    return;
  }
  if (token_is(command, "R")) {
    cdc_write_line(release_all() ? "OK RELEASE_ALL" : "ERR HID_BUSY");
    return;
  }
  if (token_is(command, "UPD")) {
    if (!parse_u32(next_token(&cursor), &code) ||
        code != XIAOJI_UPDATE_TOKEN) {
      cdc_write_line("ERR UPDATE_AUTH_REQUIRED");
      return;
    }
    enter_bootloader();
    return;
  }
  if (token_is(command, "B")) {
    cdc_write_line("ERR UPDATE_AUTH_REQUIRED");
    return;
  }

  down = token_is(command, "D");
  if (!down && !token_is(command, "U")) {
    cdc_write_line("ERR UNKNOWN_COMMAND");
    return;
  }
  if (!parse_u32(next_token(&cursor), &device) ||
      !parse_u32(next_token(&cursor), &code)) {
    cdc_write_line("ERR BAD_ARGUMENTS");
    return;
  }

  if ((device == 0 && apply_keyboard(code, down)) ||
      (device == 1 && apply_mouse(code, down))) {
    cdc_write_line("OK");
  } else {
    cdc_write_line("ERR UNSUPPORTED_KEY");
  }
}

void setup(void) {
  clear_input_state();
  USBInit();
}

void loop(void) {
  if (previousUsbConfig != UsbConfig) {
    previousUsbConfig = UsbConfig;
    if (UsbConfig == 0) {
      clear_input_state();
    }
  }

  while (USBSerial_available()) {
    __data char value = USBSerial_read();
    if (value == '\r') {
      continue;
    }
    if (value == '\n') {
      commandBuffer[commandLength] = '\0';
      handle_command(commandBuffer);
      commandLength = 0;
      continue;
    }
    if (commandLength + 1 < sizeof(commandBuffer)) {
      commandBuffer[commandLength++] = value;
    } else {
      commandLength = 0;
      release_all();
      cdc_write_line("ERR LINE_TOO_LONG");
    }
  }
}

#ifndef XIAOJI_USB_HID_BRIDGE_H
#define XIAOJI_USB_HID_BRIDGE_H

#include <stdint.h>

#include "include/ch5xx.h"

#define HID_REPORT_ID_KEYBOARD 1
#define HID_REPORT_ID_MOUSE 2
#define HID_REPORT_ID_CONSUMER 3

uint8_t HID_SendKeyboard(uint8_t modifiers, const uint8_t *keys);
uint8_t HID_SendMouse(uint8_t buttons);
uint8_t HID_SendConsumer(uint16_t usage);
void HID_ReleaseAll(void);
void USB_EP3_IN(void);

#endif

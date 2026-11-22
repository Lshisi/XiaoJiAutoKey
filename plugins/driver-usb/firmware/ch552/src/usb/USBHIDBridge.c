#include "USBHIDBridge.h"

#include "USBconstant.h"
#include "USBhandler.h"

extern __xdata __at(EP3_ADDR) uint8_t Ep3Buffer[];

static volatile __bit ep3Busy = 0;

void delayMicroseconds(__data uint16_t us);

static uint8_t wait_ep3_ready(void) {
  __data uint16_t attempts = 0;

  if (UsbConfig == 0) {
    return 0;
  }
  while (ep3Busy) {
    delayMicroseconds(5);
    if (++attempts >= 20000) {
      return 0;
    }
  }
  return 1;
}

static uint8_t send_report(uint8_t length) {
  __data uint8_t usbInterruptState;

  usbInterruptState = USB_INT_EN;
  USB_INT_EN &= (uint8_t)~bUIE_TRANSFER;
  UEP3_T_LEN = length;
  ep3Busy = 1;
  UEP3_CTRL = (UEP3_CTRL & (uint8_t)~MASK_UEP_T_RES) | UEP_T_RES_ACK;
  USB_INT_EN = usbInterruptState;
  return 1;
}

uint8_t HID_SendKeyboard(uint8_t modifiers, const uint8_t *keys) {
  __data uint8_t i;

  if (!wait_ep3_ready()) {
    return 0;
  }
  Ep3Buffer[0] = HID_REPORT_ID_KEYBOARD;
  Ep3Buffer[1] = modifiers;
  Ep3Buffer[2] = 0;
  for (i = 0; i < 6; ++i) {
    Ep3Buffer[3 + i] = keys[i];
  }
  return send_report(9);
}

uint8_t HID_SendMouse(uint8_t buttons) {
  if (!wait_ep3_ready()) {
    return 0;
  }
  Ep3Buffer[0] = HID_REPORT_ID_MOUSE;
  Ep3Buffer[1] = buttons;
  Ep3Buffer[2] = 0;
  Ep3Buffer[3] = 0;
  Ep3Buffer[4] = 0;
  Ep3Buffer[5] = 0;
  return send_report(6);
}

uint8_t HID_SendConsumer(uint16_t usage) {
  if (!wait_ep3_ready()) {
    return 0;
  }
  Ep3Buffer[0] = HID_REPORT_ID_CONSUMER;
  Ep3Buffer[1] = (uint8_t)(usage & 0xff);
  Ep3Buffer[2] = (uint8_t)(usage >> 8);
  return send_report(3);
}

void HID_ReleaseAll(void) {
  static const uint8_t emptyKeys[6] = {0, 0, 0, 0, 0, 0};
  HID_SendKeyboard(0, emptyKeys);
  HID_SendMouse(0);
  HID_SendConsumer(0);
}

void USB_EP3_IN(void) {
  UEP3_T_LEN = 0;
  UEP3_CTRL = (UEP3_CTRL & (uint8_t)~MASK_UEP_T_RES) | UEP_T_RES_NAK;
  ep3Busy = 0;
}

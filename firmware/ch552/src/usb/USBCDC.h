#ifndef __USB_CDC_H__
#define __USB_CDC_H__

// clang-format off
#include <stdbool.h>
#include <stdint.h>
#include "include/ch5xx.h"
#include "include/ch5xx_usb.h"
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

void USBInit(void);
bool USBSerial(void);
uint8_t USBSerial_available(void);
char USBSerial_read(void);
uint8_t USBSerial_write(__data char value);
void USBSerial_flush(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

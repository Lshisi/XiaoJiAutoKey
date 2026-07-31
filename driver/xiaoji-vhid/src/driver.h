#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>

#include "../../shared/XiaoJiVhidProtocol.h"

#define XIAOJI_KEYBOARD_BITMAP_BYTES 28
#define XIAOJI_KEYBOARD_REPORT_ID 1
#define XIAOJI_MOUSE_REPORT_ID 2
#define XIAOJI_CONSUMER_REPORT_ID 3

#pragma pack(push, 1)

typedef struct XIAOJI_KEYBOARD_REPORT {
    UCHAR ReportId;
    UCHAR Modifiers;
    UCHAR Keys[XIAOJI_KEYBOARD_BITMAP_BYTES];
} XIAOJI_KEYBOARD_REPORT;

typedef struct XIAOJI_MOUSE_REPORT {
    UCHAR ReportId;
    UCHAR Buttons;
    SHORT X;
    SHORT Y;
    SHORT Wheel;
    SHORT Pan;
} XIAOJI_MOUSE_REPORT;

typedef struct XIAOJI_CONSUMER_REPORT {
    UCHAR ReportId;
    USHORT Usage;
} XIAOJI_CONSUMER_REPORT;

#pragma pack(pop)

C_ASSERT(sizeof(XIAOJI_KEYBOARD_REPORT) == 30);
C_ASSERT(sizeof(XIAOJI_MOUSE_REPORT) == 10);
C_ASSERT(sizeof(XIAOJI_CONSUMER_REPORT) == 3);

typedef struct DEVICE_CONTEXT {
    VHFHANDLE VhfHandle;
    WDFSPINLOCK StateLock;
    WDFTIMER WatchdogTimer;
    WDFFILEOBJECT OwnerFile;
    ULONGLONG LastActivityMs;
    UCHAR KeyboardModifiers;
    UCHAR KeyboardBitmap[XIAOJI_KEYBOARD_BITMAP_BYTES];
    UCHAR MouseButtons;
    USHORT ConsumerUsage;
    BOOLEAN VhfStarted;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, XiaoJiGetDeviceContext)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD XiaoJiEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP XiaoJiEvtDeviceContextCleanup;
EVT_WDF_DEVICE_D0_ENTRY XiaoJiEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT XiaoJiEvtDeviceD0Exit;
EVT_WDF_DEVICE_FILE_CREATE XiaoJiEvtFileCreate;
EVT_WDF_FILE_CLEANUP XiaoJiEvtFileCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL XiaoJiEvtIoDeviceControl;
EVT_WDF_TIMER XiaoJiEvtWatchdogTimer;
EVT_VHF_ASYNC_OPERATION XiaoJiEvtVhfWriteReport;

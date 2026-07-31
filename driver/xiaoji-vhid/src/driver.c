#include <initguid.h>
#include "driver.h"

static const UCHAR g_ReportDescriptor[] = {
    // NKRO keyboard, report ID 1. Modifiers are separate and usages 0x00-0xDF use a bitmap.
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, XIAOJI_KEYBOARD_REPORT_ID,
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0xE0,       // Usage Minimum (Left Control)
    0x29, 0xE7,       // Usage Maximum (Right GUI)
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,       // Input (Data, Variable, Absolute)
    0x19, 0x00,
    0x29, 0xDF,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x96, 0xE0, 0x00, // Report Count (224)
    0x81, 0x02,
    0x05, 0x08,       // Usage Page (LEDs)
    0x19, 0x01,
    0x29, 0x05,
    0x75, 0x01,
    0x95, 0x05,
    0x91, 0x02,       // Output (Data, Variable, Absolute)
    0x75, 0x03,
    0x95, 0x01,
    0x91, 0x03,       // Output (Constant)
    0xC0,

    // Five-button relative mouse with vertical and horizontal scrolling, report ID 2.
    0x05, 0x01,
    0x09, 0x02,
    0xA1, 0x01,
    0x85, XIAOJI_MOUSE_REPORT_ID,
    0x09, 0x01,
    0xA1, 0x00,
    0x05, 0x09,
    0x19, 0x01,
    0x29, 0x05,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x05,
    0x81, 0x02,
    0x75, 0x03,
    0x95, 0x01,
    0x81, 0x03,
    0x05, 0x01,
    0x09, 0x30,
    0x09, 0x31,
    0x09, 0x38,
    0x16, 0x01, 0x80, // Logical Minimum (-32767)
    0x26, 0xFF, 0x7F, // Logical Maximum (32767)
    0x75, 0x10,
    0x95, 0x03,
    0x81, 0x06,       // Input (Data, Variable, Relative)
    0x05, 0x0C,
    0x0A, 0x38, 0x02, // Usage (AC Pan)
    0x16, 0x01, 0x80,
    0x26, 0xFF, 0x7F,
    0x75, 0x10,
    0x95, 0x01,
    0x81, 0x06,
    0xC0,
    0xC0,

    // Consumer control, report ID 3.
    0x05, 0x0C,
    0x09, 0x01,
    0xA1, 0x01,
    0x85, XIAOJI_CONSUMER_REPORT_ID,
    0x19, 0x00,
    0x2A, 0xFF, 0x03,
    0x15, 0x00,
    0x26, 0xFF, 0x03,
    0x75, 0x10,
    0x95, 0x01,
    0x81, 0x00,       // Input (Data, Array, Absolute)
    0xC0,
};

static ULONGLONG XiaoJiNowMs(void) {
    return KeQueryInterruptTime() / 10000ULL;
}

static BOOLEAN XiaoJiHeaderIsValid(const XIAOJI_VHID_HEADER* header, ULONG expectedSize) {
    return header != NULL && header->Size == expectedSize &&
        header->Version == XIAOJI_VHID_PROTOCOL_VERSION;
}

static ULONG XiaoJiCountBits(const UCHAR* bytes, ULONG byteCount) {
    ULONG count = 0;
    ULONG index;
    for (index = 0; index < byteCount; ++index) {
        UCHAR value = bytes[index];
        while (value != 0) {
            value = (UCHAR)(value & (value - 1));
            ++count;
        }
    }
    return count;
}

static NTSTATUS XiaoJiSubmitKeyboard(PDEVICE_CONTEXT context) {
    XIAOJI_KEYBOARD_REPORT report;
    HID_XFER_PACKET packet;

    if (context->VhfHandle == NULL || !context->VhfStarted) {
        return STATUS_DEVICE_NOT_READY;
    }

    report.ReportId = XIAOJI_KEYBOARD_REPORT_ID;
    WdfSpinLockAcquire(context->StateLock);
    report.Modifiers = context->KeyboardModifiers;
    RtlCopyMemory(report.Keys, context->KeyboardBitmap, sizeof(report.Keys));
    WdfSpinLockRelease(context->StateLock);

    packet.reportBuffer = (PUCHAR)&report;
    packet.reportBufferLen = sizeof(report);
    packet.reportId = XIAOJI_KEYBOARD_REPORT_ID;
    return VhfReadReportSubmit(context->VhfHandle, &packet);
}

static NTSTATUS XiaoJiSubmitMouse(PDEVICE_CONTEXT context, SHORT x, SHORT y, SHORT wheel, SHORT pan) {
    XIAOJI_MOUSE_REPORT report;
    HID_XFER_PACKET packet;

    if (context->VhfHandle == NULL || !context->VhfStarted) {
        return STATUS_DEVICE_NOT_READY;
    }

    RtlZeroMemory(&report, sizeof(report));
    report.ReportId = XIAOJI_MOUSE_REPORT_ID;
    WdfSpinLockAcquire(context->StateLock);
    report.Buttons = context->MouseButtons;
    WdfSpinLockRelease(context->StateLock);
    report.X = x;
    report.Y = y;
    report.Wheel = wheel;
    report.Pan = pan;

    packet.reportBuffer = (PUCHAR)&report;
    packet.reportBufferLen = sizeof(report);
    packet.reportId = XIAOJI_MOUSE_REPORT_ID;
    return VhfReadReportSubmit(context->VhfHandle, &packet);
}

static NTSTATUS XiaoJiSubmitConsumer(PDEVICE_CONTEXT context) {
    XIAOJI_CONSUMER_REPORT report;
    HID_XFER_PACKET packet;

    if (context->VhfHandle == NULL || !context->VhfStarted) {
        return STATUS_DEVICE_NOT_READY;
    }

    report.ReportId = XIAOJI_CONSUMER_REPORT_ID;
    WdfSpinLockAcquire(context->StateLock);
    report.Usage = context->ConsumerUsage;
    WdfSpinLockRelease(context->StateLock);

    packet.reportBuffer = (PUCHAR)&report;
    packet.reportBufferLen = sizeof(report);
    packet.reportId = XIAOJI_CONSUMER_REPORT_ID;
    return VhfReadReportSubmit(context->VhfHandle, &packet);
}

static VOID XiaoJiClearState(PDEVICE_CONTEXT context, BOOLEAN clearOwner) {
    WdfSpinLockAcquire(context->StateLock);
    context->KeyboardModifiers = 0;
    RtlZeroMemory(context->KeyboardBitmap, sizeof(context->KeyboardBitmap));
    context->MouseButtons = 0;
    context->ConsumerUsage = 0;
    if (clearOwner) {
        context->OwnerFile = NULL;
    }
    context->LastActivityMs = XiaoJiNowMs();
    WdfSpinLockRelease(context->StateLock);

    (void)XiaoJiSubmitKeyboard(context);
    (void)XiaoJiSubmitMouse(context, 0, 0, 0, 0);
    (void)XiaoJiSubmitConsumer(context);
}

static BOOLEAN XiaoJiRequestOwnsDevice(PDEVICE_CONTEXT context, WDFREQUEST request) {
    WDFFILEOBJECT fileObject = WdfRequestGetFileObject(request);
    BOOLEAN owns;
    WdfSpinLockAcquire(context->StateLock);
    owns = context->OwnerFile == fileObject;
    if (owns) {
        context->LastActivityMs = XiaoJiNowMs();
    }
    WdfSpinLockRelease(context->StateLock);
    return owns;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, XiaoJiEvtDeviceAdd);
    return WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS XiaoJiEvtDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT deviceInit) {
    WDF_PNPPOWER_EVENT_CALLBACKS powerCallbacks;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES lockAttributes;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_TIMER_CONFIG timerConfig;
    WDFDEVICE device;
    PDEVICE_CONTEXT context;
    VHF_CONFIG vhfConfig;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(driver);

    WdfDeviceInitSetDeviceType(deviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(deviceInit, FALSE);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&powerCallbacks);
    powerCallbacks.EvtDeviceD0Entry = XiaoJiEvtDeviceD0Entry;
    powerCallbacks.EvtDeviceD0Exit = XiaoJiEvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(deviceInit, &powerCallbacks);

    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, XiaoJiEvtFileCreate,
        WDF_NO_EVENT_CALLBACK, XiaoJiEvtFileCleanup);
    WdfDeviceInitSetFileObjectConfig(deviceInit, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    deviceAttributes.EvtCleanupCallback = XiaoJiEvtDeviceContextCleanup;
    status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    context = XiaoJiGetDeviceContext(device);
    RtlZeroMemory(context, sizeof(*context));

    WDF_OBJECT_ATTRIBUTES_INIT(&lockAttributes);
    lockAttributes.ParentObject = device;
    status = WdfSpinLockCreate(&lockAttributes, &context->StateLock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_XIAOJI_VHID, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = XiaoJiEvtIoDeviceControl;
    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, XiaoJiEvtWatchdogTimer, 500);
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = device;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;
    status = WdfTimerCreate(&timerConfig, &timerAttributes, &context->WatchdogTimer);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    VHF_CONFIG_INIT(&vhfConfig, WdfDeviceWdmGetDeviceObject(device),
        (USHORT)sizeof(g_ReportDescriptor), (PUCHAR)g_ReportDescriptor);
    vhfConfig.VendorID = 0x1209;
    vhfConfig.ProductID = 0x584B;
    vhfConfig.VersionNumber = 0x0100;
    vhfConfig.EvtVhfAsyncOperationWriteReport = XiaoJiEvtVhfWriteReport;
    status = VhfCreate(&vhfConfig, &context->VhfHandle);
    if (!NT_SUCCESS(status)) {
        context->VhfHandle = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}

VOID XiaoJiEvtDeviceContextCleanup(WDFOBJECT object) {
    PDEVICE_CONTEXT context = XiaoJiGetDeviceContext((WDFDEVICE)object);
    if (context->WatchdogTimer != NULL) {
        WdfTimerStop(context->WatchdogTimer, TRUE);
    }
    if (context->VhfHandle != NULL) {
        XiaoJiClearState(context, TRUE);
        VhfDelete(context->VhfHandle, TRUE);
        context->VhfHandle = NULL;
        context->VhfStarted = FALSE;
    }
}

NTSTATUS XiaoJiEvtDeviceD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previousState) {
    PDEVICE_CONTEXT context = XiaoJiGetDeviceContext(device);
    NTSTATUS status;
    UNREFERENCED_PARAMETER(previousState);

    if (!context->VhfStarted) {
        status = VhfStart(context->VhfHandle);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        context->VhfStarted = TRUE;
    }
    context->LastActivityMs = XiaoJiNowMs();
    WdfTimerStart(context->WatchdogTimer, WDF_REL_TIMEOUT_IN_MS(500));
    return STATUS_SUCCESS;
}

NTSTATUS XiaoJiEvtDeviceD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE targetState) {
    PDEVICE_CONTEXT context = XiaoJiGetDeviceContext(device);
    UNREFERENCED_PARAMETER(targetState);
    WdfTimerStop(context->WatchdogTimer, TRUE);
    XiaoJiClearState(context, TRUE);
    return STATUS_SUCCESS;
}

VOID XiaoJiEvtFileCreate(WDFDEVICE device, WDFREQUEST request, WDFFILEOBJECT fileObject) {
    UNREFERENCED_PARAMETER(device);
    UNREFERENCED_PARAMETER(fileObject);
    WdfRequestComplete(request, STATUS_SUCCESS);
}

VOID XiaoJiEvtFileCleanup(WDFFILEOBJECT fileObject) {
    WDFDEVICE device = WdfFileObjectGetDevice(fileObject);
    PDEVICE_CONTEXT context = XiaoJiGetDeviceContext(device);
    BOOLEAN release = FALSE;

    WdfSpinLockAcquire(context->StateLock);
    if (context->OwnerFile == fileObject) {
        release = TRUE;
    }
    WdfSpinLockRelease(context->StateLock);
    if (release) {
        XiaoJiClearState(context, TRUE);
    }
}

VOID XiaoJiEvtIoDeviceControl(WDFQUEUE queue, WDFREQUEST request,
    size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode) {
    WDFDEVICE device = WdfIoQueueGetDevice(queue);
    PDEVICE_CONTEXT context = XiaoJiGetDeviceContext(device);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    size_t information = 0;
    PVOID buffer = NULL;
    size_t length = 0;

    UNREFERENCED_PARAMETER(outputBufferLength);
    UNREFERENCED_PARAMETER(inputBufferLength);

    switch (ioControlCode) {
    case IOCTL_XIAOJI_VHID_ACQUIRE: {
        WDFFILEOBJECT fileObject = WdfRequestGetFileObject(request);
        WdfSpinLockAcquire(context->StateLock);
        if (context->OwnerFile == NULL || context->OwnerFile == fileObject) {
            context->OwnerFile = fileObject;
            context->LastActivityMs = XiaoJiNowMs();
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_SHARING_VIOLATION;
        }
        WdfSpinLockRelease(context->StateLock);
        break;
    }
    case IOCTL_XIAOJI_VHID_RELEASE:
        if (XiaoJiRequestOwnsDevice(context, request)) {
            XiaoJiClearState(context, TRUE);
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_ACCESS_DENIED;
        }
        break;
    case IOCTL_XIAOJI_VHID_HEARTBEAT:
        status = XiaoJiRequestOwnsDevice(context, request) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
        break;
    case IOCTL_XIAOJI_VHID_RELEASE_ALL:
        if (XiaoJiRequestOwnsDevice(context, request)) {
            XiaoJiClearState(context, FALSE);
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_ACCESS_DENIED;
        }
        break;
    case IOCTL_XIAOJI_VHID_KEYBOARD:
        if (!XiaoJiRequestOwnsDevice(context, request)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        status = WdfRequestRetrieveInputBuffer(request, sizeof(XIAOJI_VHID_KEYBOARD_INPUT), &buffer, &length);
        if (NT_SUCCESS(status)) {
            XIAOJI_VHID_KEYBOARD_INPUT* input = (XIAOJI_VHID_KEYBOARD_INPUT*)buffer;
            if (!XiaoJiHeaderIsValid(&input->Header, sizeof(*input)) ||
                (input->Usage == 0 || (input->Usage > 0xDF && (input->Usage < 0xE0 || input->Usage > 0xE7)))) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            WdfSpinLockAcquire(context->StateLock);
            if (input->Usage >= 0xE0) {
                const UCHAR mask = (UCHAR)(1U << (input->Usage - 0xE0));
                if (input->Pressed) context->KeyboardModifiers |= mask;
                else context->KeyboardModifiers &= (UCHAR)~mask;
            } else {
                const ULONG byteIndex = input->Usage / 8;
                const UCHAR mask = (UCHAR)(1U << (input->Usage % 8));
                if (input->Pressed) context->KeyboardBitmap[byteIndex] |= mask;
                else context->KeyboardBitmap[byteIndex] &= (UCHAR)~mask;
            }
            WdfSpinLockRelease(context->StateLock);
            status = XiaoJiSubmitKeyboard(context);
        }
        break;
    case IOCTL_XIAOJI_VHID_MOUSE_BUTTON:
        if (!XiaoJiRequestOwnsDevice(context, request)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        status = WdfRequestRetrieveInputBuffer(request, sizeof(XIAOJI_VHID_MOUSE_BUTTON_INPUT), &buffer, &length);
        if (NT_SUCCESS(status)) {
            XIAOJI_VHID_MOUSE_BUTTON_INPUT* input = (XIAOJI_VHID_MOUSE_BUTTON_INPUT*)buffer;
            if (!XiaoJiHeaderIsValid(&input->Header, sizeof(*input)) || input->Button < 1 || input->Button > 5) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            WdfSpinLockAcquire(context->StateLock);
            if (input->Pressed) context->MouseButtons |= (UCHAR)(1U << (input->Button - 1));
            else context->MouseButtons &= (UCHAR)~(1U << (input->Button - 1));
            WdfSpinLockRelease(context->StateLock);
            status = XiaoJiSubmitMouse(context, 0, 0, 0, 0);
        }
        break;
    case IOCTL_XIAOJI_VHID_MOUSE_MOVE:
        if (!XiaoJiRequestOwnsDevice(context, request)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        status = WdfRequestRetrieveInputBuffer(request, sizeof(XIAOJI_VHID_MOUSE_MOVE_INPUT), &buffer, &length);
        if (NT_SUCCESS(status)) {
            XIAOJI_VHID_MOUSE_MOVE_INPUT* input = (XIAOJI_VHID_MOUSE_MOVE_INPUT*)buffer;
            if (!XiaoJiHeaderIsValid(&input->Header, sizeof(*input))) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            status = XiaoJiSubmitMouse(context, input->X, input->Y, input->Wheel, input->Pan);
        }
        break;
    case IOCTL_XIAOJI_VHID_CONSUMER:
        if (!XiaoJiRequestOwnsDevice(context, request)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        status = WdfRequestRetrieveInputBuffer(request, sizeof(XIAOJI_VHID_CONSUMER_INPUT), &buffer, &length);
        if (NT_SUCCESS(status)) {
            XIAOJI_VHID_CONSUMER_INPUT* input = (XIAOJI_VHID_CONSUMER_INPUT*)buffer;
            if (!XiaoJiHeaderIsValid(&input->Header, sizeof(*input)) || input->Usage > 0x03FF) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            WdfSpinLockAcquire(context->StateLock);
            context->ConsumerUsage = input->Pressed ? input->Usage : 0;
            WdfSpinLockRelease(context->StateLock);
            status = XiaoJiSubmitConsumer(context);
        }
        break;
    case IOCTL_XIAOJI_VHID_GET_STATUS:
        status = WdfRequestRetrieveOutputBuffer(request, sizeof(XIAOJI_VHID_STATUS), &buffer, &length);
        if (NT_SUCCESS(status)) {
            XIAOJI_VHID_STATUS* output = (XIAOJI_VHID_STATUS*)buffer;
            RtlZeroMemory(output, sizeof(*output));
            output->Size = sizeof(*output);
            output->ProtocolVersion = XIAOJI_VHID_PROTOCOL_VERSION;
            output->DriverVersion = XIAOJI_VHID_DRIVER_VERSION;
            output->WatchdogTimeoutMs = XIAOJI_VHID_WATCHDOG_TIMEOUT_MS;
            WdfSpinLockAcquire(context->StateLock);
            if (context->OwnerFile != NULL) output->Flags |= XIAOJI_VHID_STATUS_ACQUIRED;
            if (context->VhfStarted) output->Flags |= XIAOJI_VHID_STATUS_STARTED;
            output->ActiveKeyboardUsages = XiaoJiCountBits(context->KeyboardBitmap,
                sizeof(context->KeyboardBitmap)) + XiaoJiCountBits(&context->KeyboardModifiers, 1);
            output->ActiveMouseButtons = XiaoJiCountBits(&context->MouseButtons, 1);
            WdfSpinLockRelease(context->StateLock);
            information = sizeof(*output);
        }
        break;
    default:
        break;
    }

    WdfRequestCompleteWithInformation(request, status, information);
}

VOID XiaoJiEvtWatchdogTimer(WDFTIMER timer) {
    WDFDEVICE device = (WDFDEVICE)WdfTimerGetParentObject(timer);
    PDEVICE_CONTEXT context = XiaoJiGetDeviceContext(device);
    BOOLEAN expired = FALSE;
    const ULONGLONG now = XiaoJiNowMs();

    WdfSpinLockAcquire(context->StateLock);
    if (context->OwnerFile != NULL && now - context->LastActivityMs > XIAOJI_VHID_WATCHDOG_TIMEOUT_MS) {
        expired = TRUE;
        context->OwnerFile = NULL;
        context->KeyboardModifiers = 0;
        RtlZeroMemory(context->KeyboardBitmap, sizeof(context->KeyboardBitmap));
        context->MouseButtons = 0;
        context->ConsumerUsage = 0;
        context->LastActivityMs = now;
    }
    WdfSpinLockRelease(context->StateLock);

    if (expired) {
        (void)XiaoJiSubmitKeyboard(context);
        (void)XiaoJiSubmitMouse(context, 0, 0, 0, 0);
        (void)XiaoJiSubmitConsumer(context);
    }
}

VOID XiaoJiEvtVhfWriteReport(PVOID vhfClientContext, VHFOPERATIONHANDLE vhfOperationHandle,
    PVOID vhfOperationContext, PHID_XFER_PACKET hidTransferPacket) {
    UNREFERENCED_PARAMETER(vhfClientContext);
    UNREFERENCED_PARAMETER(vhfOperationContext);
    UNREFERENCED_PARAMETER(hidTransferPacket);
    (void)VhfAsyncOperationComplete(vhfOperationHandle, STATUS_SUCCESS);
}

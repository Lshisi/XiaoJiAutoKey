/*
 created by Deqing Sun for use with CH55xduino
 */

#include "USBconstant.h"

// Device descriptor
__code USB_Descriptor_Device_t DeviceDescriptor = {
    .Header = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

    .USBSpecification = VERSION_BCD(1, 1, 0),
    .Class = 0xEF, // Miscellaneous
    .SubClass = 0x02,
    .Protocol = 0x01, // Interface Association Descriptor

    .Endpoint0Size = DEFAULT_ENDP0_SIZE,

    .VendorID = 0x1209,
    .ProductID = 0x584A,
    .ReleaseNumber = VERSION_BCD(2, 0, 0),

    .ManufacturerStrIndex = 1,
    .ProductStrIndex = 2,
    .SerialNumStrIndex = 3,

    .NumberOfConfigurations = 1};

/** Configuration descriptor structure. This descriptor, located in FLASH
 * memory, describes the usage of the device in one of its supported
 * configurations, including information about any device interfaces and
 * endpoints. The descriptor is read out by the USB host during the enumeration
 * process when selecting a configuration so that the host may correctly
 * communicate with the USB device.
 */
__code USB_Descriptor_Configuration_t ConfigurationDescriptor = {
    .Config = {.Header = {.Size = sizeof(USB_Descriptor_Configuration_Header_t),
                          .Type = DTYPE_Configuration},

               .TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t),
               .TotalInterfaces = 3,

               .ConfigurationNumber = 1,
               .ConfigurationStrIndex = NO_DESCRIPTOR,

               .ConfigAttributes = (USB_CONFIG_ATTR_RESERVED),

               .MaxPowerConsumption = USB_CONFIG_POWER_MA(200)},

    .CDC_IAD = {.Header = {.Size =
                               sizeof(USB_Descriptor_Interface_Association_t),
                           .Type = DTYPE_InterfaceAssociation},

                .FirstInterfaceIndex = INTERFACE_ID_CDC_CCI,
                .TotalInterfaces = 2,

                .Class = CDC_CSCP_CDCClass,
                .SubClass = CDC_CSCP_ACMSubclass,
                .Protocol = CDC_CSCP_ATCommandProtocol,

                .IADStrIndex = 4},

    .CDC_CCI_Interface = {.Header = {.Size = sizeof(USB_Descriptor_Interface_t),
                                     .Type = DTYPE_Interface},

                          .InterfaceNumber = INTERFACE_ID_CDC_CCI,
                          .AlternateSetting = 0,

                          .TotalEndpoints = 1,

                          .Class = CDC_CSCP_CDCClass,
                          .SubClass = CDC_CSCP_ACMSubclass,
                          .Protocol = CDC_CSCP_ATCommandProtocol,

                          .InterfaceStrIndex = 4},
    // refer to usbcdc11.pdf
    .CDC_Functional_Header =
        {
            .Header = {.Size = sizeof(USB_CDC_Descriptor_FunctionalHeader_t),
                       .Type = CDC_DTYPE_CSInterface},
            .Subtype = CDC_DSUBTYPE_CSInterface_Header,

            .CDCSpecification = VERSION_BCD(1, 1, 0),
        },
    // Todo: check CDC_DSUBTYPE_CSInterface_CallManagement difference?
    .CDC_Functional_ACM =
        {
            .Header = {.Size = sizeof(USB_CDC_Descriptor_FunctionalACM_t),
                       .Type = CDC_DTYPE_CSInterface},
            .Subtype = CDC_DSUBTYPE_CSInterface_ACM,

            .Capabilities = 0x02, // No Send_Break, Yes  Set_Line_Coding,
                                  // Set_Control_Line_State, Get_Line_Coding,
                                  // and the notification Serial_State.
        },

    .CDC_Functional_Union =
        {
            .Header = {.Size = sizeof(USB_CDC_Descriptor_FunctionalUnion_t),
                       .Type = CDC_DTYPE_CSInterface},
            .Subtype = CDC_DSUBTYPE_CSInterface_Union,

            .MasterInterfaceNumber = INTERFACE_ID_CDC_CCI,
            .SlaveInterfaceNumber = INTERFACE_ID_CDC_DCI,
        },

    .CDC_NotificationEndpoint =
        {.Header = {.Size = sizeof(USB_Descriptor_Endpoint_t),
                    .Type = DTYPE_Endpoint},

         .EndpointAddress = CDC_NOTIFICATION_EPADDR,
         .Attributes =
             (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
         .EndpointSize = CDC_NOTIFICATION_EPSIZE,
         .PollingIntervalMS = 0x40},

    .CDC_DCI_Interface = {.Header = {.Size = sizeof(USB_Descriptor_Interface_t),
                                     .Type = DTYPE_Interface},

                          .InterfaceNumber = INTERFACE_ID_CDC_DCI,
                          .AlternateSetting = 0,

                          .TotalEndpoints = 2,

                          .Class = CDC_CSCP_CDCDataClass,
                          .SubClass = CDC_CSCP_NoDataSubclass,
                          .Protocol = CDC_CSCP_NoDataProtocol,

                          .InterfaceStrIndex = 4},

    .CDC_DataOutEndpoint = {.Header = {.Size =
                                           sizeof(USB_Descriptor_Endpoint_t),
                                       .Type = DTYPE_Endpoint},

                            .EndpointAddress = CDC_RX_EPADDR,
                            .Attributes =
                                (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC |
                                 ENDPOINT_USAGE_DATA),
                            .EndpointSize = CDC_TXRX_EPSIZE,
                            .PollingIntervalMS = 0x00},

    .CDC_DataInEndpoint = {.Header = {.Size = sizeof(USB_Descriptor_Endpoint_t),
                                      .Type = DTYPE_Endpoint},

                           .EndpointAddress = CDC_TX_EPADDR,
                           .Attributes = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC |
                                          ENDPOINT_USAGE_DATA),
                           .EndpointSize = CDC_TXRX_EPSIZE,
                           .PollingIntervalMS = 0x00},

    .HID_Interface = {.Header = {.Size = sizeof(USB_Descriptor_Interface_t),
                                 .Type = DTYPE_Interface},

                      .InterfaceNumber = INTERFACE_ID_HID,
                      .AlternateSetting = 0x00,

                      .TotalEndpoints = 1,

                      .Class = HID_CSCP_HIDClass,
                      .SubClass = HID_CSCP_NonBootSubclass,
                      .Protocol = HID_CSCP_NonBootProtocol,

                      .InterfaceStrIndex = NO_DESCRIPTOR},

    .HID_BridgeHID = {.Header = {.Size = sizeof(USB_HID_Descriptor_HID_t),
                                 .Type = HID_DTYPE_HID},

                        .HIDSpec = VERSION_BCD(1, 1, 0),
                        .CountryCode = 0x00,
                        .TotalReportDescriptors = 1,
                        .HIDReportType = HID_DTYPE_Report,
                        .HIDReportLength = sizeof(ReportDescriptor)},

    .HID_ReportINEndpoint = {.Header = {.Size =
                                            sizeof(USB_Descriptor_Endpoint_t),
                                        .Type = DTYPE_Endpoint},

                             .EndpointAddress = HID_EPADDR,
                             .Attributes =
                                 (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC |
                                  ENDPOINT_USAGE_DATA),
                             .EndpointSize = HID_EPSIZE,
                             .PollingIntervalMS = 1}

};

// String Descriptors
__code uint8_t LanguageDescriptor[] = {0x04, 0x03, 0x09,
                                       0x04}; // Language Descriptor
__xdata uint8_t SerialDescriptor[42];
__code uint16_t ProductDescriptor[] = {
    // Produce String Descriptor
    (((20 + 1) * 2) | (DTYPE_String << 8)),
    'X', 'i', 'a', 'o', 'J', 'i', ' ', 'M', 'X', ' ',
    'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd', ' ', '2',
};

__code uint16_t CDCDescriptor[] = {
    (((15 + 1) * 2) | (DTYPE_String << 8)),
    'X', 'i', 'a', 'o', 'J', 'i', ' ', 'C', 'o', 'n', 't', 'r', 'o', 'l', '2',
};

__code uint16_t ManufacturerDescriptor[] = {
    (((6 + 1) * 2) | (DTYPE_String << 8)), 'X', 'i', 'a', 'o', 'J', 'i',
};

__code uint8_t ReportDescriptor[] = {
    // Keyboard, report ID 1.
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00,
    0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05,
    0x75, 0x01, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03,
    0x91, 0x01,
    0x05, 0x07, 0x19, 0x00, 0x2a, 0xff, 0x00, 0x15,
    0x00, 0x26, 0xff, 0x00, 0x95, 0x06, 0x75, 0x08,
    0x81, 0x00, 0xc0,

    // Five-button relative mouse, report ID 2.
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x02,
    0x09, 0x01, 0xa1, 0x00, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x05, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x03,
    0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08,
    0x95, 0x03, 0x81, 0x06, 0x05, 0x0c, 0x0a, 0x38,
    0x02, 0x95, 0x01, 0x81, 0x06, 0xc0, 0xc0,

    // Consumer control, report ID 3.
    0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x03,
    0x15, 0x00, 0x26, 0xff, 0x03, 0x19, 0x00, 0x2a,
    0xff, 0x03, 0x95, 0x01, 0x75, 0x10, 0x81, 0x00,
    0xc0,
};

static uint8_t hex_digit(uint8_t value) {
  value &= 0x0f;
  return value < 10 ? (uint8_t)('0' + value)
                    : (uint8_t)('A' + value - 10);
}

void BuildSerialDescriptor(void) {
  static const __code char prefix[] = "XJAK-USB2-";
  __data uint8_t chipId[5];
  __data uint8_t i;
  __data uint8_t output = 2;

  chipId[0] = *((__code uint8_t *)ROM_CHIP_ID_HX);
  chipId[1] = *((__code uint8_t *)ROM_CHIP_ID_HI + 1);
  chipId[2] = *((__code uint8_t *)ROM_CHIP_ID_HI);
  chipId[3] = *((__code uint8_t *)ROM_CHIP_ID_LO + 1);
  chipId[4] = *((__code uint8_t *)ROM_CHIP_ID_LO);

  SerialDescriptor[0] = sizeof(SerialDescriptor);
  SerialDescriptor[1] = DTYPE_String;
  for (i = 0; i < sizeof(prefix) - 1; ++i) {
    SerialDescriptor[output++] = prefix[i];
    SerialDescriptor[output++] = 0;
  }
  for (i = 0; i < sizeof(chipId); ++i) {
    SerialDescriptor[output++] = hex_digit(chipId[i] >> 4);
    SerialDescriptor[output++] = 0;
    SerialDescriptor[output++] = hex_digit(chipId[i]);
    SerialDescriptor[output++] = 0;
  }
}

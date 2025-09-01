#include <stdio.h>

#include "gatt_svr.h"

#define SUPPORT_REPORT_VENDOR false

// HID Report Map characteristic value
// Keyboard report descriptor (using format for Boot interface descriptor)
const uint8_t Hid_report_map[] = {
    /*** MOUSE REPORT ***/
	0x05, 0x01,        // Usage Page (Generic Desktop)
	0x09, 0x02,        // Usage (Mouse)
	0xA1, 0x01,        // Collection (Application)
	0x85, 0x01,        // Report Id (1)
	0x09, 0x01,        //   Usage (Pointer)
	0xA1, 0x00,        //   Collection (Physical)
	0x05, 0x09,        //     Usage Page (Buttons)
	0x19, 0x01,        //     Usage Minimum (01) - Button 1
	0x29, 0x03,        //     Usage Maximum (03) - Button 3
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x03,        //     Report Count (3)
	0x81, 0x02,        //     Input (Data, Variable, Absolute) - Button states
	0x75, 0x05,        //     Report Size (5)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x01,        //     Input (Constant) - Padding
	0x05, 0x01,        //     Usage Page (Generic Desktop)
	0x09, 0x30,        //     Usage (X)
	0x09, 0x31,        //     Usage (Y)
	0x16, 0x00, 0x80,  //     Logical Minimum (-32768)
	0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
	0x75, 0x10,        //     Report Size (16)
	0x95, 0x02,        //     Report Count (2)
	0x81, 0x06,        //     Input (Data, Variable, Relative) - X, Y coordinates
	0x05, 0x01,        //     Usage Page (Generic Desktop)
	0x09, 0x38,        //     Usage (Wheel) - Vertical Wheel
	0x15, 0x81,        //     Logical Minimum (-127)
	0x25, 0x7F,        //     Logical Maximum (127)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x06,        //     Input (Data, Variable, Relative) - Vertical Wheel
	0x05, 0x0C,        //     Usage Page (Consumer)
	0x0a, 0x38, 0x02,  //     Usage (AC Pan) - Horizontal Wheel
	0x15, 0x81,        //     Logical Minimum (-127)
	0x25, 0x7F,        //     Logical Maximum (127)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x06,        //     Input (Data, Variable, Relative) - Horizontal Wheel
	0xC0,              //   End Collection
	0xC0,              // End Collection
};

size_t Hid_report_map_size = sizeof(Hid_report_map);

#define NO_MINKEYSIZE     .min_key_size = DEFAULT_MIN_KEY_SIZE
#define NO_ARG_MINKEYSIZE .arg = NULL, NO_MINKEYSIZE
#define NO_ARG_DESCR_MKS  .descriptors = NULL, NO_ARG_MINKEYSIZE
#define NO_DESCR_MKS      .descriptors = NULL, NO_MINKEYSIZE

#define MY_NOTIFY_FLAGS (BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE)

const struct ble_gatt_svc_def Gatt_svr_included_services[] = {
    {
        /*** Battery Service. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_BAS_UUID16),
        .includes = NULL,
        .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Battery level characteristic */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_BAS_CHR_UUID16_BATTERY_LEVEL),
            .access_cb = ble_svc_battery_access,
            .arg = (void *) HANDLE_BATTERY_LEVEL,
            .val_handle = &Svc_char_handles[HANDLE_BATTERY_LEVEL],
            .flags = MY_NOTIFY_FLAGS,
            NO_MINKEYSIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_BAT_PRESENT_DESCR),
                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
                .access_cb = ble_svc_battery_access,
                NO_ARG_MINKEYSIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    { /*** Service: Device Information Service (DIS). */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_UUID16),
        .includes = NULL,
        .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Characteristic: Model Number String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_MODEL_NUMBER),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_MODEL_NUMBER],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_MODEL_NUMBER_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Serial Number String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_SERIAL_NUMBER),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_SERIAL_NUMBER],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_SERIAL_NUMBER_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Hardware Revision String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_HARDWARE_REVISION),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_HARDWARE_REVISION],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_HARDWARE_REVISION_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Firmware Revision String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_FIRMWARE_REVISION),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_FIRMWARE_REVISION],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_FIRMWARE_REVISION_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Software Revision String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_SOFTWARE_REVISION),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_SOFWARE_REVISION],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_SOFTWARE_REVISION_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Manufacturer Name */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_MANUFACTURER_NAME),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_MANUFACTURER_NAME],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_MANUFACTURER_NAME_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
      /*** Characteristic: System Id */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_SYSTEM_ID),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_SYSTEM_ID],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_SYSTEM_ID_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
      /*** Characteristic: System Id */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_PNP_INFO),
            .access_cb = ble_svc_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_PNP_INFO],
            .flags = BLE_GATT_CHR_F_READ,
            NO_ARG_DESCR_MKS,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },

    {
        0, /* No more services. */
    },
};

const struct ble_gatt_svc_def *Inc_svcs[] = {
    &Gatt_svr_included_services[0],
    NULL,
};

const struct ble_gatt_svc_def Gatt_svr_svcs[] = {
    {
        /*** HID Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_SERVICE),
        .includes = Inc_svcs,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
            /*** HID INFO characteristic */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_INFORMATION),
                .access_cb = hid_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_HID_INFORMATION],
                .flags = BLE_GATT_CHR_F_READ, // | BLE_GATT_CHR_F_READ_ENC,
                NO_ARG_DESCR_MKS,
            }, {
            /*** HID Control Point */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_CONTROL_POINT),
                .access_cb = hid_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_HID_CONTROL_POINT],
                .flags = BLE_GATT_CHR_F_WRITE, // | BLE_GATT_CHR_F_WRITE_ENC,
                NO_ARG_DESCR_MKS,
            }, {
            /*** Report Map */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_REPORT_MAP),
                .access_cb = hid_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_HID_REPORT_MAP],
                .flags = BLE_GATT_CHR_F_READ,
                NO_ARG_MINKEYSIZE,
                .descriptors = (struct ble_gatt_dsc_def[]) { {
                    /*** External Report Reference Descriptor */
                    .uuid = BLE_UUID16_DECLARE(GATT_UUID_EXT_RPT_REF_DESCR),
                    .att_flags = BLE_ATT_F_READ,
                    .access_cb = hid_svr_chr_access,
                    NO_ARG_MINKEYSIZE,
                }, {
                    0, /* No more descriptors in this characteristic. */
                } },
            }, {
            /*** Protocol Mode Characteristic */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_PROTO_MODE),
                .access_cb = hid_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_HID_PROTO_MODE],
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                NO_ARG_DESCR_MKS,
            }, {
            /*** Mouse hid report */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_REPORT),
                .access_cb = ble_svc_report_access,
                .arg = (void *)HANDLE_HID_MOUSE_REPORT,
                .val_handle = &Svc_char_handles[HANDLE_HID_MOUSE_REPORT],
                .flags = MY_NOTIFY_FLAGS,
                .min_key_size = DEFAULT_MIN_KEY_SIZE,
                .descriptors = (struct ble_gatt_dsc_def[]) { {
                    /* Report Reference Descriptor */
                    .uuid = BLE_UUID16_DECLARE(GATT_UUID_RPT_REF_DESCR),
                    .att_flags = BLE_ATT_F_READ,
                    .access_cb = ble_svc_report_access,
                    .arg = (void *)HANDLE_HID_MOUSE_REPORT,
                    .min_key_size = DEFAULT_MIN_KEY_SIZE,
                }, {
                    0, /* No more descriptors in this characteristic. */
                } },
            }, {
            /*** Mouse input boot hid report */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_BT_MOUSE_INPUT),
                .access_cb = ble_svc_report_access,
                .arg = (void *)HANDLE_HID_BOOT_MOUSE_REPORT,
                .val_handle = &Svc_char_handles[HANDLE_HID_BOOT_MOUSE_REPORT],
                .flags = MY_NOTIFY_FLAGS,
                NO_DESCR_MKS,
            }, {
            /*** Feature hid report */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_HID_REPORT),
                .access_cb = ble_svc_report_access,
                .arg = (void *)HANDLE_HID_FEATURE_REPORT,
                .val_handle = &Svc_char_handles[HANDLE_HID_FEATURE_REPORT],
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .min_key_size = DEFAULT_MIN_KEY_SIZE,
                .descriptors = (struct ble_gatt_dsc_def[]) { {
                    /* Report Reference Descriptor */
                    .uuid = BLE_UUID16_DECLARE(GATT_UUID_RPT_REF_DESCR),
                    .att_flags = BLE_ATT_F_READ,
                    .access_cb = ble_svc_report_access,
                    .arg = (void *)HANDLE_HID_FEATURE_REPORT,
                    .min_key_size = DEFAULT_MIN_KEY_SIZE,
                }, {
                    0, /* No more descriptors in this characteristic. */
                } },
            }, {
                0, /* No more characteristics in this service. */
            }
        },
    },

    {
        0, /* No more services. */
    },
};

/* handles for all characteristics in GATT services */
uint16_t Svc_char_handles[HANDLE_HID_COUNT];

#define BCDHID_DATA 0x0111

const uint8_t HidInfo[HID_INFORMATION_LEN] = {
    LO_UINT16(BCDHID_DATA), HI_UINT16(BCDHID_DATA),   // bcdHID (USB HID version)
    0x00,                                             // bCountryCode
    HID_KBD_FLAGS                                     // Flags
};

// HID External Report Reference Descriptor
uint16_t HidExtReportRefDesc = BLE_SVC_BAS_CHR_UUID16_BATTERY_LEVEL;
uint8_t  HidProtocolMode = HID_PROTOCOL_MODE_REPORT;

/* Report reference table, byte 0 - report id from report map, byte 1 - report type (in,out,feature)*/
struct report_reference_table Hid_report_ref_data[] = {
    { .id = HANDLE_HID_MOUSE_REPORT,    .hidReportRef = { HID_RPT_ID_MOUSE_IN,  HID_REPORT_TYPE_INPUT   }},
    { .id = HANDLE_HID_FEATURE_REPORT,  .hidReportRef = { HID_RPT_ID_FEATURE,   HID_REPORT_TYPE_FEATURE }},
};
size_t Hid_report_ref_data_count = sizeof(Hid_report_ref_data)/sizeof(Hid_report_ref_data[0]);

// battery level unit - percents
struct prf_char_pres_fmt Battery_level_units = {
    .format = 4,      // Unsigned 8-bit
    .exponent = 0,
    .unit = 0x27AD,   // percentage
    .name_space = 1,  // BLUETOOTH SIG
    .description = 0
};

/* Device information
variable name restricted in ESP_IDF
components/bt/host/nimble/nimble/nimble/host/services/dis/include/services/dis/ble_svc_dis.h
*/
struct ble_svc_dis_data Hid_dis_data = {
    .model_number      = BLE_SVC_DIS_MODEL_NUMBER_DEFAULT,
    .serial_number     = BLE_SVC_DIS_SERIAL_NUMBER_DEFAULT,
    .firmware_revision = BLE_SVC_DIS_FIRMWARE_REVISION_DEFAULT,
    .hardware_revision = BLE_SVC_DIS_HARDWARE_REVISION_DEFAULT,
    .software_revision = BLE_SVC_DIS_SOFTWARE_REVISION_DEFAULT,
    .manufacturer_name = BLE_SVC_DIS_MANUFACTURER_NAME_DEFAULT,
    .system_id         = BLE_SVC_DIS_SYSTEM_ID_DEFAULT,
    .pnp_info          = BLE_SVC_DIS_PNP_INFO_DEFAULT,
};

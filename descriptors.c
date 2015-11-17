/******************************************************************************
 * @file     descriptors.c
 * @brief    NUC123 series USBD descriptor
 *
 * @note
 * Copyright (C) 2014~2015 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
/*!<Includes */
#include "NUC123.h"
#include "hid_transfer.h"
#include "version.h"

#define BUFFER_SIZE 63

/*!<USB HID Report Descriptor */

const uint8_t HID_DeviceReportDescriptor[] = {
    0x06, 0xA0, 0xFF,   // USAGE_PAGE (Vendor Defined page 0xA1)
    0x09, 0x01,         // USAGE (Vendor Usage 0x01)
    0xA1, 0x01,         // COLLECTION (Application)
                        // Global items
    0x15, 0x00,         //   LOGICAL_MINIMUM(0)
    0x26, 0xFF, 0x00,   //   LOGICAL_MAXIMUM(255)
    0x75, 0x08,         //   REPORT_SIZE

    0x85, 0x01,         //   REPORT_ID (1)
    0x95, BUFFER_SIZE,  //   REPORT_COUNT (64)
    0x09, 0x03,         //   USAGE (Vendor Usage 0x03)
    0x81, 0x02,         //   INPUT (Data,Variable,Abs)
    0x09, 0x04,         //   USAGE (Vendor Usage 0x04)
    0x91, 0x02,         //   OUTPUT(Data, Variable,Abs
    0x09, 0x05,         //   USAGE (Vendor Usage 0x05)
    0xB1, 0x02,         //   Feature (Data, Variable,Abs)

    0x85, 0x02,         //   REPORT_ID (2)
    0x95, BUFFER_SIZE,  //   REPORT_COUNT (64)
    0x09, 0x06,         //   USAGE (Vendor Usage 0x03)
    0x81, 0x02,         //   INPUT (Data,Variable,Abs)
    0x09, 0x07,         //   USAGE (Vendor Usage 0x04)
    0x91, 0x02,         //   OUTPUT(Data, Variable,Abs
    0x09, 0x08,         //   USAGE (Vendor Usage 0x05)
    0xB1, 0x02,         //   Feature (Data, Variable,Abs)

    0x85, 0x03,         //   REPORT_ID (3)
    0x95, BUFFER_SIZE,  //   REPORT_COUNT (64)
    0x09, 0x09,         //   USAGE (Vendor Usage 0x03)
    0x81, 0x02,         //   INPUT (Data,Variable,Abs)
    0x09, 0x0A,         //   USAGE (Vendor Usage 0x04)
    0x91, 0x02,         //   OUTPUT(Data, Variable,Abs
    0x09, 0x0B,         //   USAGE (Vendor Usage 0x05)
    0xB1, 0x02,         //   Feature (Data, Variable,Abs)

    0x85, 0xF1,         //   REPORT_ID (3)
    0x95, BUFFER_SIZE,  //   REPORT_COUNT (64)
    0x09, 0x09,         //   USAGE (Vendor Usage 0x03)
    0x81, 0x02,         //   INPUT (Data,Variable,Abs)
    0x09, 0x0A,         //   USAGE (Vendor Usage 0x04)
    0x91, 0x02,         //   OUTPUT(Data, Variable,Abs
    0x09, 0x0B,         //   USAGE (Vendor Usage 0x05)
    0xB1, 0x02,         //   Feature (Data, Variable,Abs)

	0xC0                // END_COLLECTION
};

/*----------------------------------------------------------------------------*/
/*!<USB Device Descriptor */
const uint8_t gu8DeviceDescriptor[] =
{
    LEN_DEVICE,     /* bLength */
    DESC_DEVICE,    /* bDescriptorType */
    0x10, 0x01,     /* bcdUSB */
    0x00,           /* bDeviceClass */
    0x00,           /* bDeviceSubClass */
    0x00,           /* bDeviceProtocol */
    EP0_MAX_PKT_SIZE,   /* bMaxPacketSize0 */
    /* idVendor */
    USBD_VID & 0x00FF,
    (USBD_VID & 0xFF00) >> 8,
    /* idProduct */
    USBD_PID & 0x00FF,
    (USBD_PID & 0xFF00) >> 8,
    VERSION_LO,
	VERSION_HI,     /* bcdDevice */
    0x01,           /* iManufacture */
    0x02,           /* iProduct */
    0x03,           /* iSerialNumber - no serial */
    0x01            /* bNumConfigurations */
};

/*!<USB Configure Descriptor */
const uint8_t gu8ConfigDescriptor[] =
{
    LEN_CONFIG,     /* bLength */
    DESC_CONFIG,    /* bDescriptorType */
    /* wTotalLength */
    (LEN_CONFIG + LEN_INTERFACE + LEN_HID + LEN_ENDPOINT * 2) & 0x00FF,
    ((LEN_CONFIG + LEN_INTERFACE + LEN_HID + LEN_ENDPOINT * 2) & 0xFF00) >> 8,
    0x01,           /* bNumInterfaces */
    0x01,           /* bConfigurationValue */
    0x00,           /* iConfiguration */
    0x80 | (USBD_SELF_POWERED << 6) | (USBD_REMOTE_WAKEUP << 5),/* bmAttributes */
    USBD_MAX_POWER,         /* MaxPower */

    /* I/F descr: HID */
    LEN_INTERFACE,  /* bLength */
    DESC_INTERFACE, /* bDescriptorType */
    0x00,           /* bInterfaceNumber */
    0x00,           /* bAlternateSetting */
    0x02,           /* bNumEndpoints */
    0x03,           /* bInterfaceClass */
    0x00,           /* bInterfaceSubClass */
    0x00,           /* bInterfaceProtocol */
    0x00,           /* iInterface */

    /* HID Descriptor */
    LEN_HID,        /* Size of this descriptor in UINT8s. */
    DESC_HID,       /* HID descriptor type. */
    0x10, 0x01,     /* HID Class Spec. release number. */
    0x00,           /* H/W target country. */
    0x01,           /* Number of HID class descriptors to follow. */
    DESC_HID_RPT,   /* Descriptor type. */
    /* Total length of report descriptor. */
    sizeof(HID_DeviceReportDescriptor) & 0x00FF,
    (sizeof(HID_DeviceReportDescriptor) & 0xFF00) >> 8,

    /* EP Descriptor: interrupt in. */
    LEN_ENDPOINT,   /* bLength */
    DESC_ENDPOINT,  /* bDescriptorType */
    (INT_IN_EP_NUM | EP_INPUT), /* bEndpointAddress */
    EP_INT,         /* bmAttributes */
    /* wMaxPacketSize */
    EP2_MAX_PKT_SIZE & 0x00FF,
    (EP2_MAX_PKT_SIZE & 0xFF00) >> 8,
    HID_DEFAULT_INT_IN_INTERVAL,        /* bInterval */

    /* EP Descriptor: interrupt out. */
    LEN_ENDPOINT,   /* bLength */
    DESC_ENDPOINT,  /* bDescriptorType */
    (INT_OUT_EP_NUM | EP_OUTPUT),   /* bEndpointAddress */
    EP_INT,         /* bmAttributes */
    /* wMaxPacketSize */
    EP3_MAX_PKT_SIZE & 0x00FF,
    (EP3_MAX_PKT_SIZE & 0xFF00) >> 8,
    HID_DEFAULT_INT_IN_INTERVAL     /* bInterval */
};

/*!<USB Language String Descriptor */
const uint8_t gu8StringLang[4] =
{
    4,              /* bLength */
    DESC_STRING,    /* bDescriptorType */
    0x09, 0x04
};

/*!<USB Vendor String Descriptor */
const uint8_t gu8VendorStringDesc[] =
{
    18,
    DESC_STRING,
    'H', 0, 
	'o', 0, 
	'l', 0, 
	'o', 0, 
	'd', 0,
	'n', 0,
	'a', 0,
	'k', 0,
};

/*!<USB Product String Descriptor */
const uint8_t gu8ProductStringDesc[] =
{
    14,             /* bLength          */
    DESC_STRING,    /* bDescriptorType  */
    'F', 0, 
	'D', 0, 
	'S', 0, 
	'e', 0, 
	'm', 0, 
	'u', 0, 
};



const uint8_t gu8StringSerial[26] =
{
    26,             // bLength
    DESC_STRING,    // bDescriptorType
    'A', 0, 
	'0', 0, 
	'2', 0, 
	'0', 0, 
	'1', 0, 
	'4', 0, 
	'0', 0, 
	'9', 0, 
	'0', 0, 
	'3', 0, 
	'0', 0, 
	'4', 0
};

const uint8_t *gpu8UsbString[4] =
{
    gu8StringLang,
    gu8VendorStringDesc,
    gu8ProductStringDesc,
    gu8StringSerial
};

const S_USBD_INFO_T gsInfo =
{
    gu8DeviceDescriptor,
    gu8ConfigDescriptor,
    gpu8UsbString,
    HID_DeviceReportDescriptor
};


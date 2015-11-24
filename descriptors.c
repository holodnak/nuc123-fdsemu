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
#include "build.h"

#define BUFFER_SIZE 63
#define DISK_BUFFER_SIZE 63

const uint8_t HID_DeviceReportDescriptor[] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)

    0x85, ID_SPI_READ,             //   REPORT_ID (1)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x3f,                    //   REPORT_COUNT (63)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_SPI_READ_STOP,        //   REPORT_ID (2)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x3f,                    //   REPORT_COUNT (63)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_SPI_WRITE,            //   REPORT_ID (3)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x3f,                    //   REPORT_COUNT (63)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_READ_IO,              //   REPORT_ID (16)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_DISK_READ_START,      //   REPORT_ID (17)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_DISK_READ,            //   REPORT_ID (18)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0xff,                    //   REPORT_COUNT (255)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_DISK_WRITE_START,     //   REPORT_ID (19)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_DISK_WRITE,           //   REPORT_ID (20)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0xff,                    //   REPORT_COUNT (255)
    0x91, 0x02,                    //   OUTPUT (Data,Ary,Abs)

    0x85, ID_RESET,                //   REPORT_ID (240)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

    0x85, ID_UPDATEFIRMWARE,       //   REPORT_ID (241)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0xb1, 0x00,                    //   FEATURE (Data,Ary,Abs)

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
    BUILDNUM & 0xFF,
	(BUILDNUM >> 8) & 0xFF,     /* bcdDevice */
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


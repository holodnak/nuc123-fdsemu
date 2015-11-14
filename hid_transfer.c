/******************************************************************************
 * @file     hid_transfer.c
 * @brief    NUC123 series USBD HID transfer sample file
 *
 * @note
 * Copyright (C) 2014~2015 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/

/*!<Includes */
#include <stdio.h>
#include <string.h>
#include "NUC123.h"
#include "hid_transfer.h"
#include "flash.h"
#include "spiutil.h"

uint8_t volatile g_u8EP2Ready = 0;

void hexdump(char *desc, void *addr, int len);

enum {
    SPI_WRITEMAX=64-4,
    SPI_READMAX=63,

    DISK_READMAX=254,
    DISK_WRITEMAX=255,

    //HID reportIDs
    ID_RESET=0xf0,
    ID_UPDATEFIRMWARE=0xf1,
    ID_SELFTEST=0xf2,

    ID_SPI_READ=1,
    ID_SPI_READ_STOP,
    ID_SPI_WRITE,

    ID_READ_IO=0x10,
    ID_DISK_READ_START,
    ID_DISK_READ,
    ID_DISK_WRITE_START,
    ID_DISK_WRITE,
};

void process_send_feature(uint8_t *usbdata,int len);

uint8_t epdata[64 + 1];

void USBD_IRQHandler(void)
{
    uint32_t u32IntSts = USBD_GET_INT_FLAG();
    uint32_t u32State = USBD_GET_BUS_STATE();

//------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_FLDET)
    {
        // Floating detect
        USBD_CLR_INT_FLAG(USBD_INTSTS_FLDET);

        if(USBD_IS_ATTACHED())
        {
            /* USB Plug In */
            USBD_ENABLE_USB();
        }
        else
        {
            /* USB Un-plug */
            USBD_DISABLE_USB();
        }
    }

//------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_BUS)
    {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUS);

        if(u32State & USBD_STATE_USBRST)
        {
            /* Bus reset */
            USBD_ENABLE_USB();
            USBD_SwReset();
        }
        if(u32State & USBD_STATE_SUSPEND)
        {
            /* Enable USB but disable PHY */
            USBD_DISABLE_PHY();
        }
        if(u32State & USBD_STATE_RESUME)
        {
            /* Enable USB and enable PHY */
            USBD_ENABLE_USB();
        }
    }

//------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_USB)
    {
        // USB event
        if(u32IntSts & USBD_INTSTS_SETUP)
        {
            // Setup packet
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_SETUP);

            /* Clear the data IN/OUT ready flag of control end-points */
            USBD_STOP_TRANSACTION(EP0);
            USBD_STOP_TRANSACTION(EP1);

            USBD_ProcessSetupPacket();
        }

        // EP events
        if(u32IntSts & USBD_INTSTS_EP0)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP0);

            // control IN
            USBD_CtrlIn();
        }

        if(u32IntSts & USBD_INTSTS_EP1)
        {
			extern uint8_t g_usbd_SetupPacket[];
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP1);

            // control OUT
            USBD_CtrlOut();
			
			if(g_usbd_SetupPacket[1] == SET_REPORT) {
				USBD_MemCopy(epdata,(uint8_t*)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP1)),64);
				process_send_feature(epdata,64);
			}
			
        }

        if(u32IntSts & USBD_INTSTS_EP2)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP2);
            // Interrupt IN
            EP2_Handler();
        }

        if(u32IntSts & USBD_INTSTS_EP3)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP3);
            // Interrupt OUT
            EP3_Handler();
        }

        if(u32IntSts & USBD_INTSTS_EP4)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP4);
        }

        if(u32IntSts & USBD_INTSTS_EP5)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP5);
        }

        if(u32IntSts & USBD_INTSTS_EP6)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP6);
        }

        if(u32IntSts & USBD_INTSTS_EP7)
        {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP7);
        }
    }
    /* clear unknown event */
    USBD_CLR_INT_FLAG(u32IntSts);
}

void EP2_Handler(void)  /* Interrupt IN handler */
{
//    HID_SetInReport();
}

void EP3_Handler(void)  /* Interrupt OUT handler */
{
    uint8_t *ptr;
    /* Interrupt OUT */
    ptr = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP3));
//    HID_GetOutReport(ptr, USBD_GET_PAYLOAD_LEN(EP3));
    USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
}


/*--------------------------------------------------------------------------*/
/**
  * @brief  USBD Endpoint Config.
  * @param  None.
  * @retval None.
  */
void HID_Init(void)
{
    /* Init setup packet buffer */
    /* Buffer range for setup packet -> [0 ~ 0x7] */
    USBD->STBUFSEG = SETUP_BUF_BASE;

    /*****************************************************/
    /* EP0 ==> control IN endpoint, address 0 */
    USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
    /* Buffer range for EP0 */
    USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);

    /* EP1 ==> control OUT endpoint, address 0 */
    USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
    /* Buffer range for EP1 */
    USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);

    /*****************************************************/
    /* EP2 ==> Interrupt IN endpoint, address 1 */
    USBD_CONFIG_EP(EP2, USBD_CFG_EPMODE_IN | INT_IN_EP_NUM);
    /* Buffer range for EP2 */
    USBD_SET_EP_BUF_ADDR(EP2, EP2_BUF_BASE);

    /* EP3 ==> Interrupt OUT endpoint, address 2 */
    USBD_CONFIG_EP(EP3, USBD_CFG_EPMODE_OUT | INT_OUT_EP_NUM);
    /* Buffer range for EP3 */
    USBD_SET_EP_BUF_ADDR(EP3, EP3_BUF_BASE);
    /* trigger to receive OUT data */
    USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);

}

uint8_t usbbuf[64 + 1 + 192];
int usbbuflen;

enum {
    PAGESIZE=256,
    CMD_READSTATUS=0x05,
    CMD_WRITEENABLE=0x06,
    CMD_READID=0x9f,
    CMD_READDATA=0x03,
    CMD_WRITESTATUS=0x01,
    CMD_PAGEWRITE=0x0a,
    CMD_PAGEERASE=0xdb,
};

uint8_t size;
uint8_t holdcs,initcs;
uint32_t addr;

void process_send_feature(uint8_t *usbdata,int len)
{
	uint8_t *buf = epdata;
	uint8_t reportid;

    USBD_MemCopy((uint8_t *)buf, usbdata, len);

	reportid = buf[0];
	size = buf[1];
	initcs = buf[2];
	holdcs = buf[3];

//	hexdump("process_send_feature: buf",buf,64);

	//spi write
	if(reportid == ID_SPI_WRITE) {
//		printf("process_send_feature: ID_SPI_WRITE: init,hold = %d,%d : len = %d\n",initcs,holdcs,size);
		if(initcs) {
			spi_deselect_device(SPI_FLASH, 0);
			spi_select_device(SPI_FLASH, 0);
		}
		spi_write_packet(SPI_FLASH,buf + 4,size);
		if(holdcs == 0) {
			spi_deselect_device(SPI_FLASH, 0);
		}
	}
	
	//spi read
	else if(reportid == ID_SPI_READ) {
		switch(buf[4]) {
			default:
				printf("process_send_feature: ID_SPI_READ: unknown command $%X\n",buf[4]);
				break;
		}
	}

	//spi read stop
	else if(reportid == ID_SPI_READ_STOP) {
		switch(buf[4]) {
			default:
				printf("process_send_feature: ID_SPI_READ_STOP: unknown command $%X\n",buf[4]);
				break;
		}
	}
	else {
		printf("process_send_feature: unknown reportid $%X\n",reportid);
	}
}

void get_feature_report(uint8_t reportid, int len)
{
	static int readstarted = 0;

	usbbuflen = len;

	//spi read
	if(reportid == ID_SPI_READ) {
		spi_read_packet(SPI_FLASH, usbbuf, len);
//		printf("get_feature_report: ID_SPI_READ: len = %d\n",len);
	}

	//spi read stop
	else if(reportid == ID_SPI_READ_STOP) {
		spi_read_packet(SPI_FLASH, usbbuf, len);
		spi_deselect_device(SPI_FLASH, 0);
//		printf("get_feature_report: ID_SPI_READ_STOP: len = %d\n",len);
	}
	
	else {
		printf("get_feature_report: unknown report id %X\n",reportid);
	}
}

void HID_ClassRequest(void)
{
    uint8_t buf[8];
	volatile uint8_t *ptr;
	uint8_t *ptr2 = epdata;
	int len, i;

    USBD_GetSetupPacket(buf);

    if(buf[0] & 0x80)    /* request data transfer direction */
    {
		ptr = (uint8_t*)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0));
        // Device to host
        switch(buf[1])
        {
            case GET_REPORT:
                if(buf[3] == 3) {
					get_feature_report(buf[2],63);
					ptr[0] = buf[2];
					memcpy(ptr + 1,usbbuf,63);
					USBD_SET_DATA1(EP0);
					USBD_SET_PAYLOAD_LEN(EP0, 64);
					USBD_PrepareCtrlOut(0, 0);
					break;
				}
//             {
//                 break;
//             }
            default:
            {
                /* Setup error, stall the device */
                USBD_SetStall(0);
                break;
            }
        }
    }
    else
    {
		ptr = (uint8_t*)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP1));
        // Host to device
        switch(buf[1])
        {
            case SET_REPORT:
            {
                if(buf[3] == 3)
                {
					USBD_SET_DATA1(EP1);
					USBD_SET_PAYLOAD_LEN(EP1, 64);
                    USBD_PrepareCtrlIn(0, 0);
                }
                break;
            }
            case SET_IDLE:
            {
                /* Status stage */
                USBD_SET_DATA1(EP0);
                USBD_SET_PAYLOAD_LEN(EP0, 0);
                break;
            }
            case SET_PROTOCOL:
//             {
//                 break;
//             }
            default:
            {
                // Stall
                /* Setup error, stall the device */
                USBD_SetStall(0);
                break;
            }
        }
    }
}

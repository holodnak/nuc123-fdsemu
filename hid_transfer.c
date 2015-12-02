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
#include "fds.h"
#include "sram.h"

uint8_t volatile g_u8EP2Ready = 0;

void hexdump(char *desc, void *addr, int len);

enum {
    SPI_WRITEMAX=64-4,
    SPI_READMAX=63,

    DISK_READMAX=254,
    DISK_WRITEMAX=255,

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
			extern uint8_t g_usbd_SetupPacket[];
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

int wasready;
int sequence = 1;

void HID_GetOutReport(uint8_t *pu8EpBuf, uint32_t u32Size)
{
    static uint8_t u8Cmd = 0;
	static int len = 0;
	static int totallen = 0;
	int ret;

//	printf("HID_GetOutReport (report id = %d)\n",pu8EpBuf[0]);

    /* Check if it is in the data phase of write command */
    if((u8Cmd == ID_DISK_WRITE))// && (totalsize < 65500))
    {
		printf("writing data... (totallen = %d) (u32Size = %d)\n",totallen,u32Size);

//		ret = fds_diskwrite_fillbuf(pu8EpBuf,64);
		len += 64;
		totallen += 64;
		
		//fillbuf returns 0 when the buffer is completely filled
		if(ret == 0) {
			sequence++;
			u8Cmd = 0;
			len = 0;

//			printf("writing data... (totallen = %d)\n",totallen);
			
			if(IS_READY() == 0) {
				
				//if was previously ready
				if(wasready) {
					totallen = 0;
					fds_stop_diskwrite();
					USBD_SetStall(2);
				}
				
				//hasnt been ready yet
				else {
					while(IS_READY() == 0);
					wasready = 1;
				}
			}

			if(IS_READY()) {
				fds_diskwrite();
			}

		}
				
//		if(IS_READY() == 0) {
//			fds_stop_diskwrite();
//            USBD_SetStall(3);
//		}
    }
    else
    {
		u8Cmd = pu8EpBuf[0];
		
		if(u8Cmd == ID_DISK_WRITE) {
//			fds_diskwrite_fillbuf(pu8EpBuf + 1,63);
			len = 63;
			totallen += 63;
		}
		else {
			printf("unknown OUT report id = %X\n",u8Cmd);
		}
    }
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
    HID_GetOutReport(ptr, USBD_GET_PAYLOAD_LEN(EP3));
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

uint8_t usbbuf[512];

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

void update_firmware(void)
{
	int i;
	uint32_t addr, data;

	SYS_UnlockReg();
	FMC_Open();
	FMC_EnableAPUpdate();

	for(i=0x8000;i<0x10000;i+=512) {
		if(FMC_Erase(i) == -1) {
			printf("FMC_Erase failed\n");
		}
	}
	flash_read_start(0x8000);
	for(i=0x8000;i<0x10000;i+=4) {
		flash_read((uint8_t*)&data,4);
		FMC_Write(i,data);
	}
	flash_read_stop();

	FMC_DisableAPUpdate();
	FMC_Close();
	printf("firmware updated, rebooting\n");

//	SYS_LockReg();

	//reboot to bootloader
    FMC->ISPCON = 2;
    SYS->IPRSTC1 = 2;
    while(1);
}

void process_send_feature(uint8_t *usbdata,int len)
{
	uint8_t *buf = epdata;
	uint8_t reportid;
	int i;
	static int bytes;

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
			bytes = 0;
		}
		spi_write_packet(SPI_FLASH,buf + 4,size);
		bytes += size;
		if(holdcs == 0) {
			spi_deselect_device(SPI_FLASH, 0);
		}
	}

	//write firmware to aprom
	else if(reportid == ID_FIRMWARE_UPDATE) {
		printf("firmware update requested\n");
		update_firmware();
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

	//begin reading the disk
	else if(reportid == ID_DISK_READ_START) {
//		fds_setup_diskread();

		//TODO: the above and below lines of code do not work well together when being called back-to-back
		//why??
		
		fds_start_diskread();
		sequence = 1;
//		startread = 1;
//		printf("process_send_feature: ID_DISK_READ_START\n");
	}

	else if(reportid == ID_DISK_WRITE_START) {
//		fds_setup_diskread();
		
		fds_start_diskwrite();
		wasready = 0;
		sequence = 1;
//		sequence = 1;
//		startread = 1;
		printf("process_send_feature: ID_DISK_WRITE_START\n");
		fds_diskwrite();
		fds_stop_diskwrite();
	}

	else if(reportid == ID_SRAM_WRITE) {
		if(initcs) {
			spi_deselect_device(SPI_SRAM, 0);
			__NOP();
			spi_select_device(SPI_SRAM, 0);
			bytes = 0;
		}
//		printf("writing sram: initcs = %d, holdcs = %d, size = %d\n",initcs,holdcs,size);
		spi_write_packet(SPI_SRAM,buf + 4,size);
		bytes += size;
		if(holdcs == 0) {
			spi_deselect_device(SPI_SRAM, 0);
		}
	}

	else {
		printf("process_send_feature: unknown reportid $%X\n",reportid);
	}
}

int get_feature_report(uint8_t reportid, uint8_t *buf)
{
	int len = 63;

	//spi read
	if(reportid == ID_SPI_READ) {
		spi_read_packet(SPI_FLASH, buf, len);
//		printf("get_feature_report: ID_SPI_READ: len = %d\n",len);
	}

	//spi read stop
	else if(reportid == ID_SPI_READ_STOP) {
		spi_read_packet(SPI_FLASH, buf, len);
		spi_deselect_device(SPI_FLASH, 0);
//		printf("get_feature_report: ID_SPI_READ_STOP: len = %d\n",len);
	}
	
	else if(reportid == ID_DISK_READ) {
		len = 255;
		buf[0] = sequence++;
		fds_diskread_getdata(buf + 1,254);
		if(IS_READY() == 0) {
			fds_stop_diskread();
			len = 1;
		}
	}

	else {
		printf("get_feature_report: unknown report id %X\n",reportid);
	}

	return(len);
}

void HID_ClassRequest(void)
{
    uint8_t buf[8];
	int len;

    USBD_GetSetupPacket(buf);

    if(buf[0] & 0x80)    /* request data transfer direction */
    {
        // Device to host
        switch(buf[1])
        {
            case GET_REPORT:
                if(buf[3] == 3) {
					len = get_feature_report(buf[2],usbbuf + 1);
					usbbuf[0] = buf[2];
					USBD_PrepareCtrlIn(usbbuf, len + 1);
					USBD_PrepareCtrlOut(0, 0);
					break;
				}
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

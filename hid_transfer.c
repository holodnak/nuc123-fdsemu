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
#include "main.h"
#include "config.h"

uint8_t volatile g_u8EP2Ready = 0;
int wasready;
int sequence = 1;

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
//    uint8_t *ptr;
    /* Interrupt OUT */
//    ptr = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP3));
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

//update firmware stored in flash at $8000
void update_firmware_flash(void)
{
	int i;
	uint32_t data, chksum = 0;

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
		chksum ^= data;
		FMC_Write(i,data);
	}
	flash_read_stop();

	FMC_DisableAPUpdate();
	FMC_Close();

	//erase old firmware copy from the flash
	for(i=0;i<8;i++) {
		flash_erase_sector(0,8 + i);
	}

	printf("chksum = %X\n",chksum);
	if(chksum != 0) {
		printf("firmware checksum error\n");
		SYS_LockReg();
		return;
	}

	printf("firmware updated from flash, rebooting\n");

	//reboot to bootloader
    FMC->ISPCON = 2;
    SYS->IPRSTC1 = 2;
    while(1);
}

//update firmware from data stored in sram
void update_firmware_sram(void)
{
	int i;
	uint32_t data, chksum = 0;

	SYS_UnlockReg();

	FMC_Open();
	FMC_EnableAPUpdate();

	//erase upper 32kb
	for(i=0x8000;i<0x10000;i+=512) {
		if(FMC_Erase(i) == -1) {
			printf("FMC_Erase failed\n");
		}
	}

	//copy from sram to aprom
	for(i=0;i<0x8000;i+=4) {
		sram_read(i, (uint8_t*)&data, 4);
		chksum ^= data;
		FMC_Write(i + 0x8000,data);
	}

	//re-read the data written, making sure crc is ok
	if(chksum == 0) {
		for(i=0;i<0x8000;i+=4) {
			sram_read(i, (uint8_t*)&data, 4);
			data = FMC_Read(i + 0x8000);
			chksum ^= data;
		}
	}

	//if checksum is bad, erase written data
	if(chksum != 0) {
		//erase upper 32kb
		for(i=0x8000;i<0x10000;i+=512) {
			FMC_Erase(i);
		}		
	}

	//finish flash read/write
	FMC_DisableAPUpdate();
	FMC_Close();

	//report error for bad checksum
	if(chksum != 0) {
		printf("firmware checksum error\n");
		SYS_LockReg();
		return;
	}

	printf("firmware updated from sram, rebooting\n");

	//reboot to bootloader
    FMC->ISPCON = 2;
    SYS->IPRSTC1 = 2;
    while(1);
}

void update_firmware(void)
{
	uint32_t id, data, chksum;
	int i;

	//initialize local variables
	id = data = chksum = 0;

	//check for firmware stored in flash (old temporary location)
	flash_read_data(0x10000 - 8,(uint8_t*)&id,4);

	//see if id matches
	if(id == 0xDEADBEEF) {
		
		//now check the checksum to verify the firmware image
		flash_read_start(0x8000);
		for(i=0x8000;i<0x10000;i+=4) {
			flash_read((uint8_t*)&data,4);
			chksum ^= data;
		}
		flash_read_stop();

		//report checksum error
		if(chksum != 0) {
			printf("firmware id found in flash but there was checksum error\n");
		}
		
		//continue updating from flash
		else {
			update_firmware_flash();
			return;
		}
	}
	
	//re-initialize variables and prepare checking for firmware in sram
	id = data = chksum = 0;
	
	//read id from sram
	sram_read(0x8000 - 8,(uint8_t*)&id,4);

	//see if id matches
	if(id == 0xDEADBEEF) {

		//now check the checksum to verify the firmware image
		for(i=0;i<0x8000;i+=4) {
			sram_read(i,(uint8_t*)&data,4);
			chksum ^= data;
		}

		//report checksum error
		if(chksum != 0) {
			printf("firmware id found in sram but there was checksum error\n");
		}
		
		//continue updating from flash
		else {
			update_firmware_sram();
			return;
		}
	}
	
	printf("firmware update image not found\n");
}

uint8_t selftest_result = 0xFF;

void selftest(void)
{
	int n;

	LED_GREEN(0);
	LED_RED(0);
	printf("self test...\n");
	selftest_result = 0;
	if(sram_test() != 0) {
		selftest_result |= 0x10;
	}
	for(n=0;n<3;n++) {
		LED_GREEN(1);
		LED_RED(0);
		TIMER_Delay(TIMER2,100 * 1000);
		LED_GREEN(0);
		LED_RED(1);
		TIMER_Delay(TIMER2,50 * 1000);
	}
	if(selftest_result) {
		LED_GREEN(0);
		LED_RED(1);
	}
	else {
		LED_GREEN(1);
		LED_RED(0);
	}
}

void hexdump2(char *desc, uint8_t (*readfunc)(uint32_t), int pos, int len);

extern uint8_t doctor[];

void process_send_feature(uint8_t *usbdata,int len)
{
	uint8_t *buf = epdata;
	uint8_t reportid;
	uint8_t *ptr;
	static int bytes;
	int i;

    USBD_MemCopy((uint8_t *)buf, usbdata, len);

	reportid = buf[0];
	size = buf[1];
	initcs = buf[2];
	holdcs = buf[3];

	//flash write
	if(reportid == ID_SPI_WRITE) {
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

	//sram write
	else if(reportid == ID_SRAM_WRITE) {
		if(initcs) {
			spi_deselect_device(SPI_SRAM, 0);
			spi_select_device(SPI_SRAM, 0);
			bytes = 0;
		}

		ptr = buf + 4;
		for(i=0;i<size;i++) {
			if(bytes < 0x10000) {
				spi_write_packet(SPI_SRAM,ptr,1);
			}
			else {
				doctor[bytes - 0x10000] = *ptr;
			}
			bytes++;
			ptr++;
		}

//		spi_write_packet(SPI_SRAM,buf + 4,size);
//		bytes += size;
		if(holdcs == 0) {
			spi_deselect_device(SPI_SRAM, 0);
		}
	}

	else if(reportid == ID_RAM_STARTWRITE) {
		spi_deselect_device(SPI_SRAM, 0);
		spi_select_device(SPI_SRAM, 0);
		bytes = 0;
	}

	else if(reportid == ID_RAM_STOPWRITE) {
		spi_deselect_device(SPI_SRAM, 0);
	}

	else if(reportid == ID_RAM_WRITE) {
		ptr = buf + 4;
		for(i=0;i<size;i++) {
			if(bytes < 0x10000) {
				spi_write_packet(SPI_SRAM,ptr,1);
			}
			else {
				doctor[bytes - 0x10000] = *ptr;
			}
			ptr++;
			bytes++;
		}
	}

	//write firmware to aprom
	else if(reportid == ID_UPDATEFIRMWARE) {
		printf("firmware update requested\n");
		update_firmware();
	}

	else if(reportid == ID_FIRMWARE_UPDATE) {
		printf("firmware update requested\n");
		update_firmware();
	}

	//begin reading the disk
	else if(reportid == ID_DISK_READ_START) {
//		fds_setup_diskread();
		
		fds_start_diskread();
		sequence = 1;
	}

	else if(reportid == ID_DISK_WRITE_START) {
//		fds_setup_diskread();
		
		fds_start_diskwrite();
		wasready = 0;
		sequence = 1;
//		printf("process_send_feature: ID_DISK_WRITE_START\n");
//		hexdump2("write dump",lz4_read,3537,256);
		fds_diskwrite();
		fds_stop_diskwrite();
	}

	else if(reportid == ID_SELFTEST) {
		selftest();
	}

	else {
		printf("process_send_feature: unknown reportid $%X\n",reportid);
	}
}

int get_feature_report(uint8_t reportid, uint8_t *buf)
{
	int len = 63;

	//flash read
	if(reportid == ID_SPI_READ) {
		spi_read_packet(SPI_FLASH, buf, len);
	}
	else if(reportid == ID_SPI_READ_STOP) {
		spi_read_packet(SPI_FLASH, buf, len);
		spi_deselect_device(SPI_FLASH, 0);
	}

	//sram read
	else if(reportid == ID_SRAM_READ) {
		spi_read_packet(SPI_SRAM, buf, len);
	}
	else if(reportid == ID_SRAM_READ_STOP) {
		spi_read_packet(SPI_SRAM, buf, len);
		spi_deselect_device(SPI_SRAM, 0);
	}
	
	//disk read
	else if(reportid == ID_DISK_READ) {
		buf[0] = sequence++;
		len = fds_diskread_getdata(buf + 1,254) + 1;
		if(len < 255) {
			fds_stop_diskread();
		}
	}

	//self testing result
	else if(reportid == ID_SELFTEST) {
		buf[0] = selftest_result;
		len = 1;
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
					
					//data stage
					len = get_feature_report(buf[2],usbbuf + 1);
					usbbuf[0] = buf[2];
					USBD_PrepareCtrlIn(usbbuf, len + 1);
					
					//status stage
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
                if(buf[3] == 3) {
//					printf("set_report: buf[6] = %d\n",buf[6]);
					//data stage
					USBD_PrepareCtrlOut((uint8_t *)&epdata, buf[6]);

					//status stage
					USBD_SET_DATA1(EP0);
					USBD_SET_PAYLOAD_LEN(EP0, 0);
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

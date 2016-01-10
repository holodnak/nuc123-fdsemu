#include <stdio.h>
#include <string.h>
#include "NUC123.h"
#include "fds.h"
#include "fdsutil.h"
#include "flash.h"
#include "spiutil.h"
#include "fifo.h"
#include "main.h"
#include "sram.h"
#include "transfer.h"

//time in milliseconds for disk flip delay
#define FLIPDELAY 1333

void fds_init(void)
{
	int usbattached = USBD_IS_ATTACHED();
	
//	usbattached = 0;
	if(usbattached) {
		fds_setup_diskread();
		CLEAR_WRITE();
		SET_STOPMOTOR();
		CLEAR_SCANMEDIA();
	}
	else {
		fds_setup_transfer();
		CLEAR_WRITABLE();
		CLEAR_READY();
		CLEAR_MEDIASET();
		CLEAR_MOTORON();
		
		//insert loader image
		fds_insert_disk(-1);
	}
}

enum {
	MODE_TRANSFER = 0,
	MODE_DISKREAD
};

int mode = MODE_TRANSFER;

//setup for talking to the ram adaptor
void fds_setup_transfer(void)
{
    /* Unlock protected registers */
//    SYS_UnlockReg();

#ifdef PROTOTYPE
	//setup gpio pins for the fds
    GPIO_SetMode(PD, BIT5, GPIO_PMD_INPUT);
    GPIO_SetMode(PD, BIT4, GPIO_PMD_INPUT);
    GPIO_SetMode(PD, BIT3, GPIO_PMD_OUTPUT);
    GPIO_SetMode(PD, BIT2, GPIO_PMD_OUTPUT);
    GPIO_SetMode(PD, BIT1, GPIO_PMD_OUTPUT);
    GPIO_SetMode(PD, BIT0, GPIO_PMD_OUTPUT);
    GPIO_SetMode(PA, BIT12, GPIO_PMD_INPUT);
    GPIO_SetMode(PA, BIT11, GPIO_PMD_OUTPUT);
    GPIO_SetMode(PB, BIT14, GPIO_PMD_INPUT);

	GPIO_DisableInt(PA, 11);
    GPIO_EnableEINT0(PB, 14, GPIO_INT_RISING);

	GPIO_ENABLE_DEBOUNCE(PD, BIT4);
	GPIO_ENABLE_DEBOUNCE(PA, BIT12);
#else

	GPIO_SetMode(PF, BIT3, GPIO_PMD_INPUT);		//-write
	GPIO_SetMode(PB, BIT8, GPIO_PMD_INPUT);		//-scanmedia
	GPIO_SetMode(PA, BIT11,GPIO_PMD_OUTPUT);	//motoron
	GPIO_SetMode(PA, BIT10,GPIO_PMD_OUTPUT);	//-writable
	GPIO_SetMode(PB, BIT5, GPIO_PMD_OUTPUT);	//-mediaset
	GPIO_SetMode(PB, BIT6, GPIO_PMD_OUTPUT);	//-ready
	GPIO_SetMode(PB, BIT7, GPIO_PMD_INPUT);		//-stopmotor
	GPIO_SetMode(PB, BIT4, GPIO_PMD_OUTPUT);	//read data
	GPIO_SetMode(PB, BIT14,GPIO_PMD_INPUT);		//write data

	GPIO_DisableInt(PB, 4);
    GPIO_EnableEINT0(PB, 14, GPIO_INT_RISING);

	GPIO_ENABLE_DEBOUNCE(PD, BIT8);
	GPIO_ENABLE_DEBOUNCE(PB, BIT7);

#endif
//	SYS_LockReg();

	mode = MODE_TRANSFER;
	printf("entering ram adaptor transfer mode\n");
}

//setup for reading/writing disks with the drive
void fds_setup_diskread(void)
{
    /* Unlock protected registers */
//    SYS_UnlockReg();

#ifdef PROTOTYPE
	GPIO_DISABLE_DEBOUNCE(PD, BIT4);
	GPIO_DISABLE_DEBOUNCE(PA, BIT12);

	//setup gpio pins for the fds
	GPIO_SetMode(PD, BIT5, GPIO_PMD_OUTPUT);	//-write
	GPIO_SetMode(PD, BIT4, GPIO_PMD_OUTPUT);	//-scanmedia
	GPIO_SetMode(PD, BIT3, GPIO_PMD_INPUT);		//motoron
	GPIO_SetMode(PD, BIT2, GPIO_PMD_INPUT);		//-writable
	GPIO_SetMode(PD, BIT1, GPIO_PMD_INPUT);		//-mediaset
	GPIO_SetMode(PD, BIT0, GPIO_PMD_INPUT);		//-ready
	GPIO_SetMode(PA, BIT12, GPIO_PMD_OUTPUT);	//-stopmotor
	GPIO_SetMode(PA, BIT11, GPIO_PMD_INPUT);	//read data
	GPIO_SetMode(PB, BIT14, GPIO_PMD_OUTPUT);	//write data

	GPIO_DisableEINT0(PB, 14);
	GPIO_EnableInt(PA, 11, GPIO_INT_RISING);
#else
	//setup gpio pins for the fds
	GPIO_SetMode(PF, BIT3, GPIO_PMD_OUTPUT);	//-write
	GPIO_SetMode(PB, BIT8, GPIO_PMD_OUTPUT);	//-scanmedia
	GPIO_SetMode(PA, BIT11, GPIO_PMD_INPUT);		//motoron
	GPIO_SetMode(PA, BIT10, GPIO_PMD_INPUT);		//-writable
	GPIO_SetMode(PB, BIT5, GPIO_PMD_INPUT);		//-mediaset
	GPIO_SetMode(PB, BIT6, GPIO_PMD_INPUT);		//-ready
	GPIO_SetMode(PB, BIT7, GPIO_PMD_OUTPUT);	//-stopmotor
	GPIO_SetMode(PB, BIT4, GPIO_PMD_INPUT);	//read data
	GPIO_SetMode(PB, BIT14, GPIO_PMD_OUTPUT);	//write data

	GPIO_DisableEINT0(PB, 14);
	GPIO_EnableInt(PB, 4, GPIO_INT_RISING);

#endif

//	SYS_LockReg();

	mode = MODE_DISKREAD;
	printf("entering disk read mode\n");
}

int find_first_disk_side(int block)
{
	flash_header_t header;

	//ensure this isnt the first block, if it is this must be the first side of a disk
	if(block == 0) {
		return(block);
	}

	//keep going in reverse to find the first disk side
	while(block > 0) {

		flash_read_disk_header(block,&header);
		
		//part of a multiside game
		if(header.name[0] != 0) {
			break;
		}

		block--;
	}

	return(block);
}

void fds_tick(void)
{
	static int mediaset = 0;
	static int ready = 0;

	if(mode == MODE_DISKREAD) {
		if(IS_MEDIASET() && mediaset == 0) {
			mediaset = 1;
			printf("disk inserted\n");
			if(IS_WRITABLE()) {
				printf("...and it is writable\n");
			}
		}
		if(IS_MEDIASET() == 0) {
			if(mediaset) {
				printf("disk ejected\n");
			}
			mediaset = 0;
		}
		if(IS_READY() && ready == 0) {
			ready = 1;
			printf("ready activated\n");
		}
		if(IS_READY() == 0) {
			if(ready) {
				printf("ready deactivated\n");
				CLEAR_SCANMEDIA();
				SET_STOPMOTOR();
			}
			ready = 0;
		}
		return;
	}
	
	//if the button has been pressed to flip disk sides
	if(SWITCH != 0) {
		flash_header_t header;
		
		CLEAR_MEDIASET();
		CLEAR_WRITABLE();
		flash_read_disk_header(diskblock + 1,&header);
		
		//filename is 0's...must be more sides to this disk
		if(header.name[0] == 0) {
			diskblock++;
		}
		
		//try to find the first disk side
		else {
			diskblock = find_first_disk_side(diskblock);
		}

		printf("new disk side slot = %d\n",diskblock);
		delay_ms(FLIPDELAY);

		//wait for button to be released before we insert disk
		while(SWITCH != 0);

		printf("delay over\n");
		fds_insert_disk(diskblock);
		
		SET_MEDIASET();			
		SET_WRITABLE();
	}
	
	//check if ram adaptor wants to stop the motor
	if(IS_STOPMOTOR()) {
		CLEAR_MOTORON();
	}

	//if ram adaptor wants to scan the media and run the motor
	if(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {

		SET_MOTORON();

		if(diskblock == -1) {
			begin_transfer_loader();
		}
		else {
			begin_transfer();
		}
	}
}

#define COPYBUFFERSIZE	256
static uint8_t copybuffer[COPYBUFFERSIZE];

void loader_copy(int location);

static const char loaderfilename[] = "loader.fds";

void fds_insert_disk(int block)
{
	int i;

	diskblock = block;
//	fds_setup_transfer();
	
	//decompress loader to sram
	if(block == -1) {
		flash_read_data(0,copybuffer,256);
		
		//check if slot 0 has an image named "loader.fds"
		if(memcmp(copybuffer,loaderfilename,10) == 0) {
			loader_copy(1);
			printf("inserting loader disk image from flash\r\n");
		}
		
		//decompress loader into sram
		else {
			loader_copy(0);
			printf("inserting loader disk image from firmware\r\n");
		}
	}
	
	//copy image from flash to sram
	else {
		printf("copying image to sram...\r\n");
		for(i=0;i<0x10000;i+=COPYBUFFERSIZE) {
			flash_read_data((diskblock * 0x10000) + i,copybuffer,COPYBUFFERSIZE);
			sram_write(i,copybuffer,COPYBUFFERSIZE);
		}
		printf("inserting disk at block %d\r\n",block);
	}
	SET_MEDIASET();
	SET_WRITABLE();
}

void fds_remove_disk(void)
{
	CLEAR_MEDIASET();
	CLEAR_READY();
	printf("removing disk\r\n");
}

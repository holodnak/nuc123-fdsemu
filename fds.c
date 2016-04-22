#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "NUC123.h"
#include "fds.h"
#include "fdsutil.h"
#include "flash.h"
#include "spiutil.h"
#include "fifo.h"
#include "main.h"
#include "sram.h"
#include "transfer.h"

/*
6 = motor on
7 = -writable media
A = -media set
*/

//time in milliseconds for disk flip delay
#define FLIPDELAY 1500

uint8_t copybuffer[COPYBUFFERSIZE];
volatile uint8_t writebuf[WRITEBUFSIZE];

enum {
	MODE_TRANSFER = 0,
	MODE_EMULATE,
	MODE_DISKREAD
};

int mode = MODE_TRANSFER;
int ir_incoming = 0;

int savediskblock;

void fds_init(void)
{
	int usbattached = USBD_IS_ATTACHED();
	
//	usbattached = 0;
	ir_incoming = 0;
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
		savediskblock = -1;
	}
}

//setup for talking to the ram adaptor
void fds_setup_transfer(void)
{

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

	//for ir remote control disk flipping
//	GPIO_EnableInt(PB, 10, GPIO_INT_FALLING);
//    NVIC_EnableIRQ(GPAB_IRQn);

	mode = MODE_TRANSFER;
	printf("entering ram adaptor transfer mode\n");
}

//setup for reading/writing disks with the drive
void fds_setup_diskread(void)
{

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
	GPIO_SetMode(PF, BIT3, GPIO_PMD_OUTPUT);		//-write
	GPIO_SetMode(PB, BIT8, GPIO_PMD_OUTPUT);		//-scanmedia
	GPIO_SetMode(PA, BIT11, GPIO_PMD_INPUT);		//motoron
	GPIO_SetMode(PA, BIT10, GPIO_PMD_INPUT);		//-writable
	GPIO_SetMode(PB, BIT5, GPIO_PMD_INPUT);			//-mediaset
	GPIO_SetMode(PB, BIT6, GPIO_PMD_INPUT);			//-ready
	GPIO_SetMode(PB, BIT7, GPIO_PMD_OUTPUT);		//-stopmotor
	GPIO_SetMode(PB, BIT4, GPIO_PMD_INPUT);			//read data
	GPIO_SetMode(PB, BIT14, GPIO_PMD_OUTPUT);		//write data
//	GPIO_SetMode(PB, BIT14, GPIO_PMD_OPEN_DRAIN);	//write data

	GPIO_DisableEINT0(PB, 14);
	GPIO_EnableInt(PB, 4, GPIO_INT_RISING);

#endif

	//disable ir remote control
//	GPIO_DisableInt(PB, 10);

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
	static int ready = 0;
	int diskflip = 0;

	if(mode == MODE_DISKREAD) {
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

/*	if(boardver > 1) {
		int num;

		num = read_ir_pulses();
		if(num > 0) {
			printf("read %d ir pulses\n", num);
			printpulses();
			if(IRcompare(num,IRsignal,sizeof(IRsignal)/4)) {
				printf("disk flip!\n");
				diskflip = 1;
			}
		}
		
		//TODO: this has changed to be "normal" (like old board version)
		if(SWITCH == 0) {
			diskflip = 1;
			PB9 = 1;
		}
	}

	else {
		if(SWITCH != 0)
			diskflip = 1;
	}*/

	if(SWITCH != 0) {
		diskflip = 1;
	}

	//if the button has been pressed to flip disk sides
	if(diskflip) {
		flash_header_t header;

		CLEAR_MEDIASET();
		CLEAR_WRITABLE();
		
		//read current disks header
		sram_read(0,(uint8_t*)&header,256);
		
		//if the savedisk has been previously loaded, do not allow any other disk

		//if this is the save disk for gamedoctor games
		if((header.flags & 3) == 3 && savediskblock != -1) {
			
			diskblock = savediskblock;
			printf("keeping save disk\n");
		}
		
		//has valid ownerid/nextid fields
		else if(header.flags & 0x20) {
			if(header.nextid == 0xFFFF) {
				if(savediskblock == -1) {
					diskblock = header.ownerid;
				}
				else {
					diskblock = savediskblock;
				}
			}
			else {
				diskblock = header.nextid;
			}
		}

		//old linear style disk storage
		else {

			//read disk header for next disk block
			flash_read_disk_header(diskblock + 1,&header);
			
			//filename is 0's...must be more sides to this disk
			if(header.name[0] == 0) {
				diskblock++;
			}
			
			//try to find the first disk side
			else {
				diskblock = find_first_disk_side(diskblock);
			}
		}
		
		//if this disk has a save disk
		if(header.flags & 0x10) {
			savediskblock = header.saveid;
			printf("disk has a save disk, block id %d\n",savediskblock);
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
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {

		SET_MOTORON();

		if(diskblock == -1) {
			begin_transfer_loader();
		}
		else {
			begin_transfer();
		}
	}
}

void loader_copy(int location);

static const char loaderfilename[] = "loader.fds";

int decompress_lz4(uint8_t(*cb_readsrc)(uint32_t), int srclen, uint8_t(*cb_read)(uint32_t), void(*cb_write)(uint32_t,uint8_t));

static uint8_t lz4_readsrc(uint32_t addr)
{
	uint8_t ret;
	
	addr += diskblock * 0x10000 + 256;
	flash_read_data(addr,&ret,1);
	return(ret);
}

extern uint8_t doctor[];

static uint8_t lz4_read(uint32_t addr)
{
	uint8_t ret;

	addr += 256;
	if(addr < 0x10000) {
		sram_read(addr,&ret,1);
	}
	else {
		ret = doctor[addr - 0x10000];
	}
	return(ret);
}

static void lz4_write(uint32_t addr, uint8_t data)
{
	addr += 256;
	if(addr < 0x10000) {
		sram_write(addr,&data,1);
	}
	else {
		doctor[addr - 0x10000] = data;
	}
}

void fds_insert_loader()
{
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
	SET_MEDIASET();
	SET_WRITABLE();
}

/*
called when a new disk side set is selected (the loader code should only call this)
*/
void fds_insert_new_disk(int block)
{
	uint8_t flags;

	//decompress loader to sram
	if(block == -1) {
		fds_insert_loader();
		return;
	}

	//read header for this disk
	flash_read_data(block * 0x10000,copybuffer,COPYBUFFERSIZE);
	sram_write(0x0000,copybuffer,COPYBUFFERSIZE);
	flags = copybuffer[248];
//	size = copybuffer[240] | (copybuffer[241] << 8);
	savediskblock = -1;

	//check if this disk image has a save disk, if so, insert it first
	if(flags & 0x10) {
		fds_insert_disk(copybuffer[246] | (copybuffer[247] << 8));
	}
	
	//disk image doesnt have a save block
	else {
		fds_insert_disk(block);
	}
}

void fds_insert_disk(int block)
{
	int i;
	uint8_t flags;
	uint16_t size;

	//save block selection
	diskblock = block;

	//decompress loader to sram
	if(block == -1) {
		fds_insert_loader();
		return;
	}
	
	//copy image from flash to sram

	//read header
	flash_read_data(diskblock * 0x10000,copybuffer,COPYBUFFERSIZE);
	sram_write(0x0000,copybuffer,COPYBUFFERSIZE);
	flags = copybuffer[248];
	size = copybuffer[240] | (copybuffer[241] << 8);

	//disk image is compressed (and therefore read-only)
	if(flags & 0x80) {
		flags |= 0x40;		//ensure read-only bit is set
		printf("decompressing image to sram...\r\n");
		i = decompress_lz4(lz4_readsrc,size,lz4_read,lz4_write);
		printf("decompressed image from %d to %d bytes (%d%% ratio)\n",size, i, 100 * size / i);
	}
	
	else {
		printf("copying image to sram...\r\n");
		for(i=0;i<0x10000;i+=COPYBUFFERSIZE) {
			flash_read_data((diskblock * 0x10000) + i,copybuffer,COPYBUFFERSIZE);
			sram_write(i,copybuffer,COPYBUFFERSIZE);
		}
		printf("verifying image in sram...\r\n");
		for(i=0;i<0x10000;i+=COPYBUFFERSIZE) {
			flash_read_data((diskblock * 0x10000) + i,copybuffer,COPYBUFFERSIZE);
			sram_read(i,(uint8_t*)writebuf,COPYBUFFERSIZE);
			if(memcmp(copybuffer,(uint8_t*)writebuf,COPYBUFFERSIZE) != 0) {
				printf("error copying to sram\n");
				break;
			}
		}
	}
	printf("inserting disk at block %d\r\n",block);
	SET_MEDIASET();
	SET_WRITABLE();
}

void fds_remove_disk(void)
{
	CLEAR_MEDIASET();
	CLEAR_READY();
	printf("removing disk\r\n");
}

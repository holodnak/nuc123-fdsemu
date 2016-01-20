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

//time in milliseconds for disk flip delay
#define FLIPDELAY 1500

uint8_t copybuffer[COPYBUFFERSIZE];
volatile uint8_t writebuf[WRITEBUFSIZE];

enum {
	MODE_TRANSFER = 0,
	MODE_DISKREAD
};

int mode = MODE_TRANSFER;
int ir_incoming = 0;

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
	}
}

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

	//for ir remote control disk flipping
//	GPIO_EnableInt(PB, 10, GPIO_INT_FALLING);
//    NVIC_EnableIRQ(GPAB_IRQn);

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

	//disable ir remote control
//	GPIO_DisableInt(PB, 10);

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

int IRsignal[] = {
// ON, OFF (in 10's of microseconds)
        730, 340,
        50, 40,
        50, 130,
        50, 120,
        50, 130,
        50, 40,
        50, 120,
        50, 130,
        50, 130,
        50, 120,
        50, 130,
        50, 120,
        50, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 120,
        50, 130,
        50, 40,
        50, 120,
        50, 130,
        50, 120,
        50, 40,
        50, 130,
        50, 40,
        50, 40,
        50, 40,
        40, 130,
        50, 40,
        50, 130,
        40, 130,
        50, 40,
        50, 130,
        40, 2860,
        720, 350,
        40, 40,
        50, 130,
        50, 130,
        40, 130,
        50, 40,
        50, 130,
        40, 130,
        50, 130,
        50, 120,
        50, 130,
        50, 120,
        50, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 120,
        50, 40,
        50, 40,
        50, 130,
        50, 40,
        50, 40,
        40, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 120,
        50, 40,
        50, 130,
        50, 120,
        50, 40,
        50, 130,
        50, 0};


#define RESOLUTION	10
#define FUZZINESS	25
#define MAXPULSE	(65000 / RESOLUTION)
#define NUMPULSES	200

uint16_t pulses[NUMPULSES][2];  // pair is high and low pulse 
uint8_t currentpulse = 0; // index for pulses we're storing

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define DEBUG
int IRcompare(int numpulses, int Signal[], int refsize) {
  int count = MIN(numpulses,refsize);
	int i;

  printf("count set to: %d\n", count);

for (i=0; i< count-1; i++) {
    int oncode = pulses[i][1] * RESOLUTION;
    int offcode = pulses[i+1][0] * RESOLUTION;
    
#ifdef DEBUG    
	printf("%d - %d", oncode, Signal[i*2 + 0]);
#endif   
    
    // check to make sure the error is less than FUZZINESS percent
    if ( abs(oncode - Signal[i*2 + 0]) <= (Signal[i*2 + 0] * FUZZINESS / 100)) {
#ifdef DEBUG
		printf(" (ok)");
#endif
    } else {
#ifdef DEBUG
		printf(" (x)");
#endif
      // we didn't match perfectly, return a false match
      return 0;
    }
    
    
#ifdef DEBUG
	printf("  \t%d - %d", offcode, Signal[i*2 + 1]);
#endif    
    
    if ( abs(offcode - Signal[i*2 + 1]) <= (Signal[i*2 + 1] * FUZZINESS / 100)) {
#ifdef DEBUG
		printf(" (ok)");
#endif
    } else {
#ifdef DEBUG
		printf(" (x)");
#endif
      // we didn't match perfectly, return a false match
      return 0;
    }
    
#ifdef DEBUG
	printf("\n");
#endif
  }
  // Everything matched!
  return 1;
}

void printpulses(void) {
	uint8_t i;
  printf("\n\r\n\rReceived: \n\rOFF \t\tON\n");
  for (i = 0; i < currentpulse; i++) {
    printf("  %d us,\t%d us\n",pulses[i][0] * RESOLUTION,pulses[i][1] * RESOLUTION);
  }

  // print it in a 'array' format
  printf("int IRsignal[] = {\n");
  printf("// ON, OFF (in 10's of microseconds)\n");
  for (i = 0; i < currentpulse-1; i++) {
    printf("\t"); // tab
    printf("%d",pulses[i][1] * RESOLUTION);
    printf(", ");
    printf("%d",pulses[i+1][0] * RESOLUTION);
    printf(",\n");
  }
  printf("\t"); // tab
  printf("%d",pulses[currentpulse-1][1] * RESOLUTION);
  printf(", 0};\n");
}

int read_ir_pulses(void)
{
	uint16_t highpulse, lowpulse; // temporary storage timing

	currentpulse = 0;
	if(IRDATA) {
		return(0);
	}
	for(;;) {
		highpulse = lowpulse = 0; // start out with no pulse length

		while(IRDATA) {
			highpulse++;
			TIMER_Delay(TIMER2, RESOLUTION);
			if (((highpulse >= MAXPULSE) /*&& (currentpulse != 0)*/) || currentpulse == NUMPULSES) {
				return currentpulse;
			}
		}
		pulses[currentpulse][0] = highpulse;

		while(IRDATA == 0) {
			lowpulse++;
			TIMER_Delay(TIMER2, RESOLUTION);
			if (((lowpulse >= MAXPULSE) /*&& (currentpulse != 0)*/) || currentpulse == NUMPULSES) {
				return currentpulse;
			}
		}
		pulses[currentpulse][1] = lowpulse;

		currentpulse++;
	}
}


void fds_tick(void)
{
	static int mediaset = 0;
	static int ready = 0;
	int diskflip = 0;

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
	
/*	if(ir_incoming) */{
		int num;

//		NVIC_DisableIRQ(GPAB_IRQn);
//		ir_incoming = 0;

		num = read_ir_pulses();
		if(num > 0) {
			printf("read %d ir pulses\n", num);
			printpulses();
			if(IRcompare(num,IRsignal,sizeof(IRsignal)/4)) {
				printf("disk flip!\n");
				diskflip = 1;
			}
		}
//		else {printf("no ir... :(\n");}
//		NVIC_EnableIRQ(GPAB_IRQn);
	}
	
	if(boardver > 1) {
		if(SWITCH == 0) {
			diskflip = 1;
			PB9 = 1;
		}
	}
	else {
		if(SWITCH != 0)
			diskflip = 1;
	}

	//if the button has been pressed to flip disk sides
	if(diskflip) {
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
//		while(SWITCH != 0);

		printf("delay over\n");
		fds_insert_disk(diskblock);
		
		SET_MEDIASET();			
		SET_WRITABLE();

		if(boardver > 1) {
			if(SWITCH == 0) {
				diskflip = 1;
				PB9 = 1;
			}
		}
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

void fds_insert_disk(int block)
{
	int i;
	uint8_t flags;
	uint16_t size;

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

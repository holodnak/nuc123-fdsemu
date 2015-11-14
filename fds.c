#include <stdio.h>
#include "NUC123.h"
#include "fds.h"
#include "flash.h"

/*
PIN 43 = -write
PIN 42 = -scan media
PIN 41 = motor on
PIN 40 = -writeable media
PIN 39 = -media set
PIN 38 = -ready
PIN 34 = -stop motor
? = WRITEDATA
PIN 48 = data
PIN 47 = rate
*/

volatile int rate = 0;
volatile int diskblock = 0;
volatile int count = 0;

volatile uint8_t data, data2;
volatile int outbit = 0;
volatile int needbyte;
volatile int bytes;

uint8_t writelen;
int havewrite;

void TMR1_IRQHandler(void)
{
	TIMER_ClearIntFlag(TIMER1);

	rate ^= 1;
	if(rate) {
		count++;
		if(count == 8) {
			count = 0;
			data = data2;
			needbyte++;
		}
		outbit = data & 1;
		data >>= 1;
	}
	PA11 = (outbit ^ rate) & 1;
}

void EINT0_IRQHandler(void)
{
    GPIO_CLR_INT_FLAG(PB, BIT14);

	if(IS_WRITE()) {
		int ra = TIMER_GetCounter(TIMER0);

		TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
		TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;
		writelen = (uint8_t)ra;
		havewrite++;
	}
}

#define DECODEBUFSIZE	(1024 * 12)

uint8_t sectorbuf[4096];
uint8_t decodebuf[DECODEBUFSIZE];

struct write_s {
	int diskpos;			//position on disk where write was started
	int decstart;			//position in decodebuf the data begins
	int decend;				//position in decodebuf the data ends
} writes[4];

int write_num;				//number of writes

enum {
	//	DEFAULT_LEAD_IN=28300,      //#bits (~25620 min)
	GAP=976/8-1,                //(~750 min)
	MIN_GAP_SIZE=0x300,         //bits
	FDSSIZE=65500,              //size of .fds disk side, excluding header
	FLASHHEADERSIZE=0x100,
};

__inline uint8_t raw_to_raw03_byte(uint8_t raw)
{
/*
  59 59 5a 5b 5b 5b 5a 5b
  89 b8 89 5a 5b 5a 5b 89
*/
	if(raw < 0x50)
		return(3);
	else if(raw < 0x78)
		return(0);
	else if(raw < 0xA0)
		return(1);
	else if(raw < 0xD0)
		return(2);
	return(3);
}


void hexdump(char *desc, void *addr, int len);

__inline void decode(uint8_t *dst, uint8_t src, int dstSize, int *outptr) {
//	static int foundstart = 0;
	static char bitval;
	int out = *outptr;

	if(dst == 0) {
//		foundstart = 0;
		bitval = 0;
		return;
	}
/*	if(foundstart == 0 && src == 1) {
		foundstart = 1;
		dst[out / 8] = 0x80;
		*outptr = ((out / 8) + 1) * 8;
		return;
	}*/
	switch(src | (bitval << 4)) {
		case 0x11:
			out++;
		case 0x00:
			out++;
			bitval=0;
			break;
		case 0x12:
			out++;
		case 0x01:
		case 0x10:
			dst[out/8] |= 1 << (out & 7);
			out++;
			bitval=1;
			break;
		default: //Unexpected value.  Keep going, we'll probably get a CRC warning
//				printf("glitch(%d) @ %X(%X.%d)\n", src[in], in, out/8, out%8);
			out++;
			bitval=0;
			break;
	}
	*outptr = out;
}

static void begin_transfer(void)
{
	int i, j;
	int decodelen = 0;
	int leadin = 28300;

	printf("beginning transfer...\r\n");

	flash_read_disk_start(diskblock);
	needbyte = 0;
	count = 7;
	havewrite = 0;
	writelen = 0;
	
	write_num = 0;
	
	for(i=0;i<DECODEBUFSIZE;i++) {
		decodebuf[i] = 0;
	}


    NVIC_DisableIRQ(USBD_IRQn);
    NVIC_EnableIRQ(EINT0_IRQn);
    NVIC_EnableIRQ(TMR1_IRQn);	
	TIMER_Start(TIMER0);
	TIMER_Start(TIMER1);

	bytes = 0;
	needbyte = 0;
	count = 7;
	havewrite = 0;
	writelen = 0;

	//transfer lead-in
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
		if(needbyte) {
			needbyte = 0;
			data2 = 0;
			leadin -= 8;
			if(leadin <= 0) {
				flash_read_disk((uint8_t*)&data2,1);
				bytes++;
				break;
			}
		}
	}

	//transfer disk data
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
		if(IS_WRITE()) {
			int len = 0;

			writes[write_num].diskpos = bytes + 2;
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
			TIMER0->TCMPR = 0xFFFFFF;
			TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;
			decode(0,0,0,0);
			while(IS_WRITE()) {
				if(havewrite) {
					havewrite = 0;
					decode(decodebuf + decodelen,raw_to_raw03_byte(writelen),DECODEBUFSIZE,&len);
				}
				if(needbyte) {
					needbyte = 0;
					flash_read_disk((uint8_t*)&data2,1);
					bytes++;
					if(bytes >= 0xFF00) {
						printf("reached end of data block, something went wrong...\r\n");
						break;
					}
				}
			}
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
			len = (len / 8) + 2;
			writes[write_num].decstart = decodelen;
			writes[write_num].decend = decodelen + len;
			printf("finished write %d, start = %d, end = %d (len = %d)\r\n",write_num,decodelen,decodelen + len,len);
			decodelen += len;
			write_num++;
		}
		if(needbyte) {
			needbyte = 0;
			flash_read_disk((uint8_t*)&data2,1);
			bytes++;
			if(bytes >= 0xFF00) {
				printf("reached end of data block, something went wrong...\r\n");
				break;
			}
		}
	}
    NVIC_DisableIRQ(EINT0_IRQn);
    NVIC_DisableIRQ(TMR1_IRQn);
	TIMER_Stop(TIMER0);
	TIMER_Stop(TIMER1);
    NVIC_EnableIRQ(USBD_IRQn);

	flash_read_disk_stop();
	
	//needs to be cleaned up/optimized
	if(write_num) {

		//write the written data to flash
		for(i=0;i<write_num;i++) {
			int pos = writes[i].diskpos + 256;
			int start = writes[i].decstart;
			int end = writes[i].decend;
			int size = (end - start);// + 122 + 40;
			int sector = pos >> 12;
			int sectoraddr = pos & 0xFFF;
			uint8_t *decodeptr = decodebuf + start;

			printf("writing to flash: diskpos %d, size %d, sector = %d, sectoraddr = %X (decstart = %d, decsize = %d)\r\n",pos,size,sector,sectoraddr,start,size);
			flash_read_sector(diskblock,sector,sectorbuf);
			for(j=sectoraddr;j<4096 && size;j++, size--) {
				sectorbuf[j] = *decodeptr++;
			}
			flash_write_sector(diskblock,sector,sectorbuf);
			if(size) {
				printf("write spans two sectors...\n");
				sector++;
				flash_read_sector(diskblock,sector,sectorbuf);
				for(j=0;j<4096;j++, size--) {
					sectorbuf[j] = *decodeptr++;
				}
				flash_write_sector(diskblock,sector,sectorbuf);
				if(size) {
					printf("write spans three sectors!!  D: D: D:\n");
				}
			}
		}
	}
	printf("transferred %d bytes\r\n",bytes);
}

void fds_init(void)
{
	CLEAR_WRITABLE();
	CLEAR_READY();
	CLEAR_MEDIASET();
	CLEAR_MOTORON();
	SET_WRITABLE();
}

void fds_tick(void)
{
	//check if ram adaptor wants to stop the motor
	if(IS_STOPMOTOR()) {
		CLEAR_MOTORON();
	}

	//if ram adaptor wants to scan the media
	if(IS_SCANMEDIA()) {

		//if ram adaptor doesnt want to stop the motor
		if(IS_DONT_STOPMOTOR()) {

			SET_MOTORON();

			TIMER_Delay(TIMER2, 20 * 1000);

			if(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
				TIMER_Delay(TIMER2, 130 * 1000);
				SET_READY();
				begin_transfer();
				CLEAR_READY();
			}
		}
	}
}

/*
	virtual "insert disk"
	
	copies the disk in specified flash block to the spi sram chip
*/
void fds_insert_disk(int block)
{
/*	volatile uint8_t *sdram = SDRAM;
	uint8_t buf[256];
	int i,j;

	flash_read_disk_start(block);
	for(i=0;i<0xFF00;i+=256) {
		flash_read_disk(buf,256);
		for(j=0;j<256;j++) {
			*sdram++ = buf[j];
		}
	}
	flash_read_disk_stop();*/
	diskblock = block;
	printf("copied disk image to sram, inserting disk\r\n");
	SET_MEDIASET();
	SET_WRITABLE();
}

void fds_remove_disk(void)
{
	CLEAR_MEDIASET();
	CLEAR_READY();
	printf("removing disk\r\n");
}

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

int led = 0;

void TMR1_IRQHandler(void)
{
	TIMER_ClearIntFlag(TIMER1);

	rate ^= 1;
	PA10 = rate;
	if(rate) {
		count++;
		if(count == 8) {
			count = 0;
			data = data2;
			needbyte++;
		}
		outbit = data & 1;
		data >>= 1;
		PA11 = outbit;
	}
}

int catchwrites = 0;

#define BUFSIZE	(1024 * 8)

uint8_t writebuf[BUFSIZE];
uint8_t decodebuf[1024 * 4];

int write_bufend[8];		//position in writebuf the write ends at
int write_diskpos[8];		//position on disk the write starts at
int write_num;				//number of writes
int write_pos;				//position in buffer for writes








enum {
	//	DEFAULT_LEAD_IN=28300,      //#bits (~25620 min)
	GAP=976/8-1,                //(~750 min)
	MIN_GAP_SIZE=0x300,         //bits
	FDSSIZE=65500,              //size of .fds disk side, excluding header
	FLASHHEADERSIZE=0x100,
};

//Turn raw data from adapter to pulse widths (0..3)
//Input capture clock is 6MHz.  At 96.4kHz (FDS bitrate), 1 bit ~= 62 clocks
//We could be clever about this and account for drive speed fluctuations, etc. but this will do for now
static void raw_to_raw03(uint8_t *raw, int rawSize) {
	int i;
	for(i=0; i<rawSize; ++i) {
/*
  59 59 5a 5b 5b 5b 5a 5b
  89 b8 89 5a 5b 5a 5b 89
*/
		if(raw[i]<0x50) raw[i]=3;          //3=out of range
		else if(raw[i]<0x78) raw[i]=0;     //0=1 bit
		else if(raw[i]<0xA0) raw[i]=1;     //1=1.5 bits
		else if(raw[i]<0xD0) raw[i]=2;     //2=2 bits
		else raw[i]=3;                     //3=out of range
	}
}

//don't include gap end
static uint16_t calc_crc(uint8_t *buf, int size) {
	uint32_t crc=0x8000;
	int i;
	while(size--) {
		crc |= (*buf++)<<16;
		for(i=0;i<8;i++) {
			if(crc & 1) crc ^= 0x10810;
			crc>>=1;
		}
	}
	return crc;
}

static int calc_crc2(uint8_t *buf, int size) {
	uint32_t crc=0x8000;
	int i = size;
	while(size--) {
		crc |= (*buf++)<<16;
		for(i=0;i<8;i++) {
			if(crc & 1) crc ^= 0x10810;
			crc>>=1;
		}
		if(crc == 0) {
			return(i - size);
		}
	}
	return 0;
}

void hexdump(char *desc, void *addr, int len);

int block4Size = 0;

static int block_decode(uint8_t *dst, uint8_t *src, int *inP, int *outP, int srcSize, int dstSize, int blockSize, char blockType) {
//	int in=*inP;
//	int outEnd=(*outP+blockSize+2)*8;
//	int out=(*outP)*8;
	int start;
	int filelen;
	int blocktypefound = 0;
	int in = 0, out = 0;
	int outEnd = dstSize * 8;
	int zeros;
	char bitval;
	char tmp[64];

	//scan for gap end
	for(zeros=0; src[in]!=1 || zeros<MIN_GAP_SIZE; in++) {
		if(src[in] == 1)
			break;
		if(src[in]==0) {
			zeros++;
			} else {
//			printf("zero counter reset at %d (src[in] = %d)\r\n",in,src[in]);
			zeros=0;
		}
		if(in>=srcSize-2) {
			printf("failed to find gap end, in = %d, srcsize = %d\r\n",in,srcSize);
			return 0;
		}
	}
	printf("found gap end at %d, srcSize = %d\r\n",in,srcSize);

	start=in;

//	filelen = crc_detect(&src[in],in,srcSize);
//	filelen = calc_crc2(&src[in],srcSize);
	
	printf("in = %d, srcSize = %d\r\n",in,srcSize);

	bitval=1;
	in++;

	do {
		if(in>=srcSize) {   //not necessarily an error, probably garbage at end of disk
			printf("Disk end\r\n");
			return 0;
		}
		switch(src[in]|(bitval<<4)) {
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
				dst[out/8] |= 1<<(out&7);
				out++;
				bitval=1;
				break;
			default: //Unexpected value.  Keep going, we'll probably get a CRC warning
				printf("glitch(%d) @ %X(%X.%d)\n", src[in], in, out/8, out%8);
				out++;
				bitval=0;
				break;
		}
		in++;
		if((out / 8) > 0 && blocktypefound == 0) {
			blocktypefound = 1;
			blockType = dst[0];
			if(blockType == 2) {
				blockSize = 2;
			}
			if(blockType == 3) {
				blockSize = 16;
			}
			if(blockType == 4) {
				blockSize = block4Size;
			}
			outEnd=(blockSize+3)*8;
			printf("blocktype is %d, blocksize = %d, outEnd = %d\r\n",blockType,blockSize,outEnd);
		}
	} while(out<outEnd);
//	} while(src[in] != 3);
//	} while(out<outEnd);
//	usartdbg_print("block type %d\r\n",dst[*outP]);
/*	if(dst[*outP]!=blockType) {
		usartdbg_print("Wrong block type %X(%X)-%X(%X)\r\n", start, *outP, in, out-1);
		return false;
	}*/
	out=out/8-2;

	printf("Out%d %X(%X)-%X(%X)\n", blockType, start, *outP, in, out-1);

	if(calc_crc(dst+*outP,blockSize+2)) {
		uint16_t crc1=(dst[out+1]<<8)|dst[out];
		uint16_t crc2;
		dst[out]=0;
		dst[out+1]=0;
		crc2=calc_crc(dst+*outP,blockSize+2);
		dst[out] = (uint8_t)(crc1 >> 0);
		dst[out+1] = (uint8_t)(crc1 >> 8);
		printf("Bad CRC (%04X!=%04X)\r\n", crc1, crc2);
	}

	//	dst[out]=0;     //clear CRC
	//	dst[out+1]=0;
	dst[out+2]=0;   //+spare bit
	*inP=in;
	*outP=out;

	if(blockType == 3) {
		block4Size = dst[0xD];
		block4Size |= dst[0xE] << 8;
//		block4Size = 117;
	}

	return blockSize;
}

/*
	pos = position on disk
	start = position in writebuf
*/
static void decode_write(uint8_t *decodebuf,uint8_t *buf,int pos, int size)
{
	int in,out,i;
	int type, len;
//	uint8_t *disk = SDRAM;

	printf("decoding write in writebuf at disk pos %d, size = %d\r\n",pos,size);

	in = 0;
	out = 0;
	for(i=0;i<(1024 * 4);i++) {
		decodebuf[i] = 0;
	}
	len = block_decode(decodebuf,buf,&in,&out,size,0x10000,size,type);
	printf("decoded block, size = %d (len = %d)\r\n",out, len);
//	hexdump("decodebuf",decodebuf,256);

/*	out += 3;
	disk += pos;
	for(i=0;i<((976 / 8) - 1);i++) {
		*disk++ = 0;
	}
	*disk++ = 0x80;
	for(i=0;i<out;i++) {
		*disk++ = decodebuf[i];
	}
	for(i=0;i<40;i++) {
		*disk++ = 0;
	}*/
}






void EINT0_IRQHandler(void)
{
    // For PB.14, clear the INT flag
    GPIO_CLR_INT_FLAG(PB, BIT14);

	led ^= 1;
	PB8 = led;

	if(catchwrites) {
		int ra = TIMER_GetCounter(TIMER0);

		TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
		TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk;
		TIMER0->TCSR |= TIMER_TCSR_CEN_Msk;
		writebuf[write_pos++] = ra;
		if(write_pos >= (BUFSIZE)) {
			write_pos = 0;
			printf("rolling over writebuf position\n");
		}
	}
}

static void begin_transfer(void)
{
	int i, j;

	printf("beginning transfer...\r\n");

	flash_read_disk_start(diskblock);
	flash_read_disk((uint8_t*)&data2,1);
	bytes = 1;
	needbyte = 0;
	count = 7;
	catchwrites = 0;
	
	write_num = 0;
	write_pos = 0;


	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
		if(IS_WRITE()) {
			int ra, sr;
			int oldPA15 = 0;

			catchwrites = 1;
			write_diskpos[write_num] = bytes;
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
			TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk;
			TIMER0->TCMPR = 0xFFFFFF;
			TIMER0->TCSR |= TIMER_TCSR_CEN_Msk;
			while(IS_WRITE()) {
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
			write_bufend[write_num++] = write_pos;
			catchwrites = 0;
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
	flash_read_disk_stop();
	
	if(write_pos) {
		printf("write_pos = %d\n",write_pos);
		raw_to_raw03(writebuf,write_pos);
		for(i=0;i<write_num;i++) {
			int start = (i == 0 ? 0 : write_bufend[i - 1]);
			int size = write_bufend[i] - start;

			printf("--write %d started at diskpos %d, end in buffer at %d (started at %d, size = %d)\n",i,write_diskpos[i],write_bufend[i],start,size);
/*			for(j=0;j<size;j+=8) {
				printf("  %x %x %x %x %x %x %x %x\n",writebuf[j+0],writebuf[j+1],writebuf[j+2],writebuf[j+3],writebuf[j+4],writebuf[j+5],writebuf[j+6],writebuf[j+7]);
			}*/
//			prev = 0;
//			for(i=0;i<numwrites;i++) {
				printf("decoding write: start %d, size %d\r\n",start,size);
				decode_write(decodebuf,writebuf + start,write_diskpos[i],size);
				hexdump("block--",decodebuf,size);
//				prev = writepos2[i];
//			}
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

//	cpu_irq_disable();
//	init_transfer_timer();
//	init_capture_timer();
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

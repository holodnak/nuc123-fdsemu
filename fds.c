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

//#define WRITEBUFSIZE	(1024 * 8)
#define DECODEBUFSIZE	(1024 * 12)

uint8_t sectorbuf[4096];
uint8_t decodebuf[DECODEBUFSIZE];

struct write_s {
	int diskpos;			//position on disk where write was started
	int rawstart;			//position in writebuf the data begins
	int rawend;				//position in writebuf the data ends
	int decstart;			//position in decodebuf the data begins
	int decend;				//position in decodebuf the data ends
} writes[4];

//int write_bufend[8];		//position in writebuf the write ends at
//int write_diskpos[8];		//position on disk the write starts at
//int write_len[8];			//length of decoded data
int write_num;				//number of writes
//int write_pos;				//position in buffer for writes








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

//Turn raw data from adapter to pulse widths (0..3)
//Input capture clock is 6MHz.  At 96.4kHz (FDS bitrate), 1 bit ~= 62 clocks
//We could be clever about this and account for drive speed fluctuations, etc. but this will do for now
static void raw_to_raw03(uint8_t *raw, int rawSize) {
	int i;
	for(i=0; i<rawSize; ++i) {
		raw[i] = raw_to_raw03_byte(raw[i]);
/*		if(raw[i]<0x50) raw[i]=3;          //3=out of range
		else if(raw[i]<0x78) raw[i]=0;     //0=1 bit
		else if(raw[i]<0xA0) raw[i]=1;     //1=1.5 bits
		else if(raw[i]<0xD0) raw[i]=2;     //2=2 bits
		else raw[i]=3;                     //3=out of range
*/	}
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

void hexdump(char *desc, void *addr, int len);

int block4Size = 0;

static int block_decode(uint8_t *dst, uint8_t *src, int *inP, int *outP, int srcSize, int dstSize, int blockSize, char blockType) {
//	int in=*inP;
//	int outEnd=(*outP+blockSize+2)*8;
//	int out=(*outP)*8;
	int start;
//	int filelen;
	int blocktypefound = 0;
	int in = 0, out = 0;
	int outEnd = dstSize * 8;
	int zeros;
	char bitval;

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
	printf("found gap end at %d, srcSize = %d, outEnd = %d\r\n",in,srcSize,outEnd);

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
				blockSize = block4Size + 1;
			}
			outEnd=(blockSize+2)*8;
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

	printf("Out%d %X(%X)-%X(%X) (bs=%d, out=%d)\n", blockType, start, *outP, in, out-1,blockSize, out);

	if(calc_crc(dst,blockSize+2)) {
		uint16_t crc1=(dst[out+1]<<8)|dst[out];
		uint16_t crc2;
		dst[out]=0;
		dst[out+1]=0;
		crc2=calc_crc(dst,blockSize+2);
		dst[out] = (uint8_t)(crc1 >> 0);
		dst[out+1] = (uint8_t)(crc1 >> 8);
		printf("Bad CRC (%04X!=%04X) (out=%d)\r\n", crc1, crc2, out);
	}

	//	dst[out]=0;     //clear CRC
	//	dst[out+1]=0;
//	dst[out+2]=0;   //+spare bit
	*inP=in;
	*outP=out;

	if(blockType == 3) {
		block4Size = dst[0xD];
		block4Size |= dst[0xE] << 8;
	}

	return blockSize + 2;
}

__inline void decode2(uint8_t *dst, uint8_t src, int dstSize, int *outptr) {
	static int foundstart = 0;
	static char bitval;
	int out = *outptr;

	if(dst == 0) {
		foundstart = 0;
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

__inline void decode(uint8_t *dst, uint8_t src, int dstSize, int *len) {
/*	int start;
	int blocktypefound = 0;
	int in = 0, out = 0;
	int outEnd = dstSize * 8;
	int zeros;
	char bitval;*/
//	int blockSize;
//	char blockType;
	
	//states:
	// 0 = finding end of gap before file
	// 1 = decoding data
	static int state = 0;
	static int zeros = 0;
	static int out, outEnd;
	static int blockSize;
	static char blockType;
	static char bitval;
	static int blocktypefound;

	//reset to state 0
	if(dst == 0) {
		state = 0;
		return;
	}

	//finding zeros
	if(state == 0) {
		if(src == 1) {
			state = 1;
			out = 0;
			outEnd = 0x10000;
			blockSize = 0;
			blockType = 0;
			bitval = 1;
			blocktypefound = 0;
			zeros = 0;
		}
		if(src == 0) {
			zeros++;
		}
		return;
	}
	
	//decoding data
	else if(state == 1) {
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
				blockSize = block4Size + 1;
			}
			outEnd=(blockSize+2)*8;
			*len = blockSize + 2;
//			printf("blocktype is %d, blocksize = %d, outEnd = %d\r\n",blockType,blockSize,outEnd);
		}
		if(out >= outEnd) {
			state = 2;
			return;
		}
	}
	
	//finished decoding block, check crc
	else if(state == 2) {
		out = (out / 8) - 2;
		if(calc_crc(dst,blockSize+2)) {
			uint16_t crc1=(dst[out+1]<<8)|dst[out];
			uint16_t crc2;
			dst[out]=0;
			dst[out+1]=0;
			crc2=calc_crc(dst,blockSize+2);
			dst[out] = (uint8_t)(crc1 >> 0);
			dst[out+1] = (uint8_t)(crc1 >> 8);
			printf("Bad CRC (%04X!=%04X) (out=%d)\r\n", crc1, crc2, out);
		}

		if(blockType == 3) {
			block4Size = dst[0xD];
			block4Size |= dst[0xE] << 8;
		}
		state = 0;
	}
}

/*
	pos = position on disk
	start = position in writebuf
*/
static int decode_write(uint8_t *decbuf,uint8_t *buf,int pos, int size)
{
	int in,out,i;
	int type, len;
//	uint8_t *disk = SDRAM;

//	printf("decoding write in writebuf at disk pos %d, size = %d\r\n",pos,size);

	in = 0;
	out = 0;
	for(i=0;i<DECODEBUFSIZE;i++) {
		decbuf[i] = 0;
	}
	len = block_decode(decbuf,buf,&in,&out,size,0x10000,size,type);
	printf("decoded block, size = %d (len = %d)\r\n",out, len);
//	hexdump("decbuf",decbuf,128);

	return(len);
}

uint8_t writelen;
int havewrite;

//uint8_t decodebuf2[1024 * 2];

void EINT0_IRQHandler(void)
{
    GPIO_CLR_INT_FLAG(PB, BIT14);

	if(IS_WRITE()) {
		int ra = TIMER_GetCounter(TIMER0);

		TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
		TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;
//		writebuf[write_pos++] = ra;
		writelen = (uint8_t)ra;
		havewrite++;
/*		if(write_pos >= (WRITEBUFSIZE)) {
			write_pos = 0;
			printf("rolling over writebuf position\n");
		}*/
	}
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
//			decode(0,0,0,0);
			decode2(0,0,0,0);
			while(IS_WRITE()) {
				if(havewrite) {
					havewrite = 0;
//					decode(decodebuf + decodelen,raw_to_raw03_byte(writelen),DECODEBUFSIZE,&len);
					decode2(decodebuf + decodelen,raw_to_raw03_byte(writelen),DECODEBUFSIZE,&len);
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
//			writes[write_num].rawend = write_pos;
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
    NVIC_DisableIRQ(TMR1_IRQn);
    NVIC_EnableIRQ(USBD_IRQn);
	TIMER_Stop(TIMER0);
	TIMER_Stop(TIMER1);

	flash_read_disk_stop();
	
	if(write_num) {
/*		int decode_len = 0;

		printf("write_pos = %d\n",write_pos);
		raw_to_raw03(writebuf,write_pos);
		
		for(j=0;j<2048;j++) {
			decodebuf2[j] = 0;
		}

		//decode the written data
		for(i=0;i<write_num;i++) {
			int start = writes[i].rawstart;
			int end = writes[i].rawend;
			int size = end - start;
			int len;

			printf("--write %d started at diskpos %d, writebuf start, end = %d, %d (%d bytes)\n",i,writes[i].diskpos,start,end,size);
//			for(j=0;j<size;j+=8) {
//				printf("  %x %x %x %x %x %x %x %x\n",writebuf[j+0],writebuf[j+1],writebuf[j+2],writebuf[j+3],writebuf[j+4],writebuf[j+5],writebuf[j+6],writebuf[j+7]);
//			}
//			printf("decoding write: start %d, size %d\r\n",start,size);
			len = decode_write(decodebuf + decode_len,writebuf + start,decode_len,size);
			writes[i].decstart = decode_len;
			writes[i].decend = decode_len + len;
			decode_len += len;
			{
//				uint8_t writelen = 
				for(j=0;j<size;j++) {
					decode(decodebuf2 + decodelen,writebuf[start + j],DECODEBUFSIZE,&len);
				}
				printf("--write %d decodebuf2 len = %d\n",i,len);

				decodelen += len;
			}
		}
		hexdump("--decodebuf",decodebuf,256);
		hexdump("--decodebuf2",decodebuf2,256);
*/
/*		for(i=0;i<write_num;i++) {
			hexdump("block--",decodebuf,256);
		}*/
//		hexdump("--decodebuf",decodebuf,256);

		//erase the working block
//		flash_copy_block(0xF);

		//write the written data to flash
		for(i=0;i<write_num;i++) {
			int pos = writes[i].diskpos + 256;
			int start = writes[i].decstart;
			int end = writes[i].decend;
			int size = (end - start);// + 122 + 40;
			int sector = pos >> 12;
			int sectoraddr = pos & 0xFFF;
			uint8_t *ptr;
			uint8_t *decodeptr = decodebuf + start;

/*			ptr = writebuf;
			for(j=0;j<121;j++) {
				*ptr++ = 0;
			}
			*ptr++ = 0x80;
			for(j=0;j<(end - start);j++) {
				*ptr++ = *decodeptr++;
			}
			for(j=0;j<40;j++) {
				*ptr++ = 0;
			}
			decodeptr = writebuf;*/
			printf("writing to flash: diskpos %d, size %d, sector = %d, sectoraddr = %X (decstart = %d, decsize = %d)\r\n",pos,size,sector,sectoraddr,start,size);
			hexdump("--decodebuf",decodebuf,size);
			flash_read_sector(diskblock,sector,sectorbuf);
//			printf("dumping from %d\n",sectoraddr - 10);
//			hexdump("sector",(writebuf + sectoraddr) - 10,256);
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

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
#include "config.h"

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

volatile uint8_t *writeptr;
volatile int writepos, writepage;

static uint8_t tempbuffer[1024];

volatile int diskblock = 0;
volatile int savediskloaded = 0;

//for sending data to the ram adaptor
//volatile int rate = 0;
volatile int count = 0;

//volatile uint8_t data, data2;
volatile uint32_t dataout,dataout2;
//volatile int outbit;
volatile int bytes;
volatile int needpage;
volatile int needbyte;
volatile fifo_t writefifo;
volatile int writing;
volatile int decodepos;

//extra ram to load game doctor images into
volatile uint8_t doctor[8192];

const uint8_t expand[]={ 0xaa, 0xa9, 0xa6, 0xa5, 0x9a, 0x99, 0x96, 0x95, 0x6a, 0x69, 0x66, 0x65, 0x5a, 0x59, 0x56, 0x55 };

__inline uint8_t raw_to_raw03_byte(uint8_t raw)
{

#ifdef FASTCLK
	if(raw < 0x48)
		return(3);
	else if(raw < 0x70)
		return(0);
	else if(raw < 0xA0)
		return(1);
	else if(raw < 0xD0)
		return(2);
#else
//3d 5d 7c
	if(raw < 0x2D)
		return(3);
	else if(raw < 0x4D)
		return(0);
	else if(raw < 0x6D)
		return(1);
	else if(raw < 0x8D)
		return(2);	
#endif

	return(3);
}

__inline void decode(uint8_t *dst, uint8_t src, int *outptr, uint8_t *bit)
{
	int out = *outptr;

	switch(src | (*bit << 4)) {
		case 0x11:
			out++;
		case 0x00:
			out++;
			*bit = 0;
			break;
		case 0x12:
			out++;
		case 0x01:
		case 0x10:
			dst[out/8] |= (1 << (out & 7));
			out++;
			*bit = 1;
			break;
		default:
			out++;
			*bit = 0;
			break;
	}
	*outptr = out;
}

//for sending data out to ram adaptor
void TMR1_IRQHandler(void)
{
	TIMER_ClearIntFlag(TIMER1);

	//output bit to readdata pin
	PIN_READDATA = (dataout & 1) ^ 1;

	//shift dataout to get ready for next bit
	dataout >>= 1;

	//increment bit counter
	count++;

	//check if we finished with the data and need more
	if(count == 16) {
		count = 0;

		//read next byte from the page
		dataout = dataout2;
		
		//signal we need another mfm byte
		needbyte++;
	}

	//output current bit
/*	PIN_READDATA = (outbit ^ rate) & 1;

	//toggle rate
	rate ^= 1;
	
	//every other toggle we need to process the data
	if(rate) {
		count++;
		
		//if we have sent all eight bits of this byte, get next byte
		if(count == 8) {
			count = 0;

			//read next byte from the page
			data = data2;
			
			//signal we need another data byte
			needbyte++;
		}

		//get next bit to be output
		outbit = data & 1;
		
		//and shift the data byte over to the next next bit
		data >>= 1;
	}*/
}

//for writes coming out of the ram adaptor
void EINT0_IRQHandler(void)
{
	int ra;

	//clear interrupt flag
    GPIO_CLR_INT_FLAG(PB, BIT14);
	
	//if we are writing
	if(IS_WRITE()) {
		
		//get flux transition time
		ra = TIMER_GetCounter(TIMER0);
		
		//restart the counter
		TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
		TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;

		//put the data into the fifo buffer
		fifo_write_byte((uint8_t)ra);
	}
}

void hexdump(char *desc, void *addr, int len);

#define SPI_TIMEOUT		25000

__inline void spi_sram_write_byte(uint8_t byte)
{
	int timeout;

	SPI_WRITE_TX0(SPI_SRAM, byte);
	SPI_TRIGGER(SPI_SRAM);
	timeout = SPI_TIMEOUT;
	while(SPI_IS_BUSY(SPI_SRAM)) {
		if(!timeout--) {
			printf("spi_sram_write_byte timeout\n");
			break;
		}
	}
}

__inline uint8_t spi_sram_read_byte(void)
{
	int timeout;

	timeout = SPI_TIMEOUT;
	SPI_WRITE_TX0(SPI_SRAM, 0);
	SPI_TRIGGER(SPI_SRAM);
	while(SPI_IS_BUSY(SPI_SRAM)) {
		if(!timeout--) {
			printf("spi_sram_read_byte timeout\n");
			return(0xFF);
		}
	}
	return(SPI_READ_RX0(SPI_SRAM));
}

__inline uint8_t disk_read_byte(uint32_t addr)
{
	uint8_t byte;

	//if the data is in sram, fetch the needed byte
	if(addr < 0x10000) {
		SPI_SET_SS0_LOW(SPI_SRAM);
		spi_sram_write_byte(3);
		spi_sram_write_byte((uint8_t)(addr >> 8));
		spi_sram_write_byte((uint8_t)(addr >> 0));
		byte = spi_sram_read_byte();
		SPI_SET_SS0_HIGH(SPI_SRAM);
	}
	
	//additional data is stored in the mcu ram
	else {
		byte = doctor[bytes - 0x10000];
	}
	
	//return data read
	return(byte);
}

__inline void disk_write_byte(uint32_t addr, uint8_t byte)
{
	SPI_SET_SS0_LOW(SPI_SRAM);
	spi_sram_write_byte(2);
	spi_sram_write_byte((uint8_t)(addr >> 8));
	spi_sram_write_byte((uint8_t)(addr >> 0));
	spi_sram_write_byte(byte);
	SPI_SET_SS0_HIGH(SPI_SRAM);	
}

__inline void check_needbyte(void)
{
	uint8_t tmpbyte;

	if(needbyte) {
			
		//clear flag
		needbyte = 0;
		
		//read next byte to be output
		tmpbyte = disk_read_byte(bytes);

		//encode byte to mfm
		dataout2 = expand[tmpbyte & 0x0F];
		dataout2 |= expand[(tmpbyte & 0xF0) >> 4] << 8;

		//increment the byte counter
		bytes++;
	}
}

static void setup_transfer(void)
{
	int leadin;

	fifo_init((uint8_t*)writebuf,4096);

	//initialize variables
	bytes = 0;
	count = 0;
	dataout = dataout2 = 0xAAAA;

	//lead-in byte counter
	leadin = (DEFAULT_LEAD_IN / 8) - 1;
	
	//delay of minimum 150ms
	TIMER_Delay(TIMER2, 165 * 1000);

	//enable/disable necessary irq's
    NVIC_DisableIRQ(USBD_IRQn);
    NVIC_DisableIRQ(GPAB_IRQn);
	NVIC_DisableIRQ(TMR3_IRQn);
    NVIC_EnableIRQ(EINT0_IRQn);
    NVIC_EnableIRQ(TMR1_IRQn);
	
	//start timers
	TIMER_Start(TIMER0);
	TIMER_Start(TIMER1);

	//activate ready signal
	SET_READY();
	
	LED_RED(1);

	//transfer lead-in
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
		
		//if irq handler needs more data
		if(needbyte) {
			needbyte = 0;
			bytes++;
		}

		//check if enough leadin data has been sent
		if(bytes >= leadin) {
			break;
		}
	}
	
	//reset byte counter
	bytes = 256;
}

void begin_transfer(void)
{
	int dirty = 0;

	printf("beginning transfer...\r\n");
	setup_transfer();

	//transfer disk data
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {

		//check irq handler requesting another byte
		check_needbyte();

		//check if we have started writing
		if(IS_WRITE()) {
			int len = 0;
			uint8_t bitval = 0;
			uint8_t decoded[8] = {0,0,0,0,0,0,0,0};
			uint8_t byte;

			LED_GREEN(0);

			//set dirty flag to write data back to flash
			dirty = 1;

			//get initial write position
			writepos = bytes;

			//initialize the timer to get the flux transitions
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
			TIMER0->TCMPR = 0xFFFFFF;
			TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;

			//while the write line is asserted
			while(IS_WRITE()) {

				//check irq handler requesting another byte
				check_needbyte();

				//decode data in the fifo buffer in there is any
				if(fifo_read_byte(&byte)) {

					//decode the data
					decode((uint8_t*)decoded,raw_to_raw03_byte(byte),&len,&bitval);
					
					//if we have a full byte, write it to sram
					if(len >= 8) {
						len -= 8;
						disk_write_byte(writepos++,decoded[0]);
//						sram_write(writepos++,decoded,1);
						decoded[0] = decoded[1];
						decoded[1] = 0;
					}
				}
			}

			//stop the timer for gathering flux transitions
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;

			//decode data in the fifo buffer
			while(fifo_read_byte(&byte)) {

				//check irq handler requesting another byte
				check_needbyte();

				decode((uint8_t*)decoded,raw_to_raw03_byte(byte),&len,&bitval);
				if(len >= 8) {
					len -= 8;
					disk_write_byte(writepos++,decoded[0]);
//					sram_write(writepos++,decoded,1);
					decoded[0] = decoded[1];
					decoded[1] = 0;
				}
			}

			check_needbyte();

			if(len) {
				disk_write_byte(writepos++,decoded[0]);
//				sram_write(writepos,decoded,1);
			}

			LED_GREEN(1);
		}

		//check if insane
		if(bytes >= (0x10000 + 8192)) {
			printf("reached end of data block, something went wrong...\r\n");
			break;
		}
	}

    NVIC_DisableIRQ(EINT0_IRQn);
    NVIC_DisableIRQ(TMR1_IRQn);
	TIMER_Stop(TIMER0);
	TIMER_Stop(TIMER1);
    NVIC_EnableIRQ(USBD_IRQn);

	//clear the ready signal
	CLEAR_READY();
	
	LED_RED(1);
	LED_GREEN(0);

	//if data was written, write it back to the flash
	if(dirty) {
		int flashpage = (diskblock * 0x10000) / 256;
		int i;

		printf("sram data is dirty, writing to flash....\r\n");
		printf("starting flash page: %d\r\n",flashpage);

		//first erase the 64kb page
		flash_erase_block(diskblock);
		flash_busy_wait();

		//write 256 bytes at a time
		for(i=0;i<256;i++) {
			sram_read(i * 256,tempbuffer,256);
			flash_write_page(flashpage++,tempbuffer);
			flash_busy_wait();
		}
	}

	LED_RED(0);
	LED_GREEN(1);
	printf("transferred %d bytes\r\n",bytes);
}

void begin_transfer_loader(void)
{
	int dirty = 0;

	printf("beginning loader transfer...\r\n");

	setup_transfer();
	
	//transfer disk data
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {

		//check irq handler requesting another byte
		check_needbyte();

		//check if we have started writing
		if(IS_WRITE()) {
			LED_GREEN(0);

			//set dirty flag to write data back to flash
			dirty = 1;

			//initialize the timer to get the flux transitions
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
			TIMER0->TCMPR = 0xFFFFFF;
			TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;

			//while the write line is asserted, just gather data, dont decode any until later
			while(IS_WRITE()) {

				//check irq handler requesting another byte
				check_needbyte();
			}

			//stop the timer for gathering flux transitions
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;

			LED_GREEN(1);
		}

		//check if insane
		if(bytes >= 0xFF00) {
			printf("reached end of data block, something went wrong...\r\n");
			break;
		}
	}

    NVIC_DisableIRQ(EINT0_IRQn);
    NVIC_DisableIRQ(TMR1_IRQn);
	TIMER_Stop(TIMER0);
	TIMER_Stop(TIMER1);
    NVIC_EnableIRQ(USBD_IRQn);

	//clear the ready signal
	CLEAR_READY();
	
	LED_RED(1);
	LED_GREEN(0);

	//if data was written, figure out what diskblock to boot from
	if(dirty) {
		uint8_t *ptr = tempbuffer;
		int in,out,len = 0;
		uint8_t byte,bitval = 0;

//		hexdump("tempbuffer",writefifo.buf,1024);
		while(fifo_read_byte(&byte)) {
			decode(tempbuffer,raw_to_raw03_byte(byte),&len,&bitval);
		}

		memset((uint8_t*)writebuf,0,WRITEBUFSIZE);
		bin_to_raw03(tempbuffer,(uint8_t*)writebuf,len / 8,WRITEBUFSIZE);
		in = out = 0;
		memset(tempbuffer,0,1024);
		block_decode(tempbuffer,(uint8_t*)writebuf,&in,&out,4096,1024,3,0xDB);
		
		hexdump("tempbuffer",tempbuffer,256);

		printf("loader exiting, new diskblock = %d\n",ptr[1] | (ptr[2] << 8));
		
		fds_insert_new_disk(ptr[1] | (ptr[2] << 8));
	}
	else {
		printf("transferred %d bytes\r\n",bytes);
	}

	LED_RED(0);
	LED_GREEN(1);
}

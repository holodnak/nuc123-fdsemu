#include <stdio.h>
#include <string.h>
#include "NUC123.h"
#include "fds.h"
#include "fdsutil.h"
#include "flash.h"
#include "spiutil.h"
#include "fifo.h"
#include "main.h"

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

#define DECODEBUFSIZE	(1024 * 12)

uint8_t decodebuf[DECODEBUFSIZE];

#define PAGESIZE	512
#define PAGENUM		2
#define PAGEMASK	(PAGENUM - 1)

typedef struct page_s {
	uint8_t	data[PAGESIZE];		//page data
	int		num;				//page number
	int		dirty;				//is page dirty?  needs to be written?
} page_t;

volatile page_t pagebuf[PAGENUM];
volatile uint8_t *pageptr;
volatile int pagepos, curpage, pagenum;

volatile uint8_t *writeptr;
volatile int writepos, writepage;
volatile uint8_t writebuf[4096];

volatile int diskblock = 0;

//for sending data to the ram adaptor
volatile int rate = 0;
volatile int count = 0;

volatile uint8_t data;
volatile int outbit;
volatile int bytes;
int needpage;
fifo_t writefifo;

int writing;
int decodepos;

__inline uint8_t raw_to_raw03_byte(uint8_t raw)
{
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

	//output current bit
	PA11 = (outbit ^ rate) & 1;

	//toggle rate
	rate ^= 1;
	
	//every other toggle we need to process the data
	if(rate) {
		count++;
		
		//if we have sent all eight bits of this byte, get next byte
		if(count == 8) {
			count = 0;

			//read next byte from the page
			data = pageptr[pagepos++];
			
			//increment the bytes read counter
			bytes++;
			
			//if that was the last byte, reload the page buffer and change pages
			if(pagepos == PAGESIZE) {

				//signal that we need more data
				needpage = 0x10 | curpage;
				
				//increment the page buffer number
				curpage ^= 1;
//				curpage = (curpage + 1) & PAGEMASK;
				
				//get pointer to page data and reset the page read position
				pageptr = pagebuf[curpage].data;
				pagepos = 0;
			}
		}

		//get next bit to be output
		outbit = data & 1;
		
		//and shift the data byte over to the next next bit
		data >>= 1;
	}
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
		fifo_write_byte(&writefifo,(uint8_t)ra);
	}
}

volatile int bufpos, sentbufpos;

//for data coming out of the disk drive
void GPAB_IRQHandler(void)
{
    if(GPIO_GET_INT_FLAG(PA, BIT11)) {
		int ra;

//		printf("GPAB_IRQHandler");
		GPIO_CLR_INT_FLAG(PA, BIT11);
		ra = TIMER_GetCounter(TIMER0);
		TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
		TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;
		decodebuf[bufpos++] = (uint8_t)ra;
		if(bufpos >= DECODEBUFSIZE) {
			bufpos = 0;
		}
    }
}


void hexdump(char *desc, void *addr, int len);

__inline void check_needbyte(void)
{
	static int writebusy = 0;
	static page_t *p = 0;
	uint8_t spidata = 0xD7;

	//if page has been fully transferred, flush it to flash if needed and read next page
	if(needpage) {

		//get pointer to current pagebuf info
		p = (page_t*)&pagebuf[needpage & PAGEMASK];
		
		//clear need page flag
		needpage = 0;

		//check if page is dirty and needs to be written
		if(p->dirty) {
			
			//see if an old write is still going on
			spi_select_device(SPI_FLASH, 0);
			spi_write_packet(SPI_FLASH, &spidata, 1);
			spi_read_packet(SPI_FLASH, &spidata, 1);
		
			//write has finished, read the next page
			if((spidata & 0x80) == 0) {
				printf("flash still busy...waiting...not good...\n");
				while((spidata & 0x80) == 0) {
					spi_read_packet(SPI_FLASH, &spidata, 1);
				}
			}

			spi_deselect_device(SPI_FLASH, 0);

			printf("writing dirty page %d (pagepos = %d)\n",p->num,pagepos);
			
			//write page to flash
			flash_write_page(p->num,p->data);
			
			//set busy flag to delay the reading of the next page
			writebusy = 1;

			//clear the dirty flag
			p->dirty = 0;

			//increment the page that this block holds
			p->num = pagenum++;
		}

		//no data to write, just read next page
		else {
			p->num = pagenum++;
			flash_read_page(p->num,p->data);
		}
	}
	
	//check if we are still busy writing
	if(writebusy) {
		spi_select_device(SPI_FLASH, 0);
		spi_write_packet(SPI_FLASH, &spidata, 1);
		spi_read_packet(SPI_FLASH, &spidata, 1);
		spi_deselect_device(SPI_FLASH, 0);
		
		//write has finished, read the next page
		if(spidata & 0x80) {
			writebusy = 0;
			flash_read_page(p->num,p->data);
		}
	}
	
}

static void begin_transfer(void)
{
	page_t *p;
	int i, leadin = (DEFAULT_LEAD_IN / 8) - 1;

	printf("beginning transfer2...\r\n");

	//initialize variables
	outbit = 0;
	rate = 0;
	bytes = 0;

	//setup page data pointer/position
	pageptr = pagebuf[0].data;
	pagepos = 0;

	//curpage is the current buffer index that the current page is being read from
	curpage = 0;
	
	writing = 0;
	//pagenum is the page that will next be read from the flash
	pagenum = ((diskblock * 0x10000) / 512) + 1;

	//lead-in byte counter
	leadin = (DEFAULT_LEAD_IN / 8) - 1;
	
	//fill the page buffers with data
	for(i=0;i<PAGENUM;i++) {
		pagebuf[i].num = pagenum++;
		pagebuf[i].dirty = 0;
		flash_read_page(pagebuf[i].num,(uint8_t*)pagebuf[i].data);
	}

	//finish off the 0.15 second delay
	TIMER_Delay(TIMER2, 130 * 1000);

	//enable/disable necessary irq's
    NVIC_DisableIRQ(USBD_IRQn);
    NVIC_DisableIRQ(GPAB_IRQn);
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
		
		//keep zeroing these while we transfer the leadin
		pagepos = 0;
		outbit = 0;
		data = 0;

		//check if enough leadin data has been sent
		if(bytes >= leadin) {
			//todo: do some checking on the "count" variable and make sure its 0 to ensure no weird data is read
			break;
		}
	}
	
	//reset byte counter
	pagepos = 0;
	data = 0;
	count = 7;

	TIMER0->TCSR = TIMER_TCSR_CRST_Msk;

	bytes = 0;

	//transfer disk data
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {

		//check on the buffers
		check_needbyte();

		//check if we have started writing
		if(IS_WRITE()) {
			int len = 0;
			uint8_t bitval = 0;
			uint8_t decoded[8] = {0,0,0,0,0,0,0,0};
			uint8_t byte;
			int writelen = 0;

			LED_GREEN(0);
			writepos = pagepos;
			writepage = curpage;
			writeptr = pagebuf[writepage].data;
			pagebuf[writepage].dirty = 1;

			printf("write happening at %X bytes (pagepos = %d)\n",bytes,pagepos);
			
			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;
			TIMER0->TCMPR = 0xFFFFFF;
			TIMER0->TCSR = TIMER_CONTINUOUS_MODE | 7 | TIMER_TCSR_TDR_EN_Msk | TIMER_TCSR_CEN_Msk;

			//while the write line is asserted
			while(IS_WRITE()) {

				//decode data in the fifo buffer
				while(fifo_read_byte(&writefifo,&byte)) {
					decode((uint8_t*)decoded,raw_to_raw03_byte(byte),&len,&bitval);
					if(len >= 8) {
						writelen++;
						len -= 8;
						writeptr[writepos++] = decoded[0];
						decoded[0] = decoded[1];
						decoded[1] = 0;
						if(writepos >= PAGESIZE) {
							writepos = 0;
							writepage ^= 1;
							writeptr = pagebuf[writepage].data;
							pagebuf[writepage].dirty = 1;
						}
					}
				}

				//check on the buffers
				check_needbyte();

			}

			TIMER0->TCSR = TIMER_TCSR_CRST_Msk;

			//decode data in the fifo buffer
			while(fifo_read_byte(&writefifo,&byte)) {
				decode((uint8_t*)decoded,raw_to_raw03_byte(byte),&len,&bitval);
				if(len >= 8) {
					len -= 8;
					writelen++;
					writeptr[writepos++] = decoded[0];
					decoded[0] = decoded[1];
					decoded[1] = 0;
					if(writepos >= PAGESIZE) {
						writepos = 0;
						writepage ^= 1;
						writeptr = pagebuf[writepage].data;
						pagebuf[writepage].dirty = 1;
					}
				}
			}

			if(len) {
				writelen++;
				writeptr[writepos] = decoded[0];
			}
			LED_GREEN(1);
			printf("write ended at %X bytes, %d bytes written (pagepos = %d)\n",bytes,writelen,pagepos);
		}

		//check if insane
		if(bytes >= 0xFF00) {
			printf("reached end of data block, something went wrong...\r\n");
			break;
		}
	}

	if(fifo_has_data(&writefifo)) {
		printf("data still in the fifo...\n");
	}

    NVIC_DisableIRQ(EINT0_IRQn);
    NVIC_DisableIRQ(TMR1_IRQn);
	TIMER_Stop(TIMER0);
	TIMER_Stop(TIMER1);
    NVIC_EnableIRQ(USBD_IRQn);

	//get pointer to current pagebuf info
	p = (page_t*)&pagebuf[curpage];
//	hexdump("&pagebuf[curpage]",pagebuf[curpage].data,512);

	//check if page is dirty and needs to be written
	if(p->dirty) {
		p->dirty = 0;
		printf("(end) writing dirty page %d (pagepos = %d)\n",p->num,pagepos);
			
		//wait for write to finish
		flash_busy_wait();

		//write page to flash
		flash_write_page(p->num,p->data);

		//wait for write to finish
		flash_busy_wait();

	}
	
	//clear the ready signal
	CLEAR_READY();

	LED_RED(0);
	printf("transferred %d bytes\r\n",bytes);
}

void fds_init(void)
{
	int usbattached = USBD_IS_ATTACHED();
	
	usbattached = 0;
	if(usbattached) {
		fds_setup_diskread();
		CLEAR_WRITE();
	}
	else {
		fds_setup_transfer();
		CLEAR_WRITABLE();
		CLEAR_READY();
		CLEAR_MEDIASET();
		CLEAR_MOTORON();
//		fds_insert_disk(1);
	}
	
	fifo_init(&writefifo,(uint8_t*)writebuf,4096);
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
    SYS_UnlockReg();

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

	SYS_LockReg();

	mode = MODE_TRANSFER;
	printf("entering ram adaptor transfer mode\n");
}

//setup for reading/writing disks with the drive
void fds_setup_diskread(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

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

	SYS_LockReg();

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

int mediaset = 0;
int ready = 0;

void fds_tick(void)
{
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
	if(PA10 != 0) {
		flash_header_t header;
		
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
		CLEAR_MEDIASET();
		TIMER_Delay(TIMER2,1000 * 1000);
		
		//wait for button to be released before we insert disk
		while(PA10 != 0);

		SET_MEDIASET();			
	}
	
	//check if ram adaptor wants to stop the motor
	if(IS_STOPMOTOR()) {
		CLEAR_MOTORON();
	}

	//if ram adaptor wants to scan the media and run the motor
	if(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {

		SET_MOTORON();

		TIMER_Delay(TIMER2, 20 * 1000);

		if(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
			if(diskblock == 0) {
//				begin_transfer_loader();
			}
			else {
				begin_transfer();
			}
		}
	}
}

void fds_insert_disk(int block)
{
	diskblock = block;
	fds_setup_transfer();
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

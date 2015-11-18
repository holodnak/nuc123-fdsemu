#include <stdio.h>
#include "NUC123.h"
#include "fds.h"
#include "fdsutil.h"
#include "flash.h"

/*
//string to find to start sending the fake disklist
uint8_t diskliststr[17] = {0x80,0x03,0x07,0x10,'D','I','S','K','L','I','S','T',0x00,0x80,0x00,0x10,0x00};

int find_disklist()
{
	int pos = 0;
	int count = 0;
	uint8_t byte;

	flash_read_disk_start(0);
	for(pos=0;pos<65500;) {

		//read a byte from the flash
		flash_read_disk((uint8_t*)&byte,1);
		pos++;
		
		//first byte matches
		if(byte == diskliststr[0]) {
			count = 1;
			do {
				flash_read_disk((uint8_t*)&byte,1);
				pos++;
			} while(byte == diskliststr[count++]);
			if(count == 18) {
				printf("found disklist block header at %d (count = %d)\n",pos - count,count);

				//skip over the crc
				flash_read_disk((uint8_t*)&byte,1);
				flash_read_disk((uint8_t*)&byte,1);
				pos += 2;

				//skip the gap
				do {
					flash_read_disk((uint8_t*)&byte,1);
					pos++;
				} while(byte == 0 && pos < 65500);

				//make sure this is a blocktype of 4
				if(byte == 0x80) {
					flash_read_disk((uint8_t*)&byte,1);
					pos++;
					if(byte == 4) {
						flash_read_disk((uint8_t*)&byte,1);
						printf("hard coded disk count = %d\n",byte);
						flash_read_disk_stop();
						return(pos);
					}
				}
			}
		}
	}
	flash_read_disk_stop();
	return(-1);
}

uint8_t *disklistblock = decodebuf + 8192;
uint8_t *disklist = decodebuf + 8192 + 1;

void create_disklist(void)
{
	uint8_t *list = disklist + 32;
	flash_header_t header;
	int blocks = flash_get_total_blocks();
	int i,num = 0;
	uint32_t crc;

	memset(disklist,0,4096 + 2);

	for(i=0;i<blocks;i++) {
		
		//read disk header information
		flash_read_disk_header(i,&header);
		
		//empty block
		if((uint8_t)header.name[0] == 0xFF) {
			continue;
		}

		//continuation of disk sides
		if(header.name[0] == 0x00) {
			continue;
		}

		list[0] = (uint8_t)i;
		memcpy(list + 1,header.name,26);
		list[31] = 0;
		printf("block %X: id = %02d, '%s'\r\n",i,header.id,header.name);
		list += 32;
		num++;
	}
	disklistblock[0] = 4;
	disklist[0] = num;

	//correct
	crc = calc_crc(disklistblock,4096 + 1 + 2);
	printf("calc_crc() returned %X\n",crc);
	disklist[4096] = (uint8_t)(crc >> 0);
	disklist[4097] = (uint8_t)(crc >> 8);
}

static void begin_transfer_loader(void)
{
	int i, j;
	int decodelen = 0;
	int leadin = DEFAULT_LEAD_IN;
	static int disklistpos = -1;

	printf("beginning loader transfer...\r\n");

	if(disklistpos == -1) {
		disklistpos = find_disklist();
		printf("find_disklist() = %d\n",disklistpos);
		create_disklist();
	}

	flash_read_disk_start(diskblock);
	needbyte = 0;
	count = 7;
	havewrite = 0;
	writelen = 0;
	
	write_num = 0;
	
	for(i=0;i<1024;i++) {
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
			if(bytes >= disklistpos) {
				int n = bytes - disklistpos;
				if(n < (4096 + 2)) {
					data2 = disklist[n];
				}
				else {
					data2 = 0;
				}
			}
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
	
	//loader
	if(write_num) {
		uint8_t *ptr = &decodebuf[1024];
		int in,out;
		
		//ptr should look like this: $80 $02 $dd
		//where $dd is the new diskblock
//		hexdump("decodebuf",decodebuf,256);
		bin_to_raw03(decodebuf,sectorbuf,writes[0].decend,4096);
		in = 0;
		out = 0;
		block_decode(&decodebuf[1024],sectorbuf,&in,&out,4096,1024,2,2);
		
//		hexdump("&decodebuf[1024]",&decodebuf[1024],256);

		printf("loader exiting, new diskblock = %d\n",ptr[1]);
		fds_insert_disk(ptr[1]);
		return;
	}

	printf("transferred %d bytes\r\n",bytes);
}

*/

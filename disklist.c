#include <stdio.h>
#include <string.h>
#include "NUC123.h"
#include "flash.h"
#include "fdsutil.h"

extern uint8_t disklistbuf[];
extern uint8_t *disklistblock;
extern uint8_t *disklist;
extern int disklistpos;

//string to find to start sending the fake disklist
const uint8_t diskliststr[17] = {0x80,0x03,0x07,0x10,'D','I','S','K','L','I','S','T',0x00,0x80,0x00,0x10,0x00};

int find_disklist()
{
	int pos = 0;
	int count = 0;
	uint8_t byte;

	flash_read_start(0);
	for(pos=0;pos<65500;) {

		//read a byte from the flash
		flash_read((uint8_t*)&byte,1);
		pos++;
		
		//first byte matches
		if(byte == diskliststr[0]) {
			count = 1;
			do {
				flash_read((uint8_t*)&byte,1);
				pos++;
			} while(byte == diskliststr[count++]);
			if(count == 18) {
				printf("found disklist block header at %d (count = %d)\n",pos - count,count);

				//skip over the crc
				flash_read((uint8_t*)&byte,1);
				flash_read((uint8_t*)&byte,1);
				pos += 2;

				//skip the gap
				do {
					flash_read((uint8_t*)&byte,1);
					pos++;
				} while(byte == 0 && pos < 65500);

				//make sure this is a blocktype of 4
				if(byte == 0x80) {
					flash_read((uint8_t*)&byte,1);
					pos++;
					if(byte == 4) {
						flash_read((uint8_t*)&byte,1);
						printf("hard coded disk count = %d\n",byte);
						flash_read_stop();
						return(pos);
					}
				}
			}
		}
	}
	flash_read_stop();
	return(-1);
}

void create_disklist(void)
{
	uint8_t *list = (uint8_t*)disklist + 32;
	flash_header_t header;
	int blocks = flash_get_total_blocks();
	int i,num = 0;
	uint32_t crc;

	memset((uint8_t*)disklist,0,4096 + 2);

	for(i=1;i<blocks;i++) {
		
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
	crc = calc_crc((uint8_t*)disklistblock,4096 + 1 + 2);
	disklist[4096] = (uint8_t)(crc >> 0);
	disklist[4097] = (uint8_t)(crc >> 8);
}

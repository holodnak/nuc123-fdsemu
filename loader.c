#include <stdio.h>
#include <string.h>
#include "NUC123.h"
//#include "fds.h"
//#include "fdsutil.h"
#include "sram.h"
#include "flash.h"

//size of entire disk list data struct array
#define DISKLISTSIZE (32 * 256)		//size of entry * number of slots

/*
	decompress lz4 data.

	buf = raw lz4 data, including 16 byte header
	cb_read = callback for reading uncompressed data
	cb_write = callback for writing uncompressed data
*/
static int decompress_lz4(uint8_t *buf, int len, uint8_t(*cb_read)(uint32_t), void(*cb_write)(uint32_t,uint8_t))
{
	uint8_t *ptr = buf;
	uint8_t token, tmp;
	int inlen = 0;
	int outlen = 0;
	uint16_t offset;
	uint32_t n;

	inlen += 4;
	inlen += 7;

	//loop thru
	while (inlen < len) {
		token = ptr[inlen++];

		//literal part
		if ((token >> 4) & 0xF) {

			//calculate literal length
			n = (token >> 4) & 0xF;

			//length of 15 or greater
			if (n == 0xF) {
				do {
					tmp = ptr[inlen++];
					n += tmp;
				} while (tmp == 0xFF);
			}

			//write literals to output
			while (n--) {
				cb_write(outlen++, ptr[inlen++]);
			}
		}

		//match part (if it is there)
		if ((inlen + 12) >= len) {
			break;
		}

		//get match offset
		offset = ptr[inlen++];
		offset |= ptr[inlen++] << 8;

		//calculate match length
		n = token & 0xF;

		//length of 15 or greater
		if (n == 0xF) {
			do {
				tmp = ptr[inlen++];
				n += tmp;
			} while (tmp == 0xFF);
		}

		//add 4 to match length
		n += 4;
		offset = outlen - offset;

		//copy match bytes
		while (n--) {
			tmp = cb_read(offset++);
			cb_write(outlen++, tmp);
		}
	}

	return(outlen);
}

static uint8_t lz4_read(uint32_t addr)
{
	uint8_t ret;

	sram_read(addr,&ret,1);
	return(ret);
}

static void lz4_write(uint32_t addr, uint8_t data)
{
	sram_write(addr,&data,1);
}

//string to find to start sending the fake disklist
const uint8_t diskliststr[17] = {0x80,0x03,0x07,0x10,'D','I','S','K','L','I','S','T',0x00,0x80,0x00,0x20,0x00};

int find_disklist()
{
	int pos = 0;
	int count = 0;
	uint8_t byte;

	sram_read_start(0);
	for(pos=0;pos<65500;) {

		//read a byte from the flash
		sram_read_byte((uint8_t*)&byte);
		pos++;
		
		//first byte matches
		if(byte == diskliststr[0]) {
			count = 1;
			do {
				sram_read_byte((uint8_t*)&byte);
				pos++;
			} while(byte == diskliststr[count++]);
			if(count == 18) {
				printf("found disklist block header at %d (count = %d)\n",pos - count,count);

				//skip over the crc
				sram_read_byte((uint8_t*)&byte);
				sram_read_byte((uint8_t*)&byte);
				pos += 2;

				//skip the gap
				do {
					sram_read_byte((uint8_t*)&byte);
					pos++;
				} while(byte == 0 && pos < 65500);

				//make sure this is a blocktype of 4
				if(byte == 0x80) {
					sram_read_byte((uint8_t*)&byte);
					pos++;
					if(byte == 4) {
						sram_read_byte((uint8_t*)&byte);
						printf("hard coded disk count = %d\n",byte);
						sram_read_end();
						return(pos);
					}
				}
			}
		}
	}
	sram_read_end();
	return(-1);
}

uint16_t calc_crc2(uint8_t (*readfunc)(uint32_t), uint32_t pos, int size) {
    uint32_t crc=0x8000;
    int i;
    while(size--) {
        crc |= readfunc(pos++)<<16;
        for(i=0;i<8;i++) {
            if(crc & 1) crc ^= 0x10810;
            crc>>=1;
        }
    }
    return crc;
}
	void hexdump2(char *desc, uint8_t (*readfunc)(uint32_t), int pos, int len);

void insert_disklist(int blockstart)
{
	int pos = blockstart;
	flash_header_t header;
	int blocks = flash_get_total_blocks();
	int i, num = 0;
	uint32_t crc;
	uint8_t diskinfo[32];

	//zero out the diskinfo struct
	memset(diskinfo,0,32);

	//zero out all data where the disklist would go (including crc)
	for(i=0;i<(DISKLISTSIZE + 2);i++) {
		sram_write(pos + i,diskinfo,1);
	}

	//skip over first item in list, the information block
	pos += 32;

	//gather up all disk game names
	//TODO: fix loader to allow more than 256 entries
	for(num=0, i=0;i<blocks && num < 255;i++) {
		
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

		//fill diskinfo struct with data
		memset(diskinfo,0,32);
		diskinfo[0] = (uint8_t)i;
		memcpy(diskinfo + 1,header.name,26);
		printf("block %X: id = %02d, '%s'\r\n",i,header.id,header.name);
		
		//write diskinfo struct to sram
		sram_write(pos,diskinfo,32);

		//increment current position and number of games counter
		pos += 32;
		num++;
	}

	//put num into two bytes and write it to sram
	diskinfo[0] = (uint8_t)num;
	diskinfo[1] = (uint8_t)(num >> 8);
	sram_write(blockstart,diskinfo,1);
	printf("number of disks found = %d\n",num);

	//calculate block crc and store it to sram
	crc = calc_crc2(lz4_read,blockstart - 1,DISKLISTSIZE + 2 + 1);
	diskinfo[0] = (uint8_t)(crc >> 0);
	diskinfo[1] = (uint8_t)(crc >> 8);
	sram_write(blockstart + DISKLISTSIZE,diskinfo,2);
}

extern char loader_lz4[];
extern int loader_lz4_length;

void loader_copy(void)
{
	int ret;

	printf("decompressing loader to sram...\r\n");
	ret = decompress_lz4((uint8_t*)loader_lz4,loader_lz4_length,lz4_read,lz4_write);
	printf("decompressed loader from %d to %d bytes (%d%% ratio)\n",loader_lz4_length, ret, 100 * loader_lz4_length / ret);

	ret = find_disklist();
	printf("find_disklist() = %d\n",ret);
	insert_disklist(ret);
}

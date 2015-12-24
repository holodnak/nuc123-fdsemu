#include <stdio.h>
#include "NUC123.h"
//#include "fds.h"
//#include "fdsutil.h"
#include "sram.h"
#include "disklist.h"

/*
	decompress lz4 data.

	buf = raw lz4 data, including 16 byte header
	cb_read = callback for reading uncompressed data
	cb_write = callback for writing uncompressed data
*/
int decompress_lz4(uint8_t *buf, int len, uint8_t(*cb_read)(uint32_t), void(*cb_write)(uint32_t,uint8_t))
{
	uint8_t *ptr = buf;
	uint8_t token, tmp;
	int inlen = 0;
	int outlen = 0;
	uint16_t offset;
	uint32_t n;

//	printf("compressed size = %d, uncompressed size = %d\n", ptr32[1], ptr32[3]);
//	printf("magic number = $%08X\n", ptr32[0]);
	inlen += 4;
	inlen += 7;

	//loop thru
	while (inlen < len) {
//		printf("inlen, outlen = %d, %d (len = %d)\n", inlen, outlen, len);
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
//			printf("literal length = %d\n", n);

			//write literals to output
			while (n--) {
//				printf("%c", ptr[inlen]);
				cb_write(outlen++, ptr[inlen++]);
			}
//			printf("\n");

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
//		printf("match length = %d, offset = %d\n", n, offset);
		offset = outlen - offset;

		//copy match bytes
		while (n--) {
			tmp = cb_read(offset++);
			cb_write(outlen++, tmp);
		}
	}

	return(outlen);
}

uint8_t lz4_read(uint32_t addr)
{
	uint8_t ret;

	sram_read(addr,&ret,1);
	return(ret);
}

void lz4_write(uint32_t addr, uint8_t data)
{
	sram_write(addr,&data,1);
}

extern char loader_lz4[];
extern int loader_lz4_length;

#define DISKLISTSIZE (8192 + 3)

uint8_t disklistbuf[DISKLISTSIZE];
uint8_t *disklistblock = disklistbuf;
uint8_t *disklist = disklistbuf + 1;
//int disklistpos = -1;

void loader_copy(void)
{
	int ret;

	printf("decompressing loader to sram...\r\n");
	ret = decompress_lz4((uint8_t*)loader_lz4,loader_lz4_length,lz4_read,lz4_write);
	printf("decompressed loader to %d bytes\n",ret);

	ret = find_disklist();
	printf("find_disklist() = %d\n",ret);
	create_disklist();

	sram_write(ret,(uint8_t*)disklist,DISKLISTSIZE + 2);
}

#ifndef __fdsutil_h__
#define __fdsutil_h__

enum {
	DEFAULT_LEAD_IN=28300,      //#bits (~25620 min)
	GAP=976/8-1,                //(~750 min)
	MIN_GAP_SIZE=0x300,         //bits
	FDSSIZE=65500,              //size of .fds disk side, excluding header
	FLASHHEADERSIZE=0x100,
};

//__inline uint8_t raw_to_raw03_byte(uint8_t raw);
//__inline void decode(uint8_t *dst, uint8_t src, int *outptr, uint8_t *bit);
int block_decode(uint8_t *dst, uint8_t *src, int *inP, int *outP, int srcSize, int dstSize, int blockSize, char blockType);
void bin_to_raw03(uint8_t *bin, uint8_t *raw, int binSize, int rawSize);
uint16_t calc_crc(uint8_t *buf, int size);

#endif

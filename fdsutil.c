#include <stdio.h>
#include "NUC123.h"
#include "fdsutil.h"

void encode(uint8_t *bin, uint8_t *raw, int binSize, int rawSize) {
	int in, out;
	uint8_t bit, data;

	memset(raw,0xff,rawSize);
	for(bit=1, out=0, in=0; in<binSize*8; in++) {
		 if ((in & 7) == 0) {
			 data = *bin;
			 bin++;
		 }
		 bit = (bit << 7) | (1 & (data >> (in & 7)));   //LSB first
        switch(bit) {
            case 0x00:  //10 10
					 out++;
                raw[out]++;
                break;
            case 0x01:  //10 01
            case 0x81:  //01 01
                raw[out]++;
					 out++;
                break;
            case 0x80:  //01 10
                raw[out] += 2;
                break;
        }
    }
    memset(raw+out,3,rawSize-out);  //fill remainder with (undefined)
}

//don't include gap end
uint16_t calc_crc(uint8_t *buf, int size) {
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

void bin_to_raw03(uint8_t *bin, uint8_t *raw, int binSize, int rawSize) {
    int in, out;
    uint8_t bit, data;

    memset(raw,0xff,rawSize);
    for(bit=1, out=0, in=0; in<binSize*8; in++) {
		 if ((in & 7) == 0) {
			 data = *bin;
			 bin++;
		 }
		 bit = (bit << 7) | (1 & (data >> (in & 7)));   //LSB first
//     bit = (bit<<7) | (1 & (bin[in/8]>>(in%8)));   //LSB first
        switch(bit) {
            case 0x00:  //10 10
					 out++;
                raw[out]++;
                break;
            case 0x01:  //10 01
            case 0x81:  //01 01
                raw[out]++;
					 out++;
                break;
            case 0x80:  //01 10
                raw[out] += 2;
                break;
        }
    }
    memset(raw+out,3,rawSize-out);  //fill remainder with (undefined)
}

int block_decode(uint8_t *dst, uint8_t *src, int *inP, int *outP, int srcSize, int dstSize, int blockSize, char blockType) {

    int in=*inP;
    int outEnd=(*outP+blockSize+2)*8;
    int out=(*outP)*8;
    int start,zeros;
	char bitval;
	
    //scan for gap end
    for(zeros=0; src[in]!=1 || zeros<MIN_GAP_SIZE; in++) {
        if(src[in]==0) {
            zeros++;
        } else {
            zeros=0;
        }
        if(in>=srcSize-2)
            return 0;
    }
    start=in;

    bitval=1;
    in++;
    do {
        if(in>=srcSize) {   //not necessarily an error, probably garbage at end of disk
            //printf("Disk end\n"); 
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
                //printf("glitch(%d) @ %X(%X.%d)\n", src[in], in, out/8, out%8);
                out++;
                bitval=0;
                break;
        }
        in++;
    } while(out<outEnd);
    if(dst[*outP] !=blockType) {
        printf("Wrong block type %X(%X)-%X(%X) (found %d, expected %d)\n", start, *outP, in, out-1, dst[*outP], blockType);
        return 0;
    }
    out=out/8-2;

    //printf("Out%d %X(%X)-%X(%X)\n", blockType, start, *outP, in, out-1);

    if(calc_crc(dst+*outP,blockSize+2)) {
        uint16_t crc2,crc1=(dst[out+1]<<8)|dst[out];
        dst[out]=0;
        dst[out+1]=0;
        crc2=calc_crc(dst+*outP,blockSize+2);
        printf("Bad CRC (%04X!=%04X)\n", crc1, crc2);
    }

    dst[out]=0;     //clear CRC
    dst[out+1]=0;
    dst[out+2]=0;   //+spare bit
    *inP=in;
    *outP=out;
    return 1;
}

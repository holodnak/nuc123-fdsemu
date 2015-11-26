#ifndef __sram_h__
#define __sram_h__

void sram_init(void);
void sram_test(int ss);
void sram_read(int addr,uint8_t *buf,int len);
void sram_write(int addr,uint8_t *buf,int len);

#endif

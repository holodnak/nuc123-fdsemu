#ifndef __sram_h__
#define __sram_h__

void sram_init(void);
int sram_test(void);
void sram_read(int addr,uint8_t *buf,int len);
void sram_write(int addr,uint8_t *buf,int len);

void sram_read_start(int addr);
void sram_read_end(void);
void sram_read_byte(uint8_t *data);
	
#endif

#ifndef __crc32_h__
#define __crc32_h__

uint32_t crc32(uint32_t crc, const void *buf, uint32_t size);
uint32_t crc32_byte(uint8_t data,uint32_t crc);
uint32_t crc32_block(uint8_t *data,uint32_t length,uint32_t crc);

#endif

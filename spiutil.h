#ifndef __spiutil_h__
#define __spiutil_h__

#include "NUC123.h"

int spi_read_packet(SPI_T *spi, uint8_t *data, int len);
int spi_write_packet(SPI_T *spi, uint8_t *data, int len);
void spi_select_device(SPI_T *spi, int ss);
void spi_deselect_device(SPI_T *spi, int ss);

#endif

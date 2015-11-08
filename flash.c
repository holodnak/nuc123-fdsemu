#include <stdio.h>
#include "NUC123.h"
#include "flash.h"
#include "spiutil.h"

#define SPI_FLASH	SPI0
//#define SPI_FLASH2	SPI1

static uint8_t data[4];

void flash_init(void)
{
	data[0] = 0x90;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, data, 2);
	spi_deselect_device(SPI_FLASH, 0);

	printf("--flash manufacturer id: $%02X, device id: $%02X\r\n",data[0],data[1]);
/*
	data[0] = 0x90;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH2, 0);
	spi_write_packet(SPI_FLASH2, data, 4);
	spi_read_packet(SPI_FLASH2, data, 2);
	spi_deselect_device(SPI_FLASH2, 0);

	printf("--flash manufacturer id: $%02X, device id: $%02X\r\n",data[0],data[1]);*/
}

void flash_reset(void)
{
	//enable reset
	data[0] = 0x66;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);

	//reset
	data[0] = 0x99;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read_disk_header(int block,flash_header_t *header)
{
	uint8_t data[4];

	//write the data
	data[0] = 0x03;
	data[1] = block;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, (uint8_t*)header, 256);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read_disk_start(int block)
{
	uint8_t data[4];

	//read data
	data[0] = 0x03;
	data[1] = block;
	data[2] = 0x01;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
}

void flash_read_disk_stop(void)
{
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read_disk(uint8_t *data,int len)
{
	spi_read_packet(SPI_FLASH, data, len);
}

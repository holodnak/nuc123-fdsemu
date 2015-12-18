#include <stdio.h>
#include <string.h>
#include "nuc123.h"
#include "sram.h"
#include "spiutil.h"
#include "config.h"

static uint8_t test_data[256];

static int zero_page(int page)
{
	memset(test_data,0,256);
	sram_write(page * 256,test_data,256);
}

static int test_page(int page, int maxerr)
{
	int i, errors = 0;

	for(i=0;i<256;i++) {
		test_data[i] = (uint8_t)(i);
	}

	sram_write(page * 256,test_data,256);
	memset(test_data,0,256);
	sram_read(page * 256,test_data,256);

	for(i=0;i<256;i++) {
		if(test_data[i] != (uint8_t)(i)) {
			printf("sram test failed at byte %d. (wanted $%02X, got $%02X)\r\n",i + (page * 256),(uint8_t)i,test_data[i]);
			errors++;
			if(errors >= maxerr) {
				return(errors);
			}
		}
	}
	return(errors);
}

void sram_test(int ss)
{
	int maxerr = 3;
	int i, error;
	
	for(i=0; i<0x100; i++) {
		zero_page(i);
	}

	for(i=0, error=0; i<0x100; i++) {
		error += test_page(i, maxerr);
		if(error >= maxerr) {
			printf("too many errors, stopping test.\r\n");
			return;
		}
	}
	if(error) {
		printf("sram test failed, %d errors.\r\n",error);
		return;
	}
	printf("sram test passed.\r\n");
}

uint8_t sram_read_status()
{
	uint8_t data;

	data = 0x05;
	spi_select_device(SPI_SRAM, 0);
	spi_write_packet(SPI_SRAM, &data, 1);
	spi_read_packet(SPI_SRAM, &data, 1);
	spi_deselect_device(SPI_SRAM, 0);
	return(data);
}

void sram_write_status(uint8_t status)
{
	uint8_t data[2];
	
	data[0] = 0x01;
	data[1] = status;
	spi_select_device(SPI_SRAM, 0);
	spi_write_packet(SPI_SRAM, data, 2);
	spi_deselect_device(SPI_SRAM, 0);
}

void sram_init_device(int ss)
{
	uint8_t data[16 + 1];
	int i;

	//ensure it is in sequencial mode
	sram_write_status(0x40);

	//read back mode register
	printf("sram%d mode register: $%02X\n",ss,sram_read_status());
}

void sram_init(void)
{
	sram_init_device(0);
	sram_test(0);
}

void sram_read(int addr,uint8_t *buf,int len)
{
	uint8_t data[3];

	data[0] = 3;
	data[1] = (uint8_t)(addr >> 8);
	data[2] = (uint8_t)(addr >> 0);
	spi_select_device(SPI_SRAM, 0);
	spi_write_packet(SPI_SRAM, data, 3);
	spi_read_packet(SPI_SRAM, buf, len);
	spi_deselect_device(SPI_SRAM, 0);
}

void sram_write(int addr,uint8_t *buf,int len)
{
	uint8_t data[3];

	data[0] = 2;
	data[1] = (uint8_t)(addr >> 8);
	data[2] = (uint8_t)(addr >> 0);
	spi_select_device(SPI_SRAM, 0);
	spi_write_packet(SPI_SRAM, data, 3);
	spi_write_packet(SPI_SRAM, buf, len);
	spi_deselect_device(SPI_SRAM, 0);
}

void sram_read_start(int addr)
{
	uint8_t data[3];

	data[0] = 3;
	data[1] = (uint8_t)(addr >> 8);
	data[2] = (uint8_t)(addr >> 0);
	spi_select_device(SPI_SRAM, 0);
	spi_write_packet(SPI_SRAM, data, 3);
}

void sram_read_end(void)
{
	spi_deselect_device(SPI_SRAM, 0);
}

void sram_read_byte(uint8_t *data)
{
	spi_read_packet(SPI_SRAM, data, 1);
}

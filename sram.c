#include <stdio.h>
#include "nuc123.h"
#include "sram.h"
#include "spiutil.h"

#define SPI_SRAM	SPI1

static int test_page(int page, int ss)
{
	uint8_t data_write[4];
	uint8_t data_read[4];
	uint8_t test_data[256];
	int i, maxerr = 3, errors = 0;

	data_write[0] = 0x02;
	data_read[0] = 0x03;
	data_read[1] = data_write[1] = (uint8_t)(page >> 8);
	data_read[2] = data_write[2] = (uint8_t)page;
	data_read[3] = data_write[3] = 0x00;

	for(i=0;i<256;i++) {
		test_data[i] = (uint8_t)(i);
	}
	
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data_write, 4);
	spi_write_packet(SPI_SRAM, test_data, 256);
	spi_deselect_device(SPI_SRAM, ss);

	TIMER_Delay(TIMER2,1000);

	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data_read, 4);
	spi_read_packet(SPI_SRAM, test_data, 256);
	spi_deselect_device(SPI_SRAM, ss);

	for(i=0;i<256;i++) {
		if(test_data[i] != (uint8_t)(i)) {
			printf("sram test failed at byte %d. (wanted $%02X, got $%02X)\r\n",i + (page * 256),(uint8_t)i,test_data[i]);
			errors++;
			if(errors >= maxerr) {
				return(errors);
			}
		}
	}
	printf("sram%d test passed\n",ss);
	return(0);
}

void sram_test(int ss)
{
	int i, error;
	
	for(i=0, error=0; i<0x200; i++) {
		error += test_page(i, ss);
		if(error >= 3) {
			printf("too many errors, stopping test.\r\n");
			break;
		}
	}
	if(error == 0) {
		printf("sram test passed.\r\n");
	}
}

void sram_init_device(int ss)
{
	uint8_t data[4];

	spi_select_device(SPI_SRAM, ss);
	spi_deselect_device(SPI_SRAM, ss);
	spi_select_device(SPI_SRAM, ss);
	spi_deselect_device(SPI_SRAM, ss);

	//ensure it is in single io mode
	data[0] = 0xFF;
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data, 1);
	spi_deselect_device(SPI_SRAM, ss);

	//ensure it is in sequencial mode
	data[0] = 0x01;
	data[1] = 0x40;
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data, 2);
	spi_deselect_device(SPI_SRAM, ss);

	//read back mode register
	data[0] = 0x05;
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data, 1);
	spi_read_packet(SPI_SRAM, data, 1);
	spi_deselect_device(SPI_SRAM, ss);
	printf("sram%d mode register: $%02X\n",ss,data[0]);
}

void sram_init(void)
{
	sram_init_device(0);
	sram_test(0);
}

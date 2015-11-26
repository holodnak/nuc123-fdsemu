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
	data_read[1] = data_write[1] = (uint8_t)page;
	data_read[2] = data_write[2] = 0x00;

	for(i=0;i<256;i++) {
		test_data[i] = (uint8_t)(i);
	}
	
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data_write, 3);
	spi_write_packet(SPI_SRAM, test_data, 256);
	spi_deselect_device(SPI_SRAM, ss);

	TIMER_Delay(TIMER2,1000);

	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data_read, 3);
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
	return(errors);
}

void sram_test(int ss)
{
	int i, error = 0;
	uint8_t data[4] = {0,0,0,0};
	uint8_t test;

	data[0] = 0x02;
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data, 3);
	for(i=0;i<=0x20000;i++) {
		test = (uint8_t)(i & 0xFF);
		spi_write_packet(SPI_SRAM, &test, 1);
	}
	spi_deselect_device(SPI_SRAM, ss);

	data[0] = 0x03;
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data, 3);
	for(i=0;i<0x20000;i++) {
		spi_read_packet(SPI_SRAM, &test, 1);
		if(test != (uint8_t)(i & 0xFF)) {
			printf("sram test failed at byte %d (page %d, byte %d). (wanted $%02X, got $%02X)\r\n",i,i >> 8,i & 0xFF,(uint8_t)(i & 0xFF),test);
			error++;
			if(error >= 3)
				break;
		}
	}
	spi_deselect_device(SPI_SRAM, ss);
/*	for(i=0, error=0; i<0x100; i++) {
		error += test_page(i, ss);
		if(error >= 3) {
			printf("too many errors, stopping test.\r\n");
			return;
		}
	}*/
	if(error == 0) {
		printf("sram test passed.\r\n");
	}
	else {
		printf("sram test failed, %d errors.\r\n",error);
	}
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

void sram_init_device(int ss)
{
	uint8_t data[16 + 1];
	int i;

	//ensure it is in sequencial mode
/*	data[0] = 0x01;
	data[1] = 0x40;
	spi_select_device(SPI_SRAM, ss);
	spi_write_packet(SPI_SRAM, data, 2);
	spi_deselect_device(SPI_SRAM, ss);*/

	//read back mode register
	printf("sram%d mode register: $%02X\n",ss,sram_read_status());
	
	for(i=4;i<16;i++) {
		data[i] = (uint8_t)0xFF;
	}
	sram_write(0,data+4,12);
	memset(data,0xCD,16);
	sram_read(0,data+4,12);
	hexdump("data+4",data+4,12);

	for(i=4;i<16;i++) {
		data[i] = (uint8_t)i;
	}
	sram_write(0,data+4,12);
	memset(data,0xCD,16);
	sram_read(0,data+4,12);
	hexdump("data+4",data+4,12);

	sram_read(0,data+4,12);
	hexdump("data+4",data+4,12);
	
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
//	sram_test(0);
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

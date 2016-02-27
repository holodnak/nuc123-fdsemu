#include <stdio.h>
#include "NUC123.h"
#include "flash.h"
#include "spiutil.h"
#include "main.h"
#include "config.h"

typedef struct flashchip_s {
	uint8_t manufacturer, device1, device2;
	int size;
} flashchip_t;

const flashchip_t flashchips[] = {
	//default unknown chip

	//winbond chips
	{0xEF, 0x40, 0x14, 0x100000}, 	//1mbyte
	{0xEF, 0x40, 0x16, 0x400000}, 	//4mbyte
	{0xEF, 0x40, 0x17, 0x800000}, 	//8mbyte
	{0xEF, 0x40, 0x18, 0x1000000}, 	//16mbyte
	{0xEF, 0x40, 0x19, 0x2000000}, 	//32mbyte

	//micron
	{0x20, 0xBA, 0x19, 0x2000000}, 	//32mbyte

	//other flash chips
	{0x01, 0x40, 0x17, 0x800000}, 	//8mbyte

	{0, 0, 0, -1}	//end of list
};

static flashchip_t *chip = 0;

//keep waiting for chip to become not busy
void flash_busy_wait(void)
{
	uint8_t data[4] = {0x05};

	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	do {
		spi_read_packet(SPI_FLASH, data, 1);
	} while(data[0] & 1);
	spi_deselect_device(SPI_FLASH, 0);
}

static flashchip_t *flash_find_chip()
{
	uint8_t data[4];
	int i;

	//read flash chip id
	flash_get_id(data);
	printf("--flash manufacturer id: $%02X, device id: $%04X\r\n",data[0],(data[1] << 8) | data[2]);
	
	//search table of chips for this chip
	chip = 0;
	for(i=0;flashchips[i].size != -1;i++) {
		if(data[0] == flashchips[i].manufacturer && data[1] == flashchips[i].device1 && data[2] == flashchips[i].device2) {
			return((flashchip_t*)&flashchips[i]);
		}
	}
	return(0);
}

//read flash chip id
void flash_get_id(uint8_t *buf)
{
	uint8_t data[4] = {0x9F};

	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_read_packet(SPI_FLASH, buf, 3);
	spi_deselect_device(SPI_FLASH, 0);
}

//initialize flash chip stuff
void flash_init(void)
{
	//get flash chip info
	chip = flash_find_chip();
	if(chip == 0) {
		printf("flash_init: flash chip not found in list, using default info\n");

		//loop forever, indicating fatal error
		for(;;) {
			LED_GREEN(1);
			LED_RED(0);
			TIMER_Delay(TIMER2,100 * 1000);
			LED_GREEN(0);
			LED_RED(1);
			TIMER_Delay(TIMER2,100 * 1000);
		}

	}
	printf("flash_init: flash chip detected\n");		
	flash_reset();
}

void flash_reset(void)
{
	uint8_t data[4] = {0,0,0,0};

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

void flash_check_extaddr(uint32_t addr)
{
	uint8_t data[4];
	uint8_t tmp = (uint8_t)(addr >> 24);

	//if chip is smaller then skip this
	if(chip->size < 0x2000000) {
		return;
	}

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);

	//write extended address
	data[0] = 0xC5;
	data[1] = tmp;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 2);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read_start(uint32_t addr)
{
	uint8_t data[4];

	//write extended address register
	flash_check_extaddr(addr);

	//read data
	data[0] = 0x03;
	data[1] = (uint8_t)(addr >> 16);
	data[2] = (uint8_t)(addr >> 8);
	data[3] = (uint8_t)(addr);
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
}

void flash_read_stop(void)
{
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read(uint8_t *buf,int len)
{
	spi_read_packet(SPI_FLASH, buf, len);
}

void flash_read_data(uint32_t addr,uint8_t *buf,int len)
{
	uint8_t data[4];

	//write extended address register
	flash_check_extaddr(addr);

	//read data
	data[0] = 0x03;
	data[1] = (uint8_t)(addr >> 16);
	data[2] = (uint8_t)(addr >> 8);
	data[3] = (uint8_t)(addr);
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, buf, len);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read_disk_header(int block,flash_header_t *header)
{
	flash_read_data(block * 0x10000,(uint8_t*)header,256);
}

void flash_read_page(int page,uint8_t *buf)
{
	uint8_t data[4];

	//write extended address register
	flash_check_extaddr(page << 8);

	//read the data
	data[0] = 0x03;
	data[1] = page >> 8;
	data[2] = page;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, buf, 512);
	spi_deselect_device(SPI_FLASH, 0);
//	printf("flash_read_page: reading page %d (%X %X %X)\n",page,data[1],data[2],data[3]);
}

void flash_write_page(int page,uint8_t *buf)
{
	uint8_t data[4];

	//write extended address register
	flash_check_extaddr(page << 8);

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);
		
	//write the data
	data[0] = 0x02;
	data[1] = (uint8_t)(page >> 8);
	data[2] = (uint8_t)(page);
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_write_packet(SPI_FLASH, buf, 256);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_copy_block(int src,int dest)
{
	int i;
	uint8_t buf[256];

	flash_erase_block(dest);
	src <<= 8;
	dest <<= 8;
	for(i=0;i<256;i++) {
		flash_read_page(i + src,buf);
		flash_write_page(i + dest,buf);
	}
	printf("flash_copy_block: copied %X to %X\n",src >> 8,dest >> 8);
}

void flash_chip_erase()
{
	uint8_t data[4] = {0xC7, 0x94, 0x80, 0x9A};

	//enable writes
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_deselect_device(SPI_FLASH, 0);

	//wait for it to finish
	flash_busy_wait();
}

void flash_erase_block(int block)
{
	uint8_t data[4];

	//write extended address register
	flash_check_extaddr(block << 16);

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);

	//erase block
	data[0] = 0xD8;
	data[1] = block;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_deselect_device(SPI_FLASH, 0);
	
	//wait for it to finish
	flash_busy_wait();
}

void flash_erase_sector(int block,int sector)
{
	uint8_t data[4];

	//write extended address register
	flash_check_extaddr(block << 16);

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);

	//erase block
	data[0] = 0x20;
	data[1] = block;
	data[2] = (sector << 4) & 0xF0;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_deselect_device(SPI_FLASH, 0);
	
	//wait for it to finish
	flash_busy_wait();
}

int flash_get_size(void)
{
	return(chip->size);
}

int flash_get_total_blocks(void)
{
	return(flash_get_size() / 0x10000);
}

int flash_find_empty_block(void)
{
	flash_header_t header;
	int i,blocks = flash_get_total_blocks();
	
	for(i=0;i<blocks;i++) {
		flash_read_disk_header(i,&header);
		if((uint8_t)header.name[0] == 0xFF) {
			printf("block %X: empty\r\n",i);
		}
		else {
			printf("block %X: '%s'\r\n",i,header.name);
		}
	}
	return(-1);
}

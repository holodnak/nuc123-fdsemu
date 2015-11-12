#include <stdio.h>
#include "NUC123.h"
#include "flash.h"
#include "spiutil.h"

typedef struct flashchip_s {
	uint8_t manufacturer, device;
	int size;
} flashchip_t;

const flashchip_t flashchips[] = {
	{0xEF, 0x13, 0x100000}, 	//1mbyte
	{0x00, 0x00, -1}			//end of list
};

static uint8_t data[4];

//keep waiting for chip to become not busy
static void flash_busy_wait(void)
{
	data[0] = 0x05;

	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	do {
		spi_read_packet(SPI_FLASH, data, 1);
	} while(data[0] & 1);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_get_id(uint8_t *buf)
{
	data[0] = 0x9F;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_read_packet(SPI_FLASH, buf, 3);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_init(void)
{
	flash_reset();
	flash_get_id(data);
	printf("--flash manufacturer id: $%02X, device id: $%04X\r\n",data[0],(data[1] << 8) | data[2]);
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

void flash_read_start(uint32_t addr)
{
	uint8_t data[4];

	//read data
	data[0] = 0x03;
	data[1] = (uint8_t)(addr >> 16);
	data[2] = (uint8_t)(addr >> 8);
	data[3] = (uint8_t)(addr);
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	printf("flash_read_start: addr = $%08X\n",addr);
}

void flash_read_stop(void)
{
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read(uint8_t *buf,int len)
{
	spi_read_packet(SPI_FLASH, buf, len);
//	hexdump("spiread",buf,len);
}

void flash_read_disk_header(int block,flash_header_t *header)
{
	uint8_t data[4];

	//read the data
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

void flash_read_page(int page,uint8_t *buf)
{
	uint8_t data[4];

	//read the data
	data[0] = 0x03;
	data[1] = page >> 8;
	data[2] = page & 0xFF;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, buf, 256);
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read_sector(int block,int sector,uint8_t *buf)
{
	int i;
	int page;
	
	block &= 0xFF;
	sector &= 0xF;
	page = (block << 8) | (sector << 4);
	printf("reading sector %d from block %d\n",sector,block);
	for(i=0;i<16;i++) {
		flash_read_page(page,buf + (i * 256));
		page++;
	}
}

void flash_write_page(int page,uint8_t *buf)
{
	uint8_t data[4];

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);

	//write the data
	data[0] = 0x02;
	data[1] = (uint8_t)(page >> 8);
	data[2] = (uint8_t)(page & 0xFF);
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_write_packet(SPI_FLASH, buf, 256);
	spi_deselect_device(SPI_FLASH, 0);
	
	flash_busy_wait();
}

void flash_write_sector(int block,int sector,uint8_t *buf)
{
	int i;
	int page;
	
	block &= 0xFF;
	sector &= 0xF;
	page = (block << 8) | (sector << 4);
	flash_erase_sector(block,sector);
	for(i=0;i<16;i++) {
		flash_write_page(page,buf + (i * 256));
		page++;
	}
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

void flash_erase_block(int block)
{
	uint8_t data[4];

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
	int i;

	//read flash id
	data[0] = 0x90;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, data, 2);
	spi_deselect_device(SPI_FLASH, 0);

	//search table of chips
	for(i=0;flashchips[i].size != -1;i++) {
		if(data[0] == flashchips[i].manufacturer && data[1] == flashchips[i].device) {
			return(flashchips[i].size);
		}
	}
	
	//flashchip not found
	return(-1);
}

int flash_get_total_blocks(void)
{
	int size = flash_get_size();
	int blocks = size / 0x10000;
	
	//we need one block to use for writing, writes will re-organize the order of the disks stored in flash
	return(blocks - 1);
}

int flash_find_empty_block(void)
{
	flash_header_t header;
	int i,blocks = flash_get_total_blocks();
	
	for(i=0;i<blocks;i++) {
		flash_read_disk_header(i,&header);
		if(header.name[0] == 0xFF) {
			printf("block %X: empty\r\n",i);
		}
		else {
			printf("block %X: id = %02d, '%s'\r\n",i,header.id,header.name);
		}
	}
	return(-1);
}

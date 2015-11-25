#include <stdio.h>
#include "NUC123.h"
#include "flash.h"
#include "spiutil.h"

enum {
	ID_UNKNOWN = 0,		//unknown flash type
	ID_FLASH,			//standard flash chip
	ID_DATAFLASH,		//dataflash chip
};

enum {
	CMD_DF_READ_STATUS = 0xD7,
	CMD_DF_READ = 0x0B,
};

typedef struct flashchip_s {
	uint8_t type, manufacturer, device1, device2;
	int size;
} flashchip_t;

const flashchip_t flashchips[] = {
	//default unknown chip
	{ID_UNKNOWN,   0x00, 0x00, 0x00, 0x100000}, 	//1mbyte, default small size
	
	//flash chips
	{ID_FLASH,     0xEF, 0x13, 0x00, 0x100000}, 	//1mbyte
	{ID_FLASH,     0x01, 0x16, 0x00, 0x800000}, 	//8mbyte

	//dataflash chips
	{ID_DATAFLASH, 0x1F, 0x27, 0x01, 0x400000}, 	//AT45DB321E - 4mbyte

	{0, 0, 0, 0, -1}	//end of list
};

static flashchip_t *chip = 0;

//dataflash specific functions
uint8_t dataflash_read_status()
{
	uint8_t data[4] = {0xD7};

	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_read_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, 0);
	return(data[0]);
}

//keep waiting for chip to become not busy
void flash_busy_wait(void)
{
	uint8_t data[4] = {0x05};

	spi_select_device(SPI_FLASH, 0);

	//dataflash busy wait
	if(chip->type == ID_DATAFLASH) {
		data[0] = 0xD7;
		spi_write_packet(SPI_FLASH, data, 1);
		do {
			spi_read_packet(SPI_FLASH, data, 1);
		} while((data[0] & 0x80) == 0);
	}

	//flash busy wait
	else {
		data[0] = 0x05;
		spi_write_packet(SPI_FLASH, data, 1);
		do {
			spi_read_packet(SPI_FLASH, data, 1);
		} while(data[0] & 1);
	}

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
	uint8_t data[4];

	//get flash chip info
	chip = flash_find_chip();
	if(chip == 0) {
		printf("flash_init: flash chip not found in list, using default info\n");
		chip = (flashchip_t*)flashchips;
	}

	//if this is a dataflash chip, ensure it is setup properly
	if(chip->type == ID_DATAFLASH) {
		printf("flash_init: dataflash chip detected\n");
		if((dataflash_read_status() & 1) == 0) {
			printf("flash_init: dataflash is in 528 byte page size, setting to 512...\n");
			data[0] = 0x3D;
			data[1] = 0x2A;
			data[2] = 0x80;
			data[3] = 0xA6;
			spi_select_device(SPI_FLASH, 0);
			spi_write_packet(SPI_FLASH, data, 4);
			spi_deselect_device(SPI_FLASH, 0);
		}
		else {
			printf("flash_init: dataflash configured for 512 byte pages\n");
		}
	}
	
	//regular flash chip
	else {
		printf("flash_init: flash chip detected\n");		
	}
	flash_reset();
}

void flash_reset(void)
{
	uint8_t data[4] = {0,0,0,0};

	//dataflash chip
	if(chip->type == ID_DATAFLASH) {
		//reset
		data[0] = 0xF0;
		spi_select_device(SPI_FLASH, 0);
		spi_write_packet(SPI_FLASH, data, 4);
		spi_deselect_device(SPI_FLASH, 0);
	}

	//flash chip
	else {
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
//	printf("flash_read_start: addr = $%08X\n",addr);
}

void flash_read_stop(void)
{
	spi_deselect_device(SPI_FLASH, 0);
}

void flash_read(uint8_t *buf,int len)
{
	spi_read_packet(SPI_FLASH, buf, len);
}

void flash_read_disk_header(int block,flash_header_t *header)
{
	flash_read_start(block * 0x10000);
	flash_read((uint8_t*)header,256);
	flash_read_stop();
}

void flash_read_data(int addr,uint8_t *buf)
{
	uint8_t data[4];

	//read the data
	data[0] = 0x03;
	data[1] = addr >> 16;
	data[2] = addr >> 8;
	data[3] = addr;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, buf, 512);
	spi_deselect_device(SPI_FLASH, 0);
	printf("flash_read_data: reading from %d (%X %X %X)\n",addr,data[1],data[2],data[3]);
}

void flash_read_page(int page,uint8_t *buf)
{
	uint8_t data[4];

	//read the data
	data[0] = 0x03;
	data[1] = page >> 7;
	data[2] = (page << 1);
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

	//write the data
	data[0] = 0x82;
	data[1] = (uint8_t)(page >> 7);
	data[2] = (uint8_t)(page << 1);
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, 0);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_write_packet(SPI_FLASH, buf, 512);
	spi_deselect_device(SPI_FLASH, 0);
//	printf("flash_write_page: writing page %d\n",page);
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
		if((uint8_t)header.name[0] == 0xFF) {
			printf("block %X: empty\r\n",i);
		}
		else {
			printf("block %X: id = %02d, '%s'\r\n",i,header.id,header.name);
		}
	}
	return(-1);
}

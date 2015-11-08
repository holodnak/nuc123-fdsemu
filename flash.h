#ifndef __flash_h__
#define __flash_h__

typedef struct flash_header_s {
    char		name[240];			//null terminated ascii string
	uint16_t	next_disk;		//block number of next disk (after this one, for dual sided game or games with two disks)
	uint16_t	lead_in;			//number of bits for lead in (0 for default)
	uint16_t	id;					//block id number
	uint8_t		reserved[10];
//    uint8_t		data[0xff00];	//disk data, beginning with lead in
} flash_header_t;

void flash_init(void);
void flash_reset(void);
void flash_read_disk_header(int block,flash_header_t *header);
void flash_read_disk_start(int block);
void flash_read_disk_stop(void);
void flash_read_disk(uint8_t *data,int len);

#endif

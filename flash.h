#ifndef __flash_h__
#define __flash_h__

//header format cannot change!  finalized!

typedef struct flash_header_s {
    char		name[240];			//null terminated ascii string

	//16 extra bytes for "other things"
	
	//240-241 - size of disk image bin data
	uint16_t	size;				//size of data stored in the block
	
	//242-243 - duration of lead-in (not used)
	uint16_t	ownerid;			//id of first slot of this game

	//244-245 - id (slot number) of next disk in the chain
	uint16_t	nextid;				//id of next block in disk chain

	//246-247 - "save disk" block id (if first disk of a game doctor image)
	uint16_t	saveid;

	//248 - flags
	uint8_t		flags;				//disk flags cris-00tt
									// c = compressed
									// r = read only
									// i = owner id/next id fields are VALID
									// s = saveid field is VALID
									// t = type (0=fds, 1=gd, 3=savedisk)
									
	//249 - flags2
	uint8_t		flags2;

	//250-255 - unused
	uint8_t		reserved[6];

} flash_header_t;

void flash_get_id(uint8_t *buf);
void flash_init(void);
void flash_reset(void);

void flash_busy_wait(void);

void flash_read_start(uint32_t addr);
void flash_read_stop(void);
void flash_read(uint8_t *buf,int len);
void flash_read_data(uint32_t addr,uint8_t *buf,int len);

void flash_read_disk_header(int block,flash_header_t *header);
void flash_read_page(int page,uint8_t *buf);
void flash_write_page(int page,uint8_t *buf);

void flash_copy_block(int src,int dest);
void flash_erase_block(int block);
void flash_erase_sector(int block,int sector);

int flash_get_size(void);
int flash_get_total_blocks(void);
int flash_find_empty_block(void);
#endif

#ifndef __fifo_h__
#define __fifo_h__

typedef struct fifo_s {
	uint8_t *buf;
	int head, tail;
	int size;
} fifo_t;

void fifo_init(uint8_t *buf, int size);
int fifo_has_data(void);

int fifo_read(void *buf, int nbytes);
int fifo_read_byte(uint8_t *data);

int fifo_write(const void * buf, int nbytes);
void fifo_write_byte(uint8_t data);

#endif

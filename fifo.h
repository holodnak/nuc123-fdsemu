#ifndef __fifo_h__
#define __fifo_h__

typedef struct fifo_s {
	uint8_t *buf;
	int head, tail;
	int size;
} fifo_t;

void fifo_init(fifo_t *f, uint8_t *buf, int size);
int fifo_has_data(fifo_t *f);

int fifo_read(fifo_t *f, void *buf, int nbytes);
int fifo_read_byte(fifo_t *f, uint8_t *data);

int fifo_write(fifo_t * f, const void * buf, int nbytes);
void fifo_write_byte(fifo_t *f, uint8_t data);

#endif

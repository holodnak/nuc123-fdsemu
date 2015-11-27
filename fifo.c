#include <stdio.h>
#include "NUC123.h"
#include "fifo.h"

void fifo_init(fifo_t *f, uint8_t *buf, int size)
{
	f->buf = buf;
	f->size = size;
	f->head = 0;
	f->tail = 0;
}

int fifo_has_data(fifo_t *f)
{
	if(f->head != f->tail) {
		return(1);
	}
	return(0);
}

int fifo_read(fifo_t *f, void *buf, int nbytes)
{
	int i;
	char * p;
	p = buf;
	for(i=0; i < nbytes; i++) {
		if(f->tail != f->head) { //see if any data is available
			*p++ = f->buf[f->tail++];  //grab a byte from the buffer
			if(f->tail == f->size) {  //check for wrap-around
				f->tail = 0;
			}
		}
		else {
			return i; //number of bytes read 
		}
	}
	return nbytes;
}
 
int fifo_read_byte(fifo_t *f, uint8_t *data)
{
	//check if the buffer is empty
	if(f->tail != f->head) { //see if any data is available
		
		//read byte from the buffer, increment tail
		*data = f->buf[f->tail++];
		
		//check for wrapping
		if(f->tail == f->size) {
			f->tail = 0;
		}

		//returns number of bytes read
		return(1);
	}
	
	//empty buffer
	return(0);
}
 
int fifo_write(fifo_t * f, const void * buf, int nbytes)
{
	int i;
	const char * p;

	p = buf;
	for(i=0; i < nbytes; i++) {
		//first check to see if there is space in the buffer
		if( (f->head + 1 == f->tail) || ((f->head + 1 == f->size) && (f->tail == 0)) ){
			return i; //no more room
		}
		else {
			f->buf[f->head] = *p++;
			f->head++;  //increment the head
			if(f->head == f->size) {  //check for wrap-around
				f->head = 0;
			}
		}
	}
	return nbytes;
}

void fifo_write_byte(fifo_t *f, uint8_t data)
{
	//first check to see if there is space in the buffer
	if((f->head + 1 == f->tail) || ((f->head + 1 == f->size) && (f->tail == 0)) ){
		printf("fifo full!\n");
		return; //no more room
	}

	f->buf[f->head] = data;
	f->head++;  //increment the head
	if(f->head == f->size) {  //check for wrap-around
		f->head = 0;
	}

}

#include <stdio.h>
#include <string.h>
#include "NUC123.h"
#include "fds.h"
#include "fdsutil.h"
#include "flash.h"
#include "spiutil.h"
#include "fifo.h"

/*
int needfinish;

void fds_start_diskread(void)
{
	//clear decodebuf
	memset(decodebuf,0,DECODEBUFSIZE);
	bufpos = 0;
	sentbufpos = 0;
	needfinish = 0;

	CLEAR_WRITE();
	CLEAR_STOPMOTOR();
	SET_SCANMEDIA();

    NVIC_DisableIRQ(EINT0_IRQn);
    NVIC_DisableIRQ(TMR1_IRQn);

	TIMER_Start(TIMER0);
    NVIC_EnableIRQ(GPAB_IRQn);
}

void fds_stop_diskread(void)
{
	TIMER_Stop(TIMER0);
    NVIC_DisableIRQ(GPAB_IRQn);

	CLEAR_WRITE();
	CLEAR_SCANMEDIA();
	SET_STOPMOTOR();
}

static int get_buf_size()
{
	int ret = 0;

	if(bufpos >= sentbufpos) {
		ret = bufpos - sentbufpos;
	}
	else {
		ret = DECODEBUFSIZE - sentbufpos;
		ret += bufpos;
	}
	return(ret);
}

void fds_diskread_getdata(uint8_t *bufbuf, int len)
{
	int t,v,w;

	if(IS_READY() == 0) {
		printf("waiting drive to be ready\n");
		while(IS_READY() == 0);
	}
	
	while(get_buf_size() < len) {
//		printf("waiting for data\n");
	}

	t = sentbufpos + len;

	//if this read will loop around to the beginning of the buffer, handle it
	if(t >= DECODEBUFSIZE) {
		v = DECODEBUFSIZE - sentbufpos;
		w = len - v;
		memcpy(bufbuf,decodebuf + sentbufpos,v);
		memcpy(bufbuf + v,decodebuf,w);
		sentbufpos = w;
	}
	
	//this read will be one unbroken chunk of the buffer
	else {
		memcpy(bufbuf,decodebuf + sentbufpos,len);
		sentbufpos += len;
	}
}
*/

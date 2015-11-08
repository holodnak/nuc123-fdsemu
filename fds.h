#ifndef __fds_h__
#define __fds_h__

/*
PIN 43 = -write
PIN 42 = -scan media
PIN 41 = motor on
PIN 40 = -writeable media
PIN 39 = -media set
PIN 38 = -ready
PIN 34 = -stop motor
? = WRITEDATA
PIN 48 = data
PIN 47 = rate
*/
/*
#define PIN_WRITE		AVR32_PIN_PA31
#define PIN_SCANMEDIA	AVR32_PIN_PA30
#define PIN_MOTORON		AVR32_PIN_PA29
#define PIN_WRITABLE	AVR32_PIN_PA28
#define PIN_MEDIASET	AVR32_PIN_PA27
#define PIN_READY		AVR32_PIN_PB00
#define PIN_STOPMOTOR	AVR32_PIN_PB11
#define PIN_WRITEDATA	AVR32_PIN_PX19

#define PIN_DATA		AVR32_PIN_PA25
#define PIN_RATE		AVR32_PIN_PA26
*/
#define SET_READY()			PD0 = 0
#define SET_MEDIASET()		PD1 = 0
#define SET_WRITABLE()		PD2 = 0
#define SET_MOTORON()		PD3 = 1

#define CLEAR_READY()		PD0 = 1
#define CLEAR_MEDIASET()	PD1 = 1
#define CLEAR_WRITABLE()	PD2 = 1
#define CLEAR_MOTORON()		PD3 = 0

#define IS_WRITE()			(PD5 == 0)
#define IS_SCANMEDIA()		(PD4 == 0)
#define IS_STOPMOTOR()		(PA12 == 0)
#define IS_DONT_STOPMOTOR()	(PA12 != 0)

#define TRANSFER_RATE		96400
#define FDS_KHZ(hz)			(hz / TRANSFER_RATE)

extern volatile int diskblock;

void fds_tick(void);
void fds_insert_disk(int block);
void fds_remove_disk(void);
	
#endif

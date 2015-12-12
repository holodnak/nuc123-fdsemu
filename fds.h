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

#define SET_WRITE()			PD5 = 0
#define SET_SCANMEDIA()		PD4 = 0
#define SET_STOPMOTOR()		PA12 = 0

#define CLEAR_WRITE()		PD5 = 1
#define CLEAR_SCANMEDIA()	PD4 = 1
#define CLEAR_STOPMOTOR()	PA12 = 1

#define IS_READY()			(PD0 == 0)
#define IS_MEDIASET()		(PD1 == 0)
#define IS_WRITABLE()		(PD2 == 0)
#define IS_MOTORON()		(PD3 == 1)

#define TRANSFER_RATE		96400
#define FDS_KHZ(hz)			(hz / TRANSFER_RATE)

extern volatile int diskblock;
extern volatile int startread;
extern volatile uint8_t *readbuf[];
extern volatile int readbufready;

//struct with information/data for the disk read/write operations
typedef struct diskread_s {
	uint8_t	data[512];			//data storage for reads/writes
	uint8_t	*buf[2];			//buffer pointers
	int		ready;				//flag for when data is ready in a buffer
	int		curbuf;				//buffer currently reading/writing
	int		pos;				//position in the buffer
	int		sequence;			//chunk sequence number
} diskread_t;

void fds_init(void);
int fds_diskread_getdata(uint8_t *buf, int len);
void fds_start_diskread(void);
void fds_setup_transfer(void);
void fds_setup_diskread(void);
void fds_tick(void);
void fds_insert_disk(int block);
void fds_remove_disk(void);
	
int fds_diskwrite_fillbuf(uint8_t *buf, int len);
int fds_diskwrite(void);

#endif

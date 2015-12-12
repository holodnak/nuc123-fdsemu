#ifndef __main_h__
#define __main_h__

#include "version.h"
#include "build.h"

#define REVISION_3

#ifdef REVISION_3

#define LED_G_PORT	PB
#define LED_G_PIN	BIT10
#define LED_G		PB10

#define LED_R_PORT	PC
#define LED_R_PIN	BIT10
#define LED_R		PC10

#else

//prototype
#define LED_G_PORT	PC
#define LED_G_PIN	BIT12
#define LED_G		PC12

#define LED_R_PORT	PC
#define LED_R_PIN	BIT13
#define LED_R		PC13

#endif

#define LED_GREEN(nn)	LED_G = nn;
#define LED_RED(nn)		LED_R = nn;


extern const uint32_t version;
extern const uint32_t buildnum;

#endif

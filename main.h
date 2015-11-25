#ifndef __main_h__
#define __main_h__

#include "version.h"
#include "build.h"

#define LED_GREEN(nn)	PC12 = nn;
#define LED_RED(nn)		PC13 = nn;

extern const uint32_t version;
extern const uint32_t buildnum;

#endif

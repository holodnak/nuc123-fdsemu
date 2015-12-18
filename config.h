#ifndef __config_h__
#define __config_h__

#ifdef PROTOTYPE

#define SPI_FLASH	SPI0
#define SPI_SRAM	SPI1


#else

#define SPI_FLASH	SPI1
#define SPI_SRAM	SPI0

#endif

#endif

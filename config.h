#ifndef __config_h__
#define __config_h__

#define FASTCLK
//#define PROTOTYPE

#ifdef PROTOTYPE
#define SPI_FLASH	SPI0
#define SPI_SRAM	SPI1
#else
#define SPI_FLASH	SPI1
#define SPI_SRAM	SPI0
#endif

#ifdef FASTCLK
#define HCLK_CLOCK			72000000
#define USB_CLKDIV			CLK_CLKDIV_USB(3)
#define UART_CLKDIV			CLK_CLKDIV_UART(1)
#define SPI_SRAM_CLK		20000000
#define SPI_FLASH_CLK		35000000
#else
#define HCLK_CLOCK			48000000
#define USB_CLKDIV			CLK_CLKDIV_USB(2)
#define UART_CLKDIV			CLK_CLKDIV_UART(1)
#define SPI_SRAM_CLK		15000000
#define SPI_FLASH_CLK		30000000
#endif

#endif

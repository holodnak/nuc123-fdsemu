/******************************************************************************
 * @file     main.c
 * @version  V1.00
 * $Revision: 4 $
 * $Date: 15/07/02 11:18a $
 * @brief    Software Development Template.
 * @note
 * Copyright (C) 2014~2015 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

/*
TODO:

 - when device is first plugged in, and it is connected to a disk drive, it takes a
 small amount of time to get the pins ready so the drive has a little activity.
   --solution: move the fds pin init code closer to the beginning of execution.
   
 - clean up code
*/

#include <stdio.h>
#include "NUC123.h"
#include "spiutil.h"
#include "flash.h"
#include "sram.h"
#include "fds.h"
#include "hid_transfer.h"
#include "main.h"
#include "config.h"

//#define EXTERNAL_XTAL

#define HCLK_CLOCK           72000000

const uint32_t version = VERSION;
const uint32_t buildnum = BUILDNUM;

void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable Internal RC 22.1184MHz clock */
    CLK_EnableXtalRC(CLK_PWRCON_OSC22M_EN_Msk);

    /* Waiting for Internal RC clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_OSC22M_STB_Msk);

    /* Switch HCLK clock source to Internal RC and HCLK source divide 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_HIRC, CLK_CLKDIV_HCLK(1));

    /* Set core clock as HCLK_CLOCK */
    CLK_SetCoreClock(HCLK_CLOCK);

    /* Enable module clocks */
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(SPI0_MODULE);
    CLK_EnableModuleClock(SPI1_MODULE);
    CLK_EnableModuleClock(TMR0_MODULE);
    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_EnableModuleClock(TMR2_MODULE);
    CLK_EnableModuleClock(TMR3_MODULE);
    CLK_EnableModuleClock(USBD_MODULE);
    CLK_EnableModuleClock(WDT_MODULE);

    /* Select HCLK as the clock source of SPI0 */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_CLKDIV_UART(1));
    CLK_SetModuleClock(SPI0_MODULE, CLK_CLKSEL1_SPI0_S_HCLK, MODULE_NoMsk);
    CLK_SetModuleClock(SPI1_MODULE, CLK_CLKSEL1_SPI1_S_HCLK, MODULE_NoMsk);
    CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0_S_HCLK, 0);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1_S_HCLK, 0);
    CLK_SetModuleClock(TMR2_MODULE, CLK_CLKSEL1_TMR2_S_HCLK, 0);
    CLK_SetModuleClock(TMR3_MODULE, CLK_CLKSEL1_TMR3_S_HCLK, 0);
    CLK_SetModuleClock(USBD_MODULE, 0, CLK_CLKDIV_USB(3));
    CLK_SetModuleClock(WDT_MODULE, CLK_CLKSEL1_WDT_S_LIRC, 0);

    /* Select UART module clock source */

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Set GPB multi-function pins for UART0 RXD(PB.0) and TXD(PB.1) */
    SYS->GPC_MFP &= ~(SYS_GPC_MFP_PC4_Msk | SYS_GPC_MFP_PC5_Msk);
    SYS->GPC_MFP = SYS_GPC_MFP_PC4_UART0_RXD | SYS_GPC_MFP_PC5_UART0_TXD;
	SYS->ALT_MFP = SYS_ALT_MFP_PC4_UART0_RXD | SYS_ALT_MFP_PC5_UART0_TXD;

    /* Setup SPI0 multi-function pins */
    SYS->GPC_MFP |= SYS_GPC_MFP_PC0_SPI0_SS0 | SYS_GPC_MFP_PC1_SPI0_CLK | SYS_GPC_MFP_PC2_SPI0_MISO0 | SYS_GPC_MFP_PC3_SPI0_MOSI0;
    SYS->ALT_MFP |= SYS_ALT_MFP_PC0_SPI0_SS0 | SYS_ALT_MFP_PC1_SPI0_CLK | SYS_ALT_MFP_PC2_SPI0_MISO0 | SYS_ALT_MFP_PC3_SPI0_MOSI0;

    /* Setup SPI1 multi-function pins */
    SYS->GPC_MFP |= SYS_GPC_MFP_PC8_SPI1_SS0 | SYS_GPC_MFP_PC9_SPI1_CLK | SYS_GPC_MFP_PC10_SPI1_MISO0 | SYS_GPC_MFP_PC11_SPI1_MOSI0;
    SYS->ALT_MFP |= SYS_ALT_MFP_PC8_SPI1_SS0 | SYS_ALT_MFP_PC9_SPI1_CLK | SYS_ALT_MFP_PC10_SPI1_MISO0 | SYS_ALT_MFP_PC11_SPI1_MOSI0;

	//enable Port F GPIO
    SYS->GPF_MFP = 0;

	//int0
//    SYS->GPB_MFP |= SYS_GPB_MFP_PB14_INT0;

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock and cyclesPerUs automatically. */
    SystemCoreClockUpdate();

    /* Enable interrupt de-bounce function and select de-bounce sampling cycle time is 1024 clocks of LIRC clock */
	GPIO_SET_DEBOUNCE_TIME(GPIO_DBCLKSRC_HCLK, GPIO_DBCLKSEL_128);
}

void UART0_Init()
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Reset UART0 module */
    SYS_ResetModule(UART0_RST);

    /* Init UART0 to 115200-8n1 for print message */
    UART_Open(UART0, 115200);
}

void SPI_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init SPI                                                                                                */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Configure as a master, clock idle low, 32-bit transaction, drive output on falling clock edge and latch input on rising edge. */
    /* Set IP clock divider. SPI clock rate = 2MHz */
    SPI_Open(SPI_FLASH, SPI_MASTER, SPI_MODE_0, 8, 35000000);
    SPI_Open(SPI_SRAM, SPI_MASTER, SPI_MODE_0, 8, 20000000);
}

static void print_block_info(int block)
{
	flash_header_t header2;

	flash_read_disk_header(block,&header2);
	if(header2.name[0] == 0xFF) {
		printf("block %X: empty\r\n",block);
	}
	else {
		printf("block %X: id = %02d, '%s'\r\n",block,header2.id,header2.name);
	}
}

void hexdump(char *desc, void *addr, int len)
{
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char *)addr;

	// Output description if given.
	if (desc != NULL)
	printf("%s:\r\n", desc);

	// Process every byte in the data.
	for (i = 0; i < len; i++) {
		// Multiple of 16 means new line (with line offset).

		if ((i % 16) == 0) {
			// Just don't print ASCII for the zeroth line.
			if (i != 0)
			printf("  %s\r\n", buff);

			// Output the offset.
			printf("  %04x ", i);
		}
		// Now the hex code for the specific character.
		printf(" %02x", pc[i]);

		// And store a printable ASCII character for later.
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
		buff[i % 16] = '.';
		else
		buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	// Pad out last line if not exactly 16 characters.
	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}

	// And print the final ASCII bit.
	printf("  %s\r\n", buff);
}

int read_char(int *ch)
{
	if((DEBUG_PORT->FSR & UART_FSR_RX_EMPTY_Msk) == 0 ) {
		*ch = (int)(uint8_t)DEBUG_PORT->DATA;
		return (0);
	}
	return(-1);
}

uint8_t crap[256];

void console_tick(void)
{
	int ch = 0;
	
	if(read_char(&ch) == 0) {
		int n;
		char help[] =
		"help:\r\n"
		"  0-F : select block to read disk data from\r\n"
		"  i   : insert disk\r\n"
		"  r   : remove disk\r\n"
		"  f   : flip disk to next side/disk\r\n"
		"  p   : print disks stored in flash\r\n"
		"  d   : disk read mode\r\n"
		"  t   : transfer mode\r\n"
		"\r\n";

		switch((char)ch) {
		case '?':
			printf("%s",help);
			printf("currently selected disk in block is %d.\r\n\r\n",diskblock);
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			if(ch >= 'A' && ch <= 'F') {
				diskblock = 10 + (ch - 'A');
			}
			else {
				diskblock = ch - '0';
			}
			printf("selected disk in block %X.\r\n",diskblock);
			break;
		case 'i':
			fds_insert_disk(diskblock);
			break;
		case 'r':
			fds_remove_disk();
			break;
		case 'p':
			for(n=0;n<0x10;n++) {
				print_block_info(n);
			}
			break;
		case 's':
			flash_init();
			sram_init();
			break;
		case 'd':
			printf("entering disk read mode.\n");
			fds_setup_diskread();
 			break;
		case 't':
			printf("entering transfer mode.\n");
			fds_setup_transfer();
			if(IS_READY())		printf("drive is ready\n");
			else				printf("drive is not ready\n");
			if(IS_MEDIASET())	printf("media is set\n");
			else				printf("media is not set\n");
			if(IS_WRITABLE())	printf("media is writable\n");
			else				printf("media is not writable\n");
			if(IS_MOTORON())	printf("motor is on\n");
			else				printf("motor is not on\n");
 			break;
		case 'c':
			sram_read(3537,crap,256);
			hexdump("crap",crap,256);
			break;
		case 'v':
			printf("stopping read\n");
			CLEAR_WRITE();
			CLEAR_SCANMEDIA();
			SET_STOPMOTOR();
			break;
		}
	}
}

void debugz(void)
{

#define GPIO_PIN_DATA2(port, pin)    ((GPIO_PIN_DATA_BASE+(0x40*(port))) + ((pin)<<2))
#define iPA1            GPIO_PIN_DATA2(0, 1) /*!< Specify PA.10 Pin Data Input/Output */
#define iPA10            GPIO_PIN_DATA2(0, 10) /*!< Specify PA.10 Pin Data Input/Output */
#define iPA11            GPIO_PIN_DATA2(0, 11) /*!< Specify PA.11 Pin Data Input/Output */
#define iPA12            GPIO_PIN_DATA2(0, 12) /*!< Specify PA.12 Pin Data Input/Output */
#define iPA13            GPIO_PIN_DATA2(0, 13) /*!< Specify PA.13 Pin Data Input/Output */
#define iPA14            GPIO_PIN_DATA2(0, 14) /*!< Specify PA.14 Pin Data Input/Output */
#define iPA15            GPIO_PIN_DATA2(0, 15) /*!< Specify PA.15 Pin Data Input/Output */
#define iPB0             GPIO_PIN_DATA2(1, 0 ) /*!< Specify PB.0 Pin Data Input/Output */
#define iPB1             GPIO_PIN_DATA2(1, 1 ) /*!< Specify PB.1 Pin Data Input/Output */
#define iPB2             GPIO_PIN_DATA2(1, 2 ) /*!< Specify PB.2 Pin Data Input/Output */
#define iPB10            GPIO_PIN_DATA2(1, 10) /*!< Specify PB.10 Pin Data Input/Output */
#define iPB11            GPIO_PIN_DATA2(1, 11) /*!< Specify PB.10 Pin Data Input/Output */
	
printf("PA1 - %08X - %08X %08X\n",iPA1,(uint32_t)PA,BIT1);
printf("PA10 - %08X - %08X %08X\n",iPA10,(uint32_t)PA,BIT10);
printf("PA11 - %08X - %08X %08X\n",iPA11,(uint32_t)PA,BIT11);
printf("PA12 - %08X - %08X %08X\n",iPA12,(uint32_t)PA,BIT12);
printf("PB10 - %08X - %08X %08X\n",iPB10,(uint32_t)PB,BIT10);
printf("PB11 - %08X - %08X %08X\n",iPB11,(uint32_t)PB,BIT11);

#define MAKE_PORT(nn)	(((((uint32_t)&(nn)) - (uint32_t)GPIO_PIN_DATA_BASE) & 0x1C0) + GPIO_BASE)
#define MAKE_PIN(nn)	(1 << (((((uint32_t)&(nn)) - (uint32_t)GPIO_PIN_DATA_BASE) & 0x3F) / 4))

printf("PB10 %08X - %08X %08X - %08X %08X\n",&PB10,(uint32_t)PB,BIT10,MAKE_PORT(PB10),MAKE_PIN(PB10));
}

int main()
{
	//pc12 = green
	//pc13 = red
	CLEAR_WRITE();
	SET_STOPMOTOR();
	CLEAR_SCANMEDIA();
	
	//setup led and button gpio
    GPIO_SetMode(LED_G_PORT, LED_G_PIN, GPIO_PMD_OUTPUT);
    GPIO_SetMode(LED_R_PORT, LED_R_PIN, GPIO_PMD_OUTPUT);
    GPIO_SetMode(SWITCH_PORT, SWITCH_PIN, GPIO_PMD_INPUT);
	LED_GREEN(0);
	LED_RED(1);

    /* Unlock protected registers */
    SYS_UnlockReg();

    SYS_Init();

    /* Lock protected registers */
    SYS_LockReg();

    UART0_Init();
	SPI_Init();

	TIMER_Open(TIMER0, TIMER_CONTINUOUS_MODE, 6000000);
	TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 96400 * 2);
	TIMER_Open(TIMER3, TIMER_PERIODIC_MODE, 96400 * 2);
	TIMER_EnableInt(TIMER1);
	TIMER_EnableInt(TIMER3);

	/* Open USB controller */
    USBD_Open(&gsInfo, HID_ClassRequest, NULL);

    /* Init Endpoint configuration for HID */
    HID_Init();

    /* Start USB device */
    USBD_Start();

    /* Enable USB device interrupt */
    NVIC_EnableIRQ(USBD_IRQn);

//	debugz();

	LED_GREEN(1);
	LED_RED(0);
    printf("\n\nnuc123-fdsemu v%d.%02d build %d started.  Compiled on "__DATE__" at "__TIME__"\n",version / 100,version % 100,BUILDNUM);
    printf("--CPU @ %0.3f MHz\n", (double)SystemCoreClock / 1000000.0f);
    printf("--SPI0 @ %0.3f MHz\n", (double)SPI_GetBusClock(SPI0) / 1000000.0f);
    printf("--SPI1 @ %0.3f MHz\n", (double)SPI_GetBusClock(SPI1) / 1000000.0f);
	
	NVIC_SetPriority(USBD_IRQn,2);
	NVIC_SetPriority(TMR1_IRQn,1);
	NVIC_SetPriority(TMR3_IRQn,0);
	NVIC_SetPriority(GPAB_IRQn,0);
	NVIC_SetPriority(EINT0_IRQn,0);
	
	printf("USBD_IRQn priority: %d\n",NVIC_GetPriority(USBD_IRQn));
	printf("TMR1_IRQn priority: %d\n",NVIC_GetPriority(TMR1_IRQn));
	printf("TMR3_IRQn priority: %d\n",NVIC_GetPriority(TMR3_IRQn));
	printf("GPAB_IRQn priority: %d\n",NVIC_GetPriority(GPAB_IRQn));
	printf("EINT0_IRQn priority: %d\n",NVIC_GetPriority(EINT0_IRQn));

	flash_init();
	sram_init();

	fds_init();

	print_block_info(0);
	while(1) {
		console_tick();
		fds_tick();
	}
}

/*** (C) COPYRIGHT 2014~2015 Nuvoton Technology Corp. ***/

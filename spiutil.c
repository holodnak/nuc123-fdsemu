#include <stdio.h>
#include "spiutil.h"

#define SPI_TIMEOUT		20000

void spi_init(void)
{
}

int spi_fifo_read_packet(SPI_T *spi, uint8_t *data, int len)
{
/*	int timeout = SPI_TIMEOUT;

	SPI_WRITE_TX0(spi,0);
	while(len) {
		if(SPI_GET_RX_FIFO_EMPTY_FLAG(SPI0) == 0) {
			timeout = SPI_TIMEOUT;
			*data = SPI_READ_RX0(spi);
			SPI_WRITE_TX0(spi,0);
			printf("spi_fifo_read_packet: data = $%02X, len = %d\n",*data,len);
			data++;
			len--;
		}
		if(!timeout--) {
			printf("spi_fifo_read_packet timeout (len = %d)\n", len);
			return(-1);
		}
	}*/
    uint32_t u32RxCounter, u32TxCounter;

    /* /CS: active */
//    SPI_SET_SS0_LOW(spi);

    spi->FIFO_CTL |= SPI_FIFO_CTL_RX_CLR_Msk;

    u32RxCounter = 0;
    u32TxCounter = 0;
    while(u32RxCounter < len)
    {
        while(SPI_GET_RX_FIFO_EMPTY_FLAG(spi) == 0)
        {
            data[u32RxCounter] = SPI_READ_RX0(spi);
            u32RxCounter++;
        }
        if((SPI_GET_TX_FIFO_FULL_FLAG(spi) != 1) && (u32TxCounter < len))
        {
            SPI_WRITE_TX0(spi, 0x00);
            u32TxCounter++;
        }
    }

    /* /CS: inactive */
//    SPI_SET_SS0_HIGH(spi);
	return(0);
}

int spi_fifo_write_packet(SPI_T *spi, uint8_t *data, int len)
{
/*	int timeout = SPI_TIMEOUT;

	while(len) {
		//check if the fifo buffer has room for another byte
		if(SPI_GET_TX_FIFO_FULL_FLAG(spi) == 0) {
			printf("spi_fifo_write_packet: data = $%02X, len = %d\n",*data,len);
			timeout = SPI_TIMEOUT;
			SPI_WRITE_TX0(spi, *data);
			data++;
			len--;
		}
		if(!timeout--) {
			printf("spi_fifo_write_packet timeout (len = %d)\n",len);
			return(-1);
		}
	}*/
    /* /CS: active */
    uint32_t u32Counter;

//    SPI_SET_SS0_LOW(spi);

    u32Counter = 0;
    while(u32Counter < len)
    {
        if(SPI_GET_TX_FIFO_FULL_FLAG(spi) != 1)
            SPI_WRITE_TX0(spi, data[u32Counter++]);
    }
    /* Check the BUSY status and TX empty flag */
    while((spi->CNTRL & (SPI_CNTRL_GO_BUSY_Msk | SPI_CNTRL_TX_EMPTY_Msk)) != SPI_CNTRL_TX_EMPTY_Msk);
    /* Clear RX FIFO */
    spi->FIFO_CTL |= SPI_FIFO_CTL_RX_CLR_Msk;
    /* /CS: inactive */
//    SPI_SET_SS0_HIGH(spi);
	return(0);
}

int spi_read_packet(SPI_T *spi, uint8_t *data, int len)
{
	int timeout, i;
	
	for(i=0; len != 0; i++, len--) {
		timeout = SPI_TIMEOUT;
		SPI_WRITE_TX0(spi, 0);
		SPI_TRIGGER(spi);
		while(SPI_IS_BUSY(spi)) {
			if(!timeout--) {
				printf("spi_read_packet timeout (len = %d)\n",len);
				return(-1);
			}
		}
		data[i] = SPI_READ_RX0(spi);
	}
	return(0);
}

int spi_write_packet(SPI_T *spi, uint8_t *data, int len)
{
	int timeout, i;

	for(i=0; len != 0; i++, len--) {
		SPI_WRITE_TX0(spi, data[i]);
		SPI_TRIGGER(spi);
		timeout = SPI_TIMEOUT;
		while(SPI_IS_BUSY(spi)) {
			if(!timeout--) {
				return(-1);
			}
		}
	}
	return(0);
}

void spi_select_device(SPI_T *spi, int ss)
{
	SPI_SET_SS0_LOW(spi);
}

void spi_deselect_device(SPI_T *spi, int ss)
{
	SPI_SET_SS0_HIGH(spi);
}

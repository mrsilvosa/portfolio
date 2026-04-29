/*
 * SD_SPI.c
 *
 *  Created on: Oct 12, 2025
 *      Author: mrsilvosa
 */


#include "SD_SPI.h"
#include "main.h"
#include "SD_SPI_BSP_F4xx.h"

#ifdef SD_DEBUG
#include <stdio.h>

extern void initialise_monitor_handles(void);
#endif

// variable global interna:

static bool sd_is_SDHC;
static bool sd_block_is_set_512;

// función para enviar bytes dummy 0xFF:
void sd_send_dummy(uint8_t n)
{
	uint8_t dummy = 0xFF;

	while(n--)
	{
		SD_BSP_SPI_transmit(&dummy, 1);
	}
}

// enviar un comando completo 6 bytes:
uint8_t sd_send_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t* resp, uint32_t resp_amount, SDCommandMode_t mode, bool is_CS_left_LOW)
{
	uint8_t packet[6];
	uint8_t dummy_tx=0xFF;
	uint8_t dummy_rx=0;

	SD_CS_HIGH();
	/*
	if(is_CS_left_LOW)
	{
		SD_CS_HIGH();
		SD_CS_HIGH();
	}
	*/

	switch(mode)
	{
	case WR:
		packet[0] = cmd;
		packet[1] = (arg >> 24) & 0xFF;
		packet[2] = (arg >> 16) & 0xFF;
		packet[3] = (arg >> 8) & 0xFF;
		packet[4] = arg & 0xFF;
		packet[5] = crc;

		SD_CS_LOW();
		/*
		if(is_CS_left_LOW)
		{
			SD_CS_LOW();
			SD_CS_LOW();
		}
		*/
		//sd_send_dummy(1);
		SD_BSP_SPI_transmit(packet, 6);

		SD_BSP_SPI_transmitReceive(&dummy_tx, &dummy_rx, 1);
		SD_BSP_SPI_transmitReceive(&dummy_tx, resp, resp_amount);

		if(!is_CS_left_LOW)
			SD_CS_HIGH();
		//sd_send_dummy(1);
		return 0;
		break;
	case W:
		// Build command packet
		packet[0] = cmd;
		packet[1] = (arg >> 24) & 0xFF;
		packet[2] = (arg >> 16) & 0xFF;
		packet[3] = (arg >> 8) & 0xFF;
		packet[4] = arg & 0xFF;
		packet[5] = crc;

		SD_CS_LOW();
		//sd_send_dummy(1);
		SD_BSP_SPI_transmit(packet, 6);
		if(!is_CS_left_LOW)
			SD_CS_HIGH();
		//sd_send_dummy(1); // One dummy byte after command
		return 0;
		break;
	case R:
		SD_CS_LOW();

		SD_BSP_SPI_transmitReceive(&dummy_tx, &dummy_rx, 1);
		SD_BSP_SPI_transmitReceive(&dummy_tx, resp, resp_amount);

		if(!is_CS_left_LOW)
			SD_CS_HIGH();
		//sd_send_dummy(1);
		return 0;
		break;
	default:
		break;
	}

	return 1;
}

// Esperar hasta que la SD esté lista:
bool sd_wait_ready(void)
{
	uint8_t resp;
	uint32_t timeout = SD_BSP_ms_GetTick() + 500; // 2000 ms
	uint8_t dummy_tx=0xFF;

	do
	{
		SD_BSP_SPI_transmitReceive(&dummy_tx, &resp, 1);

		if(resp == 0xFF)
		{
			return true;
		}
	}while(SD_BSP_ms_GetTick() < timeout);

	return false;
}

// inicialización SD en modo SPI:
bool sd_init(void)
{
	uint8_t resp;
	uint8_t buf[5];
    //uint32_t timeout = 2000;
    uint32_t auxok = 0;

	do
	{
	    SD_BSP_ms_delay(10);	// wait power on

		SD_CS_HIGH();	// me aseguro que esté alta
		SD_BSP_ms_delay(2);
		sd_send_dummy(200); // serían 80 clocks minimo con CS alto para despertar a la SD
		sd_wait_ready();
		sd_send_command(CMD0, 0, 0x95, &resp, 1, WR, false); // GO_IDLE_STATE

		if(resp == R1_IDLE_STATE)
		{
			auxok = 1;
		}
	}while(auxok == 0);


	/*
	if(resp != 0x01)
	{
		return false;
	}
	*/

	// CMD8_ verificar tension solo para SDv2:
	// CMD8 SEND_IF_COND
	sd_send_command(CMD8, 0x000001AA, 0x87, &resp, 1, WR, false); // SEND_IF_COND

	if(resp == R1_IDLE_STATE) // SD v2
	{
		sd_send_command(CMD8, 0x000001AA, 0x87, &buf[0], 4, R, false);
	}

	// esperamos hasta que salga del estado IDLE con ACMD41:

	//uint32_t t0 = SD_BSP_ms_GetTick();

	do
	{
		sd_send_command(CMD55, 0, 0x65, &resp, 1, WR, false); // Precede ACMD41 con CMD55

		sd_send_command(ACMD41, 0x40000000, 0x77, &resp, 1, WR, false); // HCS = 1 para SDHC

		/*
		if((HAL_GetTick() - t0) > 2000) // timeout
		{
			return false;
		}
		*/
	}while(resp != 0x00);

	// Leer OCR con CMD58:
	sd_send_command(CMD58, 0, 0xFD, &buf[0], 5, WR, false);

	sd_is_SDHC = (buf[1] & 0x40) != 0;	// del OCR leído

	// Set block length to 512 bytes

	sd_send_command(CMD16, 512, 0x01, &resp, 1, WR, false);

	if(resp == 0x00)
	{
		sd_block_is_set_512 = true;
	}
	else
	{
		sd_block_is_set_512 = false;
	}
	SD_CS_HIGH();
	sd_send_dummy(3);

	return true;
}

/**
  * @brief  Lectura sobre la SD de 1 bloque de 512 bytes.
  *
  * @note   El módulo está pensado para funcionar con SPI modo 8 bits.
  * 		La lectura en este caso se hace mediante DMA hasta que finalice.
  *
  * @param  block:  Nº de bloque de la SD que se quiere leer.
  * 		La lectura se hace siempre en bloques.
  *
  * @param  dummyBufferTx: puntero a un buffer obligatoriamente lleno de 0xFF.
  *         El buffer tendrá que tener un tamaño de 514 bytes obligatoriamente.
  *
  * @param  buffer: puntero al buffer donde se almacenará la lectura de datos.
  * 		Este buffer también tendrá que ser obligatoriamente de 514 bytes.
  * 		Esto se debe a que se inyectan los 2 bytes de CRC que no se utilizan.
  *
  * @retval bool: indica si la lectura se hizo con éxito o a fallado.
  * 		@arg true: La lectura fue finalizada con exito.
  * 		@arg false: La lectura falló en alguna etapa.
  */
bool sd_read_block(uint32_t block, uint8_t* dummyBufferTx, uint8_t* buffer)
{
	uint8_t token;

	uint32_t addr=0;
	uint8_t resp=0;
	uint8_t dummy_tx=0xFF;


	if(sd_is_SDHC && sd_block_is_set_512)
	{
		addr = block;
	}
	else
	{
		addr = block * 512;
	}

	sd_send_command(CMD17, addr, 0xFF, &resp, 1, WR, true);
	resp = resp;
	if(resp != 0x00)
	{
		SD_CS_HIGH();
		sd_send_dummy(1);
		return false;
	}

	// esperamos token de inicio 0xFE:

	uint32_t timeout = SD_BSP_ms_GetTick() + 2000;

	do
	{
		SD_BSP_SPI_transmitReceive(&dummy_tx, &token, 1);

		if(token == 0xFE)
		{
			break;
		}

	}while(SD_BSP_ms_GetTick() < timeout);

	if(token != 0xFE)
	{
		SD_CS_HIGH();
		sd_send_dummy(1);
		return false;
	}

	//SD_BSP_SPI_transmitReceive_DMA_start(dummyBufferTx, buffer, 514);
	//SD_BSP_SPI_transmitReceive_DMA_waitReady();
	//SD_BSP_SPI_transmitReceive_DMA_finish();
	SD_BSP_SPI_transmitReceive(dummyBufferTx, buffer, 514);
	SD_BSP_SPI_transmitReceive_DMA_finish();

	return true;
}


/**
  * @brief  Lectura sobre la SD de 1 bloque de 512 bytes.
  *
  * @note   El módulo está pensado para funcionar con SPI modo 8 bits.
  * 		La lectura en este caso se hace mediante DMA.
  * 		La lectura no es esperada aquí a que termine, para ello
  * 		hay que llamar a SD_BSP_SPI_transmitReceive_DMA_finish()
  * 		en la ISR correspondiente para terminar la transmisión.
  *
  * @param  block:  Nº de bloque de la SD que se quiere leer.
  * 		La lectura se hace siempre en bloques.
  *
  * @param  dummyBufferTx: puntero a un buffer obligatoriamente lleno de 0xFF.
  *         El buffer tendrá que tener un tamaño de 514 bytes obligatoriamente.
  *         Se debe garantizar que el buffer siga existiendo hasta que finalice.
  *
  * @param  buffer: puntero al buffer donde se almacenará la lectura de datos.
  * 		Este buffer también tendrá que ser obligatoriamente de 514 bytes.
  * 		Esto se debe a que se inyectan los 2 bytes de CRC que no se utilizan.
  * 		Se debe garantizar que el buffer siga existiendo hasta que finalice.
  *
  * @retval bool: indica si la lectura se inició con éxito o a fallado.
  * 		@arg true: La lectura fue iniciada con exito.
  * 		@arg false: La lectura falló en alguna etapa.
  */
bool sd_read_block_DMA(uint32_t block, uint8_t* dummyBufferTx, uint8_t* buffer)
{
	uint8_t token = 0xAA;

	uint32_t addr=0;
	uint8_t resp=0;
	uint8_t dummy_tx=0xFF;

	if(sd_is_SDHC && sd_block_is_set_512)
	{
		addr = block;
	}
	else
	{
		addr = block * 512;
	}

	sd_send_command(CMD17, addr, 0xFF, &resp, 1, WR, true);
	resp = resp;
	if(resp != 0x00)
	{
		SD_CS_HIGH();
		//sd_send_dummy(1);
		return false;
	}

	// esperamos token de inicio 0xFE:

	uint32_t timeout = SD_BSP_ms_GetTick() + 2000;

	do
	{
		//HAL_SPI_Receive(&SD_SPI_HANDLE, &token, 1, HAL_MAX_DELAY);
		SD_BSP_SPI_transmitReceive(&dummy_tx, &token, 1);

		if(token == 0xFE)
		{
			break;
		}

	}while(SD_BSP_ms_GetTick() < timeout);

	if(token != 0xFE)
	{
		SD_CS_HIGH();
		sd_send_dummy(1);
		return false;
	}

	SD_BSP_SPI_transmitReceive_DMA_start(dummyBufferTx, buffer, 514);
	//SD_BSP_SPI_Receive_DMA_start(dummyBufferTx, buffer, 514);

	return true;
}

// Escritura de un bloque de 512 bytes:
bool sd_write_block(uint32_t block, const uint8_t* buffer, uint8_t* dummyBufferRx)
{
	uint32_t addr=0;
	uint8_t resp=0;
	uint8_t dummy_rx=0;
	uint8_t dummyBuffer2[2];
	uint8_t dummy_tx=0xFF;


	if(sd_is_SDHC && sd_block_is_set_512)
	{
		addr = block;
	}
	else
	{
		addr = block * 512;
	}

	sd_send_command(CMD24, addr, 0xFF, &resp, 1, WR, true);

	while(resp != 0x00)
	{
		SD_BSP_SPI_transmitReceive(&dummy_tx, &resp, 1);
	}

	if(!sd_wait_ready())
	{
		SD_CS_HIGH();
		sd_send_dummy(1);
		return false;
	}

	uint8_t token = 0xFE;

	sd_send_dummy(1);

	SD_BSP_SPI_transmitReceive(&token, &dummy_rx, 1);

	SD_BSP_SPI_transmitReceive(buffer, dummyBufferRx, 512);

	uint8_t crc[2] = {0xFF, 0xFF};

	SD_BSP_SPI_transmitReceive(crc, dummyBuffer2, 2);

	// Data response:

	do
	{
		SD_BSP_SPI_transmitReceive(&dummy_tx, &dummy_rx, 1);
		resp = dummy_rx;
		resp = (resp & 0x1F);
		if(resp == 0b01011)
		{
			// data rejected due to a CRC error
			SD_CS_HIGH();
			sd_send_dummy(1);
			return false;
		}
		if(resp == 0b01101)
		{
			// data rejected due to a write error
			SD_CS_HIGH();
			sd_send_dummy(1);
			return false;
		}
	}while(false);	// meant to retry but not worth the effort.


	if(resp != 0x05)
	{
		SD_CS_HIGH();
		sd_send_dummy(1);
		return false;
	}

	sd_send_dummy(8);

	if(!sd_wait_ready())
	{
		SD_CS_HIGH();
		sd_send_dummy(1);
		return false;
	}

	SD_CS_HIGH();
	sd_send_dummy(1);
	return true;

}


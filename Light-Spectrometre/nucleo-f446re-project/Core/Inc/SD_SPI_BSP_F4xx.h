/*
 * SD_SPI_BSP_F4xx.h
 *
 *  Created on: Nov 20, 2025
 *      Author: mrsilvosa
 */

#ifndef INC_SD_SPI_BSP_F4XX_H_
#define INC_SD_SPI_BSP_F4XX_H_

//List of includes
#include <stdbool.h>
//** CHANGE BASED ON STM32 CHIP F4/F7/F1...**//
#include "stm32f4xx_hal.h"

// Conf del HW:
// obligatoriamente se tienen que definir para funcionar

extern SPI_HandleTypeDef* SD_SPI_HANDLE;
extern GPIO_TypeDef* SD_CS_PORT;
extern uint16_t SD_CS_PIN;

// Macros de control:

#define SD_CS_LOW() HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

// funciones:

static inline void SD_BSP_SPI_transmit(uint8_t* pdata, uint16_t Size)
{
	HAL_SPI_Transmit(SD_SPI_HANDLE, pdata, Size, HAL_MAX_DELAY);
}
static inline void SD_BSP_SPI_transmitReceive(uint8_t* TxData, uint8_t* RxData, uint16_t Size)
{
	HAL_SPI_TransmitReceive(SD_SPI_HANDLE, TxData, RxData, Size, HAL_MAX_DELAY);
}
static inline uint32_t SD_BSP_ms_GetTick (void)
{
	return HAL_GetTick();
}
static inline void SD_BSP_ms_delay(uint32_t mseg)
{
	HAL_Delay(mseg);
}
static inline void SD_BSP_SPI_transmitReceive_DMA_start(uint8_t* bufferTx,uint8_t* bufferRx, uint16_t size)
{
	HAL_SPI_TransmitReceive_DMA(SD_SPI_HANDLE, bufferTx, bufferRx, size);
}
static inline void SD_BSP_SPI_Receive_DMA_start(uint8_t* bufferTx,uint8_t* bufferRx, uint16_t size)
{
	HAL_SPI_Receive_DMA(SD_SPI_HANDLE, bufferRx, size);
}
static inline void SD_BSP_SPI_transmitReceive_DMA_finish(void)
{
	// solo existe para dar contexto a la acción
	SD_CS_HIGH();
}
static inline void SD_BSP_SPI_transmitReceive_DMA_waitReady(void)
{
	while(HAL_SPI_GetState(SD_SPI_HANDLE) != HAL_SPI_STATE_READY){}
}
#endif /* INC_SD_SPI_BSP_F4XX_H_ */

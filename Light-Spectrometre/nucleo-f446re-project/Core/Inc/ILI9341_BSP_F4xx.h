/*
 * ILI9341_BSP_F446RE.h
 *
 *  Created on: Nov 19, 2025
 *      Author: mrsilvosa
 */

#ifndef INC_ILI9341_BSP_F4XX_H_
#define INC_ILI9341_BSP_F4XX_H_


//List of includes
#include <stdbool.h>
//** CHANGE BASED ON STM32 CHIP F4/F7/F1...**//
#include "stm32f4xx_hal.h"

struct ILI9341_BSP_variables
{
	SPI_HandleTypeDef* lcdSPIhandle;
	//Chip Select pin
	GPIO_TypeDef  *tftCS_GPIO;
	uint16_t tftCS_PIN;
	//Data Command pin
	GPIO_TypeDef  *tftDC_GPIO;
	uint16_t tftDC_PIN;
	//Reset pin
	GPIO_TypeDef  *tftRESET_GPIO;
	uint16_t tftRESET_PIN;
};

static struct ILI9341_BSP_variables ILI9341_vars;

static inline void ILI9341_BSP_init(struct ILI9341_BSP_variables* myHWvars)
{
	//ILI9341_vars._cp437 = myHWvars->_cp437;
	ILI9341_vars.lcdSPIhandle = myHWvars->lcdSPIhandle;
	//ILI9341_vars.rotationNum = myHWvars->rotationNum;
	ILI9341_vars.tftCS_GPIO = myHWvars->tftCS_GPIO;
	ILI9341_vars.tftCS_PIN = myHWvars->tftCS_PIN;
	ILI9341_vars.tftDC_GPIO = myHWvars->tftDC_GPIO;
	ILI9341_vars.tftDC_PIN = myHWvars->tftDC_PIN;
	ILI9341_vars.tftRESET_GPIO = myHWvars->tftRESET_GPIO;
	ILI9341_vars.tftRESET_PIN = myHWvars->tftRESET_PIN;
}

static inline void ILI9341_BSP_deselect_CS(void)
{
	HAL_GPIO_WritePin(ILI9341_vars.tftCS_GPIO, ILI9341_vars.tftCS_PIN, GPIO_PIN_SET);
}
static inline void ILI9341_BSP_select_CS(void)
{
	HAL_GPIO_WritePin(ILI9341_vars.tftCS_GPIO, ILI9341_vars.tftCS_PIN, GPIO_PIN_RESET);
}
static inline void ILI9341_BSP_DCset_commandMode(void)
{
	//Set DC HIGH for COMMAND mode
	HAL_GPIO_WritePin(ILI9341_vars.tftDC_GPIO, ILI9341_vars.tftDC_PIN, GPIO_PIN_RESET);
}
static inline void ILI9341_BSP_DCset_dataMode(void)
{
	//Set DC LOW for DATA mode
	HAL_GPIO_WritePin(ILI9341_vars.tftDC_GPIO, ILI9341_vars.tftDC_PIN, GPIO_PIN_SET);
}
static inline void ILI9341_BSP_SPI_transmit(const uint8_t* pdata, uint16_t Size)
{
	HAL_SPI_Transmit(ILI9341_vars.lcdSPIhandle, pdata, Size, HAL_MAX_DELAY);
}
static inline void ILI9341_BSP_ms_delay(uint32_t mseg)
{
	// se usan esperas hardcodeadas para la inicializacion:
	HAL_Delay(mseg);
}
static inline void ILI9341_BSP_reset(void)
{
	//Turn LCD ON
	HAL_GPIO_WritePin(ILI9341_vars.tftRESET_GPIO, ILI9341_vars.tftRESET_PIN, GPIO_PIN_SET);
}
static inline void ILI9341_BSP_SPI_transmit_DMA_start(uint8_t* bufferRx, uint16_t size)
{
	HAL_SPI_Transmit_DMA(ILI9341_vars.lcdSPIhandle, bufferRx, size);
}
static inline void ILI9341_BSP_SPI_transmit_DMA_finish(void)
{
	// solo existe para dar contexto a la acción pero es un deselectCS:
	HAL_GPIO_WritePin(ILI9341_vars.tftCS_GPIO, ILI9341_vars.tftCS_PIN, GPIO_PIN_SET);
}
static inline void ILI9341_BSP_SPI_transmit_DMA_waitReady(void)
{
	while(HAL_SPI_GetState(ILI9341_vars.lcdSPIhandle) != HAL_SPI_STATE_READY){}
}


#endif /* INC_ILI9341_BSP_F4XX_H_ */

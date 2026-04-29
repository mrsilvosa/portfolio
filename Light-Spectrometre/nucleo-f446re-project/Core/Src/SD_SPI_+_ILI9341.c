/*
 * SD_SPI_+_ILI9341.c
 *
 *  Created on: Nov 20, 2025
 *      Author: mrsilvosa
 */

#include "SD_SPI_+_ILI9341.h"

void rescue_and_print_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* dummyBufferTx, uint8_t* bufferRx, image_select_t selection)
{
	uint8_t success=0;
	uint32_t blockIndex=0;
	bool isFirstBlock = true;

	ILI9341_SetCursorPosition(x, y, w+x-1, h+y-1);

	for(blockIndex=0; blockIndex < 300; blockIndex++)
	{
		success = sd_read_block(selection+blockIndex, dummyBufferTx, bufferRx);

		if(!success)
			return;

		ILI9341_printImageSector_512bytes(x, y, w, h, bufferRx, isFirstBlock);

		isFirstBlock = false;

	}
}

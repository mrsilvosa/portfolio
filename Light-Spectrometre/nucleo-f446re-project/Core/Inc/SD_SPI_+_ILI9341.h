/*
 * SD_SPI_+_ILI9341.h
 *
 *  Created on: Nov 20, 2025
 *      Author: mrsilvosa
 */

#ifndef INC_SD_SPI___ILI9341_H_
#define INC_SD_SPI___ILI9341_H_

#include "SD_SPI.h"
#include "MY_ILI9341.h"

#define FIRST_WRITABLE_BLOCK 30281728U

typedef enum{
	LOGO=FIRST_WRITABLE_BLOCK,
	MENU=(FIRST_WRITABLE_BLOCK+300U),
	ESPECTRO=(FIRST_WRITABLE_BLOCK+300U+300U),
	ESPECTRO_COLOR=(FIRST_WRITABLE_BLOCK+300U+300U+300U)
}image_select_t;

// 30.281.728 SD 16GB first block of raw data.


void rescue_and_print_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* bufferTx, uint8_t* bufferRx, image_select_t selection);

#endif /* INC_SD_SPI___ILI9341_H_ */

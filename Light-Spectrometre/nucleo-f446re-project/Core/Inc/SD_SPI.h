/*
 * SD_SPI.h
 *
 *  Created on: Oct 12, 2025
 *      Author: mrsilvosa
 */

#ifndef INC_SD_SPI_H_
#define INC_SD_SPI_H_

#include <stdint.h>
#include <stdbool.h>

// SD command definitions
#define CMD0     (0x40 | 0)   // GO_IDLE_STATE
#define CMD13	 (0x40 | 13)  // SEND_STATUS
#define CMD8     (0x40 | 8)   // SEND_IF_COND
#define CMD16    (0x40 | 16)  // SET_BLOCKLEN
#define CMD17    (0x40 | 17)  // READ_SINGLE_BLOCK
#define CMD24    (0x40 | 24)  // WRITE_BLOCK
#define CMD55    (0x40 | 55)  // APP_CMD
#define CMD58    (0x40 | 58)  // READ_OCR
#define ACMD41   (0x40 | 41)  // SD_SEND_OP_COND

// Response definitions
#define R1_IDLE_STATE (1 << 0)

typedef enum {W=0,R,WR}SDCommandMode_t;

// Funciones principales:

bool sd_init(void);
bool sd_read_block(uint32_t block, uint8_t* dummyBufferTx, uint8_t* buffer);
bool sd_read_block_DMA(uint32_t block, uint8_t* dummyBufferTx, uint8_t* buffer);
bool sd_write_block(uint32_t block, const uint8_t* buffer, uint8_t* dummyBufferRx);
bool sd_wait_ready(void);
uint8_t sd_send_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t* resp, uint32_t resp_amount, SDCommandMode_t mode, bool is_CS_left_LOW);
void sd_send_dummy(uint8_t n);

#endif /* INC_SD_SPI_H_ */

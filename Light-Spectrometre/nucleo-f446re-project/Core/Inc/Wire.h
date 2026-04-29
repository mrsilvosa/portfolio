#ifndef WIRE_H
#define WIRE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Wire.h - simple C port of TwoWire (Wire) from ArduinoCore-avr
 *
 * This header exposes a C-style API that mirrors the behaviour of the original
 * TwoWire class. Use the functions prefixed with Wire_.
 *
 * Note: This is a best-effort, minimal C translation that depends on the
 * existing utility/twi.h implementation for low-level TWI/I2C operations.
 */

#ifndef BUFFER_LENGTH
#define BUFFER_LENGTH 32
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Time helpers (expected to be provided by the environment) */
extern uint32_t millis(void);
extern void delay(uint32_t ms);

/* Initialization / configuration */
void Wire_begin(void);
void Wire_beginAddress(uint8_t address); /* begin with slave address */
void Wire_end(void);
void Wire_setClock(uint32_t clock_hz);

/* Timeout handling */
void Wire_setWireTimeout(uint32_t timeout_us, uint8_t reset_with_timeout);
uint8_t Wire_getWireTimeoutFlag(void);
void Wire_clearWireTimeoutFlag(void);

/* Master read (requestFrom) */
uint8_t Wire_requestFromEx(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop);
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity); /* default sendStop = 1 */

/* Master write (begin/endTransmission, write) */
void Wire_beginTransmission(uint8_t address);
uint8_t Wire_endTransmission(uint8_t sendStop);
uint8_t Wire_endTransmissionDefault(void); /* endTransmission(true) */

/* write single byte or buffer (returns number of bytes written) */
size_t Wire_writeByte(uint8_t data);
size_t Wire_write(const uint8_t *data, size_t quantity);

/* Slave/master rx buffer access */
int Wire_available(void);
int Wire_read(void);
int Wire_peek(void);
void Wire_flush(void);

/* Register user callbacks (slave mode) */
void Wire_onReceive(void (*function)(int));
void Wire_onRequest(void (*function)(void));

/* Expose buffer length constant */
#define WIRE_BUFFER_LENGTH BUFFER_LENGTH

#ifdef __cplusplus
}
#endif

#endif /* WIRE_H */

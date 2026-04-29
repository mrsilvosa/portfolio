/*
 * Wire.c - C port of TwoWire (Wire) from ArduinoCore-avr
 *
 * Relies on utility/twi.h for low-level TWI operations.
 */

#include "Wire.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "twi_F4xx.h" /* expected to provide twi_* functions */

/* Internal buffers and state (analogous to TwoWire static members) */
static uint8_t rxBuffer[BUFFER_LENGTH];
static uint8_t rxBufferIndex = 0;
static uint8_t rxBufferLength = 0;

static uint8_t txAddress = 0;
static uint8_t txBuffer[BUFFER_LENGTH];
static uint8_t txBufferIndex = 0;
static uint8_t txBufferLength = 0;

static uint8_t transmitting = 0;

/* User-registered callbacks (slave mode) */
static void (*user_onRequest)(void) = NULL;
static void (*user_onReceive)(int) = NULL;

/* Forward declarations for internal service callbacks */
static void Wire_onReceiveService(uint8_t *inBytes, int numBytes);
static void Wire_onRequestService(void);

/* Public API implementation */

void Wire_begin(void)
{
    rxBufferIndex = 0;
    rxBufferLength = 0;

    txBufferIndex = 0;
    txBufferLength = 0;

    twi_init();
    /* attach the C callbacks provided by the low-level twi implementation */
    twi_attachSlaveTxEvent(Wire_onRequestService);
    twi_attachSlaveRxEvent(Wire_onReceiveService);
}

void Wire_beginAddress(uint8_t address)
{
    Wire_begin();
    twi_setAddress(address);
}

void Wire_end(void)
{
    twi_disable();
}

void Wire_setClock(uint32_t clock_hz)
{
    //twi_setFrequency(clock_hz);

	// no hace falta
}

/*
void Wire_setWireTimeout(uint32_t timeout_us, uint8_t reset_with_timeout)
{
    // twi_setTimeoutInMicros expects (uint32_t, bool) in original; pass byte as bool
    twi_setTimeoutInMicros(timeout_us, reset_with_timeout ? 1 : 0);
}
*/

/*
uint8_t Wire_getWireTimeoutFlag(void)
{
    // twi_manageTimeoutFlag(false) returns whether timeout occurred; false means don't clear
    return (uint8_t)twi_manageTimeoutFlag(0);
}
*/

/*
void Wire_clearWireTimeoutFlag(void)
{
    // twi_manageTimeoutFlag(true) clears the flag
    twi_manageTimeoutFlag(1);
}
*/

/* requestFrom with full signature */
uint8_t Wire_requestFromEx(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop)
{
    if (isize > 0)
    {
        /* send internal address; write internal register address MSB first */
        Wire_beginTransmission(address);

        if (isize > 3) {
            isize = 3;
        }

        while (isize-- > 0)
        {
            uint8_t v = (uint8_t)(iaddress >> (isize * 8));
            Wire_writeByte(v);
        }
        /* repeated start if sendStop==0 */
        Wire_endTransmission(0);
    }

    if (quantity > BUFFER_LENGTH)
    {
        quantity = BUFFER_LENGTH;
    }

    /* perform blocking read into rxBuffer */
    uint8_t read = twi_readFrom(address, rxBuffer, quantity, sendStop);
    rxBufferIndex = 0;
    rxBufferLength = read;

    return read;
}

/* simpler requestFrom */
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity)
{
    return Wire_requestFromEx(address, quantity, 0, 0, 1);
}

/* beginTransmission */
void Wire_beginTransmission(uint8_t address)
{
    transmitting = 1;
    txAddress = address;
    txBufferIndex = 0;
    txBufferLength = 0;
}

/* endTransmission with sendStop parameter */
uint8_t Wire_endTransmission(uint8_t sendStop)
{
    uint8_t ret = twi_writeTo(txAddress, txBuffer, txBufferLength, 1, sendStop);
    txBufferIndex = 0;
    txBufferLength = 0;
    transmitting = 0;
    return ret;
}

/* default endTransmission(true) */
uint8_t Wire_endTransmissionDefault(void)
{
    return Wire_endTransmission(1);
}

/* write one byte */
size_t Wire_writeByte(uint8_t data)
{
    if (transmitting)
    {
        if (txBufferLength >= BUFFER_LENGTH)
        {
            /* emulate setWriteError by returning 0; caller may track errors separately */
            return 0;
        }
        txBuffer[txBufferIndex++] = data;
        txBufferLength = txBufferIndex;
    } else
    {
        /* slave send mode: reply to master immediately */
        twi_transmit(&data, 1);
    }
    return 1;
}

/* write a buffer of bytes */
size_t Wire_write(const uint8_t *data, size_t quantity)
{
    if (transmitting)
    {
        size_t i;
        for (i = 0; i < quantity; ++i)
        {
            /* stop adding if buffer full */
            if (txBufferLength >= BUFFER_LENGTH)
            {
                break;
            }
            Wire_writeByte(data[i]);
        }
        return i;
    } else
    {
        twi_transmit(data, quantity);
        return quantity;
    }
}

/* available/read/peek/flush */
int Wire_available(void)
{
    return (int)(rxBufferLength - rxBufferIndex);
}

int Wire_read(void)
{
    int value = -1;
    if (rxBufferIndex < rxBufferLength)
    {
        value = rxBuffer[rxBufferIndex++];
    }
    return value;
}

int Wire_peek(void)
{
    int value = -1;
    if (rxBufferIndex < rxBufferLength)
    {
        value = rxBuffer[rxBufferIndex];
    }
    return value;
}

void Wire_flush(void)
{
    /* No-op in reference implementation. Keep for API compatibility. */
    (void)0;
}

/* Register user callbacks (slave) */
void Wire_onReceive(void (*function)(int))
{
    user_onReceive = function;
}

void Wire_onRequest(void (*function)(void))
{
    user_onRequest = function;
}

/* Internal service callbacks called by twi layer when events occur */
static void Wire_onReceiveService(uint8_t *inBytes, int numBytes)
{
    /* If user hasn't registered a callback, ignore */
    if (!user_onReceive)
    {
        return;
    }

    /* If rx buffer still in use by a master requestFrom(), drop the incoming data.
       This mirrors the original TwoWire behaviour. */
    if (rxBufferIndex < rxBufferLength)
    {
        return;
    }

    /* copy incoming bytes into rxBuffer */
    if (numBytes > BUFFER_LENGTH)
    {
        numBytes = BUFFER_LENGTH;
    }
    memcpy(rxBuffer, inBytes, (size_t)numBytes);
    rxBufferIndex = 0;
    rxBufferLength = (uint8_t)numBytes;

    /* call user callback with number of bytes */
    user_onReceive(numBytes);
}

static void Wire_onRequestService(void)
{
    if (!user_onRequest)
    {
        return;
    }

    /* reset tx buffer - this will drop any previous pre-master data */
    txBufferIndex = 0;
    txBufferLength = 0;

    user_onRequest();
}

/* End of Wire.c */

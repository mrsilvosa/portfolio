/*
 * twi_F4xx.h
 *
 *  Created on: Nov 21, 2025
 *      Author: mrsilvosa
 */

#ifndef INC_TWI_F4XX_H_
#define INC_TWI_F4XX_H_

//List of includes
#include <stdbool.h>
//** CHANGE BASED ON STM32 CHIP F4/F7/F1...**//
#include "stm32f4xx_hal.h"
#include <inttypes.h>
//#include "main.h"

//#define AS726X_I2C_HANDLE AS726X_HI2CX

#ifndef TWI_FREQ
#define TWI_FREQ 100000L
#endif

#ifndef TWI_BUFFER_LENGTH
#define TWI_BUFFER_LENGTH 32
#endif

#define TWI_READY 0
#define TWI_MRX   1
#define TWI_MTX   2
#define TWI_SRX   3
#define TWI_STX   4

// obligatorio implementar en el main
extern I2C_HandleTypeDef* AS726X_I2C_HANDLE;

/*
void twi_init(void);
void twi_disable(void);
void twi_setAddress(uint8_t);
void twi_setFrequency(uint32_t);
uint8_t twi_readFrom(uint8_t, uint8_t*, uint8_t, uint8_t);
uint8_t twi_writeTo(uint8_t, uint8_t*, uint8_t, uint8_t, uint8_t);
uint8_t twi_transmit(const uint8_t*, uint8_t);
void twi_attachSlaveRxEvent( void (*)(uint8_t*, int) );
void twi_attachSlaveTxEvent( void (*)(void) );
void twi_reply(uint8_t);
void twi_stop(void);
void twi_releaseBus(void);
void twi_setTimeoutInMicros(uint32_t, bool);
void twi_handleTimeout(bool);
bool twi_manageTimeoutFlag(bool);
*/

/* Time helpers (expected to be provided by the environment) */
inline uint32_t millis(void)
{
	return HAL_GetTick();
}
inline void delay(uint32_t ms)
{
	HAL_Delay(ms);
}

/*-------------- twi.c -----------------*/

static volatile uint8_t twi_state;
static volatile uint8_t twi_slarw;
static volatile uint8_t twi_sendStop;			// should the transaction end with a stop
static volatile uint8_t twi_inRepStart;			// in the middle of a repeated start

// twi_timeout_us > 0 prevents the code from getting stuck in various while loops here
// if twi_timeout_us == 0 then timeout checking is disabled (the previous Wire lib behavior)
// at some point in the future, the default twi_timeout_us value could become 25000
// and twi_do_reset_on_timeout could become true
// to conform to the SMBus standard
// http://smbus.org/specs/SMBus_3_1_20180319.pdf
static volatile uint32_t twi_timeout_us = 0ul;
static volatile bool twi_timed_out_flag = false;  // a timeout has been seen
static volatile bool twi_do_reset_on_timeout = false;  // reset the TWI registers on timeout

static void (*twi_onSlaveTransmit)(void) = NULL;
static void (*twi_onSlaveReceive)(uint8_t*, int) = NULL;

static uint8_t twi_masterBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t twi_masterBufferIndex;
static volatile uint8_t twi_masterBufferLength;

static uint8_t twi_txBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t twi_txBufferIndex;
static volatile uint8_t twi_txBufferLength;

static uint8_t twi_rxBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t twi_rxBufferIndex;

static volatile uint8_t twi_error;


// funciones:

/*
 * Function twi_init
 * Desc     readys twi pins and sets twi bitrate
 * Input    none
 * Output   none
 */
static inline void twi_init(void)
{
  // initialize state
  twi_state = TWI_READY;
  twi_sendStop = true;		// default value
  twi_inRepStart = false;
  //AS726X_I2C_HANDLE = hi2cx;

  /*
  // activate internal pullups for twi.
  digitalWrite(SDA, 1);
  digitalWrite(SCL, 1);

  // initialize twi prescaler and bit rate
  cbi(TWSR, TWPS0);
  cbi(TWSR, TWPS1);
  TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;

  // twi bit rate formula from atmega128 manual pg 204
  // SCL Frequency = CPU Clock Frequency / (16 + (2 * TWBR))
  // note: TWBR should be 10 or higher for master mode
  // It is 72 for a 16mhz Wiring board with 100kHz TWI

  // enable twi module, acks, and twi interrupt
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
  */

  // el resto relevante se hace en MX_I2CX_Init()
}

/*
 * Function twi_disable
 * Desc     disables twi pins
 * Input    none
 * Output   none
 */
static inline void twi_disable(void)
{
	/*
  // disable twi module, acks, and twi interrupt
  TWCR &= ~(_BV(TWEN) | _BV(TWIE) | _BV(TWEA));

  // deactivate internal pullups for twi.
  digitalWrite(SDA, 0);
  digitalWrite(SCL, 0);
	*/

	// no hace falta implementarlo
}

/*
 * Function twi_slaveInit
 * Desc     sets slave address and enables interrupt
 * Input    none
 * Output   none
 */
static inline void twi_setAddress(uint8_t address)
{
  // set twi slave address (skip over TWGCE bit)
  //TWAR = address << 1;

	// creo que no hace falta esto
}

static inline uint8_t twi_transmit(const uint8_t* data, uint8_t length)
{
	  uint8_t i;

	  // ensure data will fit into buffer
	  if(TWI_BUFFER_LENGTH < (twi_txBufferLength+length))
	  {
	    return 1;
	  }

	  // ensure we are currently a slave transmitter
	  if(TWI_STX != twi_state)
	  {
	    return 2;
	  }

	  // set length and copy data into tx buffer
	  for(i = 0; i < length; ++i)
	  {
	    twi_txBuffer[twi_txBufferLength+i] = data[i];
	  }
	  twi_txBufferLength += length;

	  return 0;
}

/*
 * Function twi_writeTo
 * Desc     attempts to become twi bus master and write a
 *          series of bytes to a device on the bus
 * Input    address: 7bit i2c device address
 *          data: pointer to byte array
 *          length: number of bytes in array
 *          wait: boolean indicating to wait for write or not
 *          sendStop: boolean indicating whether or not to send a stop at the end
 * Output   0 .. success
 *          1 .. length to long for buffer
 *          2 .. address send, NACK received
 *          3 .. data send, NACK received
 *          4 .. other twi error (lost bus arbitration, bus error, ..)
 *          5 .. timeout
 */
static inline uint8_t twi_writeTo(uint8_t address, uint8_t* data, uint8_t length, uint8_t wait, uint8_t sendStop)
{
  uint8_t i;

  // ensure data will fit into buffer
  if(TWI_BUFFER_LENGTH < length)
  {
    return 1;
  }

  /*
  // wait until twi is ready, become master transmitter
  uint32_t startMicros = micros();
  while(TWI_READY != twi_state)
  {
    if((twi_timeout_us > 0ul) && ((micros() - startMicros) > twi_timeout_us))
    {
      twi_handleTimeout(twi_do_reset_on_timeout);
      return (5);
    }
  }
  */

  twi_state = TWI_MTX;
  twi_sendStop = sendStop;
  // reset error state (0xFF.. no error occurred)
  twi_error = 0xFF;

  // initialize buffer iteration vars
  twi_masterBufferIndex = 0;
  twi_masterBufferLength = length;

  // copy data to twi buffer
  for(i = 0; i < length; ++i)
  {
    twi_masterBuffer[i] = data[i];
    twi_masterBufferIndex++;  	// meant to be increased on each
    							// successful byte transmitted, but not today.
  }
  HAL_StatusTypeDef status;
  // wait: could be used instead of HAL_MAX_DELAY, no need now
  (void)wait;	// discards
  status = HAL_I2C_Master_Transmit(AS726X_I2C_HANDLE, (address << 1), twi_masterBuffer, length, HAL_MAX_DELAY);
  if(status != HAL_OK)
  {
	  return 1;	// failed
  }
  else
  {
	  return 0;	// success
  }
  /*
  // build sla+w, slave device address + w bit
  twi_slarw = TW_WRITE;
  twi_slarw |= address << 1;

  // if we're in a repeated start, then we've already sent the START
  // in the ISR. Don't do it again.
  //
  if (true == twi_inRepStart)
  {
    // if we're in the repeated start state, then we've already sent the start,
    // (@@@ we hope), and the TWI statemachine is just waiting for the address byte.
    // We need to remove ourselves from the repeated start state before we enable interrupts,
    // since the ISR is ASYNC, and we could get confused if we hit the ISR before cleaning
    // up. Also, don't enable the START interrupt. There may be one pending from the
    // repeated start that we sent ourselves, and that would really confuse things.
    twi_inRepStart = false;			// remember, we're dealing with an ASYNC ISR
    startMicros = micros();
    do
    {
      TWDR = twi_slarw;
      if((twi_timeout_us > 0ul) && ((micros() - startMicros) > twi_timeout_us))
      {
        twi_handleTimeout(twi_do_reset_on_timeout);
        return (5);
      }
    } while(TWCR & _BV(TWWC));
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	// enable INTs, but not START
  } else
  {
    // send start condition
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);	// enable INTs
  }

  // wait for write operation to complete
  startMicros = micros();
  while(wait && (TWI_MTX == twi_state))
  {
    if((twi_timeout_us > 0ul) && ((micros() - startMicros) > twi_timeout_us))
    {
      twi_handleTimeout(twi_do_reset_on_timeout);
      return (5);
    }
  }

  if (twi_error == 0xFF)
    return 0;	// success
  else if (twi_error == TW_MT_SLA_NACK)
    return 2;	// error: address send, nack received
  else if (twi_error == TW_MT_DATA_NACK)
    return 3;	// error: data send, nack received
  else
    return 4;	// other twi error
    */
}

/*
 * Function twi_readFrom
 * Desc     attempts to become twi bus master and read a
 *          series of bytes from a device on the bus
 * Input    address: 7bit i2c device address
 *          data: pointer to byte array
 *          length: number of bytes to read into array
 *          sendStop: Boolean indicating whether to send a stop at the end
 * Output   number of bytes read
 */
static inline uint8_t twi_readFrom(uint8_t address, uint8_t* data, uint8_t length, uint8_t sendStop)
{
  uint8_t i;

  // ensure data will fit into buffer
  if(TWI_BUFFER_LENGTH < length)
  {
    return 0;	// return value is amount of bytes read, so 0 in this case.
  }

  /*
  // wait until twi is ready, become master receiver
  uint32_t startMicros = micros();
  while(TWI_READY != twi_state){
    if((twi_timeout_us > 0ul) && ((micros() - startMicros) > twi_timeout_us)) {
      twi_handleTimeout(twi_do_reset_on_timeout);
      return 0;
    }
  }
  */
  twi_state = TWI_MRX;
  twi_sendStop = sendStop;
  // reset error state (0xFF.. no error occurred)
  twi_error = 0xFF;


  // mi buffer no necesita lo de abajo:

  //twi_masterBufferLength = length-1;  // This is not intuitive, read on...
  // On receive, the previously configured ACK/NACK setting is transmitted in
  // response to the received byte before the interrupt is signalled.
  // Therefore we must actually set NACK when the _next_ to last byte is
  // received, causing that NACK to be sent in response to receiving the last
  // expected byte of data.

  /*
  // build sla+w, slave device address + w bit
  twi_slarw = TW_READ;
  twi_slarw |= address << 1;

  if (true == twi_inRepStart) {
    // if we're in the repeated start state, then we've already sent the start,
    // (@@@ we hope), and the TWI statemachine is just waiting for the address byte.
    // We need to remove ourselves from the repeated start state before we enable interrupts,
    // since the ISR is ASYNC, and we could get confused if we hit the ISR before cleaning
    // up. Also, don't enable the START interrupt. There may be one pending from the
    // repeated start that we sent ourselves, and that would really confuse things.
    twi_inRepStart = false;			// remember, we're dealing with an ASYNC ISR
    startMicros = micros();
    do {
      TWDR = twi_slarw;
      if((twi_timeout_us > 0ul) && ((micros() - startMicros) > twi_timeout_us)) {
        twi_handleTimeout(twi_do_reset_on_timeout);
        return 0;
      }
    } while(TWCR & _BV(TWWC));
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	// enable INTs, but not START
  } else {
    // send start condition
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTA);
  }

  // wait for read operation to complete
  startMicros = micros();
  while(TWI_MRX == twi_state)
  {
    if((twi_timeout_us > 0ul) && ((micros() - startMicros) > twi_timeout_us))
    {
      twi_handleTimeout(twi_do_reset_on_timeout);
      return 0;
    }
  }
  */
  HAL_StatusTypeDef status;

  status = HAL_I2C_Master_Receive(AS726X_I2C_HANDLE, (address << 1), twi_masterBuffer, length, HAL_MAX_DELAY);

  if(status != HAL_OK)
  {
	  return 0;	// failed
  }
  // initialize buffer iteration vars
  twi_masterBufferIndex = 0;
  twi_masterBufferLength = length;

  // copy twi buffer to data
  for(i = 0; i < length; ++i)
  {
    data[i] = twi_masterBuffer[i];
    twi_masterBufferIndex++;
  }

  return length;
}

/*
 * Function twi_attachSlaveRxEvent
 * Desc     sets function called before a slave read operation
 * Input    function: callback function to use
 * Output   none
 */
static inline void twi_attachSlaveRxEvent( void (*function)(uint8_t*, int) )
{
  twi_onSlaveReceive = function;
}

/*
 * Function twi_attachSlaveTxEvent
 * Desc     sets function called before a slave write operation
 * Input    function: callback function to use
 * Output   none
 */
static inline void twi_attachSlaveTxEvent( void (*function)(void) )
{
  twi_onSlaveTransmit = function;
}



#endif /* INC_TWI_F4XX_H_ */

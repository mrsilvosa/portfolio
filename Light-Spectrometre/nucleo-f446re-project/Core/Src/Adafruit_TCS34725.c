/*!
 *  @file Adafruit_TCS34725.cpp
 *
 *  @mainpage Driver for the TCS34725 digital color sensors.
 *
 *  @section intro_sec Introduction
 *
 *  Adafruit invests time and resources providing this open source code,
 *  please support Adafruit and open-source hardware by purchasing
 *  products from Adafruit!
 *
 *  @section author Author
 *
 *  KTOWN (Adafruit Industries)
 *
 *  @section license License
 *
 *  BSD (see license.txt)
 *
 *  @section HISTORY
 *
 *  v1.0 - First release
 */

#include <math.h>
#include <stdlib.h>

#include "Adafruit_TCS34725.h"
#include "Wire.h"

// private variables:

//static Adafruit_I2CDevice *i2c_dev = NULL; ///< Pointer to I2C bus interface
static bool _tcs34725Initialised;
static tcs34725Gain_t _tcs34725Gain;
static uint8_t _tcs34725IntegrationTime;

/* Time helpers (expected to be provided by the environment) */
extern uint32_t millis(void);
extern void delay(uint32_t ms);

/*!
 *  @brief  Implements missing powf function
 *  @param  x
 *          Base number
 *  @param  y
 *          Exponent
 *  @return x raised to the power of y
 */
float powf(const float x, const float y)
{
  return (float)(pow((double)x, (double)y));
}

/*!
 *  @brief  Writes a register and an 8 bit value over I2C
 *  @param  reg
 *  @param  value
 */
uint32_t Adafruit_TCS34725_write8(uint8_t reg, uint8_t value)
{
  uint8_t buffer[2] = {(uint8_t)(TCS34725_COMMAND_BIT | reg), value};
  //i2c_dev->write(buffer, 2);

  uint8_t err = 0xFF;

  Wire_beginTransmission(TCS34725_ADDRESS);
  if (Wire_write(buffer, 2) == 0) return (int)err;
  if (Wire_endTransmission(1) != 0) return (int)err;

  return 0;
}

/*!
 *  @brief  Reads an 8 bit value over I2C
 *  @param  reg
 *  @return value
 */
uint8_t Adafruit_TCS34725_read8(uint8_t reg)
{
  uint8_t buffer[1] = {(uint8_t)(TCS34725_COMMAND_BIT | reg)};
  //i2c_dev->write_then_read(buffer, 1, buffer, 1);

  uint8_t err = 0xFF;

  Wire_beginTransmission(TCS34725_ADDRESS);
  if (Wire_write(buffer, 1) == 0) return (int)err;
  if (Wire_endTransmission(1) != 0) return (int)err;

  if (Wire_requestFrom(TCS34725_ADDRESS, 1) == 0) return err;
  if (Wire_available())
  {
      return Wire_read();
  } else
  {
      /* No print to Serial in this C port; caller gets 0xFF */
      return err;
  }
}

/*!
 *  @brief  Reads a 16 bit values over I2C
 *  @param  reg
 *  @return value
 */
uint16_t Adafruit_TCS34725_read16(uint8_t reg)
{
  uint8_t buffer[2] = {(uint8_t)(TCS34725_COMMAND_BIT | reg), 0};
  //i2c_dev->write_then_read(buffer, 1, buffer, 2);

  uint8_t err = 0xFF;

  Wire_beginTransmission(TCS34725_ADDRESS);
  if (Wire_write(buffer, 1) == 0) return (int)err;
  if (Wire_endTransmission(1) != 0) return (int)err;

  if (Wire_requestFrom(TCS34725_ADDRESS, 2) == 0) return err;
  if (Wire_available() == 2)
  {
	  buffer[1] = Wire_read();
	  buffer[0] = Wire_read();
      return *((uint16_t*)buffer);
  } else
  {
      /* No print to Serial in this C port; caller gets 0xFF */
      return err;
  }
}

/*!
 *  @brief  Enables the device
 */
void Adafruit_TCS34725_enable()
{
	Adafruit_TCS34725_write8(TCS34725_ENABLE, TCS34725_ENABLE_PON);
	delay(3);
	Adafruit_TCS34725_write8(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
  /* Set a delay for the integration time.
    This is only necessary in the case where enabling and then
    immediately trying to read values back. This is because setting
    AEN triggers an automatic integration, so if a read RGBC is
    performed too quickly, the data is not yet valid and all 0's are
    returned */
  /* 12/5 = 2.4, add 1 to account for integer truncation */
	delay((256 - _tcs34725IntegrationTime) * 12 / 5 + 1);
}

/*!
 *  @brief  Disables the device (putting it in lower power sleep mode)
 */
void Adafruit_TCS34725_disable()
{
  /* Turn the device off to save power */
  uint8_t reg = 0;
  reg = Adafruit_TCS34725_read8(TCS34725_ENABLE);
  Adafruit_TCS34725_write8(TCS34725_ENABLE, reg & ~(TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN));
}

/*!
 *  @brief  Constructor
 *  @param  it
 *          Integration Time
 *  @param  gain
 *          Gain
 */
void Adafruit_TCS34725(uint8_t integrationTime, tcs34725Gain_t gain)
{
  _tcs34725Initialised = false;
  _tcs34725IntegrationTime = integrationTime;
  _tcs34725Gain = gain;
}

/*!
 *  @brief  Initializes I2C and configures the sensor
 *  @param  addr
 *          i2c address
 *  @param  *theWire
 *          The Wire object
 *  @return True if initialization was successful, otherwise false.
 */
bool Adafruit_TCS34725_begin()
{
  return Adafruit_TCS34725_init();
}

/*!
 *  @brief  Part of begin
 *  @return True if initialization was successful, otherwise false.
 */
bool Adafruit_TCS34725_init()
{
  /* Make sure we're actually connected */
  uint8_t x = Adafruit_TCS34725_read8(TCS34725_ID);
  if ((x != 0x4d) && (x != 0x44) && (x != 0x10))
  {
    return false;
  }
  _tcs34725Initialised = true;

  /* Set default integration time and gain */
  Adafruit_TCS34725_setIntegrationTime(_tcs34725IntegrationTime);
  Adafruit_TCS34725_setGain(_tcs34725Gain);

  /* Note: by default, the device is in power down mode on bootup */
  Adafruit_TCS34725_enable();

  return true;
}

/*!
 *  @brief  Sets the integration time for the TC34725
 *  @param  it
 *          Integration Time
 */
void Adafruit_TCS34725_setIntegrationTime(uint8_t it)
{
  if (!_tcs34725Initialised)
	  Adafruit_TCS34725_begin();

  /* Update the timing register */
  Adafruit_TCS34725_write8(TCS34725_ATIME, it);

  /* Update value placeholders */
  _tcs34725IntegrationTime = it;
}

/*!
 *  @brief  Adjusts the gain on the TCS34725
 *  @param  gain
 *          Gain (sensitivity to light)
 */
void Adafruit_TCS34725_setGain(tcs34725Gain_t gain)
{
  if (!_tcs34725Initialised)
	  Adafruit_TCS34725_begin();

  /* Update the timing register */
  Adafruit_TCS34725_write8(TCS34725_CONTROL, gain);

  /* Update value placeholders */
  _tcs34725Gain = gain;
}

/*!
 *  @brief  Reads the raw red, green, blue and clear channel values
 *  @param  *r
 *          Red value
 *  @param  *g
 *          Green value
 *  @param  *b
 *          Blue value
 *  @param  *c
 *          Clear channel value
 */
void Adafruit_TCS34725_getRawData(uint16_t *r, uint16_t *g, uint16_t *b,
                                   uint16_t *c)
{
  if (!_tcs34725Initialised)
	  Adafruit_TCS34725_begin();

  *c = Adafruit_TCS34725_read16(TCS34725_CDATAL);
  *r = Adafruit_TCS34725_read16(TCS34725_RDATAL);
  *g = Adafruit_TCS34725_read16(TCS34725_GDATAL);
  *b = Adafruit_TCS34725_read16(TCS34725_BDATAL);

  /* Set a delay for the integration time */
  /* 12/5 = 2.4, add 1 to account for integer truncation */
  delay((256 - _tcs34725IntegrationTime) * 12 / 5 + 1);
}

/*!
 *  @brief  Reads the raw red, green, blue and clear channel values in
 *          one-shot mode (e.g., wakes from sleep, takes measurement, enters
 *          sleep)
 *  @param  *r
 *          Red value
 *  @param  *g
 *          Green value
 *  @param  *b
 *          Blue value
 *  @param  *c
 *          Clear channel value
 */
void Adafruit_TCS34725_getRawDataOneShot(uint16_t *r, uint16_t *g, uint16_t *b,
                                          uint16_t *c)
{
  if (!_tcs34725Initialised)
	  Adafruit_TCS34725_begin();

  Adafruit_TCS34725_enable();
  Adafruit_TCS34725_getRawData(r, g, b, c);
  Adafruit_TCS34725_disable();
}

/*!
 *  @brief  Read the RGB color detected by the sensor.
 *  @param  *r
 *          Red value normalized to 0-255
 *  @param  *g
 *          Green value normalized to 0-255
 *  @param  *b
 *          Blue value normalized to 0-255
 */
void Adafruit_TCS34725_getRGB(float *r, float *g, float *b)
{
  uint16_t red, green, blue, clear;
  Adafruit_TCS34725_getRawData(&red, &green, &blue, &clear);
  uint32_t sum = clear;

  // Avoid divide by zero errors ... if clear = 0 return black
  if (clear == 0)
  {
    *r = *g = *b = 0;
    return;
  }

  *r = (float)red / sum * 255.0;
  *g = (float)green / sum * 255.0;
  *b = (float)blue / sum * 255.0;
}

/*!
 *  @brief  Converts the raw R/G/B values to color temperature in degrees Kelvin
 *  @param  r
 *          Red value
 *  @param  g
 *          Green value
 *  @param  b
 *          Blue value
 *  @return Color temperature in degrees Kelvin
 */
uint16_t Adafruit_TCS34725_calculateColorTemperature(uint16_t r, uint16_t g,
                                                      uint16_t b)
{
  float X, Y, Z; /* RGB to XYZ correlation      */
  float xc, yc;  /* Chromaticity co-ordinates   */
  float n;       /* McCamy's formula            */
  float cct;

  if (r == 0 && g == 0 && b == 0)
  {
    return 0;
  }

  /* 1. Map RGB values to their XYZ counterparts.    */
  /* Based on 6500K fluorescent, 3000K fluorescent   */
  /* and 60W incandescent values for a wide range.   */
  /* Note: Y = Illuminance or lux                    */
  X = (-0.14282F * r) + (1.54924F * g) + (-0.95641F * b);
  Y = (-0.32466F * r) + (1.57837F * g) + (-0.73191F * b);
  Z = (-0.68202F * r) + (0.77073F * g) + (0.56332F * b);

  /* 2. Calculate the chromaticity co-ordinates      */
  xc = (X) / (X + Y + Z);
  yc = (Y) / (X + Y + Z);

  /* 3. Use McCamy's formula to determine the CCT    */
  n = (xc - 0.3320F) / (0.1858F - yc);

  /* Calculate the final CCT */
  cct =
      (449.0F * powf(n, 3)) + (3525.0F * powf(n, 2)) + (6823.3F * n) + 5520.33F;

  /* Return the results in degrees Kelvin */
  return (uint16_t)cct;
}

/*!
 *  @brief  Converts the raw R/G/B values to color temperature in degrees
 *          Kelvin using the algorithm described in DN40 from Taos (now AMS).
 *  @param  r
 *          Red value
 *  @param  g
 *          Green value
 *  @param  b
 *          Blue value
 *  @param  c
 *          Clear channel value
 *  @return Color temperature in degrees Kelvin
 */
uint16_t Adafruit_TCS34725_calculateColorTemperature_dn40(uint16_t r,
                                                           uint16_t g,
                                                           uint16_t b,
                                                           uint16_t c)
{
  uint16_t r2, b2; /* RGB values minus IR component */
  uint16_t sat;    /* Digital saturation level */
  uint16_t ir;     /* Inferred IR content */

  if (c == 0)
  {
    return 0;
  }

  /* Analog/Digital saturation:
   *
   * (a) As light becomes brighter, the clear channel will tend to
   *     saturate first since R+G+B is approximately equal to C.
   * (b) The TCS34725 accumulates 1024 counts per 2.4ms of integration
   *     time, up to a maximum values of 65535. This means analog
   *     saturation can occur up to an integration time of 153.6ms
   *     (64*2.4ms=153.6ms).
   * (c) If the integration time is > 153.6ms, digital saturation will
   *     occur before analog saturation. Digital saturation occurs when
   *     the count reaches 65535.
   */
  if ((256 - _tcs34725IntegrationTime) > 63)
  {
    /* Track digital saturation */
    sat = 65535;
  } else
  {
    /* Track analog saturation */
    sat = 1024 * (256 - _tcs34725IntegrationTime);
  }

  /* Ripple rejection:
   *
   * (a) An integration time of 50ms or multiples of 50ms are required to
   *     reject both 50Hz and 60Hz ripple.
   * (b) If an integration time faster than 50ms is required, you may need
   *     to average a number of samples over a 50ms period to reject ripple
   *     from fluorescent and incandescent light sources.
   *
   * Ripple saturation notes:
   *
   * (a) If there is ripple in the received signal, the value read from C
   *     will be less than the max, but still have some effects of being
   *     saturated. This means that you can be below the 'sat' value, but
   *     still be saturating. At integration times >150ms this can be
   *     ignored, but <= 150ms you should calculate the 75% saturation
   *     level to avoid this problem.
   */
  if ((256 - _tcs34725IntegrationTime) <= 63)
  {
    /* Adjust sat to 75% to avoid analog saturation if atime < 153.6ms */
    sat -= sat / 4;
  }

  /* Check for saturation and mark the sample as invalid if true */
  if (c >= sat)
  {
    return 0;
  }

  /* AMS RGB sensors have no IR channel, so the IR content must be */
  /* calculated indirectly. */
  ir = (r + g + b > c) ? (r + g + b - c) / 2 : 0;

  /* Remove the IR component from the raw RGB values */
  r2 = r - ir;
  b2 = b - ir;

  if (r2 == 0)
  {
    return 0;
  }

  /* A simple method of measuring color temp is to use the ratio of blue */
  /* to red light, taking IR cancellation into account. */
  uint16_t cct = (3810 * (uint32_t)b2) / /** Color temp coefficient. */
                     (uint32_t)r2 +
                 1391; /** Color temp offset. */

  return cct;
}

/*!
 *  @brief  Converts the raw R/G/B values to lux
 *  @param  r
 *          Red value
 *  @param  g
 *          Green value
 *  @param  b
 *          Blue value
 *  @return Lux value
 */
uint16_t Adafruit_TCS34725_calculateLux(uint16_t r, uint16_t g, uint16_t b)
{
  float illuminance;

  /* This only uses RGB ... how can we integrate clear or calculate lux */
  /* based exclusively on clear since this might be more reliable?      */
  illuminance = (-0.32466F * r) + (1.57837F * g) + (-0.73191F * b);

  return (uint16_t)illuminance;
}

/*!
 *  @brief  Sets interrupt for TCS34725
 *  @param  i
 *          Interrupt (True/False)
 */
void Adafruit_TCS34725_setInterrupt(bool i)
{
  uint8_t r = Adafruit_TCS34725_read8(TCS34725_ENABLE);
  if (i)
  {
    r |= TCS34725_ENABLE_AIEN;
  } else
  {
    r &= ~TCS34725_ENABLE_AIEN;
  }
  Adafruit_TCS34725_write8(TCS34725_ENABLE, r);
}

/*!
 *  @brief  Clears inerrupt for TCS34725
 */
uint32_t Adafruit_TCS34725_clearInterrupt()
{
  uint8_t buffer[1] = {TCS34725_COMMAND_BIT | 0x66};
  //i2c_dev->write(buffer, 1);

  uint8_t err = 0xFF;

  Wire_beginTransmission(TCS34725_ADDRESS);
  if (Wire_write(buffer, 1) == 0) return (int)err;
  if (Wire_endTransmission(1) != 0) return (int)err;

  return 0;
}

/*!
 *  @brief  Sets inerrupt limits
 *  @param  low
 *          Low limit
 *  @param  high
 *          High limit
 */
void Adafruit_TCS34725_setIntLimits(uint16_t low, uint16_t high)
{
	Adafruit_TCS34725_write8(0x04, low & 0xFF);
	Adafruit_TCS34725_write8(0x05, low >> 8);
	Adafruit_TCS34725_write8(0x06, high & 0xFF);
	Adafruit_TCS34725_write8(0x07, high >> 8);
}

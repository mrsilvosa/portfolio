/*
  AS726X.c - C port of SparkFun AS726X Arduino Library (partial)
  Relies on a C-style Wire interface (Wire_beginTransmission, Wire_write,
  Wire_endTransmission, Wire_requestFrom, Wire_available, Wire_read)
  and on millis()/delay() being available in the environment.
*/

#include <string.h> /* memcpy */
#include <stdlib.h>
#include "AS726X.h"
#include "Wire.h"

/* Forward declarations for low-level I2C functions expected in Wire.h
   The actual implementations should be provided in your Wire.c/h.
   Examples of the functions expected:
     int Wire_beginTransmission(uint8_t addr);
     int Wire_write(uint8_t val);
     int Wire_endTransmission(void);
     int Wire_requestFrom(uint8_t addr, uint8_t qty);
     int Wire_available(void);
     uint8_t Wire_read(void);
*/

/* Initialize device struct */
void AS726X_init(AS726X_t *dev)
{
    if (!dev) return;
    dev->sensorVersion = 0;
}

/* BEGIN / setup */
bool AS726X_begin(AS726X_t *dev, uint8_t gain, uint8_t measurementMode)
{
    if (!dev) return false;

    dev->sensorVersion = AS726X_virtualReadRegister(dev, AS726x_HW_VERSION);

    if (dev->sensorVersion != SENSORTYPE_AS7261 &&
        dev->sensorVersion != SENSORTYPE_AS7262 &&
        dev->sensorVersion != SENSORTYPE_AS7263)
    {
        return false;
    }

    if (AS726X_setBulbCurrent(dev, 0b00) != 0) return false;
    if (AS726X_disableBulb(dev) != 0) return false;
    if (AS726X_setIndicatorCurrent(dev, 0b11) != 0) return false;
    if (AS726X_disableIndicator(dev) != 0) return false;
    if (AS726X_setIntegrationTime(dev, 50) != 0) return false;
    if (AS726X_setGain(dev, gain) != 0) return false;
    if (AS726X_setMeasurementMode(dev, measurementMode) != 0) return false;

    return true;
}

uint8_t AS726X_getVersion(AS726X_t *dev)
{
    if (!dev) return 0;
    return dev->sensorVersion;
}

/* Measurement mode (bits 2-3 of CONTROL_SETUP) */
int AS726X_setMeasurementMode(AS726X_t *dev, uint8_t mode)
{
    if (mode > 0b11) mode = 0b11;
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    value &= 0b11110011; /* clear BANK bits */
    value |= (mode << 2);
    return AS726X_virtualWriteRegister(dev, AS726x_CONTROL_SETUP, value);
}

uint8_t AS726X_getMeasurementMode(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    return (value & 0b00001100);
}

/* Gain (bits 4-5) */
int AS726X_setGain(AS726X_t *dev, uint8_t gain)
{
    if (gain > 0b11) gain = 0b11;
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    value &= 0b11001111; /* clear GAIN bits */
    value |= (gain << 4);
    return AS726X_virtualWriteRegister(dev, AS726x_CONTROL_SETUP, value);
}

uint8_t AS726X_getGain(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    return (value & 0b00110000);
}

/* Integration time */
int AS726X_setIntegrationTime(AS726X_t *dev, uint8_t integrationValue)
{
    (void)dev;
    return AS726X_virtualWriteRegister(dev, AS726x_INT_T, integrationValue);
}

uint8_t AS726X_getIntegrationTime(AS726X_t *dev)
{
    return AS726X_virtualReadRegister(dev, AS726x_INT_T);
}

/* Interrupt enable/disable */
int AS726X_enableInterrupt(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    value |= (1 << 1);
    return AS726X_virtualWriteRegister(dev, AS726x_CONTROL_SETUP, value);
}

int AS726X_disableInterrupt(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    value &= ~(1 << 1);
    return AS726X_virtualWriteRegister(dev, AS726x_CONTROL_SETUP, value);
}

/* takeMeasurements: set one-shot and wait for data ready */
int AS726X_takeMeasurements(AS726X_t *dev)
{
    if (AS726X_clearDataAvailable(dev) != 0) return -1;
    if (AS726X_setMeasurementMode(dev, 3) != 0) return -1;

    uint32_t timeout = millis() + TIMEOUT;
    while (!AS726X_dataAvailable(dev))
    {
        delay(POLLING_DELAY);
        if (millis() > timeout) return -1;
    }
    return 0;
}

int AS726X_takeMeasurementsWithBulb(AS726X_t *dev)
{
    if (AS726X_enableBulb(dev) != 0) return -1;
    if (AS726X_takeMeasurements(dev) != 0) return -1;
    if (AS726X_disableBulb(dev) != 0) return -1;
    return 0;
}

/* Channel readers */
int AS726X_getChannel(AS726X_t *dev, uint8_t channelRegister)
{
    int colorData = ((int)AS726X_virtualReadRegister(dev, channelRegister) << 8);
    colorData |= AS726X_virtualReadRegister(dev, channelRegister + 1);
    return colorData;
}

/* Convenience wrappers */
int AS726X_getViolet(AS726X_t *dev) { return AS726X_getChannel(dev, AS7262_V); }
int AS726X_getBlue(AS726X_t *dev)   { return AS726X_getChannel(dev, AS7262_B); }
int AS726X_getGreen(AS726X_t *dev)  { return AS726X_getChannel(dev, AS7262_G); }
int AS726X_getYellow(AS726X_t *dev) { return AS726X_getChannel(dev, AS7262_Y); }
int AS726X_getOrange(AS726X_t *dev) { return AS726X_getChannel(dev, AS7262_O); }
int AS726X_getRed(AS726X_t *dev)    { return AS726X_getChannel(dev, AS7262_R); }

int AS726X_getR(AS726X_t *dev) { return AS726X_getChannel(dev, AS7263_R); }
int AS726X_getS(AS726X_t *dev) { return AS726X_getChannel(dev, AS7263_S); }
int AS726X_getT(AS726X_t *dev) { return AS726X_getChannel(dev, AS7263_T); }
int AS726X_getU(AS726X_t *dev) { return AS726X_getChannel(dev, AS7263_U); }
int AS726X_getV(AS726X_t *dev) { return AS726X_getChannel(dev, AS7263_V); }
int AS726X_getW(AS726X_t *dev) { return AS726X_getChannel(dev, AS7263_W); }

int AS726X_getX(AS726X_t *dev)   { return AS726X_getChannel(dev, AS7261_X); }
int AS726X_getY(AS726X_t *dev)   { return AS726X_getChannel(dev, AS7261_Y); }
int AS726X_getZ(AS726X_t *dev)   { return AS726X_getChannel(dev, AS7261_Z); }
int AS726X_getNir(AS726X_t *dev) { return AS726X_getChannel(dev, AS7261_NIR); }
int AS726X_getDark(AS726X_t *dev){ return AS726X_getChannel(dev, AS7261_DARK); }
int AS726X_getClear(AS726X_t *dev){ return AS726X_getChannel(dev, AS7261_CLEAR); }

/* Calibrated readings */
float AS726X_getCalibratedValue(AS726X_t *dev, uint8_t calAddress)
{
    uint8_t b0 = AS726X_virtualReadRegister(dev, calAddress + 0);
    uint8_t b1 = AS726X_virtualReadRegister(dev, calAddress + 1);
    uint8_t b2 = AS726X_virtualReadRegister(dev, calAddress + 2);
    uint8_t b3 = AS726X_virtualReadRegister(dev, calAddress + 3);

    uint32_t calBytes = 0;
    calBytes |= ((uint32_t)b0 << 24);
    calBytes |= ((uint32_t)b1 << 16);
    calBytes |= ((uint32_t)b2 << 8);
    calBytes |= ((uint32_t)b3 << 0);

    return AS726X_convertBytesToFloat(calBytes);
}

//Returns the various calibration data
float AS726X_getCalibratedViolet(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7262_V_CAL)); }
float AS726X_getCalibratedBlue(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7262_B_CAL)); }
float AS726X_getCalibratedGreen(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7262_G_CAL)); }
float AS726X_getCalibratedYellow(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7262_Y_CAL)); }
float AS726X_getCalibratedOrange(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7262_O_CAL)); }
float AS726X_getCalibratedRed(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7262_R_CAL)); }

float AS726X_getCalibratedR(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7263_R_CAL)); }
float AS726X_getCalibratedS(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7263_S_CAL)); }
float AS726X_getCalibratedT(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7263_T_CAL)); }
float AS726X_getCalibratedU(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7263_U_CAL)); }
float AS726X_getCalibratedV(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7263_V_CAL)); }
float AS726X_getCalibratedW(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7263_W_CAL)); }

float AS726X_getCalibratedX(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_X_CAL)); }
float AS726X_getCalibratedY(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_Y_CAL)); }
float AS726X_getCalibratedZ(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_Z_CAL)); }
float AS726X_getCalibratedX1931(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_X1931_CAL)); }
float AS726X_getCalibratedY1931(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_Y1931_CAL)); }
float AS726X_getCalibratedUPri1976(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_UPRI_CAL)); }
float AS726X_getCalibratedVPri1976(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_VPRI_CAL)); }
float AS726X_getCalibratedU1976(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_U_CAL)); }
float AS726X_getCalibratedV1976(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_V_CAL)); }
float AS726X_getCalibratedDUV1976(AS726X_t *dev) { return(AS726X_getCalibratedValue(dev, AS7261_DUV_CAL)); }
int AS726X_getCalibratedLux(AS726X_t *dev)  { return(AS726X_getChannel(dev, AS7261_LUX_CAL)); }
int AS726X_getCalibratedCCT(AS726X_t *dev) { return(AS726X_getChannel(dev, AS7261_CCT_CAL)); }

float AS726X_convertBytesToFloat(uint32_t myLong)
{
    union
	{
        uint32_t u32;
        float f;
    } u;
    u.u32 = myLong;
    return u.f;
}

/* Data ready flag */
bool AS726X_dataAvailable(AS726X_t *dev)
{
    (void)dev;
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    return (value & (1 << 1)) != 0;
}

int AS726X_clearDataAvailable(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    value &= ~(1 << 1);
    return AS726X_virtualWriteRegister(dev, AS726x_CONTROL_SETUP, value);
}

/* LED controls */
int AS726X_enableIndicator(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_LED_CONTROL);
    value |= (1 << 0);
    return AS726X_virtualWriteRegister(dev, AS726x_LED_CONTROL, value);
}

int AS726X_disableIndicator(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_LED_CONTROL);
    value &= ~(1 << 0);
    return AS726X_virtualWriteRegister(dev, AS726x_LED_CONTROL, value);
}

int AS726X_setIndicatorCurrent(AS726X_t *dev, uint8_t current)
{
    if (current > 0b11) current = 0b11;
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_LED_CONTROL);
    value &= 0b11111001;
    value |= (current << 1);
    return AS726X_virtualWriteRegister(dev, AS726x_LED_CONTROL, value);
}

int AS726X_enableBulb(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_LED_CONTROL);
    value |= (1 << 3);
    return AS726X_virtualWriteRegister(dev, AS726x_LED_CONTROL, value);
}

int AS726X_disableBulb(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_LED_CONTROL);
    value &= ~(1 << 3);
    return AS726X_virtualWriteRegister(dev, AS726x_LED_CONTROL, value);
}

int AS726X_setBulbCurrent(AS726X_t *dev, uint8_t current)
{
    if (current > 0b11) current = 0b11;
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_LED_CONTROL);
    value &= 0b11001111;
    value |= (current << 4);
    return AS726X_virtualWriteRegister(dev, AS726x_LED_CONTROL, value);
}

/* Temperature */
uint8_t AS726X_getTemperature(AS726X_t *dev)
{
    (void)dev;
    return AS726X_virtualReadRegister(dev, AS726x_DEVICE_TEMP);
}

float AS726X_getTemperatureF(AS726X_t *dev)
{
    float temperatureF = (float)AS726X_getTemperature(dev);
    temperatureF = temperatureF * 1.8f + 32.0f;
    return temperatureF;
}

/* Soft reset */
int AS726X_softReset(AS726X_t *dev)
{
    uint8_t value = AS726X_virtualReadRegister(dev, AS726x_CONTROL_SETUP);
    value |= (1 << 7);
    return AS726X_virtualWriteRegister(dev, AS726x_CONTROL_SETUP, value);
}

/* Virtual register read/write (uses slave registers) */
uint8_t AS726X_virtualReadRegister(AS726X_t *dev, uint8_t virtualAddr)
{
    (void)dev;
    uint8_t status;
    uint8_t retries = 0;

    /* Preemptive read if RX valid */
    status = AS726X_readRegister(AS72XX_SLAVE_STATUS_REG);
    if ((status & AS72XX_SLAVE_RX_VALID) != 0)
    {
        (void)AS726X_readRegister(AS72XX_SLAVE_READ_REG); /* discard */
    }

    /* Wait for TX_VALID to be clear (ready to write) */
    while (1)
    {
        status = AS726X_readRegister(AS72XX_SLAVE_STATUS_REG);
        if (status == 0xFF) return status;
        if ((status & AS72XX_SLAVE_TX_VALID) == 0) break;
        delay(POLLING_DELAY);
        if (retries++ > MAX_RETRIES) return 0xFF;
    }

    /* Send the virtual register address (bit7 = 0 for read) */
    if (AS726X_writeRegister(AS72XX_SLAVE_WRITE_REG, virtualAddr) != 0) return 0xFF;

    retries = 0;
    /* Wait for RX_VALID */
    while (1)
    {
        status = AS726X_readRegister(AS72XX_SLAVE_STATUS_REG);
        if (status == 0xFF) return status;
        if ((status & AS72XX_SLAVE_RX_VALID) != 0) break;
        delay(POLLING_DELAY);
        if (retries++ > MAX_RETRIES) return 0xFF;
    }

    uint8_t incoming = AS726X_readRegister(AS72XX_SLAVE_READ_REG);
    return incoming;
}

int AS726X_virtualWriteRegister(AS726X_t *dev, uint8_t virtualAddr, uint8_t dataToWrite)
{
    (void)dev;
    uint8_t status;
    uint8_t retries = 0;

    /* Wait for WRITE register to be empty */
    while (1)
    {
        status = AS726X_readRegister(AS72XX_SLAVE_STATUS_REG);
        if (status == 0xFF) return -1;
        if ((status & AS72XX_SLAVE_TX_VALID) == 0) break;
        delay(POLLING_DELAY);
        if (retries++ > MAX_RETRIES) return -1;
    }

    /* Write virtual register address with bit7 set to indicate write */
    if (AS726X_writeRegister(AS72XX_SLAVE_WRITE_REG, (virtualAddr | 0x80)) != 0) return -1;

    retries = 0;
    /* Wait for WRITE register to be empty again */
    while (1)
    {
        status = AS726X_readRegister(AS72XX_SLAVE_STATUS_REG);
        if (status == 0xFF) return -1;
        if ((status & AS72XX_SLAVE_TX_VALID) == 0) break;
        delay(POLLING_DELAY);
        if (retries++ > MAX_RETRIES) return -1;
    }

    /* Send the data to complete the operation */
    if (AS726X_writeRegister(AS72XX_SLAVE_WRITE_REG, dataToWrite) != 0) return -1;

    return 0;
}

/* Low-level I2C access using the C Wire API expected by this port.
   These routines attempt to mirror the behavior from the original C++ version.
*/

uint8_t AS726X_readRegister(uint8_t addr)
{
    uint8_t err = 0xFF;

    Wire_beginTransmission(AS726X_ADDR);
    if (Wire_write(addr, 1) == 0) return err;
    if (Wire_endTransmission(1) != 0) return err;

    if (Wire_requestFrom(AS726X_ADDR, 1) == 0) return err;
    if (Wire_available())
    {
        return Wire_read();
    } else
    {
        /* No print to Serial in this C port; caller gets 0xFF */
        return err;
    }
}

int AS726X_writeRegister(uint8_t addr, uint8_t val)
{
    uint8_t err = 0xFF;

    Wire_beginTransmission(AS726X_ADDR);
    if (Wire_write(addr, 1) == 0) return (int)err;
    if (Wire_write(val, 1) == 0) return (int)err;
    if (Wire_endTransmission(1) != 0) return (int)err;

    return 0;
}

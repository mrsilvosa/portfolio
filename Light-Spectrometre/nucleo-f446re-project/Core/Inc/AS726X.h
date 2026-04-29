/*
  AS726X.h - C port of SparkFun AS726X Arduino Library (partial)
  Converted to C from AS726X.h / AS726X.cpp in the SparkFun library.
  This header assumes a C-style Wire interface is available via "Wire.h"
  (Wire_beginTransmission, Wire_write, Wire_endTransmission,
   Wire_requestFrom, Wire_available, Wire_read).
  It also assumes millis() and delay(ms) are available in the environment.
*/

#ifndef AS726X_H
#define AS726X_H

#include <stdint.h>
#include <stdbool.h>

/* I2C helper functions (expected to be provided by your Wire.c/.h) */
/* e.g. Wire_beginTransmission(addr), Wire_write(val), Wire_endTransmission(), 
   Wire_requestFrom(addr, qty), Wire_available(), Wire_read() */

/* Time helpers (expected to be provided by the environment) */
extern uint32_t millis(void);
extern void delay(uint32_t ms);

/* Device handle */
typedef struct
{
    uint8_t sensorVersion;
} AS726X_t;

/* Construction / init */
void AS726X_init(AS726X_t *dev);
bool AS726X_begin(AS726X_t *dev, uint8_t gain, uint8_t measurementMode);

/* Core controls */
int AS726X_takeMeasurements(AS726X_t *dev);
int AS726X_takeMeasurementsWithBulb(AS726X_t *dev);
uint8_t AS726X_getVersion(AS726X_t *dev);

int AS726X_setMeasurementMode(AS726X_t *dev, uint8_t mode);
uint8_t AS726X_getMeasurementMode(AS726X_t *dev);

int AS726X_setGain(AS726X_t *dev, uint8_t gain);
uint8_t AS726X_getGain(AS726X_t *dev);

int AS726X_setIntegrationTime(AS726X_t *dev, uint8_t integrationValue);
uint8_t AS726X_getIntegrationTime(AS726X_t *dev);

int AS726X_enableInterrupt(AS726X_t *dev);
int AS726X_disableInterrupt(AS726X_t *dev);

bool AS726X_dataAvailable(AS726X_t *dev);
int AS726X_clearDataAvailable(AS726X_t *dev);

/* LED / bulb */
int AS726X_enableIndicator(AS726X_t *dev);
int AS726X_disableIndicator(AS726X_t *dev);
int AS726X_setIndicatorCurrent(AS726X_t *dev, uint8_t current);

int AS726X_enableBulb(AS726X_t *dev);
int AS726X_disableBulb(AS726X_t *dev);
int AS726X_setBulbCurrent(AS726X_t *dev, uint8_t current);

/* Temperature */
uint8_t AS726X_getTemperature(AS726X_t *dev);
float AS726X_getTemperatureF(AS726X_t *dev);

/* Soft reset */
int AS726X_softReset(AS726X_t *dev);

/* Channel reads (int16 stored in two sequential registers) */
int AS726X_getChannel(AS726X_t *dev, uint8_t channelRegister);

/* Convenience color / channel accessors (names kept from original) */
int AS726X_getViolet(AS726X_t *dev);
int AS726X_getBlue(AS726X_t *dev);
int AS726X_getGreen(AS726X_t *dev);
int AS726X_getYellow(AS726X_t *dev);
int AS726X_getOrange(AS726X_t *dev);
int AS726X_getRed(AS726X_t *dev);

int AS726X_getR(AS726X_t *dev);
int AS726X_getS(AS726X_t *dev);
int AS726X_getT(AS726X_t *dev);
int AS726X_getU(AS726X_t *dev);
int AS726X_getV(AS726X_t *dev);
int AS726X_getW(AS726X_t *dev);

int AS726X_getX(AS726X_t *dev);
int AS726X_getY(AS726X_t *dev);
int AS726X_getZ(AS726X_t *dev);
int AS726X_getNir(AS726X_t *dev);
int AS726X_getDark(AS726X_t *dev);
int AS726X_getClear(AS726X_t *dev);

/* Calibrated values */
float AS726X_getCalibratedValue(AS726X_t *dev, uint8_t calAddress);
float AS726X_convertBytesToFloat(uint32_t myLong);

//Returns the various calibration data
float AS726X_getCalibratedViolet(AS726X_t *dev);
float AS726X_getCalibratedBlue(AS726X_t *dev);
float AS726X_getCalibratedGreen(AS726X_t *dev);
float AS726X_getCalibratedYellow(AS726X_t *dev);
float AS726X_getCalibratedOrange(AS726X_t *dev);
float AS726X_getCalibratedRed(AS726X_t *dev);

float AS726X_getCalibratedR(AS726X_t *dev);
float AS726X_getCalibratedS(AS726X_t *dev);
float AS726X_getCalibratedT(AS726X_t *dev);
float AS726X_getCalibratedU(AS726X_t *dev);
float AS726X_getCalibratedV(AS726X_t *dev);
float AS726X_getCalibratedW(AS726X_t *dev);

float AS726X_getCalibratedX(AS726X_t *dev);
float AS726X_getCalibratedY(AS726X_t *dev);
float AS726X_getCalibratedZ(AS726X_t *dev);
float AS726X_getCalibratedX1931(AS726X_t *dev);
float AS726X_getCalibratedY1931(AS726X_t *dev);
float AS726X_getCalibratedUPri1976(AS726X_t *dev);
float AS726X_getCalibratedVPri1976(AS726X_t *dev);
float AS726X_getCalibratedU1976(AS726X_t *dev);
float AS726X_getCalibratedV1976(AS726X_t *dev);
float AS726X_getCalibratedDUV1976(AS726X_t *dev);
int AS726X_getCalibratedLux(AS726X_t *dev);
int AS726X_getCalibratedCCT(AS726X_t *dev);

/* Low-level virtual register access */
uint8_t AS726X_virtualReadRegister(AS726X_t *dev, uint8_t virtualAddr);
int AS726X_virtualWriteRegister(AS726X_t *dev, uint8_t virtualAddr, uint8_t dataToWrite);

/* Low-level physical read/write */
uint8_t AS726X_readRegister(uint8_t addr);
int AS726X_writeRegister(uint8_t addr, uint8_t val);

/* I2C / device addresses and registers (copied from original) */
#define AS726X_ADDR 0x49 //7-bit unshifted default I2C Address

/* Register addresses */
#define AS726x_DEVICE_TYPE 0x00
#define AS726x_HW_VERSION 0x01
#define AS726x_CONTROL_SETUP 0x04
#define AS726x_INT_T 0x05
#define AS726x_DEVICE_TEMP 0x06
#define AS726x_LED_CONTROL 0x07

#define AS72XX_SLAVE_STATUS_REG 0x00
#define AS72XX_SLAVE_WRITE_REG 0x01
#define AS72XX_SLAVE_READ_REG 0x02

/* AS7262 registers */
#define AS7262_V 0x08
#define AS7262_B 0x0A
#define AS7262_G 0x0C
#define AS7262_Y 0x0E
#define AS7262_O 0x10
#define AS7262_R 0x12
#define AS7262_V_CAL 0x14
#define AS7262_B_CAL 0x18
#define AS7262_G_CAL 0x1C
#define AS7262_Y_CAL 0x20
#define AS7262_O_CAL 0x24
#define AS7262_R_CAL 0x28

/* AS7263 registers */
#define AS7263_R 0x08
#define AS7263_S 0x0A
#define AS7263_T 0x0C
#define AS7263_U 0x0E
#define AS7263_V 0x10
#define AS7263_W 0x12
#define AS7263_R_CAL 0x14
#define AS7263_S_CAL 0x18
#define AS7263_T_CAL 0x1C
#define AS7263_U_CAL 0x20
#define AS7263_V_CAL 0x24
#define AS7263_W_CAL 0x28

/* AS7261 registers */
#define AS7261_X 0x08 //16b
#define AS7261_Y 0x0A //16b
#define AS7261_Z 0x0C //16b
#define AS7261_NIR 0x0E //16b
#define AS7261_DARK 0x10 //16b
#define AS7261_CLEAR 0x12 //16b
#define AS7261_X_CAL 0x14
#define AS7261_Y_CAL 0x18
#define AS7261_Z_CAL 0x1C
#define AS7261_X1931_CAL 0x20
#define AS7261_Y1931_CAL 0x24
#define AS7261_UPRI_CAL 0x28
#define AS7261_VPRI_CAL 0x2C
#define AS7261_U_CAL 0x30
#define AS7261_V_CAL 0x34
#define AS7261_DUV_CAL 0x38
#define AS7261_LUX_CAL 0x3C //16b
#define AS7261_CCT_CAL 0x3E //16b

#define AS72XX_SLAVE_TX_VALID 0x02
#define AS72XX_SLAVE_RX_VALID 0x01

#define SENSORTYPE_AS7261 0x3D
#define SENSORTYPE_AS7262 0x3E
#define SENSORTYPE_AS7263 0x3F

#define POLLING_DELAY 5 //ms
#define MAX_RETRIES 3
#define TIMEOUT 3000

#endif /* AS726X_H */

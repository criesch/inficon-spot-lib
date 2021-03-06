/*
 * inficon_spot.cpp
 *
 * Arduino library for INFICON Spot sensors
 *
 * Copyright (c) 2018-2021 INFICON Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "Arduino.h"
#include "inficon_spot.h"

InficonSpot::InficonSpot()
{
  _ss_pin = 10;
  _rdy_pin = 7;
  _spi_freq = 4000000UL;
  _fullscale = 0;
}

InficonSpot::InficonSpot(int ss_pin, int rdy_pin, unsigned long spi_freq)
{
  _ss_pin = ss_pin;
  _rdy_pin = rdy_pin;
  _spi_freq = spi_freq;
  _fullscale = 0;
}

void InficonSpot::begin()
{
  SPI.begin();

  // initialize the data ready and slave select pins:
  digitalWrite(_ss_pin, HIGH);
  pinMode(_ss_pin, OUTPUT);
  pinMode(_rdy_pin, INPUT_PULLUP);
}

/*
 * send the reset command (0x88) to the sensor
 */
void InficonSpot::resetSensor()
{
  sendCommand(0x88);
}

/*
 * send a command to the sensor
 */
void InficonSpot::sendCommand(uint8_t cmd)
{
  SPI.beginTransaction(SPISettings(_spi_freq, MSBFIRST, SPI_MODE1));
  digitalWrite(_ss_pin, LOW);
  SPI.transfer(cmd);
  digitalWrite(_ss_pin, HIGH);
  SPI.endTransaction();
}

bool InficonSpot::isDataAvailable()
{
  return (digitalRead(_rdy_pin) == LOW);
}

/*
 * read a result register from the sensor
 *
 * The input parameter reg specifies which result
 * shall be read. The return value is the content
 * of the 24 bit result register.
 */
uint32_t InficonSpot::readRegister(byte reg)
{
  uint32_t result;
  byte buf[4] = {0, 0, 0, 0}; // buffer for the 4 byte SPI transfer

  buf[0] = reg | 0x40; // copy opcode to the first byte of the buffer

  SPI.beginTransaction(SPISettings(_spi_freq, MSBFIRST, SPI_MODE1));
  // take the chip select low to select the device:
  digitalWrite(_ss_pin, LOW);
  // send the opcode and three zero bytes. Four received bytes will be placed in buf
  SPI.transfer(buf, 4);
  // take the chip select high to de-select:
  digitalWrite(_ss_pin, HIGH);
  SPI.endTransaction();

  // copy the 24 bit result from the buffer
  result = ((uint32_t) buf[1] << 16) | ((uint32_t) buf[2] << 8) | (uint32_t) buf[3];
  return result;
}

/*
 * write a register of sensor
 */
void InficonSpot::writeRegister(byte reg, uint32_t data)
{
  byte buf[4]; // buffer for the 4 byte SPI transfer

  buf[0] = reg | 0xc0; // copy opcode to the first byte of the buffer
  buf[1] = (data >> 16) & 0xff;
  buf[2] = (data >> 8) & 0xff;
  buf[3] = (data >> 0) & 0xff;

  SPI.beginTransaction(SPISettings(_spi_freq, MSBFIRST, SPI_MODE1));
  digitalWrite(_ss_pin, LOW);
  SPI.transfer(buf, 4);
  digitalWrite(_ss_pin, HIGH);
  SPI.endTransaction();
}

/*
 * read data from the sensor OTP memory
 */
void InficonSpot::readOTP(uint16_t address, uint8_t *data, int length)
{
  uint8_t buf[3]; // buffer for three byte SPI transfer

  SPI.beginTransaction(SPISettings(_spi_freq, MSBFIRST, SPI_MODE1));

  for (int i = 0; i < length; i++) {
    buf[0] = 0x20 | (((address + i) >> 8) & 0x1f);
    buf[1] = (address + i) & 0xff;
    buf[2] = 0;

    digitalWrite(_ss_pin, LOW);
    SPI.transfer(buf, 3);
    digitalWrite(_ss_pin, HIGH);

    data[i] = buf[2];
  }

  SPI.endTransaction();
}

/*
 * read data from the sensor memory
 */
void InficonSpot::readMemory(uint16_t address, uint8_t *data, int length)
{
  uint8_t buf[3]; // buffer for three byte SPI transfer

  SPI.beginTransaction(SPISettings(_spi_freq, MSBFIRST, SPI_MODE1));

  for (int i = 0; i < length; i++) {
    buf[0] = 0x10 | (((address + i) >> 8) & 0x0f);
    buf[1] = (address + i) & 0xff;
    buf[2] = 0;

    digitalWrite(_ss_pin, LOW);
    SPI.transfer(buf, 3);
    digitalWrite(_ss_pin, HIGH);

    data[i] = buf[2];
  }

  SPI.endTransaction();
}

/*
 * write data to the sensor memory
 */
void InficonSpot::writeMemory(uint16_t address, const uint8_t data[], int length)
{
  uint8_t buf[3]; // buffer for three byte SPI transfer

  SPI.beginTransaction(SPISettings(_spi_freq, MSBFIRST, SPI_MODE1));

  for (int i = 0; i < length; i++) {
    buf[0] = 0x90 | (((address + i) >> 8) & 0x0f);
    buf[1] = (address + i) & 0xff;
    buf[2] = data[i];

    digitalWrite(_ss_pin, LOW);
    SPI.transfer(buf, 3);
    digitalWrite(_ss_pin, HIGH);
  }

  SPI.endTransaction();
}

/*
 * read a label data string
 */
String InficonSpot::readLabel(uint16_t address, byte length)
{
  char s[32];
  bool isZeroTerminated = false;

  if (length > 32) {
    length = 32;
  }

  readMemory(address, reinterpret_cast<uint8_t*>(s), length);

  // if no zero byte was received, the string is not zero-terminated and therefore
  // invalid. Something went wrong, we make the string empty.
  for (byte i = 0; i < length; i++) {
    if (s[i] == '\0') {
      isZeroTerminated = true;
    }
  }

  if (!isZeroTerminated) {
    s[0] = '\0';
  }

  return String(s);
}

/*
 * Read label data from sensor. All these commands require a pointer to a character
 * buffer. The label data will be placed in this buffer as a zero-terminated string.
 */
String InficonSpot::readProductNo()
{
  return readLabel(ADDR_PRODUCT_NO, 32);
}

String InficonSpot::readSerialNo()
{
  return readLabel(ADDR_SERIAL_NO, 32);
}

String InficonSpot::readFullscale1()
{
  return readLabel(ADDR_FULLSCALE1, 16);
}

String InficonSpot::readFullscale2()
{
  return readLabel(ADDR_FULLSCALE2, 16);
}

String InficonSpot::readType()
{
  return readLabel(ADDR_TYPE, 16);
}

String InficonSpot::readSpeed()
{
  return readLabel(ADDR_SPEED, 16);
}

/*
 * Read the CRCs from the Spot sensor memory
 */
uint32_t InficonSpot::readSramCrc()
{
  uint8_t crcbuf[4];
  readMemory(ADDR_SRAMCRC, crcbuf, 4);
  return
    ((uint32_t) crcbuf[3]) << 24 |
    ((uint32_t) crcbuf[2]) << 16 |
    ((uint32_t) crcbuf[1]) << 8 |
    (uint32_t) crcbuf[0];
}

uint32_t InficonSpot::readOtpCrc()
{
  uint8_t crcbuf[4];
  readOTP(ADDR_OTPCRC, crcbuf, 4);
  return
    ((uint32_t) crcbuf[3]) << 24 |
    ((uint32_t) crcbuf[2]) << 16 |
    ((uint32_t) crcbuf[1]) << 8 |
    (uint32_t) crcbuf[0];
}

/*
 * Set fullscale value for pressure conversion
 */
void InficonSpot::setFullscale(float fullscale)
{
  _fullscale = fullscale;
}

/*
 * Convert a the pressure result registers content to pressure.
 * requires the fullscale parameter to be set to the full scale pressure
 * in the desired pressure unit.
 */
float InficonSpot::convertPressure(uint32_t result)
{
  int32_t rawPressure = *((int32_t *) &result); // convert to signed integer
  return  rawPressure / 2097152.0f * _fullscale;
}

float InficonSpot::convertPressure(uint32_t result, float fullscale)
{
  int32_t rawPressure = *((int32_t *) &result); // convert to signed integer
  return  rawPressure / 2097152.0f * fullscale;
}

/*
 * convert the temperature result register to degree Celsius
 */
float InficonSpot::convertTemperature(uint32_t result)
{
  int32_t rawTemperature = *((int32_t *) &result); // convert to signed integer
  return rawTemperature / 2097152.0f * 25;
}

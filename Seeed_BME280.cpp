#include "Seeed_BME280.h"

bool BME280::init(int i2c_addr) {
    uint8_t retry = 0;
    uint8_t chip_id = 0;


    _devAddr = i2c_addr;
    Wire.begin();

    while ((retry++ < 5) && (chip_id != 0x60)) {
        chip_id = BME280Read8(BME280_REG_CHIPID);
        #ifdef BMP280_DEBUG_PRINT
        Serial.print("Read chip ID: ");
        Serial.println(chip_id);
        #endif
        delay(100);
    }
    if (chip_id != 0x60){
        Serial.println("Read Chip ID fail!");
        return false;
    }

    dig_T1 = BME280Read16LE(BME280_REG_DIG_T1);
    dig_T2 = BME280ReadS16LE(BME280_REG_DIG_T2);
    dig_T3 = BME280ReadS16LE(BME280_REG_DIG_T3);

    dig_P1 = BME280Read16LE(BME280_REG_DIG_P1);
    dig_P2 = BME280ReadS16LE(BME280_REG_DIG_P2);
    dig_P3 = BME280ReadS16LE(BME280_REG_DIG_P3);
    dig_P4 = BME280ReadS16LE(BME280_REG_DIG_P4);
    dig_P5 = BME280ReadS16LE(BME280_REG_DIG_P5);
    dig_P6 = BME280ReadS16LE(BME280_REG_DIG_P6);
    dig_P7 = BME280ReadS16LE(BME280_REG_DIG_P7);
    dig_P8 = BME280ReadS16LE(BME280_REG_DIG_P8);
    dig_P9 = BME280ReadS16LE(BME280_REG_DIG_P9);

    dig_H1 = BME280Read8(BME280_REG_DIG_H1);
    dig_H2 = BME280Read16LE(BME280_REG_DIG_H2);
    dig_H3 = BME280Read8(BME280_REG_DIG_H3);
    dig_H4 = (BME280Read8(BME280_REG_DIG_H4) << 4) | (0x0F & BME280Read8(BME280_REG_DIG_H4 + 1));
    dig_H5 = (BME280Read8(BME280_REG_DIG_H5 + 1) << 4) | (0x0F & BME280Read8(BME280_REG_DIG_H5) >> 4);
    dig_H6 = (int8_t)BME280Read8(BME280_REG_DIG_H6);

    writeRegister(BME280_REG_CONTROLHUMID, 0x05);  //Choose 16X oversampling
    writeRegister(BME280_REG_CONTROL, 0xB7);  //Choose 16X oversampling

    return true;
}

float BME280::getTemperature(void) {
    int32_t var1, var2;

    int32_t adc_T = BME280Read24(BME280_REG_TEMPDATA);
    // Check if the last transport successed
    if (!isTransport_OK) {
        return 0;
    }
    adc_T >>= 4;
    var1 = (((adc_T >> 3) - ((int32_t)(dig_T1 << 1))) *
            ((int32_t)dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
            ((int32_t)dig_T3)) >> 14;

    t_fine = var1 + var2;
    float T = (t_fine * 5 + 128) >> 8;
    return T / 100;
}

float BME280::getPressure(void) {
  int64_t var1, var2, p;
  // Call getTemperature to get t_fine
  getTemperature();
  // Check if the last transport successed
  if (!isTransport_OK) {
    return 0;
  }
  int32_t adc_P = BME280Read24(BME280_REG_PRESSUREDATA);
  adc_P >>= 4;
  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + (((int64_t)dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
  if (var1 == 0) {
    return 0; // avoid exception caused by division by zero
  }
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
  return (float)(p / 256.0);
}

float BME280::getHumidity(void) {
  int32_t v_x1_u32r, adc_H;
  // Call getTemperature to get t_fine
  getTemperature();
  // Check if the last transport successed
  if (!isTransport_OK) {
    return 0;
  }
  adc_H = BME280Read16(BME280_REG_HUMIDITYDATA);
  v_x1_u32r = (t_fine - ((int32_t)76800));
  v_x1_u32r = (
                ((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) +
                ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                ((int32_t)dig_H2) + 8192) >> 14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
  v_x1_u32r = v_x1_u32r >> 12;
  float h = v_x1_u32r / 1024.0;
  return h;
}

float BME280::calcAltitude(float pressure) {
    if (!isTransport_OK) {
        return 0;
    }

    float A = pressure / 101325;
    float B = 1 / 5.25588;
    float C = pow(A, B);
    C = 1.0 - C;
    C = C / 0.0000225577;
    return C;
}

uint8_t BME280::BME280Read8(uint8_t reg) {
    Wire.beginTransmission(_devAddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_devAddr, 1);
    // return 0 if slave didn't response
    if (Wire.available() < 1) {
        isTransport_OK = false;
        return 0;
    } else {
        isTransport_OK = true;
    }

    return Wire.read();
}

uint16_t BME280::BME280Read16(uint8_t reg) {
    uint8_t msb, lsb;

    Wire.beginTransmission(_devAddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_devAddr, 2);
    // return 0 if slave didn't response
    if (Wire.available() < 2) {
        isTransport_OK = false;
        return 0;
    } else {
        isTransport_OK = true;
    }
    msb = Wire.read();
    lsb = Wire.read();

    return (uint16_t) msb << 8 | lsb;
}

uint16_t BME280::BME280Read16LE(uint8_t reg) {
    uint16_t data = BME280Read16(reg);
    return (data >> 8) | (data << 8);
}

int16_t BME280::BME280ReadS16(uint8_t reg) {
    return (int16_t)BME280Read16(reg);
}

int16_t BME280::BME280ReadS16LE(uint8_t reg) {
    return (int16_t)BME280Read16LE(reg);
}

uint32_t BME280::BME280Read24(uint8_t reg) {
    uint32_t data;

    Wire.beginTransmission(_devAddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_devAddr, 3);
    // return 0 if slave didn't response
    if (Wire.available() < 3) {
        isTransport_OK = false;
        return 0;
    } else if (isTransport_OK == false) {
        isTransport_OK = true;
        if (!init(_devAddr)) {
            #ifdef BMP280_DEBUG_PRINT
            Serial.println("Device not connected or broken!");
            #endif
        }
    }
    data = Wire.read();
    data <<= 8;
    data |= Wire.read();
    data <<= 8;
    data |= Wire.read();

    return data;
}

void BME280::writeRegister(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_devAddr); // start transmission to device
    Wire.write(reg);       // send register address
    Wire.write(val);         // send value to write
    Wire.endTransmission();     // end transmission
}

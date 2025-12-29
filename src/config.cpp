/*
 * class for config data stored inside flash memory
 * part of a solar timer project
 */
#include <Arduino.h>

#include "globals.h"
#include "config.h"


// app configuration
Config_s config;



// calc crc16 checksum (CCITT 0xffff)
// src:  https://stackoverflow.com/questions/10564491/function-to-calculate-a-crc16-checksum
// test: https://www.lammertbies.nl/comm/info/crc-calculation
//
uint16_t Config_s::calcCrc16() const
{
    const uint8_t *data_p = reinterpret_cast<const uint8_t *>(this) + sizeof(Config_s::crc16);
    unsigned int length = sizeof(Config_s) - sizeof(Config_s::crc16);
    
    uint16_t crc = 0xffff;
    uint8_t x;

    while (length--) {
        x = (crc >> 8) ^ *(data_p++);
        x ^= x >> 4;
        crc = (crc << 8) ^ (static_cast<uint16_t>(x) << 12) ^ (static_cast<uint16_t>(x) << 5) ^ (static_cast<uint16_t>(x));
    }
    return crc;
}


// checks if the checksum matches the stored content in the struct. 0=does not match, 1=match OK
bool Config_s::isCrcValid() const
{
    return crc16 == calcCrc16();
}


// update the stored checksum to match the config structure content
void Config_s::updateCrc()
{
    crc16 = calcCrc16();
}


// loads data from eeprom
// returns: 0=OK data valid, -1=error
int Config_s::loadData()
{
    int n = eeprom.readBuffer(0, reinterpret_cast<uint8_t *>(&config), sizeof(config));
    if (n != sizeof(config))
        return -1;
    return isCrcValid() ? 0 : -1;
}


// saves data to eeprom
// returns: 0=OK, -1=error
int Config_s::saveData() const
{
    int n = eeprom.writeBuffer(0, reinterpret_cast<uint8_t *>(&config), sizeof(config));
    if (n != sizeof(config))
        return -1;
    return 0;
}


// print object's content to Serial output
void Config_s::debugPrint() const
{
    /*Serial.print("config: crc=");
    Serial.print(crc16, 16);
    Serial.print(" ");
    Serial.print(isCrcValid() ? "OK" : "FAIL");
    Serial.print(" {lat=");
    Serial.print(latitude, 6);
    Serial.print(", lon=");
    Serial.print(longitude, 6);
    Serial.print(", hdop=");
    Serial.print(hdop);
    Serial.print(", sunAlt=");
    Serial.print(switchSunAltitude_x10 / 10.0);
    Serial.print(", delay=");
    Serial.print(switchTimeDelay);
    Serial.println("}");*/
}


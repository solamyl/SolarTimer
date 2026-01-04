/*
 * class for config data stored inside flash memory
 *
 * SolarTimer
 * Timer switch for Arduino (fits Arduino Nano) that turns night lights
 * (like street lamps or decorative lighting) on/off depending on sunset/sunrise
 * at actual geo position. With GPS and RTC.
 *
 * Copyright (C) 2025 by Štěpán Škrob. Licensed under GNU GPL v3.0 license.
 * https://github.com/solamyl/SolarTimer
 */

#pragma once
#ifndef __CONFIG_H__
#define __CONFIG_H__


struct Config_s
{

    uint16_t crc16;
    // DateTime positionTimeUtc;
    float latitude;
    float longitude;
    float hdop;
    // switch lights on when sun the is lower than this (in tenth of degs)
    int switchSunAltitude_x10;
    // delay switching the switch by this number of seconds
    int switchTimeDelay;

protected:
    uint16_t calcCrc16() const;

public:
    // default values in default constructor
    Config_s() : crc16(0), latitude(0.0), longitude(0.0), hdop(-1.0), switchSunAltitude_x10(-20), switchTimeDelay(0)
    {
    }

    // checks if content of the struct matches the checksum. 0=does not match, 1=match OK
    // checks if the checksum matches the stored content in the struct. 0=does not match, 1=match OK
    bool isCrcValid() const;
    // update the stored checksum to match the config structure content
    void updateCrc();

    // loads data from eeprom
    // returns: 0=OK data valid, -1=error
    int loadData();
    // saves data to eeprom
    // returns: 0=OK, -1=error
    int saveData() const;

    // print object's content to Serial output
    void debugPrint() const;
};


// app configuration
extern Config_s config;


// test if eeprom is working
// return: 0=OK, -1=error, or >0 errors from getLastError()
int testEeprom();


#endif // __CONFIG_H__

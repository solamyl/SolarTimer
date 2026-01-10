/*
 * Includes for global variables of solar timer project
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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__


#include <TinyGPSPlus.h>
#include <uRTCLib.h>
#include <at24c32.h>
#include <LCDI2C_Generic.h>
#include <Mini_Button.h>

#include "DateTime.h"
//#include "display.h"
//#include "config.h"


// *** SolarTimer.ino ***
// version info string
extern const char * appVersion;


// *** main.cpp ***
// The TinyGPSPlus object
extern TinyGPSPlus gps;

// uRTCLib rtc;
extern uRTCLib rtc;

// eeprom memory 4096 bytes
extern AT24C32 eeprom;

// lcd display
extern LCDI2C_Generic lcd;

// buttons
extern Button buttonSelect;
extern AutoRepeatButton buttonPlus;
extern AutoRepeatButton buttonMinus;
extern LongPressDetector detectReset;

// uptime counter (secs)
extern unsigned long uptimeSecs;

// debug print to the serial console
// message is split into pieces for better sharing of parts of the messages
// one space is inserted between the strings
// last string must be empty "\0"
void debugPrint(const char * str[], bool newLine=true);
// misc debug info
void debugInfo();


// *** buttons.cpp ***
// method for handling buttons to be called often for being responsive
void handleButtons();


// *** gps.cpp ***
extern unsigned long datetimeSetTS; // last time of setting clocks

extern DateTime sunsetTimeLocal; // sunset today localtime
extern DateTime sunriseTimeLocal; // sunrise next day localtime
extern DateTime switchOnTimeUtc; // this day evening utc
extern DateTime switchOnTimeLocal; // this day evening localtime
extern DateTime switchOffTimeUtc; // next day morning utc
extern DateTime switchOffTimeLocal; // next day morning localtime

// return RTC current time (UTC) using DateTime object
DateTime rtcCurrentTime();
// conversion to localtime
// input: utc
// output: time adjusted to TZ and DST
DateTime localDateTime(const DateTime& dt);

// GPS sync: time to RTC and position to config
// returns: 0=OK, -1=err no gps signal, +1=sync not necessary, +2=not good conditions for resync
int gpsSync(const DateTime& nowUtc);
// calc all the dates
// inputs: force - recalc even if it was recalculated shortly
// returns: 0=OK, -1=err/problem, +1=not necessary
int calculateSwitchTimes(const DateTime& nowUtc, bool force = false);
// test if GPS is responding
// return: 0=OK, -1=error
int testGps();
// test if realtime clock is responding
// return: 0=OK, -1=error
int testRtc();


// *** print.cpp ***
// print integer number into char buf[] - chatGPT recommended
// buf - must be long enough to store the number
// value - a number to print
// reserve - how many places to reserve for right-aligning the number
// returns: length of the output string stored in buf
int printInt(char * buf, int value, bool forceSign=false, int8_t reserve=0, bool trailingZero=true);

// print float number into char buf[] - chatGPT recommended
// buf - must be long enough to store the number
// value - a number to print
// precision - how many decimal places
// reserve - how many places to reserve for right-aligning the number
// returns: length of the output string stored in buf
int printFloat(char * buf, float value, int8_t precision=1, bool forceSign=false, int8_t reserve=0, bool trailingZero=true);

// copy string into char buf
// returns: length of the output string stored in buf
int printString(char * buf, const char * value, int8_t reserve=0, bool trailingZero=true);

// print delay (secs) in form from secs to days
// returns: length of the output string stored in buf
int printDelay(char * buf, unsigned long value, int8_t reserve=0, bool trailingZero=true);

// print date in the form dd.mm.yyyy
// precision - 1=dd., 2=dd.mm., 3=dd.mm.yyyy
// returns: number of bytes produced (always 10)
int printDate(char * buf, const DateTime& date, int8_t precision=3, bool trailingZero=true);

// print time in the form of hh:mm:ss
// precision - 1=hh, 2=hh:mm, 3=hh:mm:ss
// returns: length of the output string stored in buf
int printTime(char * buf, const DateTime& time, int8_t precision=3, bool trailingZero=true);

// print date and time in form of dd.mm.yyyy hh:mm:ss
int printDateTime(char * buf, const DateTime& datetime, int8_t precision=3, bool trailingZero=true);


// *** switch.cpp ***
// initialize switch pin for output
void initSwitch();
// check switch status
void checkSwitch(const DateTime& nowUtc);
// return number of seconds to the nearest switch-ON, negative value=already was switched
long timeToSwitchOn(const DateTime& nowUtc);
// return number of seconds to the nearest switch-OFF, negative value=already was switched
long timeToSwitchOff(const DateTime& nowUtc);


#endif // __GLOBALS_H__

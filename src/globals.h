/*
 * Includes for global variables of solar timer project
 */

#ifndef __GLOBALS_H__
#define __GLOBALS_H__


#include <TinyGPSPlus.h>
#include <uRTCLib.h>
#include <at24c32.h>
#include <LCDI2C_Generic.h>
#include <Mini_Button.h>

#include "DateTime.h"
#include "config.h"


// output switch pin
constexpr int switchPin = 12;


// SolarTimer.ino
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
extern Button buttonPlus;
extern Button buttonMinus;

// display.cpp
// backlight timestamp
// make backlight for some time since this timestamp
extern unsigned long backlightTS;
// is backlight shining?
extern bool backlightOn;
// flag for redrawing whole info on the display
extern bool refreshScreen;
// selector for edited parameter
// 0 = screen1: date/time and sun altitude
// 1 = screen2: gps and switch delay
// 2 = screen3: info
extern int selectScreen;


// time.cpp
extern unsigned long rtcSetTS; // last time of setting clocks

extern DateTime sunsetTimeLocal; // sunset today localtime
extern DateTime sunriseTimeLocal; // sunrise next day localtime
extern DateTime switchOnTimeUtc; // this day evening utc
extern DateTime switchOnTimeLocal; // this day evening localtime
extern DateTime switchOffTimeUtc; // next day morning utc
extern DateTime switchOffTimeLocal; // next day morning localtime



// functions
void display();

// return RTC current time (UTC) using DateTime object
DateTime rtcCurrentTime();
// conversion to localtime
// input: utc
// output: time adjusted to TZ and DST
DateTime localDateTime(const DateTime& dt);

// GPS sync: time to RTC and position to config
// returns: 0=OK, -1=err no gps signal, +1=sync not necessary, +2=not good conditions for resync
int gpsSync();

int calculateSwitchTimes(bool force = false);

// method for handling buttons to be called often for being responsive
void handleButtons();


// print integer number into char buf[] - chatGPT recommended
// buf - must be long enough to store the number
// value - a number to print
// reserve - how many places to reserve for right-aligning the number
// returns: length of the output string stored in buf
int printInt(char * buf, int value, bool forceSign=false, int reserve=0, bool trailingZero=true);

// print float number into char buf[] - chatGPT recommended
// buf - must be long enough to store the number
// value - a number to print
// precision - how many decimal places
// reserve - how many places to reserve for right-aligning the number
// returns: length of the output string stored in buf
int printFloat(char * buf, float value, int precision=1, bool forceSign=false, int reserve=0, bool trailingZero=true);

// copy string into char buf
// returns: length of the output string stored in buf
int printString(char * buf, const char * value, int reserve=0, bool trailingZero=true);

// print delay (millis) in form from msec to days
// returns: length of the output string stored in buf
int printDelay(char * buf, unsigned long value, int reserve=0, bool trailingZero=true);

// print date in the form dd.mm.yyyy
// precision - 0=dd., 1=dd.mm., 2=dd.mm.yyyy
// returns: number of bytes produced (always 10)
int printDate(char * buf, const DateTime& date, int precision=2, bool trailingZero=true);

// print time in the form of hh:mm:ss
// precision - 0=hh, 1=hh:mm, 2=hh:mm:ss
// returns: length of the output string stored in buf
int printTime(char * buf, const DateTime& time, int precision=2, bool trailingZero=true);

// print date and time in form of dd.mm.yyyy hh:mm:ss
int printDateTime(char * buf, const DateTime& datetime, int precision=2, bool trailingZero=true);


#endif // __GLOBALS_H__

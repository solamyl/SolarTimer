/*
 * display functions
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
#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <Wire.h>

#include "DateTime.h"


// backlight timestamp
// make backlight for some time since this timestamp
extern unsigned long backlightTS;
// is backlight shining?
extern bool backlightOn;
// flag for redrawing whole info on the display
extern bool refreshScreen;

// cycle between screens or values
void nextScreen();
// get currently displayed screen/value
uint8_t getActiveScreen();

// show info on display
void display(const DateTime& nowUtc);


#endif // __DISPLAY_H__

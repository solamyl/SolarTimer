//
// this file was taken from DS323x repository and slightly modified (pull request pending)
// https://github.com/hideakitai/DS323x/blob/master/DateTime.h
// but it is not its original location; originally sources from:
// https://github.com/adafruit/RTClib/blob/master/src/RTClib.cpp
//

// port of https://github.com/adafruit/RTClib

#include "DateTime.h"

const uint8_t DateTime::daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

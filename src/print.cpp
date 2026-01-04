/*
 * misc print functions.
 *
 * SolarTimer
 * Timer switch for Arduino (fits Arduino Nano) that turns night lights
 * (like street lamps or decorative lighting) on/off depending on sunset/sunrise
 * at actual geo position. With GPS and RTC.
 *
 * Copyright (C) 2025 by Štěpán Škrob. Licensed under GNU GPL v3.0 license.
 * https://github.com/solamyl/SolarTimer
 */

#include <math.h>

#include "DateTime.h"
#include "globals.h"


// print integer number into char buf[] - chatGPT recommended
// buf - must be long enough to store the number
// value - a number to print
// reserve - how many places to reserve for right-aligning the number
// returns: length of the output string stored in buf
int printInt(char * buf, int value, bool forceSign=false, int8_t reserve=0, bool trailingZero=true)
{
    bool neg = value < 0;
    if (neg)
        value = -value;

    char tmp[12];
    int i = 0;
    do {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (value);

    if (neg)
        tmp[i++] = '-';
    else if (forceSign)
        tmp[i++] = '+';

    int r = reserve - i;
    if (r < 0)
        r = 0;

    // reverse
    int j = 0;
    while (j < r)
        buf[j++] = ' ';
    while (i > 0)
        buf[j++] = tmp[--i];

    if (trailingZero)
        buf[j] = '\0';
    return j;
}


// print float number into char buf[] - chatGPT recommended
// buf - must be long enough to store the number
// value - a number to print
// precision - how many decimal places
// reserve - how many places to reserve for right-aligning the number
// returns: length of the output string stored in buf
int printFloat(char * buf, float value, int8_t precision=1, bool forceSign=false, int8_t reserve=0, bool trailingZero=true)
{
    // check for exceptional values
    if (isnan(value))
        return printString(buf, "NaN", reserve, trailingZero);
    /*if (isinf(value))
        return printString(buf, value < 0 ? "-Inf" : "+Inf", reserve, trailingZero);*/

    char tmp[12];
    int i = 0;

    bool neg = value < 0;
    if (neg) {
        value = -value;
        tmp[i++] = '-';
    }
    else if (forceSign) {
        tmp[i++] = '+';
    }

    // Apply rounding
    float rounder = 0.5;
    for (int p = 0; p < precision; p++)
        rounder /= 10.0;
    value += rounder;
    
    // Integer part
    int intPart = static_cast<int>(value);
    value -= intPart; // fraction part

    i += printInt(tmp + i, intPart, false, 0, false);

    int r = reserve - i;
    if (precision > 0)
        r -= precision + 1;
    if (r < 0)
        r = 0;

    // copy integer part
    int j = 0;
    while (j < r)
        buf[j++] = ' ';
    while (i > 0) {
        buf[j] = tmp[j - r];
        ++j;
        --i;
    }

    // Decimal point
    if (precision > 0) {
        buf[j++] = '.';
        // Fractional part
        do {
            value *= 10.0;
            int digit = static_cast<int>(value);
            buf[j++] = '0' + digit;
            value -= digit;
        }
        while (--precision > 0);
    }
    
    if (trailingZero)
        buf[j] = '\0';
    return j;
}


// copy string into char buf
// returns: length of the output string stored in buf
int printString(char * buf, const char * value, int8_t reserve=0, bool trailingZero=true)
{
    int i = 0;
    while (value[i] != '\0')
        ++i;

    int r = reserve - i;
    if (r < 0)
        r = 0;
    
    // copy result into output
    int j = 0;
    while (j < r)
        buf[j++] = ' ';
    while (i > 0) {
        buf[j] = value[j - r];
        ++j;
        --i;
    }

    if (trailingZero)
        buf[j] = '\0';
    return j;    
}


#if 0
// print delay (millis) in form from msec to days
// returns: length of the output string stored in buf
int printDelay(char * buf, unsigned long value, int8_t reserve=0, bool trailingZero=true)
{
    uint16_t d = value / 86400000ul;
    uint8_t h = (value / 3600000ul) % 24;
    uint8_t m = (value / 60000ul) % 60;
    uint8_t s = (value / 1000) % 60;
    uint16_t ms = value % 1000;

    char tmp[12];
    int i = 0;
    if (d > 0) {
        i = printInt(tmp, d, false, 0, false);
        i += printString(tmp + i, "dni", 4, false);
    }
    else if (h > 0) {
        i = printInt(tmp, h, false, 0, false);
        i += printString(tmp + i, "hod", 4, false);
    }
    else if (m > 0) {
        i = printInt(tmp, m, false, 0, false);
        i += printString(tmp + i, "min", 4, false);
    }
    else if (s > 0) {
        i = printInt(tmp, s, false, 0, false);
        i += printString(tmp + i, "sec", 4, false);
    }
    else {
        i = printInt(tmp, ms, false, 0, false);
        i += printString(tmp + i, "msec", 4, false);
    }

    int r = reserve - i;
    if (r < 0)
        r = 0;

    // copy result into output
    int j = 0;
    while (j < r)
        buf[j++] = ' ';
    while (i > 0) {
        buf[j] = tmp[j - r];
        ++j;
        --i;
    }

    if (trailingZero)
        buf[j] = '\0';
    return j;
}

#else

// print delay (secs) in form from secs to days
// returns: length of the output string stored in buf
int printDelay(char * buf, unsigned long value, int8_t reserve=0, bool trailingZero=true)
{
    uint16_t d = (value / 86400ul);
    uint8_t h = (value / 3600ul) % 24;
    uint8_t m = (value / 60ul) % 60;
    uint8_t s = (value) % 60;
    
    char tmp[12];
    int i = 0;
    if (d > 0) {
        i = printInt(tmp, d, false, 0, false);
        i += printString(tmp + i, "dni", 4, false);
    }
    else if (h > 0) {
        i = printInt(tmp, h, false, 0, false);
        i += printString(tmp + i, "hod", 4, false);
    }
    else if (m > 0) {
        i = printInt(tmp, m, false, 0, false);
        i += printString(tmp + i, "min", 4, false);
    }
    else {
        i = printInt(tmp, s, false, 0, false);
        i += printString(tmp + i, "sec", 4, false);
    }

    int r = reserve - i;
    if (r < 0)
        r = 0;

    // copy result into output
    int j = 0;
    while (j < r)
        buf[j++] = ' ';
    while (i > 0) {
        buf[j] = tmp[j - r];
        ++j;
        --i;
    }

    if (trailingZero)
        buf[j] = '\0';
    return j;
}
#endif


// print date in the form dd.mm.yyyy
// precision - 1=dd., 2=dd.mm., 3=dd.mm.yyyy
// returns: number of bytes produced (always 10)
int printDate(char * buf, const DateTime& date, int8_t precision=3, bool trailingZero=true)
{
    int j = 0;

    uint8_t x = date.day();
    buf[j++] = (x / 10) + '0';
    buf[j++] = (x % 10) + '0';

    if (precision >= 2) {
        x = date.month();
        buf[j++] = '.';
        buf[j++] = (x / 10) + '0';
        buf[j++] = (x % 10) + '0';

        if (precision >= 3) {
            uint16_t x = date.year();
            buf[j++] = '.';
            buf[j++] = (x / 1000) + '0';
            buf[j++] = (x / 100) % 10 + '0';
            buf[j++] = (x / 10) % 10 + '0';
            buf[j++] = (x % 10) + '0';
        }
    }

    if (trailingZero)
        buf[j] = '\0';
    return j;
}


// print time in the form of hh:mm:ss
// precision - 1=hh, 2=hh:mm, 3=hh:mm:ss
// returns: length of the output string stored in buf
int printTime(char * buf, const DateTime& time, int8_t precision=3, bool trailingZero=true)
{
    int j = 0;

    uint8_t x = time.hour();
    buf[j++] = (x / 10) + '0';
    buf[j++] = (x % 10) + '0';

    if (precision >= 2) {
        x = time.minute();
        buf[j++] = ':';
        buf[j++] = (x / 10) + '0';
        buf[j++] = (x % 10) + '0';

        if (precision >= 3) {
            x = time.second();
            buf[j++] = ':';
            buf[j++] = (x / 10) + '0';
            buf[j++] = (x % 10) + '0';
        }
    }

    if (trailingZero)
        buf[j] = '\0';
    return j;
}


// print date and time in form of dd.mm.yyyy hh:mm:ss
int printDateTime(char * buf, const DateTime& dateTime, int8_t precision=3, bool trailingZero=true)
{
    int j = printDate(buf, dateTime, 3, false);
    buf[j++] = ' ';
    j += printTime(buf + j, dateTime, precision, false);

    if (trailingZero)
        buf[j] = '\0';
    return j;
}


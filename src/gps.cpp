/*
 * handling of rtc and gps. calculations of the dates and times
 *
 * SolarTimer
 * Timer switch for Arduino (fits Arduino Nano) that turns night lights
 * (like street lamps or decorative lighting) on/off depending on sunset/sunrise
 * at actual geo position. With GPS and RTC.
 *
 * Copyright (C) 2025 by Štěpán Škrob. Licensed under GNU GPL v3.0 license.
 * https://github.com/solamyl/SolarTimer
 */

#include <Arduino.h>
#include <math.h>

#include <TinyGPSPlus.h>
#include <uRTCLib.h>
#include <SolarCalculator.h>

#include "DateTime.h"
#include "globals.h"


unsigned long datetimeSetTS = 0; // last time of setting clocks
unsigned long positionSetTS = 0; // last time of setting gps position

unsigned long switchTimesCalcTS = 0; // switch times re-calculation timestamp

DateTime sunsetTimeLocal; // sunset localtime
DateTime sunriseTimeLocal; // sunrise localtime
DateTime switchOnTimeUtc; // evening utc
DateTime switchOnTimeLocal; // evening localtime
DateTime switchOffTimeUtc; // morning utc
DateTime switchOffTimeLocal; // morning localtime

// european timezone CET (prague)
const TimeSpan TZ_offset(0, +01/*hh*/, 00/*mm*/, 0);
// EU summer time shift
const TimeSpan DST_offset(0, +01/*hh*/, 00/*mm*/, 0);

// stats of processed data, accumulate in periods of 10s
// stats period: 0:((millis()/10000ul)%2)==0, 1:((millis()/10000ul)%2)==1, -1:uninitialized
int8_t statsPeriod = -1;
unsigned long charsProcessed[2] = {0, 0};
//unsigned long sentencesWithFix[2] = {0, 0};
unsigned long failedChecksum[2] = {0, 0};
unsigned long passedChecksum[2] = {0, 0};



// return RTC current time (UTC) using DateTime object
DateTime rtcCurrentTime()
{
    uint8_t d = rtc.day();
    uint8_t m = rtc.month();
    if (d < 1 || d > 31 || m < 1 || m > 12) //probably rtc error
        return DateTime(); //return default date 01.01.2000 00:00:00

    return DateTime(rtc.year(), rtc.month(), rtc.day(), rtc.hour(), rtc.minute(), rtc.second());
}


// Hours as a float to the DateTime structure
// fillup date as supplied or default 2000-01-01
DateTime hoursToDateTime(double h, int year=2000, int8_t month=1, int8_t day=1)
{
    if (isnan(h) /*|| isinf(h)*/) //handle exceptional values
        return DateTime();

    int8_t daysOff /*= floor(h / 24.0)*/;
    {
        //trivial floor() implementation
        double d = h / 24.0;
        daysOff = static_cast<int>(d);
        if (h < 0 && daysOff != d)
            daysOff--;
    }
    h = h - static_cast<double>(daysOff) * 24.0;

    long s = static_cast<long>((h * 3600.0) + 0.5);
    int8_t hh = (s / 3600) % 24;
    int8_t mm = (s / 60) % 60;
    int8_t ss = s % 60;

    /*Serial.print("hours2date: h=");
    Serial.print(h);
    Serial.print(" daysOff=");
    Serial.print(daysOff);
    Serial.print(" castlong=");
    Serial.print(static_cast<long>(h));
    Serial.print(", s=");
    Serial.print(s);
    Serial.print(", hh=");
    Serial.print(hh);
    Serial.print(", mm=");
    Serial.print(mm);
    Serial.print(", ss=");
    Serial.print(ss);
    Serial.print(", year=");
    Serial.print(year);
    Serial.print(", month=");
    Serial.print(month);
    Serial.print(", day=");
    Serial.println(day);*/

    /*DateTime dt = DateTime(year, month, day, hh, mm, ss);
    if (daysOff)
        dt = dt + TimeSpan(daysOff, 0, 0, 0);
    return dt;*/
    
    // not the fastest, but more memory efficient
    return DateTime(year, month, day, hh, mm, ss) + TimeSpan(daysOff, 0, 0, 0);
}


// check if it is US summer "daylight saving" time
// US DST rule is different from EU
// code taken from: https://github.com/andydoro/DST_RTC/blob/master/DST_RTC.cpp
bool isDST_US(const DateTime& dt)
{
    // Get the day of the week. 0 = Sunday, 6 = Saturday
    int8_t d = dt.day();
    int8_t m = dt.month();
    int8_t h = dt.hour();
    int8_t dow = dt.dayOfTheWeek();
    int8_t previousSunday = d - dow;

    bool dst = false; //Assume we're not in DST

    if (m > 3 && m < 11) dst = true; //DST is happening in America!
    //In the USA in March we are DST if the previous Sunday was on or after the 8th (and on or before the 14th).
    else if (m == 3)
    {
        if (dow == 0)  // if today is Sunday
        {
            if (previousSunday >= 8    // on or after 8th
                    && previousSunday <= 14     // but on or before 14th
                    && h >= 2)  // and at or after 2:00 AM
                dst = true;
            else if (previousSunday >= 15)  // it is a Sunday after the second Sunday
                dst = true;
        }
        else if (previousSunday >= 8) // it is not Sunday and we are after the change to DST
            dst = true;
    }
    //In November we must be before the first Sunday to be dst for USA.
    //In this case we are changing time at 2:00 AM so since the change to the previous Sunday
    //happens at midnight the previous Sunday is actually this Sunday at 2:00 AM
    //That means the previous Sunday must be on or before the 7th.
    else if (m == 11)   // November for the USA
    {
        if (dow == 0)   // if today is Sunday
        {
            if (previousSunday <= 7  // and it is also the first Sunday
                    && h <= 1)  // less than 2:00 AM
                dst = true;
        }
        else if (previousSunday <= 0)   // it is not yet the first Sunday and the previous Sunday was before Nov 1
            dst = true;
    }
    return dst;
}


// check if it is EU summer "daylight saving" time
// EU DST rule is different from US
// code taken from: https://github.com/andydoro/DST_RTC/blob/master/DST_RTC.cpp
bool isDST_EU(const DateTime& dt)
{
    // Get the day of the week. 0 = Sunday, 6 = Saturday
    int8_t d = dt.day();
    int8_t m = dt.month();
    int8_t h = dt.hour();
    int8_t dow = dt.dayOfTheWeek();
    int8_t previousSunday = d - dow;

    bool dst = false; //Assume we're not in DST

    if (m > 3 && m < 10) dst = true; //DST is happening in Europe!
    //In Europe in March, we are DST if the previous Sunday was on or after the 25th.
    else if (m == 3)
    {
        if (dow == 0)    // Today is Sunday
        {
            if (previousSunday >= 25  // and it is a Sunday on or after 25th (there can't be a Sunday in March after this)
                    && h >= 2)  // 2:00 AM for Europe
                dst = true;
        }
        else if (previousSunday >= 25) // if not Sunday and the last Sunday has passed
            dst = true;
    }
    //In October we must be before the last Sunday to be in DST for Europe.
    //In this case we are changing time at 2:00 AM so since the change to the previous Sunday
    //happens at midnight the previous Sunday is actually this Sunday at 2:00 AM
    //That means the previous Sunday must be on or before the 31st but after the 25th.
    else if (m == 10) // October for Europe
    {
        if (dow == 0)   // if today is Sunday
        {
            if (previousSunday >= 25  // and it is also on or after 25th
                    && h < 2)  // less than 2:00 AM for Europe
                dst = true;
            else if (previousSunday < 25)   // it is not yet the last Sunday
                dst = true;
        }
        else if (previousSunday < 25) // it is not Sunday
            dst = true;
    }
    return dst;
}


// conversion to localtime
// input: utc
// output: time adjusted to TZ and DST
DateTime localDateTime(const DateTime& dt)
{
    DateTime localtime = dt + TZ_offset;
    bool summerTime = isDST_EU(localtime);
    if (summerTime)
        localtime = localtime + DST_offset; // summer time is happening
    return localtime;
}


// GPS sync: time to RTC and position to config
// returns: 0=OK, -1=err no gps signal, +1=sync not necessary, +2=not good conditions for resync
int gpsSync(const DateTime& nowUtc)
{
    unsigned long nowTS = millis();
    unsigned int hoursSinceRtcResync = (nowTS - datetimeSetTS) / (3600ul * 1000ul);

    // update stats of processed data
    int8_t sp = (nowTS / 10000ul) % 2;
    if (sp != statsPeriod) {
        // sp would be a new "production" period, otherSp is initialized for gathering data
        int8_t otherSp = (sp + 1) % 2;

        // finish new period stats
        charsProcessed[sp] = gps.charsProcessed() - charsProcessed[sp];
        //sentencesWithFix[sp] = gps.sentencesWithFix() - sentencesWithFix[sp];
        failedChecksum[sp] = gps.failedChecksum() - failedChecksum[sp];
        passedChecksum[sp] = gps.passedChecksum() - passedChecksum[sp];

        // store init-counts in the other period
        charsProcessed[otherSp] = gps.charsProcessed();
        //sentencesWithFix[otherSp] = gps.sentencesWithFix();
        failedChecksum[otherSp] = gps.failedChecksum();
        passedChecksum[otherSp] = gps.passedChecksum();

        /*/Serial.print("charsProcessed=");
        Serial.println(charsProcessed[sp]);
        //Serial.print("sentencesWithFix=");
        //Serial.println(sentencesWithFix[sp]);
        Serial.print("failedChecksum=");
        Serial.println(failedChecksum[sp]);
        Serial.print("passedChecksum=");
        Serial.println(passedChecksum[sp]);*/

        // switch active periods
        statsPeriod = sp;
    }

    float hdop = -1.0; //invalid value
    int8_t setTime = 0; //0=false, 1=set if necessary, 2=force
    bool setPosition = false;

    // check for gps input validity
    if (gps.hdop.isValid()) {
        hdop = gps.hdop.hdop();
        int sats = gps.satellites.isValid() ? gps.satellites.value() : -1;
        if (hdop > 80.0) {
            ; //useless value
        }
        else if (hdop < 0.1 || sats < 3 || sats > 30) {
            Serial.print("resync: suspicious GPS data: hdop=");
            //Serial.print(hdop);
            Serial.print(static_cast<int>(hdop * 10.0));
            Serial.print(", sats=");
            Serial.println(sats);
            hdop = -1.0; // reset hdop to invalid value
        }
    }

    // decide what to do on signal quality
    if (hdop > 0.0) {
        if (hdop < config.hdop) { //best quality ever
            // we have reached best quality ever, set GPS and RTC
            setTime = 2; //force
            setPosition = true;
        }
        else if (hdop < 4.0) { //sufficient quality

            if (hdop < 2.0/*great*/ && hoursSinceRtcResync >= 1/*short time since RTC resync*/) {
                // great quality, set RTC
                setTime = 2; //force
            }
            else if (datetimeSetTS == 0/*rtc was not set since restart*/
                    || hoursSinceRtcResync >= 168/*long time since RTC resync*/) {
                // still ok quality, set RTC
                setTime = 1; //if necessary
            }
            if (positionSetTS == 0/*position was not set since restart*/) {
                // sufficient value for replacing stored value
                setPosition = true;
            }
        }
        else if (hdop < 50.0) { //quality is not good

            if (nowUtc.yearOffset() == 0/*RTC not set*/) {
                // poor, but we may have someting. set RTC
                setTime = 1; //if necessary
            }
            if (config.hdop < 0.0/*position not set*/) {
                // poor, but we may have something. set position
                setPosition = true;
            }
        }
    }

    // if wanna set, but not valid or too old (1000 msec)
    if (setTime && (!gps.date.isValid() || !gps.time.isValid() || gps.date.age() > 1000 || gps.time.age() > 1000)) {
        Serial.println("resync: GPS time not valid");
        setTime = 0; //false
    }
    if (setPosition && (!gps.location.isValid() || gps.location.age() > 1000)) {
        Serial.println("resync: GPS pos not valid");
        setPosition = false;
    }

    /*Serial.print("resync: nowTS=");
    Serial.print(nowTS);
    Serial.print(", rtcTS=");
    Serial.print(datetimeSetTS);
    Serial.print(", posTS=");
    Serial.print(positionSetTS);
    Serial.print(", hrsSince=");
    Serial.print(hoursSinceRtcResync);
    Serial.print(", rtcYOff=");
    Serial.print(nowUtc.yearOffset());
    Serial.print(", hdop=");
    Serial.println(hdop);
    Serial.print(":  setTime=");
    Serial.print(setTime);
    Serial.print(", setPos=");
    Serial.println(setPosition);*/

    // set new (updated) time in RTC module
    if (setTime) {
        // construct gps time
        DateTime gpsNow = DateTime(gps.date.year(), gps.date.month(), gps.date.day(),
                gps.time.hour(), gps.time.minute(), gps.time.second());

        // nowUtc is from RTC
        long timediff = (gpsNow - nowUtc).totalseconds();
        Serial.print("resync: RTC diff ");
        Serial.print(timediff);
        Serial.print(" sec");

        // set if necessary
        if (setTime == 1) {
            if (abs(timediff) < 3/*secs*/) {
                // insignificant difference
                setTime = 0; //don't set
                datetimeSetTS = nowTS; //set flag, like it was set
                Serial.print(" - not modified");
            }
        }
        Serial.println();

        // we need to set it
        if (setTime) {
            rtc.set(gpsNow.second(), gpsNow.minute(), gpsNow.hour(), gpsNow.dayOfTheWeek(),
                    gpsNow.day(), gpsNow.month(), gpsNow.yearOffset());
            rtc.lostPowerClear(); //for sure
            rtc.refresh();

            datetimeSetTS = nowTS; //set flag
        }
    }

    // store (update) GPS position in config struct
    if (setPosition) {
        Serial.print("resync: GPS hdop ");
        //Serial.print(config.hdop);
        Serial.print(static_cast<int>(config.hdop * 10.0));
        Serial.print("=>");
        //Serial.print(hdop);
        Serial.print(static_cast<int>(hdop * 10.0));
        Serial.println();

        config.latitude = gps.location.lat();
        config.longitude = gps.location.lng();
        config.hdop = hdop;

        positionSetTS = nowTS; //set flag
    }

    return 0;
}


// calc all the dates
// inputs: force - recalc even if it was recalculated shortly
// returns: 0=OK, -1=err/problem, +1=not necessary
int calculateSwitchTimes(const DateTime& nowUtc, bool force = false)
{
    // utc time
    //DateTime nowUtc = rtcCurrentTime();
    unsigned long nowTS = millis();
    unsigned long secsSinceLastCalc = (nowTS - switchTimesCalcTS) / (1000ul);

    if (!force && nowUtc.yearOffset() > 0 && switchTimesCalcTS > 0 && secsSinceLastCalc < 3600ul) {
        // times have been calculated recently
        //Serial.println("calc: not necessary");
        return +1; //not necessary
    }

    if (nowUtc.yearOffset() == 0/*rtc not set*/ || config.hdop < 0.0/*position not valid*/) {
        Serial.println("calc: time+pos not valid");
        return -1; // input data not valid
    }

    Serial.println("calc: switch times");

    // calc solar noon (and eventually sunset or sunrise)
    // "transit" is the time of the highest altitude of the sun (alias "solar noon")
    // Calculate the times of sunrise => transit => sunset, in hours (UTC) for a given date
    double transit, sunrise, sunset;
    calcSunriseSunset(nowUtc.year(), nowUtc.month(), nowUtc.day(),
            config.latitude, config.longitude,
            transit, sunrise, sunset, SUNRISESET_STD_ALTITUDE);
    DateTime solarNoon = hoursToDateTime(transit, nowUtc.year(), nowUtc.month(), nowUtc.day());

    char buf[32];
    /*Serial.print("solarNoon: "); //highest point of the sun
    printDateTime(buf, solarNoon);
    Serial.println(buf);*/

    // calculate highest sun elevation (at solar noon)
    // (now turned off, because app does not fit in arduino nano memory then)
    /*double azimuth, elevation;
    calcHorizontalCoordinates(solarNoon.year(), solarNoon.month(), solarNoon.day(),
            solarNoon.hour(), solarNoon.minute(), solarNoon.second(),
            config.latitude, config.longitude, azimuth, elevation);
    Serial.print("azimuth: ");
    printFloat(buf, azimuth);
    Serial.println(buf);
    Serial.print("elevation: ");
    printFloat(buf, elevation);
    Serial.println(buf);*/

    // if we are:
    // - before noon: sw-on=yesterday, sw-off=today
    // - after noon: sw-on=today, sw-off=tomorrow
    DateTime swOnDay, swOffDay; //date when to switch on and off
    const TimeSpan oneDay(1/*day*/, 0, 0, 0);
    if (nowUtc >= solarNoon) { //now is after noon - lights-ON period is starting today evening
        swOnDay = solarNoon;
        swOffDay = solarNoon + oneDay;
    }
    else { //now is before noon - lights-ON period has started yesterday
        swOnDay = solarNoon - oneDay;
        swOffDay = solarNoon;
    }
    /*Serial.print("swOnDay: ");
    printDateTime(buf, swOnDay);
    Serial.println(buf);
    Serial.print("swOffDay: ");
    printDateTime(buf, swOffDay);
    Serial.println(buf);*/

    // solar calculator
    // Calculate the times of sunrise, transit, and sunset, in hours (UTC)
    double void1, void2;
    if (swOnDay == solarNoon) {
        // sunset already calculated, calculate also sunrise next day
        calcSunriseSunset(swOffDay.year(), swOffDay.month(), swOffDay.day(),
                config.latitude, config.longitude,
                void1, sunrise, void2, SUNRISESET_STD_ALTITUDE);
    }
    else {
        // sunrise already calculated, calculate also sunset previous day
        calcSunriseSunset(swOnDay.year(), swOnDay.month(), swOnDay.day(),
                config.latitude, config.longitude,
                void1, void2, sunset, SUNRISESET_STD_ALTITUDE);
    }
    sunsetTimeLocal = localDateTime(hoursToDateTime(sunset, swOnDay.year(), swOnDay.month(), swOnDay.day()));
    sunriseTimeLocal = localDateTime(hoursToDateTime(sunrise, swOffDay.year(), swOffDay.month(), swOffDay.day()));

    // switch-ON time
    // calc sunset, but with specified altitude
    calcSunriseSunset(swOnDay.year(), swOnDay.month(), swOnDay.day(),
            config.latitude, config.longitude,
            transit, void1, sunset, SUNRISESET_STD_ALTITUDE + config.switchSunAltitude_x10 / 10.0);
    
    /*Serial.print("sunset hours: ");
    printFloat(buf, sunset);
    Serial.println(buf);*/

    // if the given altitude is too high or too low - NaN is returned
    // KNOWN BUG: following solution is not correct and will not work in polar areas (behind
    // the polar circle) where the sun could be all day above or bellow the horizont.
    // the proper solution is to obtain min/max sun altitude and evalueate it agains
    // "sunAltitude" setting. but it is too expensive (limited program space) for this marginal case 
    if (isnan(sunset)) {
        // sunset is wrong, use some substitute
        if (config.switchSunAltitude_x10 > 0)
            // use solar noon as switch-ON time or all day ON
            switchOnTimeUtc = hoursToDateTime(transit, swOnDay.year(), swOnDay.month(), swOnDay.day());
        else
            // use default day in the past for all day OFF
            switchOnTimeUtc = DateTime();
    }
    else {
        // correct sunset
        switchOnTimeUtc = hoursToDateTime(sunset, swOnDay.year(), swOnDay.month(), swOnDay.day());
    }
    /*Serial.print("ON-utc:  ");
    printDateTime(buf, switchOnTimeUtc);
    Serial.println(buf);*/

    switchOnTimeLocal = localDateTime(switchOnTimeUtc);

    Serial.print("ON-loc:  ");
    printDateTime(buf, switchOnTimeLocal);
    Serial.println(buf);

    // switch-OFF time
    // calc sunrise, but with specified altitude
    calcSunriseSunset(swOffDay.year(), swOffDay.month(), swOffDay.day(),
            config.latitude, config.longitude,
            transit, sunrise, void1, SUNRISESET_STD_ALTITUDE + config.switchSunAltitude_x10 / 10.0);

    /*Serial.print("sunrise hours: ");
    printFloat(buf, sunrise);
    Serial.println(buf);*/

    // if the given altitude is too high or too low - NaN is returned
    // KNOWN BUG: following solution is not correct. read comment above for sunset
    if (isnan(sunrise)) {
        // sunrise is wrong, use some substitute
        if (config.switchSunAltitude_x10 > 0)
            // use solar noon as switch-OFF time for all day ON
            switchOffTimeUtc = hoursToDateTime(transit, swOffDay.year(), swOffDay.month(), swOffDay.day());
        else
            // use default day in the past for all day OFF
            switchOffTimeUtc = DateTime();
    }
    else {
        // correct sunrise
        switchOffTimeUtc = hoursToDateTime(sunrise, swOffDay.year(), swOffDay.month(), swOffDay.day());
    }
    /*Serial.print("OFF-utc:  ");
    printDateTime(buf, switchOffTimeUtc);
    Serial.println(buf);*/

    switchOffTimeLocal = localDateTime(switchOffTimeUtc);
    
    Serial.print("OFF-loc: ");
    printDateTime(buf, switchOffTimeLocal);
    Serial.println(buf);

    // values changed redraw screen
    refreshScreen = true;

    // mark calc time
    switchTimesCalcTS = nowTS;
    return 0; // OK
}


// test if GPS is responding
// BUG: this test will indicate an error in first 10sec after device boot
// return: 0=OK, -1=error
int testGps()
{
    // hdop value and num of satellites should be working almost always
    if (!gps.hdop.isValid() || !gps.satellites.isValid())
        return -1;

    // check amount of processed data (in 10sec period)
    if (charsProcessed[statsPeriod] < 100)
        return -1; //no data on serial line, gps not connected?
    if (failedChecksum[statsPeriod] > passedChecksum[statsPeriod])
        return -1; //probably wrong serial line speed

    return 0;
}


// test if realtime clock is responding
// return: 0=OK, -1=error
int testRtc()
{
    if (rtc.refresh() == false)
        return -1; //error refreshing the data from rtc

    uint8_t d = rtc.day();
    uint8_t m = rtc.month();
    if (d < 1 || d > 31 || m < 1 || m > 12)
        return -1; //probably rtc error

    return 0;
}

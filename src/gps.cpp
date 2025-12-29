/*
 * calculations with the dates
 */

#include <Arduino.h>

#include <TinyGPSPlus.h>
#include <SolarCalculator.h>

#include "DateTime.h"
#include "globals.h"


unsigned long rtcSetTS = 0; // last time of setting clocks
unsigned long positionSetTS = 0; // last time of setting gps position

unsigned long sunsetCalcTS = 0; // switch times re-calculation timestamp

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


// return RTC current time (UTC) using DateTime object
DateTime rtcCurrentTime()
{
    return DateTime(rtc.year(), rtc.month(), rtc.day(), rtc.hour(), rtc.minute(), rtc.second());
}


// Hours as a float to DateTime structure
// fillup date as supplied or default 2000-01-01
DateTime hoursToDateTime(double h, int year=2000, int month=1, int day=1)
{
    long s = static_cast<long>(round(h * 3600.0));
    int hh = (s / 3600) % 24;
    int mm = (s / 60) % 60;
    int ss = s % 60;
    return DateTime(year, month, day, hh, mm, ss);
}


// check if it is EU summer "daylight saving" time
// EU DST rule is different from US
// code from: https://github.com/andydoro/DST_RTC/blob/master/DST_RTC.cpp
bool isDST_EU(const DateTime& dt)
{
    // Get the day of the week. 0 = Sunday, 6 = Saturday
    int d = dt.day();
    int m = dt.month();
    int h = dt.hour();
    int dow = dt.dayOfTheWeek();
    int previousSunday = d - dow;

    bool dst = false; //Assume we're not in DST

    if (m > 3 && m < 10) dst = true; //DST is happening in Europe!
    //In Europe in March, we are DST if the previous Sunday was on or after the 25th.
    if (m == 3)
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
    if (m == 10) // October for Europe
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
int gpsSync(void)
{
    DateTime rtcNow = rtcCurrentTime(); //utc time
    unsigned long nowTS = millis();
    unsigned long hoursSinceRtcResync = (nowTS - rtcSetTS) / (3600ul * 1000ul);

    float hdop = -1.0; //invalid value
    int setRtc = 0; //0=false, 1=set if necessary, 2=force
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
            setRtc = 2; //force
            setPosition = true;
        }
        else if (hdop < 4.0) { //sufficient quality

            if (hdop < 2.0/*great*/ && hoursSinceRtcResync >= 1/*short time since RTC resync*/) {
                // great quality, set RTC
                setRtc = 2; //force
            }
            else if (rtcSetTS == 0/*rtc was not set since restart*/
                    || hoursSinceRtcResync >= 168/*long time since RTC resync*/) {
                // still ok quality, set RTC
                setRtc = 1; //if necessary
            }
            if (positionSetTS == 0/*position was not set since restart*/) {
                // sufficient value for replacing stored value
                setPosition = true;
            }
        }
        else if (hdop < 50.0) { //quality is not good

            if (rtcNow.yearOffset() == 0/*RTC not set*/) {
                // poor, but we may have someting. set RTC
                setRtc = 1; //if necessary
            }
            if (config.hdop < 0.0/*position not set*/) {
                // poor, but we may have something. set position
                setPosition = true;
            }
        }
    }

    // if wanna set, but not valid or too old (1000 msec)
    if (setRtc && (!gps.date.isValid() || !gps.time.isValid() || gps.date.age() > 1000 || gps.time.age() > 1000)) {
        Serial.println("resync: wanna set RTC, but date/time not valid");
        setRtc = 0; //false
    }
    if (setPosition && (!gps.location.isValid() || gps.location.age() > 1000)) {
        Serial.println("resync: wanna set GPS position, but location not valid");
        setPosition = false;
    }

    /*Serial.print("resync: nowTS=");
    Serial.print(nowTS);
    Serial.print(", rtcTS=");
    Serial.print(rtcSetTS);
    Serial.print(", posTS=");
    Serial.print(positionSetTS);
    Serial.print(", hrsSince=");
    Serial.print(hoursSinceRtcResync);
    Serial.print(", rtcYOff=");
    Serial.print(rtcNow.yearOffset());
    Serial.print(", hdop=");
    Serial.println(hdop);
    Serial.print(":  setRtc=");
    Serial.print(setRtc);
    Serial.print(", setPos=");
    Serial.println(setPosition);*/

    if (setRtc) {
        DateTime gpsNow = DateTime(gps.date.year(), gps.date.month(), gps.date.day(),
                gps.time.hour(), gps.time.minute(), gps.time.second());

        long timediff = (gpsNow - rtcNow).totalseconds();
        Serial.print("RTC sync: diff ");
        Serial.print(timediff);
        Serial.print(" secs");

        if (setRtc == 1) { //if necessary
            if (abs(timediff) < 3/*secs*/) {
                // insignificant difference
                setRtc = 0; //don't set
                rtcSetTS = nowTS; //set flag, it is like it was set
                Serial.print(" - not modified");
            }
        }
        Serial.println();

        if (setRtc) {
            rtc.set(gpsNow.second(), gpsNow.minute(), gpsNow.hour(), gpsNow.dayOfTheWeek(),
                    gpsNow.day(), gpsNow.month(), gpsNow.yearOffset());

            rtc.lostPowerClear(); //for sure
            rtc.refresh();

            rtcSetTS = nowTS; //set flag
        }
    }

    if (setPosition) {
        Serial.print("GPS sync: hdop ");
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
int calculateSwitchTimes(bool force = false)
{
    // utc time
    DateTime rtcNow = rtcCurrentTime();
    unsigned long nowTS = millis();
    unsigned long secsSinceLastCalc = (nowTS - sunsetCalcTS) / (1000ul);

    if (!force && rtcNow.yearOffset() > 0 && sunsetCalcTS > 0 && secsSinceLastCalc < 3600ul) {
        // times have been calculated recently
        //Serial.println("calc: not necessary");
        return +1; //not necessary
    }

    if (rtcNow.yearOffset() == 0/*rtc not set*/ || config.hdop < 0.0/*position not valid*/) {
        Serial.println("calc: time/loc not valid");
        return -1; // input data not valid
    }

    Serial.println("*calc solar times*");

    // before noon: on=yesterday, off=today
    // after noon: on=this day, off=tomorrow
    DateTime evening, morning;
    const TimeSpan oneDay(1/*day*/, 0, 0, 0);
    if (rtcNow.hour() >= 12) { //UTC afternoon - light period starts today
        evening = rtcNow;
        morning = rtcNow + oneDay;
    }
    else { //UTC before noon - light period started yesterday
        evening = rtcNow - oneDay;
        morning = rtcNow;
    }

    // solar calculator
    // Calculate the times of sunrise, transit, and sunset, in hours (UTC)
    double transit, sunrise, sunset;
    calcSunriseSunset(evening.year(), evening.month(), evening.day(), config.latitude, config.longitude,
            transit, sunrise, sunset, SUNRISESET_STD_ALTITUDE);
    sunsetTimeLocal = localDateTime(hoursToDateTime(sunset, evening.year(), evening.month(), evening.day()));

    calcSunriseSunset(morning.year(), morning.month(), morning.day(), config.latitude, config.longitude,
            transit, sunrise, sunset, SUNRISESET_STD_ALTITUDE);
    sunriseTimeLocal = localDateTime(hoursToDateTime(sunrise, morning.year(), morning.month(), morning.day()));

    calcSunriseSunset(evening.year(), evening.month(), evening.day(), config.latitude, config.longitude,
            transit, sunrise, sunset, SUNRISESET_STD_ALTITUDE + config.switchSunAltitude_x10 / 10.0);
    switchOnTimeUtc = hoursToDateTime(sunset, evening.year(), evening.month(), evening.day());
    switchOnTimeLocal = localDateTime(switchOnTimeUtc);

    char buf[32];
    Serial.print("switch ON:  ");
    printDateTime(buf, switchOnTimeLocal);
    Serial.println(buf);

    calcSunriseSunset(morning.year(), morning.month(), morning.day(), config.latitude, config.longitude,
            transit, sunrise, sunset, SUNRISESET_STD_ALTITUDE + config.switchSunAltitude_x10 / 10.0);
    switchOffTimeUtc = hoursToDateTime(sunrise, morning.year(), morning.month(), morning.day());
    switchOffTimeLocal = localDateTime(switchOffTimeUtc);
    
    Serial.print("switch OFF: ");
    printDateTime(buf, switchOffTimeLocal);
    Serial.println(buf);

    // values changed redraw screen
    refreshScreen = true;

    // mark calc time
    sunsetCalcTS = nowTS;
    return 0; // OK
}

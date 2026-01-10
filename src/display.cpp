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

#include <Arduino.h>

#include <TinyGPSPlus.h>
#include <LCDI2C_Generic.h>

#include "DateTime.h"
#include "config.h"
#include "globals.h"
#include "display.h"


// backlight timestamp
// make backlight for some time since this timestamp
unsigned long backlightTS = 0;
// is backlight shining?
bool backlightOn = false;
// flag for redrawing whole info on the display
bool refreshScreen = true;

// selector for currently displayed screen (0=default "home" screen)
uint8_t screenSelector = 0;
// (HEXADECIMAL) upper digit is the screen, lower digit is subscreen/certain value on the screen
// 1x = (default) date/time, sun altitude, switch dedlay, switch times
// 2x = gps info
// 3x = diagnostics
// 4x = version info
uint8_t screenSequence[] = {0x11, 0x12, 0x20, 0x30, 0x40};



// fill rest of the line with "spaces", up to given "N" nuber of characters
// longer lines than N will be truncated to N
void fillUpToN(char * buf, uint8_t n)
{
    uint8_t i = 0;
    while (i < n && buf[i] != '\0')
        ++i;
    while (i < n)
        buf[i++] = ' ';

    buf[i] = '\0';
}


// cycle between screens or values
void nextScreen()
{
    // advance to the next screen
    screenSelector = (screenSelector + 1) % (sizeof(screenSequence) / sizeof(uint8_t));
    refreshScreen = true;
}

// get currently displayed screen/value
uint8_t getActiveScreen()
{
    return screenSequence[screenSelector];
}


// print a message about close upcomming switch-state change
// return: true=msg was printed, false=nothing done
bool switchExpectedSoon(const DateTime& nowUtc)
{
    int8_t isComming = 0; //0=none, 1=ON, 2=OFF
    long diff = timeToSwitchOn(nowUtc);
    if (diff >= 0 && diff <= 300) {
        // switch ON is comming
        isComming = 1;
    }
    else {
        diff = timeToSwitchOff(nowUtc);
        if (diff >= 0 && diff <= 300) {
            // switch OFF is comming
            isComming = 2;
        }
    }

    if (isComming) {
        char buf[32];
        buf[0] = '\0';
        fillUpToN(buf, 20);

        printString(buf, isComming == 1 ? "ZAPNUTI ZA" : "VYPNUTI ZA", 0, false);
        //printString(buf + 3, "PNUTI ZA", 0, false);
        printInt(buf + 12, diff, false, 4, false);
        printString(buf + 17, "sec", 0, false);

        lcd.println(buf);

        backlightTS = millis(); //light up display
        return true; //comming soon msg printed
    }
    return false; //nothing happened
}


// draw screen on lcd display:
// 1 = screen1: date/time, sun altitude, switch times
void switchTimeScreen(const DateTime& nowUtc, uint8_t subScreen)
{
    // začátek obrazovky
    //lcd.setCursor(0, 0);
    lcd.home();

    // ========= LINE 1 - local date/time
    DateTime nowLocal = localDateTime(nowUtc);
    char buf[32];

    if (!switchExpectedSoon(nowUtc)) {
        // switch is not comming, normal msg
        buf[0] = '\0';
        fillUpToN(buf, 20);

        printDate(buf, nowLocal, 3, false);
        printTime(buf + 12, nowLocal, 3, false);

        lcd.println(buf);
    }
    // když není potřeba refreshovat vše, ukonči
    if (!refreshScreen)
        return;

    // ========= LINE 2 - vyska slunce / zpoždění spínače
    buf[0] = '\0';
    fillUpToN(buf, 20);

    if (subScreen == 1) {
        printString(buf, "slunce", 0, false);
        printFloat(buf + 7, config.switchSunAltitude_x10 / 10.0, 1, true, 6, false);
        printString(buf + 14, "stupne", 0, false);
        //printFloat(buf + 9, config.switchSunAltitude_x10 / 10.0, 1, true, 7, false);
        //printString(buf + 17, "st.", 0, false);
    }
    else if (subScreen == 2) {
        printString(buf, "zpozdeni", 0, false);
        printInt(buf + 9, config.switchTimeDelay, false, 7, false);
        printString(buf + 17, "sec", 0, false);
    }
    lcd.println(buf);

    // ========= LINE 3 - zapad
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "zapad", 0, false);
    printTime(buf + 9, sunsetTimeLocal, 2, false);
    buf[14] = '-';
    printTime(buf + 15, sunriseTimeLocal, 2, false);

    lcd.println(buf);

    // ========= LINE 4 - sviceni
    buf[0] = '\0';
    fillUpToN(buf, 20);
    
    printString(buf, "sviceni", 0, false);
    printTime(buf + 9, switchOnTimeLocal, 2, false);
    buf[14] = '-';
    printTime(buf + 15, switchOffTimeLocal, 2, false);

    lcd.println(buf);
}


// draw screen on lcd display:
// 2 = screen2: gps info, switch delay
void gpsInfoScreen(const DateTime& nowUtc, uint8_t subScreen)
{
    // začátek obrazovky
    lcd.home();

    // ========= LINE 1 - GPS signal quality
    char buf[32];
    if (!switchExpectedSoon(nowUtc)) {
        // switch is not comming, normal msg
        buf[0] = '\0';
        fillUpToN(buf, 20);

        printString(buf, "GPS", 0, false);

        int pct = gps.hdop.hdop() > 0 ? (199 / gps.hdop.hdop() - 1) : 0;
        if (pct > 100)
            pct = 100;
        int n = printInt(buf + 4, pct, false, 0, false);
        buf[4 + n] = '%';

        printInt(buf + 9, gps.satellites.value(), false, 2, false);
        printString(buf + 12, "satelitu", 0, false);

        lcd.println(buf);
    }

    // ========= LINE 2 - current position
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "akt:", 0, false);
    printFloat(buf + 5, gps.location.lat(), 4, false, 7, false);
    buf[12] = ',';
    printFloat(buf + 13, gps.location.lng(), 4, false, 7, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);

    // když není potřeba refreshovat vše, ukonči
    if (!refreshScreen)
        return;

    // ========= LINE 3 - stored position
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "pam:", 0, false);
    printFloat(buf + 5, config.latitude, 4, false, 7, false);
    buf[12] = ',';
    printFloat(buf + 13, config.longitude, 4, false, 7, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);

    // ========= LINE 4 - rtc sync
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "serizeni pred", 0, false);
    if (datetimeSetTS > 0)
        printDelay(buf + 14, (millis() - datetimeSetTS) / 1000ul, 6, false);
    else
        printString(buf + 18, "--", 0, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);
}


// draw screen on lcd display:
// 3 = screen3: diagnostics
void diagnosticScreen(const DateTime& nowUtc, uint8_t subScreen)
{
    // začátek obrazovky
    lcd.home();

    char buf[32];
    if (!switchExpectedSoon(nowUtc)) {
        // switch is not comming, normal msg
        buf[0] = '\0';
        fillUpToN(buf, 20);
        printString(buf, "GPS pozice/cas", 0, false);
        printString(buf + 16, testGps() ? "FAIL" : "OK", 4, false);
        lcd.println(buf);
    }
    // když není potřeba refreshovat vše, ukonči
    if (!refreshScreen)
        return;

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "RTC hodinky", 0, false);
    printString(buf + 16, testRtc() ? "FAIL" : "OK", 4, false);
    lcd.println(buf);

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "EEPROM config", 0, false);
    printString(buf + 16, testEeprom() ? "FAIL" : "OK", 4, false);
    lcd.println(buf);

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "doba behu", 0, false);
    printDelay(buf + 12, uptimeSecs, 8, false);
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);
}


// draw screen on lcd display:
// 4 = screen4: version info
void versionInfoScreen(const DateTime& nowUtc, uint8_t subScreen)
{
    // začátek obrazovky
    lcd.home();

    char buf[32];
    if (!switchExpectedSoon(nowUtc)) {
        // switch is not comming, normal msg
        buf[0] = '\0';
        fillUpToN(buf, 20);
        printString(buf, appVersion, 0, false);
        fillUpToN(buf, 20); //for sure
        lcd.println(buf);
    }
    // když není potřeba refreshovat vše, ukonči
    if (!refreshScreen)
        return;

#if 1
    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "build: " __DATE__, 0, false);
    lcd.println(buf);
#else
    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "build", 0, false);
    printString(buf + 9, __DATE__, 11, false);
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);
#endif

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "solamyl@seznam.cz", 0, false);
    lcd.println(buf);

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "github.com/solamyl/", 0, false);
    lcd.println(buf);
}


// show info on display
void display(const DateTime& nowUtc)
{
    // check backlight state
    // BUG: display will light up by itself once in 49days... (overflow)
    if (millis() - backlightTS < 60000ul/*60sec*/) {
        // should be ON
        if (!backlightOn) {
            lcd.backlight();
            backlightOn = true;
        }
    }
    else {
        // should be OFF
        if (backlightOn) {
            lcd.noBacklight();
            backlightOn = false;
            // switch back to default screen
            screenSelector = 0;
            refreshScreen = true;
        }
    }

    // pro jistotu obnovuj celý display jednou za minutu
    if (nowUtc.second() == 0)
        refreshScreen = true;

    // switch on selected screen
    uint8_t scr = (getActiveScreen() & 0xf0) >> 4;
    uint8_t sub = (getActiveScreen() & 0x0f);

    if (scr == 1) 
        switchTimeScreen(nowUtc, sub);
    else if (scr == 2)
        gpsInfoScreen(nowUtc, sub);
    else if (scr == 3)
        diagnosticScreen(nowUtc, sub);
    else
        versionInfoScreen(nowUtc, sub);

    // clear flag
    refreshScreen = false;
}

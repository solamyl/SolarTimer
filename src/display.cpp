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
#include "globals.h"


// backlight timestamp
// make backlight for some time since this timestamp
unsigned long backlightTS = 0;
// is backlight shining?
bool backlightOn = false;

// flag for redrawing whole info on the display
bool refreshScreen = true;

// selector for currently displayed info (ie. screens)
// 1 = screen1 (default): date/time, sun altitude, switch times
// 2 = screen2: gps info, switch delay
// 3 = screen3: diagnostics
// 4 = screen4: version info
int8_t selectScreen = 1;



// fill rest of the line up to N characters
void fillUpToN(char * buf, int n)
{
    int i = 0;
    while (i < n && buf[i] != '\0')
        ++i;
    while (i < n)
        buf[i++] = ' ';

    buf[i] = '\0';
}


// cycle between screens
void displayNextScreen()
{
    // advance to the next screen
    selectScreen = (selectScreen % 4) + 1; // cycle 4 screens
    refreshScreen = true;
}


// print a message about close upcomming switch-state change
// return: true=msg was printed, false=nothing done
bool switchChangeExpectedSoon(const DateTime& nowUtc)
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

        printString(buf, isComming == 1 ? "*ZAPNUTI ZA" : "*VYPNUTI ZA", 0, false);
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
void screen1(const DateTime& nowUtc)
{
    // začátek obrazovky
    //lcd.setCursor(0, 0);
    lcd.home();

    // ========= LINE 1 - local date/time
    DateTime nowLocal = localDateTime(nowUtc);
    char buf[32];

    if (!switchChangeExpectedSoon(nowUtc)) {
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

    // ========= LINE 2 - vyska slunce
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "slunce", 0, false);
    printFloat(buf + 7, config.switchSunAltitude_x10 / 10.0, 1, true, 6, false);
    printString(buf + 14, "stupnu", 0, false);

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
void screen2(const DateTime& nowUtc)
{
    // začátek obrazovky
    lcd.home();

    // ========= LINE 1 - GPS signal quality
    char buf[32];
    if (!switchChangeExpectedSoon(nowUtc)) {
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
    // když není potřeba refreshovat vše, ukonči
    if (!refreshScreen)
        return;

    // ========= LINE 2 - position
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printFloat(buf, config.latitude, 6, false, 9, false);
    buf[9] = ',';
    printFloat(buf + 11, config.longitude, 6, false, 9, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);

    // ========= LINE 3 - rtc sync
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "serizeni pred", 0, false);
    if (datetimeSetTS > 0)
        printDelay(buf + 14, (millis() - datetimeSetTS) / 1000ul, 6, false);
    else
        printString(buf + 17, "***", 0, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);

    // ========= LINE 4 - switch delay
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "posun spinace", 0, false);
    printString(buf + 17, "sec", 0, false);
    printInt(buf + 14, config.switchTimeDelay, false, 2, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);
}


// draw screen on lcd display:
// 3 = screen3: diagnostics
void screen3(const DateTime& nowUtc)
{
    // začátek obrazovky
    lcd.home();

    char buf[32];
    if (!switchChangeExpectedSoon(nowUtc)) {
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
void screen4(const DateTime& nowUtc)
{
    // začátek obrazovky
    lcd.home();

    char buf[32];
    if (!switchChangeExpectedSoon(nowUtc)) {
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
            selectScreen = 1;
            refreshScreen = true;
        }
    }

    // pro jistotu obnovuj celý display jednou za minutu
    if (nowUtc.second() == 0)
        refreshScreen = true;

    // switch on selected screen
    if (selectScreen == 1) 
        screen1(nowUtc);
    else if (selectScreen == 2)
        screen2(nowUtc);
    else if (selectScreen == 3)
        screen3(nowUtc);
    else
        screen4(nowUtc);

    // clear flag
    refreshScreen = false;
}

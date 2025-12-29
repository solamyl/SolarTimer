/*
 * display functions
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

// selector for edited parameter
// 0 = screen1: date/time and sun altitude
// 1 = screen2: gps and switch delay
// 2 = screen3: info
int selectScreen = 0;


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


void screen1()
{
    // začátek obrazovky
    //lcd.setCursor(0, 0);
    lcd.home();

    // ========= LINE 1 - local date/time
    DateTime t = localDateTime(rtcCurrentTime());

    char buf[32];
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printDate(buf, t, 2, false);
    printTime(buf + 12, t, 2, false);

    lcd.println(buf);

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
    printTime(buf + 9, sunsetTimeLocal, 1, false);
    buf[14] = '-';
    printTime(buf + 15, sunriseTimeLocal, 1, false);

    lcd.println(buf);

    // ========= LINE 4 - sviceni
    buf[0] = '\0';
    fillUpToN(buf, 20);
    
    printString(buf, "sviceni", 0, false);
    printTime(buf + 9, switchOnTimeLocal, 1, false);
    buf[14] = '-';
    printTime(buf + 15, switchOffTimeLocal, 1, false);

    lcd.println(buf);
}


void screen2()
{
    // začátek obrazovky
    lcd.home();

    // ========= LINE 1 - GPS signal quality
    char buf[32];
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
    if (rtcSetTS > 0)
        printDelay(buf + 14, millis() - rtcSetTS, 6, false);
    else
        printString(buf + 17, "***", 0, false);
    
    fillUpToN(buf, 20); //for sure
    lcd.println(buf);

    // ========= LINE 4 - switch delay
    buf[0] = '\0';
    fillUpToN(buf, 20);

    printString(buf, "posun spinace    sec", 0, false);
    printInt(buf + 14, config.switchTimeDelay, false, 2, false);
    
    lcd.println(buf);
}


void screen3()
{
    // když není potřeba refreshovat vše, ukonči
    if (!refreshScreen)
        return;

    // začátek obrazovky
    lcd.home();

    char buf[32];
    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "SolarTimer v1.1", 0, false);
    lcd.println(buf);

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "Build: " __DATE__, 0, false);
    lcd.println(buf);

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "solamyl@seznam.cz", 0, false);
    lcd.println(buf);

    buf[0] = '\0';
    fillUpToN(buf, 20);
    printString(buf, "github.com/solamyl/", 0, false);
    lcd.println(buf);
}


void display()
{
    // check backlight state
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
        }
    }

    // switch on selected screen
    if (selectScreen == 0) 
        screen1();
    else if (selectScreen == 1)
        screen2();
    else
        screen3();

    // clear flag
    refreshScreen = false;
}

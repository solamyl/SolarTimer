/*
 * button handling
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
#include <avr/wdt.h>

#include "config.h"
#include "display.h"
#include "globals.h"


// handle buttons
// call often for being responsive
void handleButtons()
{
    //Serial.println("handleButtons()");
    unsigned long nowTS = millis();

    buttonSelect.read();
    buttonPlus.read();
    buttonMinus.read();
    detectReset.read();

    // == RESET ==
    if (detectReset.wasPressed()) {
        Serial.println("[RESET]");

        lcd.noBacklight(); //give sign of reset activation

        rtc.set(0, 0, 0, 6, 1, 1, 0); //reset rtc
        rtc.lostPowerClear();

        config = Config_s(); //reset config with defaults
        config.updateCrc();
        config.saveData();

        //Serial.println("!! Configuration and RTC was set to defaults !!");
        Serial.print("Config and RTC");
        Serial.print(" - resetting!");
        Serial.println();
        Serial.print("Restarting");
        delay(1);

        // this should cause the device reboot
        wdt_enable(WDTO_15MS);   // shortest timeout

        while (true) {
            Serial.print(".");
            delay(1);
        }
        // this place should be never reached
    }

    // == SELECT ==
    if (buttonSelect.wasPressed()) {
        Serial.println("[SEL]");
        backlightTS = nowTS;
        refreshScreen = true; //refresh always after keypress

        if (!backlightOn) {
            // just light up the display
        }
        else {
            nextScreen(); //advance to the next screen
        }
    }

    uint8_t scr = getActiveScreen();

    // == PLUS ==
    if (buttonPlus.wasPressed()) {
        Serial.println("[+]");
        backlightTS = nowTS;
        refreshScreen = true; //refresh always after keypress
    
        if (!backlightOn) {
            // just light up the display
        }
        else if (scr == 0x11) {
            config.switchSunAltitude_x10 += 1;
            if (config.switchSunAltitude_x10 > 900)
                config.switchSunAltitude_x10 = 900;
        }
        else if (scr == 0x12) {
            config.switchTimeDelay += 10;
            if (config.switchTimeDelay > 990)
                config.switchTimeDelay = 990;
        }
    }

    // == MINUS ==
    if (buttonMinus.wasPressed()) {
        Serial.println("[-]");
        backlightTS = nowTS;
        refreshScreen = true; //refresh always after keypress

        if (!backlightOn) {
            // just light up the display
        }
        else if (scr == 0x11) {
            config.switchSunAltitude_x10 -= 1;
            if (config.switchSunAltitude_x10 < -900)
                config.switchSunAltitude_x10 = -900;
        }
        else if (scr == 0x12) {
            config.switchTimeDelay -= 10;
            if (config.switchTimeDelay < 0)
                config.switchTimeDelay = 0;
        }
    }  
}

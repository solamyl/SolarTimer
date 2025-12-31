// button hadling

#include <Arduino.h>
#include <avr/wdt.h>

#include "globals.h"


// handle buttons
// call often for being responsive
void handleButtons()
{
    //Serial.println("handleButtons()");
    unsigned long now = millis();

    buttonSelect.read();
    buttonPlus.read();
    buttonMinus.read();
    detectReset.read();

    // debug print
    /*if (buttonPlus.wasReleased())
        Serial.println("[(+) RELEASE]");
    if (buttonMinus.wasReleased())
        Serial.println("[(-) RELEASE]");*/

    // == RESET ==
    if (detectReset.wasPressed()) {
        Serial.println("[(RESET) PUSH]");

        rtc.set(0, 0, 0, 6, 1, 1, 0); //reset rtc
        rtc.lostPowerClear();

        config = Config_s(); //reset config with defaults
        config.updateCrc();
        config.saveData();

        Serial.println("!! Configuration and RTC was set to defaults !!");
        Serial.print("Waiting for device restart");
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
        Serial.println("[(X) PUSH]");
        backlightTS = now;

        if (!backlightOn) {
            // just light up the display
            refreshScreen = true;
        }
        else {
            selectScreen = (selectScreen + 1) % 3; // cycle 3 screens
            refreshScreen = true;
        }
    }

    // == PLUS ==
    if (buttonPlus.wasPressed()) {
        Serial.println("[(+) PUSH]");
        backlightTS = now;
    
        if (!backlightOn) {
            // just light up the display
            refreshScreen = true;
        }
        else if (selectScreen == 0) {
            config.switchSunAltitude_x10 += 1;
            if (config.switchSunAltitude_x10 > 900)
                config.switchSunAltitude_x10 = 900;
        }
        else if (selectScreen == 1) {
            config.switchTimeDelay += 10;
            if (config.switchTimeDelay > 990)
                config.switchTimeDelay = 990;
        }
    }

    // == MINUS ==
    if (buttonMinus.wasPressed()) {
        Serial.println("[(-) PUSH]");
        backlightTS = now;

        if (!backlightOn) {
            // just light up the display
            refreshScreen = true;
        }
        else if (selectScreen == 0) {
            config.switchSunAltitude_x10 -= 1;
            if (config.switchSunAltitude_x10 < -900)
                config.switchSunAltitude_x10 = -900;
        }
        else if (selectScreen == 1) {
            config.switchTimeDelay -= 10;
            if (config.switchTimeDelay < 0)
                config.switchTimeDelay = 0;
        }
    }  
}


/*
 * control the switch
 *
 * SolarTimer
 * Timer switch for Arduino (fits Arduino Nano) that turns night lights
 * (like street lamps or decorative lighting) on/off depending on sunset/sunrise
 * at actual geo position. With GPS and RTC.
 *
 * Copyright (C) 2025 by Štěpán Škrob. Licensed under GNU GPL v3.0 license.
 * https://github.com/solamyl/SolarTimer
 */

#include "DateTime.h"
#include "globals.h"


// pin for controling the switch (LOW=active, switch is ON)
constexpr int switchPin = 12;

// switch state: true=ON, false=OFF
bool currentSwitchState = false;
// planned future switch state: true=ON, false=OFF
bool newSwitchState = false;
// if currentSwState!=newSwState, then here is the start of the delay period (millis())
unsigned long switchDelayStartTS;



// initialize the switch pin for output
void initSwitch()
{
    //this should make the pin being already in HIGH state during the pinmode change to OUTPUT
    pinMode(switchPin, INPUT_PULLUP);
    delay(1);
    pinMode(switchPin, OUTPUT);
    digitalWrite(switchPin, HIGH); //for sure, but should be already in HIGH state
}


// check switch status
void checkSwitch(const DateTime& nowUtc)
{
    unsigned long nowTS = millis();

    // check for desired state
    bool desiredState = nowUtc >= switchOnTimeUtc && nowUtc < switchOffTimeUtc;

    // if the switch is switched different than desired
    if (currentSwitchState != desiredState) {
        if (newSwitchState != desiredState) {
            // start countdown of time delay
            switchDelayStartTS = nowTS;
            newSwitchState = desiredState;
        }
    }
    else {
        // current state is the same as desired. reset any possible future changes
        newSwitchState = desiredState;
    }

    // if plan to switch to the new state
    if (newSwitchState != currentSwitchState) {

        /*Serial.print("desir=");
        Serial.print(desiredState);
        Serial.print(", cur=");
        Serial.print(currentSwitchState);
        Serial.print(", new=");
        Serial.print(newSwitchState);
        Serial.print(", switch delay=");
        Serial.print(nowTS - switchDelayStartTS);
        Serial.print(", config delay=");
        Serial.println(static_cast<unsigned long>(config.switchTimeDelay) * 1000ul);*/

        unsigned long cfgDelay = static_cast<unsigned long>(config.switchTimeDelay) * 1000ul;
        if (nowTS - switchDelayStartTS >= cfgDelay) {
            // time delay has elapsed
            if (newSwitchState) {
                Serial.println("*switch ON*");
                digitalWrite(switchPin, LOW); //lights on
            }
            else  {
                Serial.println("*switch OFF*");
                digitalWrite(switchPin, HIGH); //lights off
            }
            currentSwitchState = newSwitchState;
        }
    }

    // inspect real state (LOW=switch is ON)
    Serial.print(digitalRead(switchPin) ? "OFF" : "ON");
    Serial.print(" \t");
}


// return number of seconds to the nearest switch-ON, negative value=already was switched
long timeToSwitchOn(const DateTime& nowUtc)
{
    if (currentSwitchState == true)
        return -1; //already switched on
    if (newSwitchState == true && currentSwitchState == false) {
        // the new state is awaited
        long diff = (millis() - switchDelayStartTS) / 1000ul;
        return config.switchTimeDelay - diff;
    }
    long diff = (switchOnTimeUtc - nowUtc).totalseconds() + config.switchTimeDelay;
    return diff;
}


// return number of seconds to the nearest switch-OFF, negative value=already was switched
long timeToSwitchOff(const DateTime& nowUtc)
{
    if (currentSwitchState == false)
        return -1; //already switched off
    if (newSwitchState == false && currentSwitchState == true) {
        // the new state is awaited
        long diff = (millis() - switchDelayStartTS) / 1000ul;
        return config.switchTimeDelay - diff;
    }
    long diff = (switchOffTimeUtc - nowUtc).totalseconds() + config.switchTimeDelay;
    return diff;
}

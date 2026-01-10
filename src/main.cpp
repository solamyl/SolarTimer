/*
 * main program functions setup() and loop()
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
#include <SoftwareSerial.h>
#include <uRTCLib.h>
#include <at24c32.h>
#include <LCDI2C_Generic.h>
#include <Mini_Button.h>

#include "DateTime.h"
#include "config.h"
#include "display.h"
#include "globals.h"


/**** GLOBALS ****/

// The TinyGPSPlus object
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial ss(4/*RXpin*/, 3/*TXpin*/);

// uRTCLib rtc; RTC time is in UTC
uRTCLib rtc(0x68); //i2c addr

// eeprom memory 4096 bytes
AT24C32 eeprom(0x57); //i2c addr

// lcd display
LCDI2C_Generic lcd(0x27, 20, 4);  // I2C address: 0x27; Display size: 20x4

// buttons
Button buttonSelect(9/*pin*/);
AutoRepeatButton buttonPlus(8/*pin*/, 1000/*delay*/, 100/*rate*/);
AutoRepeatButton buttonMinus(7/*pin*/, 1000/*delay*/, 100/*rate*/);
LongPressDetector detectReset(&buttonSelect, 3000);

// uptime counter (secs)
unsigned long uptimeSecs = 0;



/**** MAIN ****/

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    while (!Serial) {
        ;
    }

    // display init
    lcd.init();

    // prepare output
    initSwitch();

    // serial port to gps
    ss.begin(9600);

    // init rtc
    rtc.set_model(URTCLIB_MODEL_DS3231);
    //rtc.set_12hour_mode(false); //24h mode
    if (!rtc.enableBattery()) {
        Serial.println("RTC: battery error!");
        //const char * msg[] = {"RTC:", "battery error!", ""};
        //debugPrint(msg);
    }
    // refresh data from RTC HW in RTC class object so flags like rtc.lostPower(), rtc.getEOSCFlag(), etc, can get populated
    rtc.refresh();
    if (rtc.lostPower()) {
        Serial.print("RTC: power lost");
        Serial.print(" - resetting!");
        Serial.println();
        //const char * msg[] = {"RTC:", "power lost", " - resetting!", ""};
        //debugPrint(msg);
        // RTCLib::set(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
        rtc.set(0, 0, 0, 6, 1, 1, 0);
        rtc.lostPowerClear();
    }

    // load config
    if (config.loadData() < 0) {
        // config is not valid
        Serial.print("EEPROM: invalid content");
        Serial.print(" - resetting!");
        Serial.println();
        //const char * msg[] = {"EEPROM:", "invalid content", " - resetting!", ""};
        //debugPrint(msg);
        config = Config_s(); //reseting with defaults
        config.updateCrc();
        config.saveData();
    }
    config.debugPrint();

    // buttons init
    buttonSelect.begin();
    buttonPlus.begin();
    buttonMinus.begin();
    detectReset.begin();

    // print misc info
    debugInfo();
}


void loop()
{
    // put your main code here, to run repeatedly:
    // timestamp (from millis()) of the last screen update (not loop() run!!)
    static unsigned long last = 0;
    // accumulator of milisecs for updating uptime counter
    static unsigned int acc = 0;

    unsigned long now = millis();
    //Serial.println(now);

    rtc.refresh();

    // if config has changed
    bool recalc = false;
    if (!config.isCrcValid()) {
        recalc = true;
        refreshScreen = true;
        config.updateCrc();
        config.saveData();
        config.debugPrint();
    }

    // refresh things - do once per second
    if (refreshScreen || (now - last) >= 999/*little bit less than 1sec*/) {

        Serial.print(now);
        Serial.print(" \t");

        // update uptime counter
        acc += now - last;
        uptimeSecs += acc / 1000;
        acc %= 1000;

        // get fresh time from RTC
        DateTime nowUtc = rtcCurrentTime();
        char buf[32];
        printDateTime(buf, nowUtc);
        Serial.print(buf);
        Serial.print(" \t");

        // resync locality and rtc if necessary
        gpsSync(nowUtc);

        handleButtons(); //call often

        calculateSwitchTimes(nowUtc, recalc);
        checkSwitch(nowUtc);

        handleButtons(); //call often

unsigned long t1, t2;
t1=millis();
        // refresh display
        display(nowUtc);
t2=millis();Serial.print("display(): ");Serial.println(t2-t1);

        handleButtons(); //call often

        // notice timestamp of the last "refresh"
        last = now;
    }

    // handle gps
    while (ss.available()) {
        gps.encode(ss.read());
    }

    // handle buttons - call often
    handleButtons();
}


// debug print to the serial console
// message is split into pieces for better sharing of parts of the messages
// one space is inserted between the strings
// last string must be empty "\0"
void debugPrint(const char * str[], bool newLine=true)
{
    int i = 0;
    while (str[i][0] != '\0') {
        Serial.print(str[i]);
        Serial.print(" ");
        i++;
    }
    if (newLine)
        Serial.println();
}


// misc debug info
void debugInfo()
{
    /*Serial.print("sizeof(gps)=");
    Serial.println(sizeof(gps));
    Serial.print("sizeof(ss)=");
    Serial.println(sizeof(ss));
    Serial.print("sizeof(rtc)=");
    Serial.println(sizeof(rtc));
    Serial.print("sizeof(eeprom)=");
    Serial.println(sizeof(eeprom));
    Serial.print("sizeof(lcd)=");
    Serial.println(sizeof(lcd));
    Serial.print("sizeof(buttonSelect)=");
    Serial.println(sizeof(buttonSelect));
    Serial.print("sizeof(buttonPlus)=");
    Serial.println(sizeof(buttonPlus));
    Serial.print("sizeof(buttonMinus)=");
    Serial.println(sizeof(buttonMinus));
    Serial.print("sizeof(detectReset)=");
    Serial.println(sizeof(detectReset));
    Serial.print("sizeof(uptimeSecs)=");
    Serial.println(sizeof(uptimeSecs));*/
    
    /*Serial.print("sizeof(config)=");
    Serial.println(sizeof(config));
    Serial.print("sizeof(DateTime::daysInMonth)=");
    Serial.println(sizeof(uint8_t)*12);
    Serial.print("sizeof(backlightTS)=");
    Serial.println(sizeof(backlightTS));
    Serial.print("sizeof(backlightOn)=");
    Serial.println(sizeof(backlightOn));
    Serial.print("sizeof(refreshScreen)=");
    Serial.println(sizeof(refreshScreen));
    Serial.print("sizeof(screenSelector)=");
    Serial.println(sizeof(screenSelector));*/

    /*Serial.print("sizeof(datetimeSetTS)=");
    Serial.println(sizeof(datetimeSetTS));
    Serial.print("sizeof(positionSetTS)=");
    Serial.println(sizeof(unsigned long));
    Serial.print("sizeof(switchTimesCalcTS)=");
    Serial.println(sizeof(unsigned long));
    Serial.print("sizeof(sunsetTimeLocal)=");
    Serial.println(sizeof(sunsetTimeLocal));
    Serial.print("sizeof(sunriseTimeLocal)=");
    Serial.println(sizeof(sunriseTimeLocal));
    Serial.print("sizeof(switchOnTimeUtc)=");
    Serial.println(sizeof(switchOnTimeUtc));
    Serial.print("sizeof(switchOnTimeLocal)=");
    Serial.println(sizeof(switchOnTimeLocal));
    Serial.print("sizeof(switchOffTimeUtc)=");
    Serial.println(sizeof(switchOffTimeUtc));
    Serial.print("sizeof(switchOffTimeLocal)=");
    Serial.println(sizeof(switchOffTimeLocal));
    Serial.print("sizeof(TZ_offset)=");
    Serial.println(sizeof(const TimeSpan));
    Serial.print("sizeof(DST_offset)=");
    Serial.println(sizeof(const TimeSpan));*/

    /*Serial.print("sizeof(currentSwitchState)=");
    Serial.println(sizeof(bool));
    Serial.print("sizeof(newSwitchState)=");
    Serial.println(sizeof(bool));
    Serial.print("sizeof(switchDelayStartTS)=");
    Serial.println(sizeof(unsigned long));*/

    /*Serial.print("sizeof(DateTime)=");
    Serial.println(sizeof(DateTime));
    Serial.print("sizeof(TimeSpan)=");
    Serial.println(sizeof(TimeSpan));
    Serial.print("sizeof(Button)=");
    Serial.println(sizeof(Button));
    Serial.print("sizeof(int)=");
    Serial.println(sizeof(int));
    Serial.print("sizeof(long)=");
    Serial.println(sizeof(long));
    Serial.print("sizeof(float)=");
    Serial.println(sizeof(float));*/
}
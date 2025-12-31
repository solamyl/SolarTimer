
#include <Arduino.h>

#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <uRTCLib.h>
#include <at24c32.h>
#include <LCDI2C_Generic.h>
#include <Mini_Button.h>

#include "DateTime.h"
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

    //this should make the pin being in HIGH state when changing pinmode to OUTPUT
    pinMode(switchPin, INPUT_PULLUP);
    delay(1);
    pinMode(switchPin, OUTPUT);
    digitalWrite(switchPin, HIGH); //for sure, but should be made with previous sequence

    // serial port to gps
    ss.begin(9600);

    // init rtc
    rtc.set_model(URTCLIB_MODEL_DS3231);
    if (!rtc.enableBattery()) {
        Serial.println("RTC: battery error!");
    }
    // refresh data from RTC HW in RTC class object so flags like rtc.lostPower(), rtc.getEOSCFlag(), etc, can get populated
    rtc.refresh();
    if (rtc.lostPower()) {
        Serial.println("RTC: lost power - reseting!");
        // RTCLib::set(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
        rtc.set(0, 0, 0, 6, 1, 1, 0);
        rtc.lostPowerClear();
    }

    // load config
    if (config.loadData() < 0)
    {
        // 
        Serial.println("eeprom: invalid content - reseting!");
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

    /*Serial.print("sizeof(Config_s)=");
    Serial.println(sizeof(Config_s));
    Serial.print("sizeof(DateTime)=");
    Serial.println(sizeof(DateTime));
    Serial.print("sizeof(Button)=");
    Serial.println(sizeof(Button));
    Serial.print("sizeof(int)=");
    Serial.println(sizeof(int));
    Serial.print("sizeof(long)=");
    Serial.println(sizeof(long));
    Serial.print("sizeof(float)=");
    Serial.println(sizeof(float));*/
}


void loop()
{
    // put your main code here, to run repeatedly:
    static unsigned long last = 0;

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

    // do once per second
    if (refreshScreen || (now - last) >= 999/*little bit less than 1sec*/) {

        Serial.print(now);
        Serial.print(" \t");

        DateTime utc = rtcCurrentTime();
        char buf[32];
        printDateTime(buf, utc);
        Serial.print(buf);
        Serial.print(" \t");

        if (utc >= switchOnTimeUtc && utc < switchOffTimeUtc) {
            Serial.println("SVITI");
            digitalWrite(switchPin, LOW); //lights on
        }
        else {
            Serial.println("zhas");
            digitalWrite(switchPin, HIGH); //lights off
        }
        //Serial.print(" \t");

        calculateSwitchTimes(recalc);
        handleButtons(); //call often

        // pro jistotu obnovuj celý display jednou za minutu
        if (utc.second() == 0)
            refreshScreen = true;

unsigned long t1, t2;
t1=millis();
        // refresh display
        display();
t2=millis();Serial.print("display(): ");Serial.println(t2-t1);
        handleButtons(); //call often

        // resync locality and rtc if necessary
        gpsSync();

        last = now;
    }

    // handle gps
    while (ss.available()) {
        gps.encode(ss.read());
    }

    // handle buttons - call often
    handleButtons();
}

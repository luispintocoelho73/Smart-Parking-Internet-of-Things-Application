#include <Arduino.h>
#include <iostream>
#include<string>
#include<sstream> // for using stringstream
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <SRF05.h>
#endif

#include <EEPROM.h>

#include "settings.h" // Set your Parse keys and hostnames here! (+WiFi credentials)

#define LED D3
#include "Parse-Esp.hpp"
ParseEsp parse(parseHost, parsePath);

#ifdef LED_BUILTIN
const int OutputPin = LED_BUILTIN; // ESP8266 D1 Mini
#else
const int OutputPin = 22; // ESP32 dev board without build in LED, connect LED+R b/w 3.3v and PIN
#endif
static bool outputState = HIGH; // Light/LED is off when GPIO high 

volatile bool shouldSetOnline = false; // In main loop write /online=true to DB
const char *resp;
// Address in EEPROM where session token is saved
#define STADDR 400
static char sessionTokenBuf[35] = {0};
const uint8_t trigger = 13;
const uint8_t echo = 15;
const uint8_t out = 0;
SRF05 distanceSensor = SRF05(trigger,echo,out);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

// Set GPIO high/low, for LED_BUILTIN low means light off
static void turnLightOn(bool isOn) {
    outputState = isOn ? LOW : HIGH;
    digitalWrite(OutputPin, outputState);
}

static void clearTokenFromEeprom() {
        EEPROM.begin(512);
        EEPROM.write(STADDR, 0); 
        EEPROM.end();
}

/**************************************************************************/
/*
    Displays some basic information on this sensor from the unified
    sensor API sensor_t type (see Adafruit_Sensor for more information)
*/
/**************************************************************************/
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
 /* Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");*/
  delay(500);
}

/**************************************************************************/
/*
    Configures the gain and integration time for the TSL2561
*/
/**************************************************************************/
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  //tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
   tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  /*Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");*/
}


/**
    Callback called when we receive changes from LiveQuery.
    Do not do here anything that blocks main loop for a long time.

    @params: data JSON document containing changed Object 
*/
static int liveQueryCb(const char *data) {

    turnLightOn(parseBool(data, "light")); // Set GPIO based in value of key "light"

    if (!parseBool(data, "online"))  // System/UI is checking if we are alive
        shouldSetOnline = true; // Execute actual API call in main loop, not in this callback

    if (parseBool(data, "debug")) { // This was used during development
        clearTokenFromEeprom();     
    }

    return 1;
}

static char objectPath[8+11+5] = {0}; // Full Object path, that will be queried in setup()

// Set Object key "online" to true 
static void setOnline() {
    if (objectPath[0]) {
        Serial.print("Setting device online. objectpath:");
        Serial.println(objectPath);
        parse.set(objectPath,"{\"online\":true}");
    }
    // --- or if you wnt to hard code object path, 
    //parse.set("Devices/dzEli8kFvF","{\"online\":true}");

}

// Forward declaration
void connectStream();
const char* id;
void setup() {
    Serial.begin(115200);
    pinMode(0, OUTPUT);
    digitalWrite(0, outputState);
    digitalWrite(0, LOW);
   // Serial.println("Light Sensor Test"); Serial.println("");
      /* Initialise the sensor */
    //use tsl.begin() to default to Wire, 
    //tsl.begin(&Wire2) directs api to use Wire2, etc.
    if(!tsl.begin())
    {
        /* There was a problem detecting the TSL2561 ... check your connections */
      //  Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
        while(1);
    }
    /* Display some basic information on this sensor */
    displaySensorDetails();
  
    /* Setup the sensor gain and integration time */
    configureSensor();
  
    /* We're ready to go! */
  //  Serial.println("");
    
    // Connect to a WiFi network (ssid and pass from settings.h)
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
     // Serial.printf("Não conseguimos ligar à internet");
    }

    //Serial.println(ESP.getChipId());  // TODO: Could use this ID as device name in server

    parse.setApplicationId(applicationId); // Required
    parse.setRestApiKey(restApiKey); // Only needed with Back4app or old versions (<3) of Parse-server // For set and get
    parse.setJavascriptApiKey(javascriptApiKey); // Only needed with Back4app or old versions (<3) of Parse-server // For stream

    
    /* 
       Session token is stored in EEPROM.
       Read if EEPROM contains the session token, if not then try logging in using username and password
       ("device1" and "demo") TODO/FIXME: provision these properly somehow...
    */
#if 1
    EEPROM.begin(512);
    if (EEPROM.read(STADDR) == 0xA1 && 
            EEPROM.read(STADDR+1) == 0xA3 && 
            EEPROM.read(STADDR+2) == 0xA2) {
        for (int i=0; i < 34; i++) { // FIXME, use DEFINE
            sessionTokenBuf[i] = EEPROM.read(STADDR+3+i);
        }
       // Serial.print("Read Session Token from EEPROM:");
       // Serial.println(sessionTokenBuf);
        parse.setSessionToken(sessionTokenBuf);
    } else {
        resp = parse.login("device1", "demo"); // TODO/FIXME: put these into settings/provisioning!!!
        const char *sessionToken = parseText(resp, "sessionToken");
      //  Serial.println(sessionToken ? sessionToken : "????");
        if (!sessionToken) { // Login failed
            EEPROM.write(STADDR, 0);
            Serial.printf_P(PSTR("Error: login failed\n"));
        } else { // Login success. Write Session Token to EEPROM
            EEPROM.write(STADDR, 0xA1); 
            EEPROM.write(STADDR+1, 0xA3); 
            EEPROM.write(STADDR+2, 0xA2);           
            for (int i=0; i < 34; i++) {
                sessionTokenBuf[i] = sessionToken[i]; 
                EEPROM.write(STADDR+3+i, sessionToken[i]);
            }
          //  Serial.println(sessionTokenBuf);
            parse.setSessionToken(sessionTokenBuf);
        }
    }
    EEPROM.end();
    // ... or just use hardcoded session token: 
    // parse.setSessionToken("r:8e7b1900856910ccb02b2b08ac89ea75");
#endif

    // Query server for "Devices" Object with name "device1".
    // parse.get parameters: Class/object path and query (as defined in REST API)
    resp = parse.get("Node", "where={\"objectId\":\"MlH0WV6Bq6\"}");  

    // Some error handling...
    if (hasErrorResp(resp, 403)) {  // "unauthorized"
        Serial.println("parse.get Devices failed: unauthorized");
        // TODO: reboot?
    }
    if (hasErrorResp(resp, 209)) {  // "Invalid session token"
        Serial.print("Invalid session token");
        clearTokenFromEeprom();
        // TODO: reboot?
    }
   id = parseText(resp,"objectId");
}

String setDistance;
String setLuminosity;
String str1;
String str2;
float lastLuminosity = 0;
float lastDistance = 0;
int timePassed = 0;
float distance = 222.22;

void loop() {
    char *isReserved;
    int b;

    resp = parse.get("Node", "where={\"objectId\":\"MlH0WV6Bq6\"}");   
    id = parseText(resp,"objectId");

    isReserved = parseText(resp, "Reserved");
    b = strcmp(isReserved,"rue,");

    if ( b==0 ) {
        Serial.println("\nThe parking spot was successfully reserved\n");
        digitalWrite(0, HIGH);
        timePassed = 0;
    }
    else {
        Serial.println("\nThe parking spot isn't reserved\n");
        digitalWrite(0, LOW);
    }

    /* Get a new sensor event */ 
    sensors_event_t event;
    tsl.getEvent(&event);

    if( (event.light != lastLuminosity) || (timePassed==900000) ){
        resp = parse.get("Node", "where={\"objectId\":\"MlH0WV6Bq6\"}");   
        id = parseText(resp,"objectId");
        str2 = event.light;
        setLuminosity = "{\"Luminosity\":" + str2 + "}";
        const char *c1 = setLuminosity.c_str();
        if (id) {
            strcpy_P(objectPath, PSTR("Node/"));
            strcat(objectPath, id);
            parse.set(objectPath,c1);
        }
        lastLuminosity = event.light;
        timePassed = 0;
    }

    distance = distanceSensor.getCentimeter();
    if (lastDistance!=distance || (timePassed==900000) ){
        resp = parse.get("Node", "where={\"objectId\":\"MlH0WV6Bq6\"}");  
        id = parseText(resp,"objectId");
        str1 = distance;
        setDistance = "{\"Distance\":" + str1 + "}";
        const char *c = setDistance.c_str();
        if (id) {
            strcpy_P(objectPath, PSTR("Node/"));
            strcat(objectPath, id);
            parse.set(objectPath,c);
        }
        lastDistance = distance;
        timePassed = 0;
    }

    timePassed = timePassed + 2000; // increment time passed since last update given to the cloud server
    delay(2000);
}

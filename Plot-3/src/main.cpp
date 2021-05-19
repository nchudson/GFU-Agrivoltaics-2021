//------------------------------------------------------------------------------
// GFU Agrivoltaics ThingSpeak Test
// Nathaniel Hudson
// nhudson18@georgefox.edu
// Summer 2021
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//             __             __   ___  __
//     | |\ | /  ` |    |  | |  \ |__  /__`
//     | | \| \__, |___ \__/ |__/ |___ .__/
//
//------------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ThingSpeak.h>
#include <NTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_AM2315.h>
#include <Time.h>
#include <TimeLib.h>
#include "secrets.h"

//------------------------------------------------------------------------------
//      __   ___  ___         ___  __
//     |  \ |__  |__  | |\ | |__  /__`
//     |__/ |___ |    | | \| |___ .__/
//
//------------------------------------------------------------------------------

// ThingSpeak PV Fields
#define NUM_FIELDS_PV      (3)
#define TEMP_5_TEMP_FIELD  (1)
#define TEMP_6_TEMP_FIELD  (2)
#define TEMP_7_TEMP_FIELD  (3)
#define IRAD_2_CNTS_FIELD  (4)

// Pin definitions
#define ONE_WIRE_PIN       (48)
#define SD_CS_PIN          (4)
#define RELAY_TRIG_PIN     (7)

// Sensor parameters
#define TEMP_PRECISION     (12)

// Program parameters
#define TIME_ZONE          (-7)
#define SECS_PER_HOUR      (3600)
#define NUM_SAMPLES        (5)

//------------------------------------------------------------------------------
//     ___      __   ___  __   ___  ___  __
//      |  \ / |__) |__  |  \ |__  |__  /__`
//      |   |  |    |___ |__/ |___ |    .__/
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//                __          __        ___  __
//     \  /  /\  |__) |  /\  |__) |    |__  /__`
//      \/  /~~\ |  \ | /~~\ |__) |___ |___ .__/
//
//------------------------------------------------------------------------------

static const uint8_t mac[] = {MAC_3_BYTE_0, MAC_3_BYTE_1, MAC_3_BYTE_2,
  MAC_3_BYTE_3, MAC_3_BYTE_4, MAC_3_BYTE_5};
EthernetClient client;
EthernetUDP udp;
NTPClient ntp(udp, TIME_ZONE * SECS_PER_HOUR);
int32_t thingspeak_response = 0;
const char* plot_3_pv_api_key = PLOT_3_PV_API_KEY;
uint16_t num_tries = 0;
DeviceAddress temp_5_addr = {TEMP_5_ADDR_0, TEMP_5_ADDR_1, TEMP_5_ADDR_2,
  TEMP_5_ADDR_3, TEMP_5_ADDR_4, TEMP_5_ADDR_5, TEMP_5_ADDR_6, TEMP_5_ADDR_7};
DeviceAddress temp_6_addr = {TEMP_6_ADDR_0, TEMP_6_ADDR_1, TEMP_6_ADDR_2,
  TEMP_6_ADDR_3, TEMP_6_ADDR_4, TEMP_6_ADDR_5, TEMP_6_ADDR_6, TEMP_6_ADDR_7};
DeviceAddress temp_7_addr = {TEMP_7_ADDR_0, TEMP_7_ADDR_1, TEMP_7_ADDR_2,
  TEMP_7_ADDR_3, TEMP_7_ADDR_4, TEMP_7_ADDR_5, TEMP_7_ADDR_6, TEMP_7_ADDR_7};

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature temp_sensors(&oneWire);

Adafruit_ADS1115 ads;

time_t cur_time;
time_t prev_time;

// Sensor data
float temp_5_temp;
float temp_6_temp;
float temp_7_temp;

int16_t irad_2_cnts;

//------------------------------------------------------------------------------
//      __   __   __  ___  __  ___      __   ___  __
//     |__) |__) /  \  |  /  \  |  \ / |__) |__  /__`
//     |    |  \ \__/  |  \__/  |   |  |    |___ .__/
//
//------------------------------------------------------------------------------

time_t get_ntp_time();
void read_sensors();

//------------------------------------------------------------------------------
//      __        __          __
//     |__) |  | |__) |    | /  `
//     |    \__/ |__) |___ | \__,
//
//------------------------------------------------------------------------------

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Ethernet.begin(mac);
  Serial.println(Ethernet.localIP());
  ThingSpeak.begin(client);
  pinMode(7, OUTPUT);
  udp.begin(2390);
  ntp.begin();
  ntp.update();
  setSyncProvider(get_ntp_time);

  temp_sensors.begin();
  Serial.println("Temp sensors initialized");
  Serial.print(temp_sensors.getDeviceCount());
  Serial.println(" sensors found");
  ads.begin();
  Serial.println("ADC initialized");

  cur_time = now();
  Serial.println(ntp.getFormattedTime());
  prev_time = now();
}

void loop() {
  // put your main code here, to run repeatedly:
  prev_time = cur_time;
  cur_time = now();

  if(minute(prev_time) != minute(cur_time)) {
    Serial.println("Reading sensors");
    read_sensors();
    Serial.println("Sending data to ThingSpeak");
    ThingSpeak.setField(TEMP_5_TEMP_FIELD, temp_5_temp);
    ThingSpeak.setField(TEMP_6_TEMP_FIELD, temp_6_temp);
    ThingSpeak.setField(TEMP_7_TEMP_FIELD, temp_7_temp);
    ThingSpeak.setField(IRAD_2_CNTS_FIELD, irad_2_cnts);
    Serial.println(ThingSpeak.writeFields(PLOT_3_PV_CHANNEL, plot_3_pv_api_key));
  }

}

// Provides the time library with the current real-world time
// We take care of giving it the time, it takes care of all the calculations with leap years and stuff
// Returns a time_t type... not sure what it is (time, presumably) just basing format off the example code
time_t get_ntp_time()
{
  return ntp.getEpochTime(); // Return seconds since Jan. 1 1970, adjusted for time zone in seconds
}

/**
   Reads data for all sensors into variables
*/
void read_sensors()
{

  // Sensor sampling loop; Temperature
  // Grab 100 samples; samples every 100us
  float temp_samples_1 = 0;
  float temp_samples_2 = 0;
  float temp_samples_3 = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    temp_sensors.requestTemperatures();
    temp_samples_1 += temp_sensors.getTempC(temp_5_addr);
    temp_samples_2 += temp_sensors.getTempC(temp_6_addr);
    temp_samples_3 += temp_sensors.getTempC(temp_7_addr);
    
    delayMicroseconds(100);
  }
  temp_5_temp = temp_samples_1 / NUM_SAMPLES; // Report the average of the samples we gathered
  temp_6_temp = temp_samples_2 / NUM_SAMPLES;
  temp_7_temp = temp_samples_3 / NUM_SAMPLES;
  Serial.print("Temp 1: ");
  Serial.println(temp_5_temp);
  Serial.print("Temp 2: ");
  Serial.println(temp_6_temp);
  Serial.print("Temp 3: ");
  Serial.println(temp_7_temp);


  // Sensor sampling loop; Irradiance
  // Grab 100 samples; samples every 100us
  long irradiance_samples = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    irradiance_samples += ads.readADC_SingleEnded(0);

    delayMicroseconds(100);
  }
  irad_2_cnts = irradiance_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  Serial.print("Irradiance: ");
  Serial.println(irad_2_cnts);

}

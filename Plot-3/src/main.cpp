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
#include <avr/wdt.h>
#include "secrets.h"

//------------------------------------------------------------------------------
//      __   ___  ___         ___  __
//     |  \ |__  |__  | |\ | |__  /__`
//     |__/ |___ |    | | \| |___ .__/
//
//------------------------------------------------------------------------------

// General ThingSpeak Parameters
#define THINGSPEAK_SUCCESS  (200)
#define THINGSPEAK_FAIL     (-301)

// ThingSpeak PV Fields
#define NUM_FIELDS_PV       (3)
#define TEMP_5_TEMP_FIELD   (1)
#define TEMP_6_TEMP_FIELD   (2)
#define TEMP_7_TEMP_FIELD   (3)
#define IRAD_2_WSQM_FIELD   (4)


// Pin definitions
#define ONE_WIRE_PIN        (2)
#define SD_CS_PIN           (4)
#define RELAY_TRIG_PIN      (7)

// Sensor parameters
#define TEMP_PRECISION      (12)

// Program parameters
#define TIME_ZONE           (-7)
#define SECS_PER_HOUR       (3600)
#define NUM_SAMPLES         (20)
#define NTP_SYNC_INTERVAL   (600)

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

static char date_string[23];
static char file_name[12];

File log_file;

time_t cur_time;
time_t prev_time;

// Sensor data
float temp_5_temp;
float temp_6_temp;
float temp_7_temp;

int16_t irad_2_wsqm;

//------------------------------------------------------------------------------
//      __   __   __  ___  __  ___      __   ___  __
//     |__) |__) /  \  |  /  \  |  \ / |__) |__  /__`
//     |    |  \ \__/  |  \__/  |   |  |    |___ .__/
//
//------------------------------------------------------------------------------

time_t get_ntp_time();
void read_sensors();
void create_log_file();
void system_reset();

//------------------------------------------------------------------------------
//      __        __          __
//     |__) |  | |__) |    | /  `
//     |    \__/ |__) |___ | \__,
//
//------------------------------------------------------------------------------

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  wdt_enable(WDTO_4S);
  Ethernet.begin(mac);
  wdt_reset();
  Serial.println(Ethernet.localIP());
  ThingSpeak.begin(client);
  udp.begin(2390);
  ntp.begin();
  ntp.update();
  setSyncProvider(get_ntp_time);
  setSyncInterval(NTP_SYNC_INTERVAL);

  temp_sensors.begin();
  Serial.println("Temp sensors initialized");
  Serial.print(temp_sensors.getDeviceCount());
  Serial.println(" sensors found");
  ads.begin();
  Serial.println("ADC initialized");

  cur_time = now();
  Serial.println(ntp.getFormattedTime());
  prev_time = now();

  SD.begin(SD_CS_PIN);
  create_log_file();
  wdt_reset();
}

void loop() {
  // put your main code here, to run repeatedly:
  wdt_reset();
  prev_time = cur_time;
  cur_time = now();

  if(minute(prev_time) != minute(cur_time)) {
    if(minute(cur_time) == 0) {
      create_log_file();
    }

    Serial.println("Reading sensors");
    read_sensors();
    wdt_reset();
    Serial.println("Sending PV data to ThingSpeak");
    thingspeak_response = 0;
    ThingSpeak.setField(TEMP_5_TEMP_FIELD, temp_5_temp);
    ThingSpeak.setField(TEMP_6_TEMP_FIELD, temp_6_temp);
    ThingSpeak.setField(TEMP_7_TEMP_FIELD, temp_7_temp);
    ThingSpeak.setField(IRAD_2_WSQM_FIELD, irad_2_wsqm);
    thingspeak_response = ThingSpeak.writeFields(PLOT_3_PV_CHANNEL, plot_3_pv_api_key);
    Serial.println(thingspeak_response);
    wdt_reset();


    time_t t = now();
    sprintf(date_string, "%04d-%02d-%02d %02d:%02d:%02d PDT", year(t), month(t), day(t), hour(t), minute(t), second(t));
    log_file.print(date_string);
    log_file.print(",");
    log_file.print(temp_5_temp);
    log_file.print(",");
    log_file.print(temp_6_temp);
    log_file.print(",");
    log_file.print(temp_7_temp);
    log_file.print(",");
    log_file.println(irad_2_wsqm);
    log_file.flush();
    if(thingspeak_response == THINGSPEAK_FAIL) system_reset();
  }
  Ethernet.maintain();
}

// Provides the time library with the current real-world time
// We take care of giving it the time, it takes care of all the calculations with leap years and stuff
// Returns a time_t type... not sure what it is (time, presumably) just basing format off the example code
time_t get_ntp_time()
{
  ntp.update();
  return ntp.getEpochTime(); // Return seconds since Jan. 1 1970, adjusted for time zone in seconds
}

/**
   Reads data for all sensors into variables
*/
void read_sensors()
{

  // Sensor sampling loop; Irradiance
  // Grab 100 samples; samples every 100us
  long irradiance_samples = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    irradiance_samples += ads.readADC_SingleEnded(0);

    delayMicroseconds(100);
  }
  irad_2_wsqm = irradiance_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  // irad_2_wsqm = (0.5458 * irad_2_wsqm) - 100.59;
  Serial.print("Irradiance: ");
  Serial.println(irad_2_wsqm);

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
    wdt_reset();
    
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

}

// Manages log_file files for the SD card
void create_log_file()
{
  // Close the current file and create a new one
  log_file.close();
  time_t t = now();

  // Get the current time (MM/DD_HH) and use it as the file name
  sprintf(file_name, "%02d-%02d_%02d.log", month(t), day(t), hour(t));
  log_file = SD.open(file_name, FILE_WRITE);

  if (!log_file)
  {
    Serial.print("File failed to open with name '");
    Serial.print(file_name);
    Serial.println("'");
  }
  else
  {
    Serial.print("Opened log_file file with name '");
    Serial.print(file_name);
    Serial.println("'");
  }
  log_file.println("created_at,entry_id,field1,field2,field3,field4");
}

void system_reset() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while(1) ;
}
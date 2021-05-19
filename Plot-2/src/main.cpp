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

// ThingSpeak Environmental Fields
#define NUM_FIELDS_ENV     (7)
#define SOIL_1_VOLW_FIELD  (1)
#define SOIL_3_SOWP_FIELD  (2)
#define TEMP_1_TEMP_FIELD  (3)
#define TMPH_1_TEMP_FIELD  (4)
#define TMPH_1_HUMD_FIELD  (5)
#define IRAD_1_CNTS_FIELD  (6)
#define FLOW_1_TICK_FIELD  (7)

// ThingSpeak PV Fields
#define NUM_FIELDS_PV      (3)
#define TEMP_2_TEMP_FIELD  (1)
#define TEMP_3_TEMP_FIELD  (2)
#define TEMP_4_TEMP_FIELD  (3)

// Pin definitions
#define ONE_WIRE_PIN       (2)
#define SD_CS_PIN          (4)
#define RELAY_TRIG_PIN     (7)

// Sensor parameters
#define TEMP_PRECISION     (12)

// Program parameters
#define TIME_ZONE          (-7)
#define SECS_PER_HOUR      (3600)
#define NUM_SAMPLES        (100)

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

static const uint8_t mac[] = {MAC_2_BYTE_0, MAC_2_BYTE_1, MAC_2_BYTE_2,
  MAC_2_BYTE_3, MAC_2_BYTE_4, MAC_2_BYTE_5};
EthernetClient client;
EthernetUDP udp;
NTPClient ntp(udp, TIME_ZONE * SECS_PER_HOUR);
int32_t thingspeak_response = 0;
const char* plot_2_env_api_key = PLOT_2_ENV_API_KEY;
const char* plot_2_pv_api_key = PLOT_2_PV_API_KEY;
uint16_t num_tries = 0;
DeviceAddress temp_1_addr = {TEMP_1_ADDR_0, TEMP_1_ADDR_1, TEMP_1_ADDR_2,
  TEMP_1_ADDR_3, TEMP_1_ADDR_4, TEMP_1_ADDR_5, TEMP_1_ADDR_6, TEMP_1_ADDR_7};
DeviceAddress temp_2_addr = {TEMP_2_ADDR_0, TEMP_2_ADDR_1, TEMP_2_ADDR_2,
  TEMP_2_ADDR_3, TEMP_2_ADDR_4, TEMP_2_ADDR_5, TEMP_2_ADDR_6, TEMP_2_ADDR_7};
DeviceAddress temp_3_addr = {TEMP_3_ADDR_0, TEMP_3_ADDR_1, TEMP_3_ADDR_2,
  TEMP_3_ADDR_3, TEMP_3_ADDR_4, TEMP_3_ADDR_5, TEMP_3_ADDR_6, TEMP_3_ADDR_7};
DeviceAddress temp_4_addr = {TEMP_4_ADDR_0, TEMP_4_ADDR_1, TEMP_4_ADDR_2,
  TEMP_4_ADDR_3, TEMP_4_ADDR_4, TEMP_4_ADDR_5, TEMP_4_ADDR_6, TEMP_4_ADDR_7};

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature temp_sensors(&oneWire);

Adafruit_AM2315 am2315;
Adafruit_ADS1115 ads;

time_t cur_time;
time_t prev_time;

// Sensor data
float temp_1_temp;
float temp_2_temp;
float temp_3_temp;
float temp_4_temp;

float tmph_1_temp;
float tmph_1_humd;

int16_t irad_1_cnts;

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
  // am2315.begin();
  // Serial.println("Ambient temp sensor initialized");
  // ads.begin();
  // Serial.println("ADC initialized");

  cur_time = now();
  Serial.println(ntp.getFormattedTime());
  prev_time = now();
}

void loop() {
  // put your main code here, to run repeatedly:
  prev_time = cur_time;
  cur_time = now();

  if(minute(cur_time) == 0 && minute(prev_time) != 0) {
    Serial.println("Enabling loads");
    digitalWrite(RELAY_TRIG_PIN, HIGH);
    delay(100);
    digitalWrite(RELAY_TRIG_PIN, LOW);
  }

  if(minute(prev_time) != minute(cur_time)) {
    Serial.println("Reading sensors");
    read_sensors();
    Serial.println("Sending data to ThingSpeak");
    ThingSpeak.setField(TEMP_1_TEMP_FIELD, temp_1_temp);
    ThingSpeak.setField(TMPH_1_TEMP_FIELD, tmph_1_temp);
    ThingSpeak.setField(TMPH_1_HUMD_FIELD, tmph_1_humd);
    ThingSpeak.setField(IRAD_1_CNTS_FIELD, irad_1_cnts);
    Serial.println(ThingSpeak.writeFields(PLOT_2_ENV_CHANNEL, plot_2_env_api_key));
    ThingSpeak.setField(TEMP_2_TEMP_FIELD, temp_2_temp);
    ThingSpeak.setField(TEMP_3_TEMP_FIELD, temp_3_temp);
    ThingSpeak.setField(TEMP_4_TEMP_FIELD, temp_4_temp);
    Serial.println(ThingSpeak.writeFields(PLOT_2_PV_CHANNEL, plot_2_pv_api_key));
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
  float temp_samples_4 = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    temp_sensors.requestTemperatures();
    // temp_samples_1 += temp_sensors.getTempC(temp_1_addr);
    temp_samples_2 += temp_sensors.getTempC(temp_2_addr);
    temp_samples_3 += temp_sensors.getTempC(temp_3_addr);
    temp_samples_4 += temp_sensors.getTempC(temp_4_addr);
    
    delayMicroseconds(100);
  }
  temp_1_temp = temp_samples_1 / NUM_SAMPLES; // Report the average of the samples we gathered
  temp_2_temp = temp_samples_2 / NUM_SAMPLES;
  temp_3_temp = temp_samples_3 / NUM_SAMPLES;
  temp_4_temp = temp_samples_4 / NUM_SAMPLES;
  Serial.print("Temp 1: ");
  Serial.println(temp_1_temp);
  Serial.print("Temp 2: ");
  Serial.println(temp_2_temp);
  Serial.print("Temp 3: ");
  Serial.println(temp_3_temp);
  Serial.print("Temp 4: ");
  Serial.println(temp_4_temp);


  // Sensor sampling loop; Irradiance
  // Grab 100 samples; samples every 100us
  long irradiance_samples = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    // irradiance_samples += ads.readADC_SingleEnded(0);

    delayMicroseconds(100);
  }
  irad_1_cnts = irradiance_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  Serial.print("Irradiance: ");
  Serial.println(irad_1_cnts);

  // Sensor sampling loop; Temperature and Humidity
  // Grab 100 samples; samples every 100us
  float amb_hum_samples = 0;  
  float amb_temp_samples = 0;
  float amb_temp, amb_hum;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    // am2315.readTemperatureAndHumidity(&amb_temp, &amb_hum); // Read temp & humidity
    amb_temp_samples += amb_temp;
    amb_hum_samples += amb_hum;

    delayMicroseconds(100);
  }
  tmph_1_humd = amb_hum_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  tmph_1_temp = amb_temp_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  Serial.print("Ambient Temp: ");
  Serial.println(tmph_1_temp);
  Serial.println("Ambient Humidity: ");
  Serial.println(tmph_1_humd);



}

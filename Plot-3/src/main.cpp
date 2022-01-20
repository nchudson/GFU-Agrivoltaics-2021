//------------------------------------------------------------------------------
// GFU Agrivoltaics Plot 3 (Roof Panel) Monitor
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


// Pin Definitions
#define ONE_WIRE_PIN        (2)
#define SD_CS_PIN           (4)
#define RELAY_TRIG_PIN      (7)

// Sensor Parameters
#define TEMP_PRECISION      (12)

// Program Parameters
#define TIME_ZONE           (-7)
#define SECS_PER_HOUR       (3600)
#define NUM_SAMPLES         (20)
#define NTP_SYNC_INTERVAL   (600)

// Debug Parameters
#define FAIL_RESET
#define THINGSPEAK_DEBUG

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

// Network Variables
static const uint8_t mac[] = {MAC_3_BYTE_0, MAC_3_BYTE_1, MAC_3_BYTE_2,
                              MAC_3_BYTE_3, MAC_3_BYTE_4, MAC_3_BYTE_5};
static IPAddress     onedot(1, 1, 1, 1);
EthernetClient       client;
EthernetUDP          udp;
NTPClient            ntp(udp, TIME_ZONE * SECS_PER_HOUR);
const char*          plot_3_pv_api_key = PLOT_3_PV_API_KEY;
int32_t              thingspeak_response = 0;

// Sensor Objects
OneWire              oneWire(ONE_WIRE_PIN);
DallasTemperature    temp_sensors(&oneWire);
DeviceAddress        temp_5_addr = {TEMP_5_ADDR_0, TEMP_5_ADDR_1, TEMP_5_ADDR_2, TEMP_5_ADDR_3,
                                    TEMP_5_ADDR_4, TEMP_5_ADDR_5, TEMP_5_ADDR_6, TEMP_5_ADDR_7};
DeviceAddress        temp_6_addr = {TEMP_6_ADDR_0, TEMP_6_ADDR_1, TEMP_6_ADDR_2, TEMP_6_ADDR_3,
                                    TEMP_6_ADDR_4, TEMP_6_ADDR_5, TEMP_6_ADDR_6, TEMP_6_ADDR_7};
DeviceAddress        temp_7_addr = {TEMP_7_ADDR_0, TEMP_7_ADDR_1, TEMP_7_ADDR_2, TEMP_7_ADDR_3,
                                    TEMP_7_ADDR_4, TEMP_7_ADDR_5, TEMP_7_ADDR_6, TEMP_7_ADDR_7};

Adafruit_ADS1115     ads;

static const char    date_string[23];
static const char    file_name[12];

File                 log_file;

time_t               cur_time;
time_t               prev_time;

// Sensor Data
float                temp_5_temp;
float                temp_6_temp;
float                temp_7_temp;

int16_t              irad_2_wsqm;

//------------------------------------------------------------------------------
//      __   __   __  ___  __  ___      __   ___  __
//     |__) |__) /  \  |  /  \  |  \ / |__) |__  /__`
//     |    |  \ \__/  |  \__/  |   |  |    |___ .__/
//
//------------------------------------------------------------------------------

time_t get_ntp_time();
void   read_sensors();
bool   create_log_file();
void   system_reset();

//------------------------------------------------------------------------------
//      __        __          __
//     |__) |  | |__) |    | /  `
//     |    \__/ |__) |___ | \__,
//
//------------------------------------------------------------------------------

//==============================================================================
// Setup Routine
//==============================================================================
void setup() {
  // Basic system setup.
  Serial.begin(9600);
  wdt_enable(WDTO_4S);
  Ethernet.begin(mac);
  #ifdef ONEDOT
  Ethernet.setDnsServerIP(onedot);
  #endif
  Serial.println(Ethernet.localIP());
  Serial.println(Ethernet.linkStatus());
  wdt_reset();

  // Initialize ThingSpeak.
  ThingSpeak.begin(client);

  // Initialize NTP.
  udp.begin(2390);
  ntp.begin();
  ntp.update();
  setSyncProvider(get_ntp_time);
  setSyncInterval(NTP_SYNC_INTERVAL);
  cur_time = now();
  prev_time = now();
  Serial.println(ntp.getFormattedTime());
  wdt_reset();

  // Initialize sensors.
  temp_sensors.begin();
  Serial.println("Temp sensors initialized");
  Serial.print(temp_sensors.getDeviceCount());
  Serial.println(" sensors found");
  ads.begin();
  Serial.println("ADC initialized");

  // Initialize SD card.
  SD.begin(SD_CS_PIN);
  create_log_file();
  wdt_reset();
}

//==============================================================================
// Infinite Loop of Science!
//==============================================================================
void loop() {
  // Get current time.
  wdt_reset();
  prev_time = cur_time;
  cur_time = now();

  // If it the start of a new minute.
  if(minute(prev_time) != minute(cur_time)) {
    if(minute(cur_time) == 0) {
      create_log_file();
    }

    // Read system sensors.
    Serial.println("Reading sensors");
    read_sensors();
    wdt_reset();
    // Log new sensor data to SD card, getting current time first.
    Serial.println("Writing to card");
    time_t t = now();
    sprintf(date_string, "%04d-%02d-%02d %02d:%02d:%02d PDT",
      year(t), month(t), day(t), hour(t), minute(t), second(t));
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
    wdt_reset();

    // Before attempting to upload to ThingSpeak,
    // log status of internet connection.
    Serial.println(Ethernet.linkStatus());
    Serial.println(Ethernet.hardwareStatus());
    wdt_reset();

    // Set ThingSpeak PV fields.
    ThingSpeak.setField(TEMP_5_TEMP_FIELD, temp_5_temp);
    ThingSpeak.setField(TEMP_6_TEMP_FIELD, temp_6_temp);
    ThingSpeak.setField(TEMP_7_TEMP_FIELD, temp_7_temp);
    ThingSpeak.setField(IRAD_2_WSQM_FIELD, irad_2_wsqm);

    // Attempt ThingSpeak upload.
    Serial.println("Sending PV data to ThingSpeak");
    thingspeak_response = ThingSpeak.writeFields(
      PLOT_3_PV_CHANNEL, plot_3_pv_api_key);
    Serial.println(thingspeak_response);
    wdt_reset();

    #ifdef FAIL_RESET
      // Reset system if -301 error encountered
      if(thingspeak_response == THINGSPEAK_FAIL) system_reset();
    #endif

  }

  #ifdef THINGSPEAK_DEBUG
  // If the 30th second of the minute has just begun,
  // write to debug channel
  if(second(cur_time) == 30 && second(prev_time) == 29) {
    thingspeak_response = ThingSpeak.writeField(
      PLOT_3_DBG_CHANNEL, 1, 1, PLOT_3_DBG_API_KEY);

    #ifdef FAIL_RESET
      // Reset system if -301 error encountered
      if(thingspeak_response == THINGSPEAK_FAIL) system_reset();
    #endif
  }
  #endif

  // Maintain Ethernet connection.
  Ethernet.maintain();
}

// Provides the time library with the current real-world time
// We take care of giving it the time, it takes care of all the calculations with leap years and stuff
// Returns a time_t type... not sure what it is (time, presumably) just basing format off the example code
time_t get_ntp_time()
{
  ntp.update();
  // Return seconds since Jan. 1 1970, adjusted for time zone in seconds.
  return ntp.getEpochTime();
}

//==============================================================================
// Read Sensor Data
//==============================================================================
void read_sensors() {

  // Sensor sampling loop.
  // Irradiance.
  int32_t irad_samples = 0;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    irad_samples += ads.readADC_SingleEnded(0);

    delayMicroseconds(100);
  }
  // Report the average of the samples we gathered.
  irad_2_wsqm = (irad_samples < 0) ? 0 : irad_samples / NUM_SAMPLES;
  Serial.print("Irradiance ADC: ");
  Serial.println(irad_2_wsqm);
  // Convert ADC counts to W/m^2.
  irad_2_wsqm = (float)((-6E-10 * pow(irad_2_wsqm, 4)) +
    (2.7E-6 * pow(irad_2_wsqm, 3)) - (3.1E-3 * pow(irad_2_wsqm, 2)) +
    (1.1 * (double)irad_2_wsqm));
  Serial.print("Irradiance: ");
  Serial.println(irad_2_wsqm);

  // Sensor sampling loop.
  // DS18B20 temperature.
  float temp_samples_1 = 0;
  float temp_samples_2 = 0;
  float temp_samples_3 = 0;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    // Request temperatures from all sensors.
    temp_sensors.requestTemperatures();

    //Add to cumulative samples.
    temp_samples_1 += temp_sensors.getTempC(temp_5_addr);
    temp_samples_2 += temp_sensors.getTempC(temp_6_addr);
    temp_samples_3 += temp_sensors.getTempC(temp_7_addr);
    wdt_reset();
  }
  // Report the average of the samples we gathered.
  temp_5_temp = temp_samples_1 / NUM_SAMPLES;
  temp_6_temp = temp_samples_2 / NUM_SAMPLES;
  temp_7_temp = temp_samples_3 / NUM_SAMPLES;

  // Print DS18B20 temperatures.
  Serial.print("Temp 1: ");
  Serial.println(temp_5_temp);
  Serial.print("Temp 2: ");
  Serial.println(temp_6_temp);
  Serial.print("Temp 3: ");
  Serial.println(temp_7_temp);

}

//==============================================================================
// Manage log_file Creation
//==============================================================================
bool create_log_file() {
  // Local variables.
  time_t t;
  bool exists;
  bool created;

  t       = now();
  exists  = false;
  created = false;

  // Close the current file and create a new one.
  log_file.close();

  // Get the current time (MM-DD_HH) and use it as the file name,
  // first checking whether file already exists.
  sprintf(file_name, "%02d-%02d_%02d.log", month(t), day(t), hour(t));
  exists = SD.exists(file_name);
  log_file = SD.open(file_name, FILE_WRITE);

  // Print filename and failure state.
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
    created = true;
  }

  // If file did not already exist, initialize first line
  // with ThingSpeak CSV header.
  if(!exists) {
    log_file.println("created_at,entry_id,field1,field2,field3,field4");
  }
  return created;
}

//==============================================================================
// Reset System
//==============================================================================
void system_reset() {
  // Use watchdog timer and spin-wait to trigger reset.
  wdt_disable();
  Serial.println("//////////////////");
  Serial.println("// SYSTEM RESET //");
  Serial.println("//////////////////");
  delay(100);
  wdt_enable(WDTO_15MS);
  while(1) ;
}

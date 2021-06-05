//------------------------------------------------------------------------------
// GFU Agrivoltaics Plot 1 (Sun Plot) Monitor
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
#include <SDI12.h>
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

// ThingSpeak Environmental Fields
#define NUM_FIELDS_ENV      (7)
#define SOIL_0_VOLW_FIELD   (1)
#define SOIL_0_TEMP_FIELD   (2)
#define TEMP_0_TEMP_FIELD   (3)
#define TMPH_0_TEMP_FIELD   (4)
#define TMPH_0_HUMD_FIELD   (5)
#define IRAD_0_WSQM_FIELD   (6)
#define SOIL_2_SOWP_FIELD   (7)

// Pin Definitions
#define ONE_WIRE_PIN        (2)
#define SD_CS_PIN           (4)
#define RELAY_TRIG_PIN      (7)
#define SDI_12_PIN          (62)

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
static const uint8_t mac[] = {MAC_1_BYTE_0, MAC_1_BYTE_1, MAC_1_BYTE_2,
  MAC_1_BYTE_3, MAC_1_BYTE_4, MAC_1_BYTE_5};
static IPAddress onedot(1, 1, 1, 1);
EthernetClient client;
EthernetUDP udp;
NTPClient ntp(udp, TIME_ZONE * SECS_PER_HOUR);
static const char* plot_1_env_api_key = PLOT_1_ENV_API_KEY;
int32_t thingspeak_response = 0;

// Sensor Objects
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature temp_sensors(&oneWire);
DeviceAddress temp_0_addr = {TEMP_0_ADDR_0, TEMP_0_ADDR_1, TEMP_0_ADDR_2,
  TEMP_0_ADDR_3, TEMP_0_ADDR_4, TEMP_0_ADDR_5, TEMP_0_ADDR_6, TEMP_0_ADDR_7};

SDI12 sdi(SDI_12_PIN);

Adafruit_AM2315 am2315;
Adafruit_ADS1115 ads;

static const char date_string[23];
static const char file_name[12];

File log_file;

time_t cur_time;
time_t prev_time;

// Sensor Data
double soil_0_volw;
double soil_2_sowp;

float soil_0_temp;

float temp_0_temp;

float tmph_0_temp;
float tmph_0_humd;

int16_t irad_0_wsqm;

int16_t flow_0_tick;

//------------------------------------------------------------------------------
//      __   __   __  ___  __  ___      __   ___  __
//     |__) |__) /  \  |  /  \  |  \ / |__) |__  /__`
//     |    |  \ \__/  |  \__/  |   |  |    |___ .__/
//
//------------------------------------------------------------------------------

time_t get_ntp_time();
void read_sensors();
bool create_log_file();
bool teros_12_read(double *vwc, float *temp, uint16_t *conductivity);
bool teros_21_read(double *matric_potential, float *temp);
void system_reset();

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

  // Initialize internet connection.
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
  am2315.begin();
  Serial.println("Ambient temp sensor initialized");
  ads.begin();
  Serial.println("ADC initialized");
  sdi.begin();
  Serial.println("SDI-12 bus initialized");
  wdt_reset();

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
    log_file.print(soil_0_volw);
    log_file.print(",");
    log_file.print(soil_0_temp);
    log_file.print(",");
    log_file.print(temp_0_temp);
    log_file.print(",");
    log_file.print(tmph_0_temp);
    log_file.print(",");
    log_file.print(tmph_0_humd);
    log_file.print(",");
    log_file.print(irad_0_wsqm);
    log_file.print(",");
    log_file.println(soil_2_sowp);
    log_file.flush();
    wdt_reset();

    // Before attempting to upload to ThingSpeak,
    // log status of internet connection.
    Serial.println(Ethernet.linkStatus());
    Serial.println(Ethernet.hardwareStatus());
    wdt_reset();

    // If the current minute is a multiple of 10,
    // upload environmental data to ThingSpeak.
    if(minute(cur_time) % 10 == 0) {

      // Set ThingSpeak environmental fields.
      ThingSpeak.setField(SOIL_0_VOLW_FIELD, (float)soil_0_volw);
      ThingSpeak.setField(SOIL_0_TEMP_FIELD, soil_0_temp);
      ThingSpeak.setField(TEMP_0_TEMP_FIELD, temp_0_temp);
      ThingSpeak.setField(TMPH_0_TEMP_FIELD, tmph_0_temp);
      ThingSpeak.setField(TMPH_0_HUMD_FIELD, tmph_0_humd);
      ThingSpeak.setField(IRAD_0_WSQM_FIELD, irad_0_wsqm);
      ThingSpeak.setField(SOIL_2_SOWP_FIELD, (float)soil_2_sowp);

      // Attempt ThingSpeak upload.
      Serial.println("Sending environmental data to ThingSpeak");
      thingspeak_response = ThingSpeak.writeFields(
        PLOT_1_ENV_CHANNEL, plot_1_env_api_key);
      Serial.println(thingspeak_response);
      wdt_reset();

      #ifdef FAIL_RESET
        // Reset system if -301 error encountered
        if(thingspeak_response == THINGSPEAK_FAIL) system_reset();
      #endif
    }
  }

  // If the 30th second of the minute has just begun,
  // write to debug channel
  if(second(cur_time) == 30 && second(prev_time) == 29) {
    thingspeak_response = ThingSpeak.writeField(
      PLOT_1_DBG_CHANNEL, 1, 1, PLOT_1_DBG_API_KEY);

    #ifdef FAIL_RESET
      // Reset system if -301 error encountered
      if(thingspeak_response == THINGSPEAK_FAIL) system_reset();
    #endif
  }

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
  int32_t irradiance_samples = 0;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    irradiance_samples += ads.readADC_SingleEnded(0);
    delayMicroseconds(100);
  }

  // Report the average of the samples we gathered.
  irad_0_wsqm = irradiance_samples / NUM_SAMPLES;
  // irad_0_wsqm = (0.7423 * irad_0_wsqm) - 370.54; // Convert ADC counts to W/m^2.
  Serial.print("Irradiance: ");
  Serial.println(irad_0_wsqm);

  // Sensor sampling.
  // Soil stuff.
  // Only use one sample for these because they're fancy
  uint16_t conductivity;
  float temp_12, temp_21;
  double vwc_counts, matric_potential;
  wdt_reset();

  // Read from TEROS 12.
  if(teros_12_read(&vwc_counts, &temp_12, &conductivity)) {
    // Convert ADC counts to volumetric water content using Equation 6 from
    // TEROS 12 user manual 4.1.1.
    soil_0_volw = (0.0003879 * vwc_counts) - 0.6956;
    soil_0_temp = temp_12;

    // Print soil VWC and temperature.
    Serial.print("Soil VWC: ");
    Serial.println(soil_0_volw);
    Serial.print("Soil Temp: ");
    Serial.println(soil_0_temp);
  }
  else {
    Serial.println("TEROS-12 Error!");
  }
  wdt_reset();

  // Read from TEROS 21
  if(teros_21_read(&matric_potential, &temp_21)) {
    soil_2_sowp = matric_potential;
    Serial.print("Soil Matric Potential: ");
    Serial.println(soil_2_sowp);
  }
  else {
    Serial.println("TEROS-21 Error!");
  }
  wdt_reset();

  // Sensor sampling loop.
  // Ambient temperature and humidity.
  float amb_hum_samples = 0;
  float amb_temp_samples = 0;
  float amb_temp, amb_hum;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++)
  {
    am2315.readTemperatureAndHumidity(&amb_temp, &amb_hum);
    amb_temp_samples += amb_temp;
    amb_hum_samples += amb_hum;
    wdt_reset();
    delayMicroseconds(100);
  }
  // Report the average of the samples we gathered.
  tmph_0_humd = amb_hum_samples / NUM_SAMPLES;
  tmph_0_temp = amb_temp_samples / NUM_SAMPLES;

  // Print ambient temperature and humidity.
  Serial.print("Ambient Temp: ");
  Serial.println(tmph_0_temp);
  Serial.print("Ambient Humidity: ");
  Serial.println(tmph_0_humd);

  // Sensor sampling loop.
  // DS18B20 temperature.
  float temp_samples_1 = 0;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    // Request temperatures from all sensors.
    temp_sensors.requestTemperatures();

    //Add to cumulative samples.
    temp_samples_1 += temp_sensors.getTempC(temp_0_addr);
    wdt_reset();
  }
  // Report the average of the samples we gathered.
  temp_0_temp = temp_samples_1 / NUM_SAMPLES;

  // Print DS18B20 temperatures.
  Serial.print("Temp 0: ");
  Serial.println(temp_0_temp);
}

//==============================================================================
// Manage log_file Creation
//==============================================================================
bool create_log_file() {
  // Local variables.
  time_t t = now();
  bool exists = false;
  bool created = false;

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
    log_file.print("created_at,entry_id,field1,field2,field3,");
    log_file.println("field4,field5,field6,field7");
  }
  return created;
}

//==============================================================================
// Read TEROS 12 Data
//==============================================================================
bool teros_12_read(double *vwc_counts, float *temp, uint16_t *conductivity) {
  // Local variables.
  static char* input = (char*)malloc(25 * sizeof(char));
  char* temp_str = NULL;
  char* vwc_str = NULL;
  char* cond_str = NULL;
  size_t str_size;
  bool valid = false;
  bool temp_neg = true;

  // Clear SDI buffer and send measure command.
  sdi.clearBuffer();
  sdi.sendCommand("0M!");
  delay(100);

  // If read command succeded...
  if(sdi.available() > 5) {
    // Wait remainder of specified 1s, clear buffer,
    // and send read command.
    delay(900);
    sdi.clearBuffer();
    sdi.sendCommand("0D0!");

    // Spin wait until response is available.
    while(sdi.available() < 10) ;

    // Read response from sensor, terminate with null character,
    // and advance starting pointer past prepended null characters.
    str_size = sdi.readBytesUntil('\n', input, 25);
    input[str_size] = 0;
    while(*input == 0) {
      input++;
      str_size--;
    }

    // Get pointers to delimiters within response string.
    temp_str = strchr(input, '-');
    vwc_str = strchr(input, '+');
    cond_str = strrchr(input, '+');

    // If valid VWC and conductiity delimeters are found,
    // input is most likely valid.
    if(vwc_str && cond_str) {
      valid = true;
      // Replace delimeters with null characters and advance pointers.
      vwc_str++;
      *cond_str = 0;
      cond_str++;

      // If valid temperature delimeter was not found, temperature is positive.
      if(!temp_str) {
        // Find positive delimeter for temperature.
        temp_str = strchr(vwc_str, '+');
        temp_neg = false;
      }

      // Replace temperature delimeter with null character and advance pointer.
      *temp_str = 0;
      temp_str++;

      // Convert response substrings to numerical values and store
      // at specified addresses.
      *vwc_counts = atof(vwc_str);
      *temp = temp_neg ? atof(temp_str) * -1 : atof(temp_str);
      *conductivity = atoi(cond_str);
    }
  }

  // Clear SDI buffer once again, just for good measure.
  sdi.clearBuffer();
  return valid;
}

//==============================================================================
// Read TEROS 21 Data
//==============================================================================
bool teros_21_read(double *matric_potential, float *temp) {
  // Local variables.
  static char* input = (char*)malloc(25 * sizeof(char));
  char* mtc_pot_str = NULL;
  char* temp_str = NULL;
  size_t str_size;
  bool valid = false;
  bool temp_neg = false;

  // Clear SDI buffer and send measure command.
  sdi.clearBuffer();
  sdi.sendCommand("1M!");
  delay(100);

  // If read command succeeded...
  if(sdi.available() > 5) {
    // Wait remainder of specified 1s, clear buffer,
    // and send read command.
    delay(900);
    sdi.clearBuffer();
    sdi.sendCommand("1D0!");

    // Spin wait until response is available.
    while(sdi.available() < 10) ;

    // Read response from sensor, terminate with null character,
    // and advance starting pointer past prepended null characters.
    str_size = sdi.readBytesUntil('\n', input, 25);
    input[str_size] = 0;
    while(*input == 0) {
      input++;
      str_size--;
    }

    // Get pointers to delimiters within response string.
    mtc_pot_str = strchr(input, '-');
    temp_str = strchr(input, '+');


    // If valid matric potential delimeter is found,
    // input is most likely valid.
    if(mtc_pot_str) {
      valid = true;
      // Advance pointer to matric potential.
      mtc_pot_str++;

      // If valid temperature delimeter was not found, temperature is negative.
      if(!temp_str) {
        temp_str = strrchr(input, '-');
        temp_neg = true;
      }

      // Replace temperature delimeter with null character and advance pointer.
      *temp_str = 0;
      temp_str++;

      // Convert response substrings to numerical values and store
      // at specified addresses.
      *matric_potential = atof(mtc_pot_str) * -1;
      *temp = temp_neg? atof(temp_str) * -1 : atof(temp_str);
    }
  }

  // Clear SDI buffer once again, just for good measure.
  sdi.clearBuffer();
  return valid;
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
  wdt_enable(WDTO_15MS);
  while(1) ;
}

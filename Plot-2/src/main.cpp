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
#define SOIL_1_VOLW_FIELD   (1)
#define SOIL_1_TEMP_FIELD   (2)
#define TEMP_1_TEMP_FIELD   (3)
#define TMPH_1_TEMP_FIELD   (4)
#define TMPH_1_HUMD_FIELD   (5)
#define IRAD_1_WSQM_FIELD   (6)
#define SOIL_3_SOWP_FIELD   (7)

// ThingSpeak PV Fields
#define NUM_FIELDS_PV       (3)
#define TEMP_2_TEMP_FIELD   (1)
#define TEMP_3_TEMP_FIELD   (2)
#define TEMP_4_TEMP_FIELD   (3)

// Pin definitions
#define ONE_WIRE_PIN        (2)
#define SD_CS_PIN           (4)
#define RELAY_TRIG_PIN      (7)
#define SDI_12_PIN          (62)

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

static const uint8_t mac[] = {MAC_2_BYTE_0, MAC_2_BYTE_1, MAC_2_BYTE_2,
  MAC_2_BYTE_3, MAC_2_BYTE_4, MAC_2_BYTE_5};
static IPAddress onedot(1, 1, 1, 1);
EthernetClient client;
EthernetUDP udp;
NTPClient ntp(udp, TIME_ZONE * SECS_PER_HOUR);
int32_t thingspeak_response = 0;
const char* plot_2_env_api_key = PLOT_2_ENV_API_KEY;
const char* plot_2_pv_api_key = PLOT_2_PV_API_KEY;
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
SDI12 sdi(SDI_12_PIN);

Adafruit_AM2315 am2315;
Adafruit_ADS1115 ads;

static const char date_string[23];
static const char file_name[12];

File log_file;

time_t cur_time;
time_t prev_time;

// Sensor data
double soil_1_volw;
double soil_3_sowp;

float soil_1_temp;

float temp_1_temp;

float tmph_1_temp;
float tmph_1_humd;

int16_t irad_1_wsqm;

float flow_1_tick;

float temp_2_temp;
float temp_3_temp;
float temp_4_temp;

//------------------------------------------------------------------------------
//      __   __   __  ___  __  ___      __   ___  __
//     |__) |__) /  \  |  /  \  |  \ / |__) |__  /__`
//     |    |  \ \__/  |  \__/  |   |  |    |___ .__/
//
//------------------------------------------------------------------------------

time_t get_ntp_time();
void read_sensors();
void create_log_file();
bool teros_12_read(double *vwc, float *temp, uint16_t *conductivity);
bool teros_21_read(double *matric_potential, float *temp);
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
  Ethernet.setDnsServerIP(onedot);
  wdt_reset();
  Serial.println(Ethernet.localIP());
  ThingSpeak.begin(client);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  udp.begin(2390);
  ntp.begin();
  ntp.update();
  setSyncProvider(get_ntp_time);
  setSyncInterval(NTP_SYNC_INTERVAL);

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

    if(minute(cur_time) == 0 && (hour(cur_time) >= 9 && hour(cur_time) <= 17)) {
      Serial.println("Enabling loads");
      digitalWrite(RELAY_TRIG_PIN, LOW);
      delay(20);
      digitalWrite(RELAY_TRIG_PIN, HIGH);
      delay(20);
      digitalWrite(RELAY_TRIG_PIN, LOW);
      delay(20);
      digitalWrite(RELAY_TRIG_PIN, HIGH);
    }
    Serial.println("Reading sensors");
    read_sensors();
    wdt_reset();
    Serial.println(Ethernet.linkStatus());
    Serial.println(Ethernet.hardwareStatus());

    if(minute(cur_time) % 10 == 0) {
      Serial.println("Sending environmental data to ThingSpeak");
      thingspeak_response = 0;
      ThingSpeak.setField(SOIL_1_VOLW_FIELD, (float)soil_1_volw);
      ThingSpeak.setField(SOIL_1_TEMP_FIELD, soil_1_temp);
      ThingSpeak.setField(TEMP_1_TEMP_FIELD, temp_1_temp);
      ThingSpeak.setField(TMPH_1_TEMP_FIELD, tmph_1_temp);
      ThingSpeak.setField(TMPH_1_HUMD_FIELD, tmph_1_humd);
      ThingSpeak.setField(IRAD_1_WSQM_FIELD, irad_1_wsqm);
      ThingSpeak.setField(SOIL_3_SOWP_FIELD, (float)soil_3_sowp);
      thingspeak_response = ThingSpeak.writeFields(PLOT_2_ENV_CHANNEL, plot_2_env_api_key);
      Serial.println(thingspeak_response);
      if(thingspeak_response == THINGSPEAK_FAIL) system_reset();
    }

    Serial.println("Sending PV data to ThingSpeak");
    thingspeak_response = 0;
    ThingSpeak.setField(TEMP_2_TEMP_FIELD, temp_2_temp);
    ThingSpeak.setField(TEMP_3_TEMP_FIELD, temp_3_temp);
    ThingSpeak.setField(TEMP_4_TEMP_FIELD, temp_4_temp);
    thingspeak_response = ThingSpeak.writeFields(PLOT_2_PV_CHANNEL, plot_2_pv_api_key);
    Serial.println(thingspeak_response);
    if(thingspeak_response == THINGSPEAK_FAIL) system_reset();


    time_t t = now();
    sprintf(date_string, "%04d-%02d-%02d %02d:%02d:%02d PDT", year(t), month(t), day(t), hour(t), minute(t), second(t));
    log_file.print(date_string);
    log_file.print(",");
    log_file.print(soil_1_volw);
    log_file.print(",");
    log_file.print(soil_1_temp);
    log_file.print(",");
    log_file.print(temp_1_temp);
    log_file.print(",");
    log_file.print(tmph_1_temp);
    log_file.print(",");
    log_file.print(tmph_1_humd);
    log_file.print(",");
    log_file.print(irad_1_wsqm);
    log_file.print(",");
    log_file.print(soil_3_sowp);
    log_file.print(",");
    log_file.print(temp_2_temp);
    log_file.print(",");
    log_file.print(temp_3_temp);
    log_file.print(",");
    log_file.println(temp_4_temp);
    log_file.flush();
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
  irad_1_wsqm = irradiance_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  // irad_1_wsqm = (0.6433 * irad_1_wsqm) - 341.17; // Convert ADC counts to W/m^2.
  Serial.print("Irradiance: ");
  Serial.println(irad_1_wsqm);

  // Sensor sampling; Soil stuff
  // Only use one sample for these because they're fancy
  uint16_t conductivity;
  float temp_12, temp_21;
  double vwc_counts, matric_potential;
  if(teros_12_read(&vwc_counts, &temp_12, &conductivity)) {
    soil_1_volw = (0.0003879 * vwc_counts) - 0.6956;
    soil_1_temp = temp_12;
    Serial.print("Soil VWC: ");
    Serial.println(soil_1_volw);
    Serial.print("Soil Temp: ");
    Serial.println(soil_1_temp);
  }
  else {
    Serial.println("TEROS-12 Error!");
  }
  wdt_reset();

  if(teros_21_read(&matric_potential, &temp_21)) {
    soil_3_sowp = matric_potential;
    Serial.print("Soil Matric Potential: ");
    Serial.println(soil_3_sowp);
  }
  else {
    Serial.println("TEROS-21 Error!");
  }
  wdt_reset();

  // Sensor sampling loop; Temperature and Humidity
  // Grab 100 samples; samples every 100us
  float amb_hum_samples = 0;  
  float amb_temp_samples = 0;
  float amb_temp, amb_hum;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    am2315.readTemperatureAndHumidity(&amb_temp, &amb_hum); // Read temp & humidity
    amb_temp_samples += amb_temp;
    amb_hum_samples += amb_hum;
    wdt_reset();
    delayMicroseconds(100);
  }
  tmph_1_humd = amb_hum_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  tmph_1_temp = amb_temp_samples / NUM_SAMPLES; // Report the average of the samples we gathered
  Serial.print("Ambient Temp: ");
  Serial.println(tmph_1_temp);
  Serial.print("Ambient Humidity: ");
  Serial.println(tmph_1_humd);

  // Sensor sampling loop; Temperature
  // Grab 100 samples; samples every 100us
  float temp_samples_1 = 0;
  float temp_samples_2 = 0;
  float temp_samples_3 = 0;
  float temp_samples_4 = 0;
  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    temp_sensors.requestTemperatures();
    temp_samples_1 += temp_sensors.getTempC(temp_1_addr);
    temp_samples_2 += temp_sensors.getTempC(temp_2_addr);
    temp_samples_3 += temp_sensors.getTempC(temp_3_addr);
    temp_samples_4 += temp_sensors.getTempC(temp_4_addr);
    wdt_reset();
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
  log_file.println("created_at,entry_id,field1,field2,field3,field4,field5,field6,field7,field1,field2,field3");
}

bool teros_12_read(double *vwc_counts, float *temp, uint16_t *conductivity) {
  bool valid = false;
  bool temp_neg = true;
  static char* input = (char*)malloc(25 * sizeof(char));
  sdi.clearBuffer();
  sdi.sendCommand("0M!");
  delay(100);
  if(sdi.available() > 5) {
    delay(900);
    sdi.clearBuffer();
    sdi.sendCommand("0D0!");
    while(sdi.available() < 10) ;
    size_t str_size = sdi.readBytesUntil('\n', input, 25);
    input[str_size] = 0;
    while(*input == 0) {
      input++;
      str_size--;
    }
    char* temp_str = strchr(input, '-');
    char* vwc_str = strchr(input, '+');
    char* cond_str = strrchr(input, '+');
    if(vwc_str && cond_str) {
      valid = true;
      vwc_str++;
      *cond_str = 0;
      cond_str++;
      if(!temp_str) {
        temp_str = strchr(vwc_str, '+');
        temp_neg = false;
      }
      *temp_str = 0;
      temp_str++;
      *vwc_counts = atof(vwc_str);
      *temp = temp_neg? atof(temp_str) * -1 : atof(temp_str);
      *conductivity = atoi(cond_str);
    }
  }
  sdi.clearBuffer();
  return valid;
}

bool teros_21_read(double *matric_potential, float *temp) {
  bool valid = false;
  bool temp_neg = false;
  static char* input = (char*)malloc(25 * sizeof(char));
  sdi.clearBuffer();
  sdi.sendCommand("1M!");
  delay(100);
  if(sdi.available() > 5) {
    delay(900);
    sdi.clearBuffer();
    sdi.sendCommand("1D0!");
    while(sdi.available() < 10) ;
    size_t str_size = sdi.readBytesUntil('\n', input, 25);
    input[str_size] = 0;
    while(*input == 0) {
      input++;
      str_size--;
    }
    char* mtc_pot_str = strchr(input, '-');
    char* temp_str = strchr(input, '+');
    if(mtc_pot_str) {
      valid = true;
      mtc_pot_str++;
      if(!temp_str) {
        temp_str = strrchr(input, '-');
        temp_neg = true;
      }
      *temp_str = 0;
      temp_str++;
      *matric_potential = atof(mtc_pot_str) * -1;
      *temp = temp_neg? atof(temp_str) * -1 : atof(temp_str);
    }
  }
  sdi.clearBuffer();
  return valid;
}

void system_reset() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while(1) ;
}
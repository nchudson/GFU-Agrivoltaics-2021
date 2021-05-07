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
#include "secrets.h"

//------------------------------------------------------------------------------
//      __   ___  ___         ___  __
//     |  \ |__  |__  | |\ | |__  /__`
//     |__/ |___ |    | | \| |___ .__/
//
//------------------------------------------------------------------------------

#define SOIL_0_VOLW_FIELD  1
#define SOIL_2_SOWP_FIELD  2
#define TEMP_0_TEMP_FIELD  3
#define TMPH_0_TEMP_FIELD  4
#define TMPH_0_HUMD_FIELD  5
#define IRAD_0_CNTS_FIELD  6
#define FLOW_0_TICK_FIELD  7

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

static const uint8_t mac[] = {MAC_BYTE_0, MAC_BYTE_1, MAC_BYTE_2,
  MAC_BYTE_3, MAC_BYTE_4, MAC_BYTE_5};
static const uint32_t channel_id = CHANNEL_PLOT_1;
static const char* api_key = API_KEY;
EthernetClient client;
int32_t thingspeak_response = 0;
uint16_t num_tries = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Ethernet.begin(mac);

}

void loop() {
  // put your main code here, to run repeatedly:

}
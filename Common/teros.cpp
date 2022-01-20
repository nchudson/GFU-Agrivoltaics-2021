//------------------------------------------------------------------------------
// METER TEROS-12 and TEROS-21 Driver
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

#include "teros.h"

//------------------------------------------------------------------------------
//      __   ___  ___         ___  __
//     |  \ |__  |__  | |\ | |__  /__`
//     |__/ |___ |    | | \| |___ .__/
//
//------------------------------------------------------------------------------

#define MEASURE_LEN    (4)
#define READ_LEN       (5)
#define INPUT_BUF_LEN  (25)
#define MIN_MEAS_LEN   (5)
#define MIN_READ_LEN   (25)

#define REQUEST_WAIT     (100)
#define MEASURE_WAIT     (900)

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

static char* teros_12_measure_cmd = (char*)malloc(MEASURE_LEN * sizeof(char));
static char* teros_12_read_cmd = (char*)malloc(READ_LEN * sizeof(char));

static char* teros_21_measure_cmd = (char*)malloc(MEASURE_LEN * sizeof(char));
static char* teros_21_read_cmd = (char*)malloc(READ_LEN * sizeof(char));

static SDI12* sdi; 


//------------------------------------------------------------------------------
//      __   __   __  ___  __  ___      __   ___  __
//     |__) |__) /  \  |  /  \  |  \ / |__) |__  /__`
//     |    |  \ \__/  |  \__/  |   |  |    |___ .__/
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//      __        __          __
//     |__) |  | |__) |    | /  `
//     |    \__/ |__) |___ | \__,
//
//------------------------------------------------------------------------------

//==============================================================================
// Initialize TEROS Sensors
//==============================================================================
void teros_init(uint8_t sdi_pin, uint8_t teros_12_addr, uint8_t teros_21_addr) {
  // Initialize command strings.
  sprintf(teros_12_measure_cmd, "%dM!", teros_12_addr);
  sprintf(teros_12_read_cmd, "%dD0!", teros_12_addr);
  sprintf(teros_21_measure_cmd, "%dM!", teros_12_addr);
  sprintf(teros_21_read_cmd, "%dD0!", teros_12_addr);

  // Setup SDI-12 bus.
  sdi = new SDI12(sdi_pin);
  sdi.begin();
}

//==============================================================================
// Read TEROS 12 Data
//==============================================================================
bool teros_12_read(double *vwc_counts, float *temp, uint16_t *conductivity) {
  // Local variables.
  static char* input = (char*)malloc(INPUT_BUF_LEN * sizeof(char));
  char* temp_str = NULL;
  char* vwc_str = NULL;
  char* cond_str = NULL;
  size_t str_size;
  bool valid = false;
  bool temp_neg = true;

  // Clear SDI buffer and send measure command.
  sdi.clearBuffer();
  sdi.sendCommand(teros_12_measure_cmd);
  delay(100);

  // If measure command succeeded...
  if(sdi.available() > MIN_MEAS_LEN) {
    // Wait remainder of specified 1s, clear buffer,
    // and send read command.
    delay(900);
    sdi.clearBuffer();
    sdi.sendCommand(teros_12_read_cmd);

    // Spin wait until response is available.
    while(sdi.available() < MIN_READ_LEN) ;

    // Read response from sensor, terminate with null character,
    // and advance starting pointer past prepended null characters.
    str_size = sdi.readBytesUntil('\n', input, INPUT_BUF_LEN);
    input[str_size] = 0;
    while(*input == 0) {
      input++;
      str_size--;
    }

    // Get pointers to delimiters within response string.
    temp_str = strchr(input, '-');
    vwc_str = strchr(input, '+');
    cond_str = strrchr(input, '+');

    // If valid VWC and conductivity delimeters are found,
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
  static char* input = (char*)malloc(INPUT_BUF_LEN * sizeof(char));
  char* mtc_pot_str = NULL;
  char* temp_str = NULL;
  size_t str_size;
  bool valid = false;
  bool temp_neg = false;

  // Clear SDI buffer and send measure command.
  sdi.clearBuffer();
  sdi.sendCommand(teros_21_measure_cmd);
  delay(100);

  // If measure command succeeded...
  if(sdi.available() > MIN_MEAS_LEN) {
    // Wait remainder of specified 1s, clear buffer,
    // and send read command.
    delay(900);
    sdi.clearBuffer();
    sdi.sendCommand(teros_21_read_cmd);

    // Spin wait until response is available.
    while(sdi.available() < MIN_MEAS_LEN) ;

    // Read response from sensor, terminate with null character,
    // and advance starting pointer past prepended null characters.
    str_size = sdi.readBytesUntil('\n', input, MIN_READ_LEN);
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
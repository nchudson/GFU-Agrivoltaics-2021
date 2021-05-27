#include <Arduino.h>
#include <SDI12.h>

#define DATA_PIN   (11)
#define POWER_PIN  (-1)

SDI12 sdi(DATA_PIN);

bool teros_12_read(double *vwc, float *temp, uint16_t *conductivity);
bool teros_21_read(double *matric_potential, float *temp);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Opening SDI-12 bus");
  sdi.begin();
  Serial.println("Opening successful");
}

void loop() {
  // put your main code here, to run repeatedly:
  uint16_t conductivity;
  float temp;
  double vwc, matric_potential;
  if(teros_12_read(&vwc, &temp, &conductivity)) {
    Serial.print(vwc);
    Serial.print(", ");
    Serial.print(temp);
    Serial.print(", ");
    Serial.println(conductivity);
  }

  if(teros_21_read(&matric_potential, &temp)) {
    Serial.print(matric_potential);
    Serial.print(", ");
    Serial.println(temp);
  }
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
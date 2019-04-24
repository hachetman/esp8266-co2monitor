#include <ESP8266WiFi.h>
#include "settings.h"

#define IDX_CMD 0
#define IDX_MSB 1
#define IDX_LSB 2
#define IDX_CHECKSUM 3
#define IDX_END 4

#define CMD_TEMPERATURE 0x42
#define CMD_CO2_MEASUREMENT 0x50

WiFiClient wifiClient;

uint8_t bitIndex = 0;
uint8_t byteIndex = 0;
uint8_t clkValue = LOW;
uint8_t lastClkValue = LOW;

uint8_t tmp = 0;
unsigned long currentMillis = 0;
unsigned long lastMillis = 0;
unsigned long lastUpdateMs = 0;

uint16_t co2Measurement = 0;
float smoothCo2Measurement = 0.0;

float temperature = 0;

byte bits[8];
byte bytes[5] = {0};

char sprintfHelper[16] = {0};

void setup() {

  // Power up wait
  delay(2000);
  
  Serial.begin(115200);
  Serial.setTimeout(2000);
  Serial.println("Hello");

  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  pinMode(PIN_CLK, INPUT);
  pinMode(PIN_DATA, INPUT);

  attachInterrupt(PIN_CLK, onClock, RISING);
}

void onClock() {
  lastMillis = millis();
  bits[bitIndex++] = (digitalRead(PIN_DATA) == HIGH) ? 1 : 0;

  // Transform bits to byte
  if (bitIndex >= 8) {
    tmp = 0;
    for (uint8_t i = 0; i < 8; i++) {
      tmp |= (bits[i] << (7 - i));
    }

    bytes[byteIndex++] = tmp;
    bitIndex = 0;
  }

  if (byteIndex >= 5) {
    byteIndex = 0;
    decodeDataPackage(bytes);
  }
}


void loop() {
  currentMillis = millis();

  // Over 50ms no bits? Reset!
  if (currentMillis - lastMillis > 50) {
    bitIndex = 0;
    byteIndex = 0;
  }


  // If the change is above a specific threshold, we update faster!
  float percentChange = abs(((float) co2Measurement / smoothCo2Measurement) - 1.0);
	if (co2Measurement > 0) {
		sprintf(sprintfHelper, "%d", co2Measurement);
	}

	if (temperature > 0) {
		dtostrf(temperature, 4, 2, sprintfHelper);
	}


}

bool decodeDataPackage(byte data[5]) {

  if (data[IDX_END] != 0x0D) {
    return false;
  }

  uint8_t checksum = data[IDX_CMD] + data[IDX_MSB] + data[IDX_LSB];
  if (data[IDX_CHECKSUM] != checksum) {
    return false;
  }

  switch (data[IDX_CMD]) {
	case CMD_CO2_MEASUREMENT:
		co2Measurement = (data[IDX_MSB] << 8) | data[IDX_LSB];

		// Exponential smoothing
		smoothCo2Measurement = EXP_SMOOTH_ALPHA * (float) co2Measurement + (1.0 - EXP_SMOOTH_ALPHA) * smoothCo2Measurement;

		Serial.print("CO2: ");
		Serial.println(co2Measurement);
		break;
	case CMD_TEMPERATURE:
		temperature = ((data[IDX_MSB] << 8) | data[IDX_LSB]) / 16.0 - 273.15;
		Serial.print("Temp: ");
		Serial.println(temperature);
		break;
  }

}


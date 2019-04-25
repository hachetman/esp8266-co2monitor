#include <ESP8266WiFi.h>
#include "settings.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

#define IDX_CMD 0
#define IDX_MSB 1
#define IDX_LSB 2
#define IDX_CHECKSUM 3
#define IDX_END 4

#define CMD_TEMPERATURE 0x42
#define CMD_CO2_MEASUREMENT 0x50

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


// MQTTS related:
BearSSL::WiFiClientSecure secureClient;
PubSubClient mqttclient;
void callback(char* topic, byte* payload, unsigned int length) {Serial.println("Callback\n");}
// Letsencrypt root ca
const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow
SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT
GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF
q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8
SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0
Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA
a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj
/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T
AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG
CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv
bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k
c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw
VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC
ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz
MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu
Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF
AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo
uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/
wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu
X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG
PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6
KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==
-----END CERTIFICATE-----
)EOF";
BearSSL::X509List x509(ca_cert);

void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
void setup() {
  int success;
  // Power up wait
  delay(2000);

  Serial.begin(115200);
  Serial.setTimeout(2000);
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  secureClient.setTrustAnchors(&x509);
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  setClock();
	mqttclient = PubSubClient(mqtt_server, 8883, callback, secureClient);
	success = mqttclient.connect(sensor_name, mqtt_name, mqtt_password);

	if (success) {
		Serial.println("Connected to MQTT");
	}
	else {
		Serial.print("failed, rc=");
		Serial.println(mqttclient.state());
		Serial.print("Going to Sleep ZZZZZ");
		delay(30);
		ESP.deepSleep(600e6);
	}
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


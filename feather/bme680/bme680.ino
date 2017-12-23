/**
 * Generate metrics from BME680 and send them to telegraf via udp to be forwarded to influxdb
 */
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "common/common.h"

const char* ssid     = "<sid>";
const char* password = "<pass>";

char hostString[16] = {0};

WiFiUDP udp;
Adafruit_BME680 bme;

void setupBME() {
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms  
}

void setup() {
  Serial.begin(115200);
  setupWIFI(hostString, ssid, password);

  if (!MDNS.begin(hostString)) {
    Serial.println("Error setting up MDNS responder!");
  }

  while(!searchInflux()) {
    Serial.println("No Influx Service found");
    delay(500);
  }
  Serial.print(MDNS.hostname(0));
  Serial.print(" (");
  Serial.print(MDNS.IP(0));
  Serial.print(":");
  Serial.print(MDNS.port(0));
  Serial.println(")");

  setupBME();
}

void loop() {
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }

  sendUDPLine(udp, influxString(String(hostString), "ambient_celcius", String(bme.temperature,2)));
  sendUDPLine(udp, influxString(String(hostString), "ambient_humidity", String(bme.humidity,2)));  
  sendUDPLine(udp, influxString(String(hostString), "ambient_pressure", String(bme.pressure / 100.0,2)));
  sendUDPLine(udp, influxString(String(hostString), "ambient_gas_resistance", String(bme.gas_resistance,0)+"i"));
  
  Serial.println();
  
  delay(60000);
}

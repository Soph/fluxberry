#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include "common/common.h"

WiFiUDP udp;
Adafruit_MAX31865 max1 = Adafruit_MAX31865(4);
Adafruit_MAX31865 max2 = Adafruit_MAX31865(5);

const char* ssid     = "<sid>";
const char* password = "<pass>";

char hostString[16] = {0};

// The value of the Rref resistor. Use 430.0!
#define RREF 430.0
  
void setupMax() {
  max1.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  max2.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary  
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

  setupMax();
}

void printFault(Adafruit_MAX31865 max) {
  uint8_t fault = max.readFault();
  if (fault) {
    Serial.print("Fault 0x"); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println("RTD High Threshold"); 
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println("RTD Low Threshold"); 
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println("REFIN- > 0.85 x Bias"); 
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println("REFIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println("RTDIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println("Under/Over voltage"); 
    }
    max.clearFault();
  }
}

void printInfo(uint16_t rtd) {
  Serial.print("RTD value: "); Serial.println(rtd);
  float ratio = rtd;
  ratio /= 32768;
  Serial.print("Ratio = "); Serial.println(ratio,8);
  Serial.print("Resistance = "); Serial.println(RREF*ratio,8);
}

void loop() {
  printInfo(max1.readRTD());
  printFault(max1);
  sendUDPLine(udp, influxString(String(hostString), "temperature,number=1", String(max1.temperature(100, RREF))));

  printInfo(max2.readRTD());
  printFault(max2);  
  sendUDPLine(udp, influxString(String(hostString), "temperature,number=2", String(max2.temperature(100, RREF))));
  
  Serial.println();
  delay(60000);
}
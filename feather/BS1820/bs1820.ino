#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "common/common.h"

#define TEMPERATURE_PRECISION 12

const char* ssid     = "<sid>";
const char* password = "<password>";
char hostString[16] = {0};
WiFiUDP udp;
 
OneWire  ds(D4);  // on pin D4 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);
 
void setup(void) 
{
  Serial.begin(9600);
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

  sensors.begin();

  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDS18Count(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
}

void initializeSensors()
{
  DeviceAddress address;
  for (uint8_t i = 0; i < sensors.getDS18Count(); i++) 
  {
    if (sensors.getAddress(address, i)) 
    {
      sensors.setResolution(address, TEMPERATURE_PRECISION);
    }
  }
}

String printableAddress(DeviceAddress deviceAddress)
{
  String hexAddress = "";
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) 
    {
      hexAddress = hexAddress + "0";
    }
    hexAddress = hexAddress + String(deviceAddress[i], HEX);
  }
  return hexAddress;
}

String influxStringWithAddress(String host, String key, String value, String address) {
  return String(key + ",host=" + host + ",address="+address+" value="+value);
}

void loop(void) 
{
  initializeSensors();
  DeviceAddress address;

  sensors.requestTemperatures();

  delay(1000);

  for (uint8_t i = 0; i < sensors.getDS18Count(); i++) {
    if (sensors.getAddress(address, i)) 
    {
      float tempC = sensors.getTempC(address);
      sendUDPLine(udp, influxStringWithAddress(String(hostString), "ambient_celcius", String(tempC,2), printableAddress(address)));
    }
  }
  delay(60000);
}
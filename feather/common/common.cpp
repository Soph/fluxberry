#include "common.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#define HOSTNAME "ESP_%06X"

void setupWIFI(char* hostString, char const* ssid, char const* password) {
  sprintf(hostString, HOSTNAME, ESP.getChipId());
  WiFi.hostname(hostString);

  Serial.print("Hostname: ");
  Serial.println(hostString);
      
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

bool searchInflux() {
  int n = MDNS.queryService("influx", "udp"); // Send out query for esp tcp services  
  if (n == 0) {
    Serial.println("no services found");
    return false;
  }
  else {
    Serial.print(n);
    Serial.println(" service(s) found");
    return true;
  }  
}


String influxString(String host, String key, String value) {
  return String(key + ",host=" + host + " value="+value);
}

void sendUDPLine(WiFiUDP udp, String line) {
  udp.beginPacket(MDNS.IP(0), MDNS.port(0));
  Serial.println("Sending: " + line);
  udp.print(line);
  udp.endPacket();
}
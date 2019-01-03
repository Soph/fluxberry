#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include "common/common.h"

const char* ssid     = "<ssid>";
const char* password = "<password>";
char hostString[16] = {0};
WiFiUDP udp;

int const signal_pin = 4; // D2
int const signal_pin_interrupt_number = digitalPinToInterrupt(signal_pin); // INT0, 
static int events = 0;
static long old_state = HIGH;

void setup() {
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

  pinMode(signal_pin, INPUT_PULLUP);
  // attach an interrupt handler when the signal goes from low to high
  attachInterrupt(signal_pin_interrupt_number, counter, CHANGE);
}
 
void loop() {
  delay(60000);
  Serial.print(" Counts : ");Serial.println(events);
  sendUDPLine(udp, influxString(String(hostString), "gas", String(events)));
  events = 0;
}
 
void counter() {
  long reading = digitalRead(signal_pin);
  if (old_state != reading) {
    old_state = reading;
    switch( reading ) {
      case LOW:  // SWITCH DOWN = ON
          ++events;
        //getHttp(); // THIS WILL FAIL, must be in the loop
        break;
      case HIGH: // SWITCH UP = OFF
        break;
    }
  }  
}
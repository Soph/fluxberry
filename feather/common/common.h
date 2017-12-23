#include <WiFiUdp.h>

// WIFI
void setupWIFI(char* hostString, char const* ssid, char const* password);

// Influx
bool searchInflux();
String influxString(String host, String key, String value);

// UDP
void sendUDPLine(WiFiUDP udp, String line);
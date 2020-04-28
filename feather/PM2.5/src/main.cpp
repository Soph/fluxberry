#include "Arduino.h"
#include "PMS.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#define HOSTNAME "ESP_%06X"
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

const char *ssid = "";
const char *password = "";

char hostString[16] = {0};
WiFiUDP udp;

void setupWIFI(char *hostString, char const *ssid, char const *password);
bool searchInflux();
void sendUDPLine(WiFiUDP udp, String line);
String influxString(String host, String key, String value);
void measureVoltageLevels();
void wakeupPMS();
void readPMS();

RTC_DATA_ATTR boolean readData = false;

PMS pms(Serial1);
PMS::DATA data;

void setup()
{
  Serial.begin(9600);  // GPIO1, GPIO3 (TX/RX pin on ESP-12E Development Board)
  Serial1.begin(9600); // GPIO2 (D4 pin on ESP-12E Development Board)

  if (readData) {
    setupWIFI(hostString, ssid, password);

    if (!MDNS.begin(hostString))
    {
      Serial.println("Error setting up MDNS responder!");
    }

    while (!searchInflux())
    {
      Serial.println("No Influx Service found");
      delay(500);
    }
    Serial.print(MDNS.hostname(0));
    Serial.print(" (");
    Serial.print(MDNS.IP(0));
    Serial.print(":");
    Serial.print(MDNS.port(0));
    Serial.println(")");

    measureVoltageLevels();
  }
  else 
  {
    Serial.println("Skipping WIFI since only sensor booting");
  }

  pms.passiveMode(); // Switch to passive mode

  if (readData) {
    readPMS();
    readData = false;
    esp_sleep_enable_timer_wakeup(570 * uS_TO_S_FACTOR);
  }
  else
  {
    wakeupPMS();
    readData = true;
    esp_sleep_enable_timer_wakeup(30 * uS_TO_S_FACTOR);
  }
  esp_deep_sleep_start();
}

void wakeupPMS() {
  Serial.println("Waking up, wait 30 seconds for stable readings...");
  pms.wakeUp();
}

void readPMS() {
  Serial.println("Send read request...");

  // throw away first
  pms.requestRead();
  pms.readUntil(data);

  int pm_ae_ug_1_0 = 0;
  int pm_ae_ug_2_5 = 0;
  int pm_ae_ug_10_0 = 0;
  int pm_sp_ug_1_0 = 0;
  int pm_sp_ug_2_5 = 0;
  int pm_sp_ug_10_0 = 0;

  int measurements = 0;
  for (int i = 0; i < 3; i++)
  {
    pms.requestRead();
    if (pms.readUntil(data))
    {
      measurements++;
      pm_ae_ug_1_0 += data.PM_AE_UG_1_0;
      pm_ae_ug_2_5 += data.PM_AE_UG_2_5;
      pm_ae_ug_10_0 += data.PM_AE_UG_10_0;
      pm_sp_ug_1_0 += data.PM_SP_UG_1_0;
      pm_sp_ug_2_5 += data.PM_SP_UG_2_5;
      pm_sp_ug_10_0 += data.PM_SP_UG_10_0;
      Serial.print("measure 1.0: ");
      Serial.println(String(pm_ae_ug_1_0));
    }
    delay(1000);
  }

  if (measurements > 0) {
    sendUDPLine(udp, influxString(String(hostString), "pm_ae_ug_1_0", String((int)round(pm_ae_ug_1_0 / measurements)) + "i"));
    sendUDPLine(udp, influxString(String(hostString), "pm_ae_ug_2_5", String((int)round(pm_ae_ug_2_5 / measurements)) + "i"));
    sendUDPLine(udp, influxString(String(hostString), "pm_ae_ug_10_0", String((int)round(pm_ae_ug_10_0 / measurements)) + "i"));
    sendUDPLine(udp, influxString(String(hostString), "pm_sp_ug_1_0", String((int)round(pm_sp_ug_1_0 / measurements)) + "i"));
    sendUDPLine(udp, influxString(String(hostString), "pm_sp_ug_2_5", String((int)round(pm_sp_ug_2_5 / measurements)) + "i"));
    sendUDPLine(udp, influxString(String(hostString), "pm_sp_ug_10_0", String((int)round(pm_sp_ug_10_0 / measurements)) + "i"));
  }

  Serial.println("Going to sleep for 60 seconds.");
  pms.sleep();
}

void measureVoltageLevels() {
  float powerVoltage = analogRead(A13) / 4095.00 * 3.3 * 2 * 1.1;
  sendUDPLine(udp, influxString(String(hostString), "power_voltage", String(powerVoltage, 2)));
  float batteryVoltage = analogRead(A3) / 4095.00 * 3.3 * 1.1 * 5/3.41;
  sendUDPLine(udp, influxString(String(hostString), "battery_voltage", String(batteryVoltage, 2)));
}

void loop()
{
}

void setupWIFI(char *hostString, char const *ssid, char const *password)
{
  sprintf(hostString, HOSTNAME, ESP.getEfuseMac());
  WiFi.setHostname(hostString);

  Serial.print("Hostname: ");
  Serial.println(hostString);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

bool searchInflux()
{
  int n = MDNS.queryService("influx", "udp"); // Send out query for esp tcp services
  if (n == 0)
  {
    Serial.println("no services found");
    return false;
  }
  else
  {
    Serial.print(n);
    Serial.println(" service(s) found");
    return true;
  }
}

void sendUDPLine(WiFiUDP udp, String line)
{
  udp.beginPacket(MDNS.IP(0), MDNS.port(0));
  Serial.println("Sending: " + line);
  udp.print(line);
  udp.endPacket();
}

String influxString(String host, String key, String value)
{
  return String(key + ",host=" + host + " value=" + value);
}

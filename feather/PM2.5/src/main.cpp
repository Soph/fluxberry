#include "Arduino.h"
#include "PMS.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#define HOSTNAME "ESP_%06llu"
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define MAX_PMS_READ_COUNT 3

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
void setupNetworking();
void sendVoltageLevels();
void sendPMS();

enum STATE {
  STATE_START,
  STATE_READ_DATA_PMS,
  STATE_SEND_DATA
};

RTC_DATA_ATTR boolean readData = false;
RTC_DATA_ATTR STATE currentState = STATE_START;
RTC_DATA_ATTR double powerVoltage = 0;
RTC_DATA_ATTR double batteryVoltage = 0;
RTC_DATA_ATTR int voltageReadCount = 0;
RTC_DATA_ATTR int pmsReadCount = 0;
RTC_DATA_ATTR int pm_ae_ug_1_0 = 0;
RTC_DATA_ATTR int pm_ae_ug_2_5 = 0;
RTC_DATA_ATTR int pm_ae_ug_10_0 = 0;
RTC_DATA_ATTR int pm_sp_ug_1_0 = 0;
RTC_DATA_ATTR int pm_sp_ug_2_5 = 0;
RTC_DATA_ATTR int pm_sp_ug_10_0 = 0;
RTC_DATA_ATTR int pmsMeasurementsCount = 0;

PMS pms(Serial1);
PMS::DATA data;

void setup()
{
  Serial.begin(9600);  // GPIO1, GPIO3 (TX/RX pin on ESP-12E Development Board)
  Serial1.begin(9600); // GPIO2 (D4 pin on ESP-12E Development Board)

  measureVoltageLevels();
  
  switch (currentState)
  {
    case STATE_START:
      Serial.println("STATE: START");
      wakeupPMS();
      esp_sleep_enable_timer_wakeup(30 * uS_TO_S_FACTOR);
      currentState = STATE_READ_DATA_PMS;
      break;
    case STATE_READ_DATA_PMS:
      Serial.println("STATE: READ DATA PMS");
      readPMS();
      pmsReadCount++;
      esp_sleep_enable_timer_wakeup(3 * uS_TO_S_FACTOR);
      if (pmsReadCount >= MAX_PMS_READ_COUNT)
      {
        currentState = STATE_SEND_DATA;
      }
      break;
    case STATE_SEND_DATA:
      Serial.println("STATE: SEND DATA");
      setupNetworking();
      sendVoltageLevels();
      readPMS();
      pms.sleep();
      sendPMS();
      esp_sleep_enable_timer_wakeup(570 * uS_TO_S_FACTOR);
      break;
  }
 
  esp_deep_sleep_start();
}

void setupNetworking() {
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
}

void wakeupPMS() {
  Serial.println("Waking up, wait 30 seconds for stable readings...");
  pms.wakeUp();
  pms.passiveMode(); // Switch to passive mode
}

void readPMS() {
  Serial.println("Send read request...");

  // throw away first
  pms.requestRead();
  pms.readUntil(data);
  pms.requestRead();
  if (pms.readUntil(data))
  {
    pmsMeasurementsCount++;
    pm_ae_ug_1_0 += data.PM_AE_UG_1_0;
    pm_ae_ug_2_5 += data.PM_AE_UG_2_5;
    pm_ae_ug_10_0 += data.PM_AE_UG_10_0;
    pm_sp_ug_1_0 += data.PM_SP_UG_1_0;
    pm_sp_ug_2_5 += data.PM_SP_UG_2_5;
    pm_sp_ug_10_0 += data.PM_SP_UG_10_0;
    Serial.print("measure 1.0: ");
    Serial.println(String(pm_ae_ug_1_0));
  }
}

void sendPMS() {
  sendUDPLine(udp, influxString(String(hostString), "pm_ae_ug_1_0", String((int)round(pm_ae_ug_1_0 / pmsMeasurementsCount)) + "i"));
  sendUDPLine(udp, influxString(String(hostString), "pm_ae_ug_2_5", String((int)round(pm_ae_ug_2_5 / pmsMeasurementsCount)) + "i"));
  sendUDPLine(udp, influxString(String(hostString), "pm_ae_ug_10_0", String((int)round(pm_ae_ug_10_0 / pmsMeasurementsCount)) + "i"));
  sendUDPLine(udp, influxString(String(hostString), "pm_sp_ug_1_0", String((int)round(pm_sp_ug_1_0 / pmsMeasurementsCount)) + "i"));
  sendUDPLine(udp, influxString(String(hostString), "pm_sp_ug_2_5", String((int)round(pm_sp_ug_2_5 / pmsMeasurementsCount)) + "i"));
  sendUDPLine(udp, influxString(String(hostString), "pm_sp_ug_10_0", String((int)round(pm_sp_ug_10_0 / pmsMeasurementsCount)) + "i"));
  pmsMeasurementsCount = 0;
  pm_ae_ug_1_0 = 0;
  pm_ae_ug_2_5 = 0;
  pm_ae_ug_10_0 = 0;
  pm_sp_ug_1_0 = 0;
  pm_sp_ug_2_5 = 0;
  pm_sp_ug_10_0 = 0;
}

void measureVoltageLevels() {
  voltageReadCount++;
  powerVoltage += analogRead(A13) / 4095.00 * 3.3 * 2 * 1.1;
  batteryVoltage += analogRead(A3) / 4095.00 * 3.3 * 1.1 * 5/3.41;
}

void sendVoltageLevels() {
  sendUDPLine(udp, influxString(String(hostString), "power_voltage", String(powerVoltage / voltageReadCount, 2)));
  sendUDPLine(udp, influxString(String(hostString), "battery_voltage", String(batteryVoltage / voltageReadCount, 2)));
  voltageReadCount = 0;
  powerVoltage = 0;
  batteryVoltage = 0;
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

#include "Arduino.h"
#include "LoRaWAN.h"
#include <SPI.h>
#include "PMS.h"
#include <EEPROM.h>
#include "ttn.h"

#define HOSTNAME "ESP_%06llu"
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define MAX_PMS_READ_COUNT 3

#define RF_LORA

#define DIO0 32
#define NSS 15
RFM95 rfm(DIO0, NSS);
LoRaWAN lora = LoRaWAN(rfm);
uint16_t Frame_Counter_Tx = 0x0000;

uint8_t calcEepromAddr(uint16_t framecounter);

struct lora_data
{
  uint16_t batVoltage;
  uint16_t powerVoltage;
  uint8_t pm_ae_ug_1_0;
  uint8_t pm_ae_ug_2_5;
  uint8_t pm_ae_ug_10_0;
  uint8_t pm_sp_ug_1_0;
  uint8_t pm_sp_ug_2_5;
  uint8_t pm_sp_ug_10_0;
} __attribute__((packed)) ldata;

void measureVoltageLevels();
void wakeupPMS();
void readPMS();
void setupNetworking();
void generatePayload();
void sendPayload();
void setupTTN();

enum STATE {
  STATE_START,
  STATE_READ_DATA_PMS,
  STATE_SEND_DATA
};

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
      setupTTN();
      readPMS();
      pms.sleep();
      generatePayload();
      sendPayload();
      esp_sleep_enable_timer_wakeup(570 * uS_TO_S_FACTOR);
      currentState = STATE_START;
      pmsReadCount = 0;
      break;
  }
 
  esp_deep_sleep_start();
}

void printHex2(unsigned v)
{
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
}

void setupTTN() {
  // Setup LoraWAN
  rfm.init();
  lora.setKeys(NwkSkey, AppSkey, DevAddr);

  // Get Framecounter from EEPROM
  // Check if EEPROM is initialized
  if (EEPROM.read(511) != 0x42)
  {
    // Set first 64 byte to 0x00 for the wear leveling hack to work
    for (int i = 0; i < 64; i++)
      EEPROM.write(i, 0x00);
    // Write the magic value so we know it's initialized
    EEPROM.write(511, 0x42);
  }
  else
  {
    // Get the Last Saved (=Highest) Frame Counter
    uint16_t Frame_Counter_Sv = 0x00000000;
    uint8_t eeprom_addr = 0x0000;
    EEPROM.get(eeprom_addr, Frame_Counter_Sv);
    while (eeprom_addr < 32 * sizeof(Frame_Counter_Tx))
    {
      if (Frame_Counter_Sv > Frame_Counter_Tx)
      {
        Frame_Counter_Tx = Frame_Counter_Sv;
      }
      else
      {
        break;
      }
      eeprom_addr += sizeof(Frame_Counter_Tx);
      EEPROM.get(eeprom_addr, Frame_Counter_Sv);
    }
  }

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

void measureVoltageLevels() {
  voltageReadCount++;
  powerVoltage += analogRead(A13) / 4095.00 * 3.3 * 2 * 1.1;
  batteryVoltage += analogRead(A3) / 4095.00 * 3.3 * 1.1 * 5/3.41;
}

void generatePayload()
{
  // PM Data
  ldata.pm_ae_ug_1_0 = (uint8_t)round(pm_ae_ug_1_0 / pmsMeasurementsCount);
  ldata.pm_ae_ug_2_5 = (uint8_t)round(pm_ae_ug_2_5 / pmsMeasurementsCount);
  ldata.pm_ae_ug_10_0 = (uint8_t)round(pm_ae_ug_10_0 / pmsMeasurementsCount);
  ldata.pm_sp_ug_1_0 = (uint8_t)round(pm_sp_ug_1_0 / pmsMeasurementsCount);
  ldata.pm_sp_ug_2_5 = (uint8_t)round(pm_sp_ug_2_5 / pmsMeasurementsCount);
  ldata.pm_sp_ug_10_0 = (uint8_t)round(pm_sp_ug_10_0 / pmsMeasurementsCount);
  pmsMeasurementsCount = 0;
  pm_ae_ug_1_0 = pm_ae_ug_2_5 = pm_ae_ug_10_0 = pm_sp_ug_1_0 = pm_sp_ug_2_5 = pm_sp_ug_10_0 = 0;

  ldata.batVoltage = (uint16_t)(batteryVoltage / voltageReadCount * 100);
  ldata.powerVoltage = (uint16_t)(powerVoltage / voltageReadCount * 100);

  voltageReadCount = powerVoltage = batteryVoltage = 0;
}

void sendPayload()
{
  Serial.println("Sending via LORAWAN");
  lora.Send_Data((unsigned char *)&ldata, sizeof(ldata), Frame_Counter_Tx, SF7BW125, 0x01);
  Serial.println("Done Sending via LORAWAN");
  Frame_Counter_Tx++;
  EEPROM.put(calcEepromAddr(Frame_Counter_Tx), Frame_Counter_Tx);
}

// Crude  Wear Leveling Algorithm to Spread the EEPROM Cell Wear Over
// the first 64 Byte. Using this Method the Theoretical EEPROM Livetime
// should be around 60 Years at a 10 Minute Sending Interval
// (100000 Erase Cycles per Cell * 32 Locations / 144 Measurements a day * 365)
//
// Returns the Next EEPROM Address for Saving the Frame Counter
uint8_t calcEepromAddr(uint16_t framecounter)
{
  uint8_t eeprom_addr = ((framecounter % 32) * sizeof(framecounter));
  if (eeprom_addr == 0)
  {
    eeprom_addr = 62;
  }
  else
  {
    eeprom_addr = eeprom_addr - sizeof(framecounter);
  }
  return eeprom_addr;
}

void loop()
{
}

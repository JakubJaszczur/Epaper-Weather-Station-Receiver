// ************************************* //
// ** Epaper Weather Station Receiver ** //
// ***** Version 1.0 ** 2021-01-10 ***** //
// ************************************* //

// 2021-01-10 Added service commands, optimalization
// 2020-08-25 Optimalization

#include <Arduino.h>
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h>      // 4.2" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <myFont.h>
#include <myBitmaps.h>
#include <data.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// Sensors
#include <Wire.h>
#include "Adafruit_MCP9808.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Adafruit_CCS811.h"

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808(); // temp
Adafruit_BME280 bme; // temp, hum, press
Adafruit_CCS811 ccs;  // CO2
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Eink - > ESP32 mapping:
// BUSY -> D4, RST -> RX2, DC -> TX2, CS -> D5, CLK -> D18, DIN -> D23, GND -> GND, VCC -> 3V3
GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

WiFiClient espClient;
PubSubClient client(espClient);

// Globals

// JSON object to keep data
const size_t capacity = JSON_OBJECT_SIZE(3) + 40;
DynamicJsonDocument doc(capacity);

float tempOut;
float tempFeel;

String timeToDisplay;
String dateToDisplay;
String sunriseTime;
String sunsetTime;
String weatherDescription;
String weekDay;

String forecastString;

int weatherIcon = 0;
int forecast1Icon = 0;
int forecast2Icon = 0;
int forecast3Icon = 0;
int forecast4Icon = 0;
int forecast5Icon = 0;

String date1 = "00-00";
String date2 = "00-00";
String date3 = "00-00";
String date4 = "00-00";
String date5 = "00-00";

float forecastTemp1;
float forecastTemp2;
float forecastTemp3;
float forecastTemp4;
float forecastTemp5;

bool MPC9808status = false;
bool BME280status = false;
bool CCS811status = false;

unsigned long startTime = 0;

// Battery measurement function

float measureBattery()
{
  int measurement = analogRead(BAT_PIN);
  float voltage = (((measurement / 4095.00) * VIN) / DIVIDER);

  return voltage;
}

// NeoPixel function

void colorWipe(uint32_t color)
{
  for(int i = 0; i < strip.numPixels(); i ++)
  {
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
  }
}

void mapColours(int testVariable, int medLevel, int maxLevel, int colours[])
{
  if(testVariable <= medLevel)
    {
      /*Rcolor = map(testVariable, 0, medLevel, 0, 255);
      Gcolor = map(testVariable, 0, medLevel, 0, 255);
      Wcolor = map(testVariable, 0, medLevel, 255, 0);*/
      colours[0] = map(testVariable, 0, medLevel, 0, 255);
      colours[1] = map(testVariable, 0, medLevel, 0, 255);
      colours[3] = map(testVariable, 0, medLevel, 255, 0);
    }

  else
    {
      /*Rcolor = 255;
      Gcolor = map(testVariable, medLevel + 1, medLevel, 255, 0);
      Wcolor = 0;*/
      colours[0] = 255;
      colours[1] = map(testVariable, medLevel + 1, maxLevel, 255, 0);
      colours[3] = 0;
    }
}

// Checking presence

bool CheckPresence(int pinNo, bool lastStatus, bool mqttConnected)
{
  boolean status = digitalRead(pinNo);

  if((status != lastStatus) && (mqttConnected))
  {
    char msg[5];
    String tempString = String(status);

    tempString.toCharArray(msg, sizeof(msg));
    client.publish("home/presence", msg);
    Serial.println("Message sent!");
  }

  return status;
}

// MCP9808 functions

boolean InitialiseMPC9808(byte mode)
{
  if(!tempsensor.begin(MCP9808_ADDR))
  {
    Serial.println("MCP9808 Error");
    return false;
  }
  else
  {
    Serial.println("MCP9808 Initialised");
    tempsensor.setResolution(mode); // sets the resolution mode of reading, the modes are defined in the table bellow:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms
    return true;
  }
}

float GetTemperatureMCP9808()
{
  tempsensor.wake();   // wake up, ready to read!
  float temperature = tempsensor.readTempC() - 2.3;
  tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling

  return temperature;
}

// BME280 functions

boolean InitialiseBME280()
{
  if (!bme.begin(BME280_ADDR, &Wire))
  //if (!bme.begin())
  {
    Serial.println("BME280 Error");
    return false;
  }
  else
  {
    Serial.println("BME280 Initialised");
    return true;
  }

}

float GetTemperatureBME280()
{
  float temperature = bme.readTemperature();

  return temperature;
}

float GetHumidityBME280()
{
  float humidity = bme.readHumidity();

  return humidity;
}

float GetPressureBME280()
{
  float pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(ALTITUDE, pressure);
  pressure = pressure / 100.0F;

  return pressure;
}

// CCS811 functions

boolean InitialiseCCS811()
{
  if (!ccs.begin())
  {
    Serial.println("CCS811 Error");
    return false;
  }
  else
  {
    Serial.println("CCS811 Initialised");
    while(!ccs.available());
    return true;
  }

}

int GetCO2()
{
  int co2 = 0;

  if(ccs.available())
  {
    if(!ccs.readData())
    {
      co2 = ccs.geteCO2();
    }
    else
    {
      Serial.println("CO2 read error!");
    }
  }

  return co2;
}

int GetTVOC()
{
  int tvoc = ccs.getTVOC();

  return tvoc;
}

void SetEnvData(int humidity, double temperature)
{
  ccs.setEnvironmentalData(humidity, temperature);
}

// Eink functions

void showBackground()
{
  display.setRotation(0);
  // draw background
  display.drawExampleBitmap(background, 0, 0, 400, 300, GxEPD_BLACK);
  display.update();

  // partial update to full screen to preset for partial update of box window
  // (this avoids strange background effects)
  display.updateWindow(0, 0, 400, 300, false);
}

void displayIcon(int iconID)
{

  switch(iconID)
  {
    case 1:
    {
      display.drawExampleBitmap(d01, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 2:
    {
      display.drawExampleBitmap(n01, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 3:
    {
      display.drawExampleBitmap(d02, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 4:
    {
      display.drawExampleBitmap(n02, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 5:
    {
      display.drawExampleBitmap(d03, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 6:
    {
      display.drawExampleBitmap(n03, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 7:
    case 8:
    {
      display.drawExampleBitmap(d04, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 9:
    {
      display.drawExampleBitmap(d09, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 10:
    {
      display.drawExampleBitmap(n09, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 11:
    {
      display.drawExampleBitmap(d10, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 12:
    {
      display.drawExampleBitmap(n10, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 13:
    {
      display.drawExampleBitmap(d11, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 14:
    {
      display.drawExampleBitmap(n11, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 15:
    case 16:
    {
      display.drawExampleBitmap(d13, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 17:
    {
      display.drawExampleBitmap(d50, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    case 18:
    {
      display.drawExampleBitmap(n50, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
    default:
    {
      display.drawExampleBitmap(na, ICON_X, ICON_Y, ICON_W, ICON_H, GxEPD_BLACK);
      break;
    }
  }
}

void displayForecastIcon(int field, int iconID)
{
  int positionX;

  switch(field)
  {
    case 1:
    {
      positionX = ICON1_X;
      break;
    }
    case 2:
    {
      positionX = ICON2_X;
      break;
    }
    case 3:
    {
      positionX = ICON3_X;
      break;
    }
    case 4:
    {
      positionX = ICON4_X;
      break;
    }
    case 5:
    {
      positionX = ICON5_X;
      break;
    }
    default:
    {
      positionX = ICON1_X;
      break;
    }
  }

  switch(iconID)
  {
    case 1:
    {
      display.drawExampleBitmap(small_d01, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 2:
    {
      display.drawExampleBitmap(small_n01, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 3:
    {
      display.drawExampleBitmap(small_d02, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 4:
    {
      display.drawExampleBitmap(small_n02, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 5:
    {
      display.drawExampleBitmap(small_d03, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 6:
    {
      display.drawExampleBitmap(small_n03, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 7:
    case 8:
    {
      display.drawExampleBitmap(small_d04, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 9:
    {
      display.drawExampleBitmap(small_d09, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 10:
    {
      display.drawExampleBitmap(small_n09, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 11:
    {
      display.drawExampleBitmap(small_d10, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 12:
    {
      display.drawExampleBitmap(small_n10, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 13:
    {
      display.drawExampleBitmap(small_d11, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 14:
    {
      display.drawExampleBitmap(small_n11, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 15:
    case 16:
    {
      display.drawExampleBitmap(small_d13, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 17:
    {
      display.drawExampleBitmap(small_d50, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    case 18:
    {
      display.drawExampleBitmap(small_n50, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
    default:
    {
      display.drawExampleBitmap(small_na, positionX, BOT_ICONS_Y, BOT_ICONS_W, BOT_ICONS_H, GxEPD_BLACK);
      break;
    }
  }
}

void displayValue(float value, int field)
{
  display.setFont(&SansSerif_bold_24);
  display.setTextColor(GxEPD_BLACK);

  uint16_t box_x = 0;
  uint16_t box_y = 0;
  uint16_t box_w = 0;
  uint16_t box_h = TOP_TEXT_H;
  uint16_t cursor_y = 0;
  uint16_t cursor_x = 0;

  switch(field) //position
  {
    case 1:         //tempIn
    {
      box_x = TEMP_IN_X;
      box_y = TEMP_IN_Y;
      box_w = TEMP_IN_W;
      break;
    }
    case 2:         //tempOut
    {
      box_x = TEMP_OUT_X;
      box_y = TEMP_OUT_Y;
      box_w = TEMP_OUT_W;
      break;
    }
    case 3:         //tempFeel
    {
      box_x = TEMP_FEEL_X;
      box_y = TEMP_FEEL_Y;
      box_w = TEMP_FEEL_W;
      break;
    }
    case 4:         //pressureIn
    {
      box_x = PRESSURE_X;
      box_y = PRESSURE_Y;
      box_w = PRESSURE_W;
      break;
    }
    case 5:         //humidityIn
    {
      box_x = HUMIDITY_X;
      box_y = HUMIDITY_Y;
      box_w = HUMIDITY_W;
      break;
    }
    case 6:         // reserved
    {
      //box_x = ;
      //box_y = ;
      //box_w = ;
      break;
    }
  }

  switch(field) //spaces
  {
    case 1:   //temperatures
    case 2:
    case 3:
    {
      if(value >= 0 && value < 10)
        cursor_x = box_x + 16;
      else if((value >= 10) || (value < 0 && value > -10))
        cursor_x = box_x + 8;
      else
        cursor_x = box_x;
      break;
    }
    case 5:   //humidity
    {
      cursor_x = box_x + 2;
      break;
    }
    case 4:   //pressure
    {
      if(value > 1000)
        cursor_x = box_x;
      else if(value > 100)
        cursor_x = box_x + 16;
      else
        cursor_x = box_x + 48;
      break;
    }
  }

  display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
  cursor_y = box_y + box_h - 5;
  display.setCursor(cursor_x + 2, cursor_y);

  switch(field)   //display settings
  {
    case 1:
    case 2:
    case 3:
    case 5:
    {
      display.print(value, 1);
      break;
    }
    case 4:
    {
      display.print(value, 0);
      break;
    }
  }
}

void displayForecastTemperature(float value, int field)
{
  display.setFont(&SansSerif_plain_16);
  display.setTextColor(GxEPD_BLACK);

  uint16_t box_x = 0;
  uint16_t box_y = BOT_TEMP_Y;
  uint16_t box_w = BOT_TEMP_W;
  uint16_t box_h = BOT_TEMP_H;
  uint16_t cursor_y = 0;
  uint16_t cursor_x = 0;

  switch(field) //position
  {
    case 1:         //temp1
    {
      box_x = TEMP1_X;
      break;
    }
    case 2:         //temp2
    {
      box_x = TEMP2_X;
      break;
    }
    case 3:         //temp3
    {
      box_x = TEMP3_X;
      break;
    }
    case 4:         //temp4
    {
      box_x = TEMP4_X;
      break;
    }
    case 5:         //temp5
    {
      box_x = TEMP5_X;
      break;
    }
  }

  display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
  cursor_y = box_y + box_h - 5;

  if(value >= 0 && value < 10)
    cursor_x = box_x + 10;
  else if((value >= 10) || (value < 0 && value > -10))
    cursor_x = box_x + 5;
  else
    cursor_x = box_x;

  display.setCursor(cursor_x + 2, cursor_y);

  display.print(value, 1);
}

void displayText(String text, int field)
{
  display.setFont(&SansSerif_plain_16);
  display.setTextColor(GxEPD_BLACK);

  uint16_t box_x = 0;
  uint16_t box_y = 0;
  uint16_t box_w = 0;
  uint16_t box_h = MIDDLE_TEXT_H;
  uint16_t cursor_y = 0;
  uint16_t cursor_x = 0;
  float fontWidth = 9.5;  //average font width

  switch(field)
  {
    case 1:         //sunrise
    {
      box_x = SUNRISE_X;
      box_y = SUNRISE_Y;
      box_w = SUNRISE_W;
      cursor_x = box_x + 2;
      break;
    }
    case 2:         //sunset
    {
      box_x = SUNSET_X;
      box_y = SUNSET_Y;
      box_w = SUNSET_W;
      cursor_x = box_x + 2;
      break;
    }
    case 3:         //weather description
    {
      box_x = DESCR_X;
      box_y = DESCR_Y;
      box_w = DESCR_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
    case 4:         //week day
    {
      box_x = DAY_X;
      box_y = DAY_Y;
      box_w = DAY_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
    case 5:         //date 1
    {
      box_x = DATE1_X;
      box_y = BOT_DATE_Y;
      box_w = BOT_DATE_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
    case 6:         //date 2
    {
      box_x = DATE2_X;
      box_y = BOT_DATE_Y;
      box_w = BOT_DATE_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
    case 7:         //date 3
    {
      box_x = DATE3_X;
      box_y = BOT_DATE_Y;
      box_w = BOT_DATE_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
    case 8:         //date 4
    {
      box_x = DATE4_X;
      box_y = BOT_DATE_Y;
      box_w = BOT_DATE_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
    case 9:         //date 5
    {
      box_x = DATE5_X;
      box_y = BOT_DATE_Y;
      box_w = BOT_DATE_W;
      cursor_x = (uint16_t)(box_x + (box_w - text.length() * fontWidth) / 2);
      break;
    }
  }

  display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
  cursor_y = box_y + box_h - 3;
  display.setCursor(cursor_x, cursor_y);
  display.print(text);
  //display.updateWindow(box_x, box_y, box_w, box_h, false);
}

void displayTime(String tempTime)
{
  display.setFont(&SansSerif_bold_50);
  display.setTextColor(GxEPD_BLACK);

  uint16_t box_x = TIME_X;
  uint16_t box_y = TIME_Y;
  uint16_t box_w = TIME_W;
  uint16_t box_h = TIME_H;
  uint16_t cursor_y = box_y + box_h - 6;;

  display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
  display.setCursor(box_x + 2, cursor_y);
  display.print(tempTime);
  //display.updateWindow(box_x, box_y, box_w, box_h, false);
}

void displayDate(String tempDate)
{
  display.setFont(&SansSerif_bold_24);
  display.setTextColor(GxEPD_BLACK);

  uint16_t box_x = DATE_X;
  uint16_t box_y = DATE_Y;
  uint16_t box_w = DATE_W;
  uint16_t box_h = DATE_H;
  uint16_t cursor_y = box_y + box_h - 6;

  display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
  display.setCursor(box_x + 2, cursor_y);
  display.print(tempDate);
  //display.updateWindow(box_x, box_y, box_w, box_h, false);
}

// WiFi setup

void setupWifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  WiFi.setHostname(DEVICE_NAME);
  delay(100);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

bool wifiConnectionStatus(int counter)
{
  bool status = false;

  for(int i = 0; i < counter; i++)  //try to reconnect x times
  {
    if(WiFi.isConnected())
    {
      status = true;
      break;
    }
    else
    {
      delay(500);
      WiFi.reconnect();
      status = WiFi.isConnected();
      if(status)
        break;
    }
  }

  return status;
}

// SERVICE HANDLE

String checkOnlineTime()
{
  unsigned long actualTime = millis() / 1000;
  unsigned long onlineTime = actualTime - (startTime / 1000);

  int days = onlineTime / 86400;
  int hours = (onlineTime - days * 86400) / 3600;
  int minutes = (onlineTime - days * 86400 - hours * 3600) / 60;

  return String(days) + " d " + String(hours) + " h " + String(minutes) + " min";
}

void handleService(String command)
{
  char msg[50];
  String toSend;

  if(command == "RESTART")
  {
    client.publish(DEBUG_TOPIC, "Weather Station restart");
    ESP.restart();
  }
  if(command == "SENSORS")
  {
    toSend = "MPC9808 " + String(MPC9808status) + ", BME280 " + String(BME280status) + ", CCS811 " + String(CCS811status);
    toSend.toCharArray(msg, sizeof(msg));
    client.publish(DEBUG_TOPIC, msg);
  }
  if(command == "STATUS")
  {
    toSend = "Online time: " + checkOnlineTime();
    toSend.toCharArray(msg, sizeof(msg));
    client.publish(DEBUG_TOPIC, msg);
  }
}

// MQTT setup

void callback(char* topic, byte* payload, unsigned int length)
{
  char messageBuffer [250];

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (uint i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    messageBuffer[i] = payload[i];
  }
  Serial.println();

  char *tempPayload = messageBuffer;

  if(String(topic) == TEMPOUT_TOPIC)
  {
    tempOut = atof(tempPayload);
    Serial.println(tempOut);
  }
  if(String(topic) == TIME_TOPIC)
  {
    timeToDisplay = String(messageBuffer);
    Serial.println(timeToDisplay);
  }
  if(String(topic) == DATE_TOPIC)
  {
    dateToDisplay = String(messageBuffer);
    Serial.println(dateToDisplay);
  }
  if(String(topic) == SUNRISE_TOPIC)
  {
    sunriseTime = String(messageBuffer);
    Serial.println(sunriseTime);
  }
  if(String(topic) == SUNSET_TOPIC)
  {
    sunsetTime = String(messageBuffer);
    Serial.println(sunsetTime);
  }
  if(String(topic) == DESCRIPTION_TOPIC)
  {
    weatherDescription = String(messageBuffer);
    Serial.println(weatherDescription);
  }
  if(String(topic) == WEEKDAY_TOPIC)
  {
    weekDay = String(messageBuffer);
    Serial.println(weekDay);
  }
  if(String(topic) == WEATHER_ICON_TOPIC)
  {
    weatherIcon = atoi(tempPayload);
    Serial.println(weatherIcon);
  }
  if(String(topic) == FELT_TEMP_TOPIC)
  {
    tempFeel = atof(tempPayload);
    Serial.println(tempFeel);
  }
  if(String(topic) == FORECAST1_TOPIC)
  {
    forecastString = String(messageBuffer);
    Serial.println(forecastString);
    deserializeJson(doc, forecastString);

    const char* time1 = doc["time1"];
    date1 = String(time1);
    const char* icon1 = doc["icon1"];
    forecast1Icon = atoi(icon1);
    forecastTemp1 = doc["temp1"];
  }
  if(String(topic) == FORECAST2_TOPIC)
  {
    forecastString = String(messageBuffer);
    Serial.println(forecastString);
    deserializeJson(doc, forecastString);

    const char* time2 = doc["time2"];
    date2 = String(time2);
    const char* icon2 = doc["icon2"];
    forecast2Icon = atoi(icon2);
    forecastTemp2 = doc["temp2"];
  }
  if(String(topic) == FORECAST3_TOPIC)
  {
    forecastString = String(messageBuffer);
    Serial.println(forecastString);
    deserializeJson(doc, forecastString);

    const char* time3 = doc["time3"];
    date3 = String(time3);
    const char* icon3 = doc["icon3"];
    forecast3Icon = atoi(icon3);
    forecastTemp3 = doc["temp3"];
  }
  if(String(topic) == FORECAST4_TOPIC)
  {
    forecastString = String(messageBuffer);
    Serial.println(forecastString);
    deserializeJson(doc, forecastString);

    const char* time4 = doc["time4"];
    date4 = String(time4);
    const char* icon4 = doc["icon4"];
    forecast4Icon = atoi(icon4);
    forecastTemp4 = doc["temp4"];
  }
  if(String(topic) == FORECAST5_TOPIC)
  {
    forecastString = String(messageBuffer);
    Serial.println(forecastString);
    deserializeJson(doc, forecastString);

    const char* time5 = doc["time5"];
    date5 = String(time5);
    const char* icon5 = doc["icon5"];
    forecast5Icon = atoi(icon5);
    forecastTemp5 = doc["temp5"];
  }
    if(String(topic) == SERVICE_TOPIC)
  {
    String serviceString = String(messageBuffer);
    handleService(serviceString);
  }
}

bool MQTTconnectionStatus(bool wifiStatus, int counter)
{
  bool status = false;

  if(wifiStatus)
  {
    for(int i = 0; i < counter; i++)  //try to reconnect x times
    {
      if(client.connected())
      {
        status = true;
        break;
      }
      else
      {
        Serial.print("Attempting MQTT connection...");
        String clientId = "WeatherStationESP32";

        // Attempt to connect
        //if(client.connect(clientId.c_str()))
        if(client.connect(clientId.c_str(), mqtt_user, mqtt_password))
        {
          Serial.println("MQTT connected");
          client.subscribe(TIME_TOPIC);
          client.subscribe(DATE_TOPIC);
          client.subscribe(WEEKDAY_TOPIC);
          client.subscribe(DESCRIPTION_TOPIC);
          client.subscribe(SUNRISE_TOPIC);
          client.subscribe(SUNSET_TOPIC);
          client.subscribe(WEATHER_ICON_TOPIC);
          client.subscribe(FELT_TEMP_TOPIC);
          client.subscribe(TEMPOUT_TOPIC);
          client.subscribe(FORECAST1_TOPIC);
          client.subscribe(FORECAST2_TOPIC);
          client.subscribe(FORECAST3_TOPIC);
          client.subscribe(FORECAST4_TOPIC);
          client.subscribe(FORECAST5_TOPIC);
          client.subscribe(SERVICE_TOPIC);
          break;
        }

        else
        {
          Serial.print("failed, rc=");
          Serial.print(client.state());
          Serial.println("try again in 1 second");
          // Wait 1 second before retrying
          delay(1000);
        }

        status = client.connected();
      }
    }
  }

  return status; 
}

void sendDataMQTT(float temperature, float humidity, float pressure, int co2Level, int tvocLevel, float bat)
{
  const size_t cap = JSON_OBJECT_SIZE(6);
  DynamicJsonDocument mqttJson(cap);

  String toSend;

  mqttJson["tempIn"] = temperature;
  mqttJson["humIn"] = humidity;
  mqttJson["pressIn"] = pressure;
  mqttJson["co2"] = co2Level;
  mqttJson["tvoc"] = tvocLevel;
  mqttJson["bat"] = bat;

  serializeJson(mqttJson, toSend);

  char msg[150];

  toSend.toCharArray(msg, sizeof(msg));
  client.publish(PUBLISH_TOPIC, msg);
  Serial.println("Message sent!");
}

// ******* MAIN SECTION *******


void setup(void)
{
  startTime = millis();

  pinMode(LED_ON, OUTPUT);
  strip.begin();                      // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();                       // Turn OFF all pixels
  strip.setBrightness(BRIGHTNESS);    // Set BRIGHTNESS
  colorWipe(strip.Color(0, 0, 0, 0)); // RGBW
  digitalWrite(LED_ON, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");
  display.init(115200);               // enable diagnostic output on Serial
  Serial.println("setup done");

  showBackground();

  setupWifi();
  WiFi.softAPdisconnect(true); //disable hotspot mode

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Wire.begin();

  Serial.println();

  MPC9808status = InitialiseMPC9808(2);
  BME280status = InitialiseBME280();
  CCS811status = InitialiseCCS811();

  pinMode(PRESENCE_PIN, INPUT);
}

void loop()
{
  unsigned long timeNow;
  static unsigned long timeLast;
  static unsigned long timeLastPresence;

  static float tempIn = 0;
  static float humIn = 0;
  static float pressIn = 0;

  static int co2 = 0;
  static int tvoc = 0;

  static float battery = 0;

  static bool presenceStatus = false;
  static int rgbw[4] = {0};     // colours array for NEOPIXEL

  timeNow = millis();

  bool wifiStatus = wifiConnectionStatus(5);              // Check WiFI status and try to reconnect in case of no connection
  bool mqttStatus = MQTTconnectionStatus(wifiStatus, 5);  // Check MQTT status and try to reconnect in case of no connection
    
  client.loop();

  if((timeNow - timeLastPresence) > PRESENCE_TIME * 1000) // Get Presence and send is MQTT is available
  {
    presenceStatus = CheckPresence(PRESENCE_PIN, presenceStatus, mqttStatus);
    timeLastPresence = timeNow;
  }

  if((timeNow - timeLast) > REFRESH_TIME * 1000)          // Get data
  {
    if(MPC9808status)
      tempIn = GetTemperatureMCP9808();
    if(BME280status)
    {
      humIn = GetHumidityBME280();
      pressIn = GetPressureBME280();
    }
    if(CCS811status)
    {
      co2 = GetCO2();
      tvoc = GetTVOC();
    }

    battery = measureBattery();

    SetEnvData((int)humIn, (double)tempIn);

    if(mqttStatus)  // Send data only if device is connected to MQTT
    {
      sendDataMQTT(tempIn, humIn, pressIn, co2, tvoc, battery);
    }

    display.drawExampleBitmap(background, 0, 0, 400, 300, GxEPD_BLACK);
    displayValue(tempIn, 1);
    displayValue(tempOut, 2);
    displayValue(tempFeel, 3);
    displayValue(pressIn, 4);
    displayValue(humIn, 5);

    displayTime(timeToDisplay);
    displayDate(dateToDisplay);
    displayText(sunriseTime, 1);
    displayText(sunsetTime, 2);
    displayText(weatherDescription, 3);
    displayText(weekDay, 4);

    displayIcon(weatherIcon);
    displayForecastIcon(1, forecast1Icon);
    displayForecastIcon(2, forecast2Icon);
    displayForecastIcon(3, forecast3Icon);
    displayForecastIcon(4, forecast4Icon);
    displayForecastIcon(5, forecast5Icon);

    displayText(date1, 5);
    displayText(date2, 6);
    displayText(date3, 7);
    displayText(date4, 8);
    displayText(date5, 9);

    displayForecastTemperature(forecastTemp1, 1);
    displayForecastTemperature(forecastTemp2, 2);
    displayForecastTemperature(forecastTemp3, 3);
    displayForecastTemperature(forecastTemp4, 4);
    displayForecastTemperature(forecastTemp5, 5);

    display.update();

    timeLast = timeNow;
  }

  if(digitalRead(PRESENCE_PIN))            // Turn on backlight
  {
    digitalWrite(LED_ON, HIGH);
    mapColours(co2, MED_CO2, MAX_CO2, rgbw);

    colorWipe(strip.Color(rgbw[0], rgbw[1], rgbw[2], rgbw[3])); // RGBW
    //strip.setBrightness(BRIGHTNESS); // Set BRIGHTNESS to about 1/5 (max = 255)
    strip.show();                          //  Update strip to match
  }
  else
  {
    colorWipe(strip.Color(0, 0, 0, 0)); // RGBW
    digitalWrite(LED_ON, LOW);
    strip.show();                          //  Update strip to match
  }
}

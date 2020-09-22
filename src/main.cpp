// ************************************* //
// ** Epaper Weather Station Receiver ** //
// *********** Version 0.1 ************* //
// ********** No optimized ************* //
// ************************************* //

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

unsigned long timeNow = 0;
unsigned long timeLast = 0;
unsigned long timeLastPresence = 0;

// Globals

// JSON object to keep incoming data
const size_t capacity = JSON_OBJECT_SIZE(3) + 40;
DynamicJsonDocument doc(capacity);

boolean presence = LOW;

int Rcolor = 0;
int Gcolor = 0;
int Wcolor = 0;

float tempIn;
float tempOut;
float tempFeel;
float humIn;
float pressIn;

int co2;
int tvoc;

float battery;

String timeToDisplay;
String dateToDisplay;
String sunriseTime;
String sunsetTime;
String weatherDescription;
String weekDay;

String forecastString1;
String forecastString2;
String forecastString3;
String forecastString4;
String forecastString5;

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

void mapColors(int testVariable)
{
  if(testVariable <= MED_CO2)
    {
      Rcolor = map(testVariable, 0, MED_CO2, 0, 255);
      Gcolor = map(testVariable, 0, MED_CO2, 0, 255);
      Wcolor = map(testVariable, 0, MED_CO2, 255, 0);
    }

  else
    {
      Rcolor = 255;
      Gcolor = map(testVariable, MED_CO2 + 1, MAX_CO2, 255, 0);
      Wcolor = 0;
    }
}

// Checking presence

void CheckPresence(int pinNo)
{
  boolean status = digitalRead(pinNo);

  if(status != presence)
  { 
    char msg[5];
    String tempString = String(status);

    tempString.toCharArray(msg, sizeof(msg));
    client.publish("home/presence", msg);
    Serial.println("Message sent!");
    presence = status;
  }
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
  int co2;

  if(ccs.available())
  {
    if(!ccs.readData())
    {
      co2 = ccs.geteCO2();
    }
    else
    {
      co2 = 0;
      Serial.println("Error!");
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

void setup_wifi() 
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

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

  if(String(topic) == "home/weatherstation/tempout")
  {
    tempOut = atof(tempPayload);
    Serial.println(tempOut);
  }
  if(String(topic) == "basic/time/time")
  {
    timeToDisplay = String(messageBuffer);
    Serial.println(timeToDisplay);
  }
  if(String(topic) == "basic/time/date")
  {
    dateToDisplay = String(messageBuffer);
    Serial.println(dateToDisplay);
  }
  if(String(topic) == "conditions/weather/sunrise")
  {
    sunriseTime = String(messageBuffer);
    Serial.println(sunriseTime);
  }
  if(String(topic) == "conditions/weather/sunset")
  {
    sunsetTime = String(messageBuffer);
    Serial.println(sunsetTime);
  }
  if(String(topic) == "conditions/weather/description")
  {
    weatherDescription = String(messageBuffer);
    Serial.println(weatherDescription);
  }
  if(String(topic) == "basic/time/day")
  {
    weekDay = String(messageBuffer);
    Serial.println(weekDay);
  }
  if(String(topic) == "conditions/weather/icon")
  {
    weatherIcon = atoi(tempPayload);
    Serial.println(weatherIcon);
  }
  if(String(topic) == "conditions/weather/tempfeel")
  {
    tempFeel = atof(tempPayload);
    Serial.println(tempFeel);
  }
  if(String(topic) == "forecast1")
  {
    forecastString1 = String(messageBuffer);
    Serial.println(forecastString1);
    deserializeJson(doc, forecastString1);
    
    const char* time1 = doc["time1"];
    date1 = String(time1);
    const char* icon1 = doc["icon1"];
    forecast1Icon = atoi(icon1);
    forecastTemp1 = doc["temp1"];
  }
  if(String(topic) == "forecast2")
  {
    forecastString2 = String(messageBuffer);
    Serial.println(forecastString2);
    deserializeJson(doc, forecastString2);
    
    const char* time2 = doc["time2"];
    date2 = String(time2);
    const char* icon2 = doc["icon2"];
    forecast2Icon = atoi(icon2);
    forecastTemp2 = doc["temp2"];
  }
  if(String(topic) == "forecast3")
  {
    forecastString3 = String(messageBuffer);
    Serial.println(forecastString3);
    deserializeJson(doc, forecastString3);
    
    const char* time3 = doc["time3"];
    date3 = String(time3);
    const char* icon3 = doc["icon3"];
    forecast3Icon = atoi(icon3);
    forecastTemp3 = doc["temp3"];
  }
  if(String(topic) == "forecast4")
  {
    forecastString4 = String(messageBuffer);
    Serial.println(forecastString4);
    deserializeJson(doc, forecastString4);
    
    const char* time4 = doc["time4"];
    date4 = String(time4);
    const char* icon4 = doc["icon4"];
    forecast4Icon = atoi(icon4);
    forecastTemp4 = doc["temp4"];
  }
  if(String(topic) == "forecast5")
  {
    forecastString5 = String(messageBuffer);
    Serial.println(forecastString5);
    deserializeJson(doc, forecastString5);
    
    const char* time5 = doc["time5"];
    date5 = String(time5);
    const char* icon5 = doc["icon5"];
    forecast5Icon = atoi(icon5);
    forecastTemp5 = doc["temp5"];
  }
}

void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "WeatherStationESP32";
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) 
    {
      Serial.println("connected");
      client.subscribe("basic/time/time");
      client.subscribe("basic/time/date");
      client.subscribe("basic/time/day");
      client.subscribe("conditions/weather/description");
      client.subscribe("conditions/weather/sunrise");
      client.subscribe("conditions/weather/sunset");
      client.subscribe("conditions/weather/icon");
      client.subscribe("conditions/weather/tempfeel");
      client.subscribe("home/weatherstation/tempout");
      client.subscribe("forecast1");
      client.subscribe("forecast2");
      client.subscribe("forecast3");
      client.subscribe("forecast4");
      client.subscribe("forecast5");
    } 

    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sendDataMQTT()
{
  const size_t cap = JSON_OBJECT_SIZE(6);
  DynamicJsonDocument mqttJson(cap);

  String toSend;

  mqttJson["tempIn"] = tempIn;
  mqttJson["humIn"] = humIn;
  mqttJson["pressIn"] = pressIn;
  mqttJson["co2"] = co2;
  mqttJson["tvoc"] = tvoc;
  mqttJson["bat"] = battery;

  serializeJson(mqttJson, toSend);

  char msg[150];

  toSend.toCharArray(msg, sizeof(msg));
  client.publish("home/station", msg);
  Serial.println("Message sent!");
}


// ******* MAIN SECTION *******


void setup(void)
{
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

  setup_wifi();
  WiFi.softAPdisconnect (true); //disable hotspot mode
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Wire.begin();

  Serial.println();

  InitialiseMPC9808(2);
  InitialiseBME280();
  InitialiseCCS811();

  pinMode(PRESENCE_PIN, INPUT);
}

void loop()
{
  timeNow = millis();

  if(WiFi.status() != WL_CONNECTED)
  {
    ESP.restart();
  }
  //Check MQTT connection
  if(!client.connected())
  {
    reconnect();
  }

  client.loop();

  if((timeNow - timeLastPresence) > PRESENCE_TIME * 1000)
  {
    CheckPresence(PRESENCE_PIN);
    timeLastPresence = timeNow;
  }

  if((timeNow - timeLast) > REFRESH_TIME * 1000)
  {
    tempIn = GetTemperatureMCP9808();
    humIn = GetHumidityBME280();
    pressIn = GetPressureBME280();
    co2 = GetCO2();
    tvoc = GetTVOC();
    battery = measureBattery();

    SetEnvData((int)humIn, (double)tempIn);

    sendDataMQTT();
    
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

  if(digitalRead(PRESENCE_PIN))
  {
    digitalWrite(LED_ON, HIGH);
    mapColors(co2);
    colorWipe(strip.Color(Rcolor, Gcolor, 0, Wcolor)); // RGBW
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


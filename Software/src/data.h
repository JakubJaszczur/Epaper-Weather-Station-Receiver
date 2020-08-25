// WiFi Settings
const char* ssid = "";
const char* password = "";

// Battery measurement settings

#define BAT_PIN     33
#define VIN         3.28
#define DIVIDER     0.7674   // R1/(R1+R2)

// NeoPixel settings

#define LED_PIN     15
#define LED_ON      32
#define LED_COUNT   6       // No of LEDs
#define BRIGHTNESS  32     // NeoPixel brightness, 0 (min) to 255 (max)
#define MED_CO2     1500    // Poziom dyskomfortu
#define MAX_CO2     5000    // Max poziom

// MQTT Settings
const char* mqtt_server = "";
//const char* mqtt_user = "";
//const char* mqtt_password = "";

#define MQTT_MAX_PACKET_SIZE 256

// Refresh time in seconds
#define REFRESH_TIME	30
#define PRESENCE_TIME   5

// Sensors config data
#define ALTITUDE        515.0 //define altitude of location
#define BME280_ADDR     0x76 //BME280 I2C Address
#define MCP9808_ADDR    0x18
#define CCS811_ADDR     0x5A    // 0x5A or 0x5B

#define PRESENCE_PIN    34

// I2C settings
#define BME_SCK         22 //SCL
//#define BME_MISO 12//
//#define BME_MOSI 11//
#define BME_CS          21  //SDA

// Top text positions
#define TOP_TEXT_H      28

#define TEMP_IN_X		28
#define TEMP_IN_Y		0
#define TEMP_IN_W		80

#define TEMP_OUT_X		30
#define TEMP_OUT_Y		28
#define TEMP_OUT_W		80

#define TEMP_FEEL_X		170
#define TEMP_FEEL_Y		0
#define TEMP_FEEL_W		80

#define PRESSURE_X		170
#define PRESSURE_Y		28
#define PRESSURE_W		80

#define HUMIDITY_X		308
#define HUMIDITY_Y		0
#define HUMIDITY_W		45

// Middle objects positions
#define ICON_X			0
#define ICON_Y			58
#define ICON_W			120
#define ICON_H			120

#define TIME_X			175
#define TIME_Y			58
#define TIME_W			170
#define TIME_H			50

#define DATE_X			175
#define DATE_Y			108
#define DATE_W			170
#define DATE_H			30

#define MIDDLE_TEXT_H	20

#define DESCR_X			120
#define DESCR_Y			138
#define DESCR_W			280

#define SUNRISE_X		140
#define SUNRISE_Y		158
#define SUNRISE_W		50

#define SUNSET_X		328 //330
#define SUNSET_Y		158
#define SUNSET_W		50

#define DAY_X			200
#define DAY_Y			158
#define DAY_W			120

// Bottom objects positions
#define BOT_DATE_Y		180
#define BOT_DATE_W		79
#define BOT_DATE_H		20

#define DATE1_X			0
#define DATE2_X			80
#define DATE3_X			160
#define DATE4_X			240
#define DATE5_X			320

#define BOT_ICONS_Y		200
#define BOT_ICONS_W		79
#define BOT_ICONS_H		79

#define ICON1_X			0
#define ICON2_X			80
#define ICON3_X			160
#define ICON4_X			240
#define ICON5_X			320

#define BOT_TEMP_Y		280
#define BOT_TEMP_W		50
#define BOT_TEMP_H		20

#define TEMP1_X			15
#define TEMP2_X			95
#define TEMP3_X			175
#define TEMP4_X			255
#define TEMP5_X			335

## Epaper Weather Station Receiver

### Introduction

The project is a part of bigger IOT system. Weather Station receiver measures basic parameters, such as temperature, pressure, humidity, but also CO2, TVOC level. Other parameters, like outside temperature, weather forecast can be received by MQTT and displayed on 4.2" Epaper display.

### Features

- 4.2" E-paper display with additional NeoPixel backlight
- Li-Ion battery as backup PS
- Additional fan for better air flow
- Temperature, humidity, pressure measurements
- CO2, TVOC level measurementsr
- PIR sensor
- Time, date, sunrise/sunset time display
- Current weather forecast with description
- Weather forecast for next 5 days
- Outside temperature, sensed temperature display
- All displayed parameters/icons can be easily changed
- All not measured parameters are send to/from weather station by MQTT

### Bill of Materials

| Device  		    | Description 	                          | Price		  | 
| --------------- | --------------------------------------- |-----------|
| ESP32 DEVKIT V1 | Main IC                                 |	$3.50     |
| BME280          | Temperature, humidity, pressure sensor  |	$2.10     |
| MCP9808         | High accuracy temperature sensor        | $0.75     |
| CCS811          | CO2, TVOC sensor                        | $5.89     |
| HC-SR505        | PIR sensor (AM312)                      | $1.15     |
| J5019           | Battery charger with DC-DC converter    | $0.69     |
| 4.2" Epaper     | Waveshare E-paper Display Module        | $28.47    |
| 25mm fan        | 5V fan (25x25x7mm)                      | $1.22     |
| SK6812          | RGBW NeoPixel LED                       | $0.25     |
| 1000mAh Li-Ion  | Battery                                 | $2.50     |
| PCB             | Custom PCB                              |           |
| Housing         | Custom housing (3D printed)             |           |

### Libraries used

- Arduino JSON
- BME280

### Getting Started

### Description

### Changelog

### Improvement list

- Software:
- [ ] Firmware code optimization
- [ ] Service commands (e.g. for temperature calibration)
- [ ] RTC support to keep date/time

- Hardware:
- [ ] Change NeoPixel data MOSFET to AND gate

## Pictures

![](Pictures/IMG1.jpg)

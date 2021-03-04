# Volumio-Control-ESP32
External remote controller for volumio music player (https://www.volumio.com) using Arduino ESP32.

## Use-Case
This IoT device can be used to control the Volumio system in another room or installed behind any furnitures. The device always shows the currently playing music from Volumio. It is battery operated and can be transported anywhere in the wifi network. In contrast to the mobile control of Volumio, the device is quicker to access and always on.

It supports the following functions: 
* WiFi connect to network
* IO Socket connection for Volumio music player
* Display of current playing music
* visualisiation for OLED 128 x 64 monochrome
* play, pause and volume control 
* Remote control support
* Battery Management (3.7V Lithium)

## Part List
The following list of parts are necessary and tested:
* ESP32 Lolit32 v 1.0.0
* OLED 128X64 Display for SSD130 or SH1106 
* 3.7V Lithium Battery 1000 - 1300 mAh
* Rotary Encoder Modul KY-040 360°
* (optional) IRC receiver modul

## Libraries
* U8g2lib by olikraus (https://github.com/olikraus/u8g2)
* U8g2Fonts: ProFont (http://tobiasjung.name/profont/)
* Volumio IO Socket connection by drvolcano (https://github.com/drvolcano/Volumio-ESP32)
* RoteryEncoder for Rotary Encoder Modul (https://github.com/mathertel/RotaryEncoder) 
* RunningMedian for median calcualtion (https://github.com/RobTillaart/RunningMedian)
* IRremote
* WiFiMulti
* Arduino


## Configuration
All settings for pin and device can be made in main.h 

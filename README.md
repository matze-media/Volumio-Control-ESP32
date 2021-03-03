# Volumio-Control-ESP32
External controller for volumio music player (https://www.volumio.com) using Arduino ESP32.

It supports the following feature: 
* WiFi Connect to network
* IO Socket connection for Volumio music player
* Display of current playing music
* visualisiation for OLED 128 x 64 monochrome
* play, pause and volume control 
* Remote Control support
* Battery Management (3.7V Lithium)

## Part List
The following list of parts are supported:
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

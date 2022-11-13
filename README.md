# ESP8266 IoT Framework (fork)

This is a fork of the great ["ESP8266 IoT Framework"](https://github.com/maakbaas/esp8266-iot-framework) project by [maakbaas](https://github.com/maakbaas). 

The purpose of this fork is to just play with it and see if there is an elegant way for persisting some wifi settings (like SSID and password) within the EPROM. This is a must in some of my battery-powered IoT projects in which the ESP8266 mcu is normally powered down for the sake of battery consumption and woken up for a few seconds on a regular basis (e.g., every 2 hours) to do some quick operations. Thus, ESP8266 needs to "remember" the wifi SSID and password of the access point used last time to access the Internet.

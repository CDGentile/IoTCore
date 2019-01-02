# IoTCore
Baseline package for ESP8266 initialization

This package provides a set of baseline functionality enabling the inclusion of a new module in an IoT application.  It can be flashed to a raw ESP module prior to inclusion in a board without serial access.

Includes the following items:

-WiFiManager (AP/station mode via captive portal)
-ArduinoOTA (allows OTA code flashing)
-Hard Coded upload page to access SPIFFS and upload additional files

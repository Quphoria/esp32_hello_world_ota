# OTA Example App

A demo program which prints "Hello World", and checks for OTA updates on startup.  

This program implements a very basic OTA implementation on the ESP32, which uses a simple HTTP server for OTA updates.  
It has the wifi and ota code split into seperate files, allow this program to be used as a template for other projects.  
It only runs the update if the version or hash of the OTA update differs from the running code. This can be configured using the SDK Configuration Editor.  
There are 2 custom menus in the SDK Configuration Editor: "Wifi Configuration" and "OTA Configuration".  

## How to use

Edit the configuration with your wifi network information and the firmware upgrade url endpoint of your OTA HTTP Server.  

A very basic OTA HTTP Server is here called "ota_http_server.py", run it with the path of the .bin file for your update, e.g. `python ota_http_server.py .\build\hello_world_ota.bin`.  
Alternatively, you just need to put the .bin file on any basic HTTP server and change the upgrade url to point to it.  

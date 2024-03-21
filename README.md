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

## HTTPS Support

To use HTTPS (recommended for anything in production, or where security is required), disable "ESP HTTPS OTA -> Allow HTTP for OTA" in the configuration and use an HTTPS firmware upgrade url.

To use a custom self-signed certificate, you will need to modify `main/CMakeLists.txt` to enabled embedding a certificate, and add the certificates in a directory called `server_certs`, and configure the necessary configuration options. For more information, see: https://github.com/espressif/esp-idf/tree/master/examples/system/ota/simple_ota_example  





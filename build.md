```
%IDF_PATH%/export.bat
idf.py set-target esp32
idf.py menuconfig
```

ESP32-WROOM-32
```
40MHz
4MB Flash
```

`idf.py -p COM14 -b 115200 flash`
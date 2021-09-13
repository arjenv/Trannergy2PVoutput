# Trannergy2PVoutput

Read your Trannergy Solar power Inverter over WIFI.
Process data and sends it to PVoutput.org
You will need an API from PVoutput.org if you want it to upload.

For ESP8266. Might work with ESP32, not tested!

Rename your_secrets.h to secrets.h

Edit secrets.h. Fill in your SSID, password, api, ID, Ipnumber of your inverter etc.

Recommend to put your inverter's IP addres into a static addres since it will shut down every night.
Otherwise you might get a new IPnumber via DHCP every morning and the ESP will not connect.

You can connect a temperature sensor to pin 14 to get surrounding temperature data.

This software is ported from omnikstats and not optimised for ESP8266.

Besides, I am not a software guy... :-)

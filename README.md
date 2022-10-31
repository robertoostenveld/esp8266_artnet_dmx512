# Overview

This sketch implements a WiFi module that uses the Art-Net protocol over a wireless connection to control wired stage lighting fixtures/lamps that are DMX512 compatible. It listens to incoming Art-Net packets and forwards a single universe to DMX512. It features over-the-air (OTA) configuration of the wifi network that it connects to, configuration of the universe that is forwarded, and monitoring of the incoming packets and the frame rate.

Upon switching on, the LED turns yellow to indicate that setup is done. After that the LED turns red to indicate that it is not connected to WiFi. It will try connect as client to the previously used WiFi network; if that succeeds, the LED turns green and setup is ready. If that fails, the LED remains red and the node creates a wireless access point (AP) with the name Artnet. You can connect with laptop or smartphone to that network to configure the WIFi client settings and provide the password of the network to which it should connect. After that it resets.

Wherever there is activity on the web interface (configuration, monitoring), the LED turns blue. During web interface activity, the DMX512 output is silenced. A smooth web interface and smooth DMX signalling don't go together.

See http://robertoostenveld.nl/art-net-to-dmx512-with-esp8266/ for more details and photos, and https://robertoostenveld.nl/timing-and-jitter-in-dmx512-signals/ for a detailled look at the timing of the DMX signals.

# Components

  - Wemos D1 mini
  - MAX485 module, e.g. http://ebay.to/2iuKQlr
  - DC-DC boost/buck converter 5V power supply, e.g. http://ebay.to/2iAfOei
  - common cathode RGB LED
  - 2x 220 Ohm and 1x 100 Ohm resistors
  - 3 or 5 pin female XLR connector
  - panel mount 2.1 x 5.5 mm DC barrel jack
  - 82 x 58 x 34 mm ABS enclosure box

# Wiring scheme

![](schematic.png)

## 5V power and GND
- connect 5V and GND from the power supply to Vcc and GND of the MAX485 module
- connect 5V and GND from the power supply to the 5V and GND of the Wemos D1 mini

## MAX485
- connect MAX485 module pin DE (data enable)    to 3.3V (using 3.3V TTL)
- connect MAX485 module pin RE (receive enable) to GND
- connect MAX485 module pin DI (data in)        to D4/TX1   of the Wemos D1 mini for UART operation
- connect MAX485 module pin DI (data in)        to RX/GPIO3 of the Wemos D1 mini for I2S operation
- connect MAX485 module VCC   to 3.3V (or to DE)
- connect MAX485 module pin A to XLR 3
- connect MAX485 module pin B to XLR 2
- connect MAX485 module GND   to XLR 1

## RGB LED
- connect the blue  leg of the LED over the 100 Ohm resistor to GPIO16/D0
- connect the green leg of the LED over the 220 Ohm resistor to GPIO05/D1
- connect the red   leg of the LED over the 220 Ohm resistor to GPIO04/D2

# SPIFFS for static files

Please note that you should not only write the firmware to the ESP8266 module, but also the static content for the web interface. The html, css and javascript files located in the data directory should be written to the SPIFS filesystem on the ESP8266. See for example http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html and https://www.instructables.com/id/Using-ESP8266-SPIFFS for instructions. You will get a `file not found` error if the firmware cannot access the data files.

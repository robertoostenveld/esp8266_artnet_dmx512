/*
   This sketch receives Art-Net data of one DMX universes over WiFi
   and sends it to a MAX485 module as an interface between wireless
   Art-Net and wired DMX512.

   This firmware can either use the UART (aka Serial) interface to
   the MAX485 module, or the I2S interface. Note that the wiring
   depends on whether you use UART or I2S.

   See https://robertoostenveld.nl/art-net-to-dmx512-with-esp8266/
   and comments to that blog post.

   See https://github.com/robertoostenveld/esp8266_artnet_dmx512
*/

#include <ESP8266WiFi.h>         // https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <ArtnetWifi.h>          // https://github.com/rstephan/ArtnetWifi
#include <ArduinoJson.h>
#include <FS.h>

#include "rgb_led.h"
#include "webinterface.h"

#define MIN(x,y) (x<y ? x : y)
#define MAX(x,y) (x>y ? x : y)

/*********************************************************************************/

// Uncomment to send DMX data using the microcontroller's builtin UART.
// This is the original way this sketch used to work and expects the max485 level
// shifter to be connected to the pin that corresponds to Serial1.
// On a Wemos D1 this is pin D4 aka TX1.
#define ENABLE_UART

// Uncomment to send DMX data via I2S instead of UART.
// I2S allows for better control of number of stop bits and DMX timing to meet the
// requirements of sloppy devices. Moreover using DMA reduces strain of the CPU and avoids
// issues with background activity such as handling WiFi, interrupts etc.
// However - because of the extra timing/pauses for sloppy device, sending DMX over I2S
// will cause throughput to drop from approx 40 packets/s to around 30.
// On a Wemos D1 I2S is available on pin RX0.
#define ENABLE_I2S

// Enable kind of unit test for new I2S code moving around a knowingly picky device
// (china brand moving head with timing issues)
//#define WITH_TEST_CODE

// Comment in to enable standalone mode. This means that the setup function won't
// block until the device was configured to connect to a Wifi network but will start
// to receive Artnet data right away on the access point network that the WifiManager
// created for this purpose. You can then simply ignore the configuration attempt and
// use the device without a local Wifi network or choose to connect one later.
// Consider setting also a password in standalone mode, otherwise someone else might
// configure your device to connect to a random Wifi.
//#define ENABLE_STANDALONE
//#define STANDALONE_PASSWORD "wifisecret"

// Enable OTA (over the air programming in the Arduino GUI, not via the web server)
//#define ENABLE_ARDUINO_OTA
//#define ARDUINO_OTA_PASSWORD "otasecret"

// Enable the web interface that allows to configure the ArtNet universe, the number
// of channels, etcetera
#define ENABLE_WEBINTERFACE

// Enable multicast DNS, which resolves hostnames to IP addresses within small networks
// that do not include a local name server
#define ENABLE_MDNS

/*********************************************************************************/

#ifdef ENABLE_UART
#include "c_types.h"
#include "eagle_soc.h"
#include "uart_register.h"
// there are two different implementations for the break, one using serial, the other using low-level timings; both should work
#define USE_SERIAL_BREAK
/* UART for DMX output */
#define SEROUT_UART 1
/* DMX minimum timings per E1.11 */
#define DMX_BREAK 92
#define DMX_MAB 12
#endif // ENABLE_UART

/*********************************************************************************/

#ifdef ENABLE_I2S
#include <I2S.h>
#include <i2s_reg.h>
#include "i2s_dmx.h"
#include "DmxArray.h"
#define DMX_CHANNELS 256

// 256 channels, byte aligned, 40.1 packets/s with generous timing for slow fixtures:
DmxArray dmxArray;

/* To set your own timing parameters use this constructor:
DmxArray(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits);
e.g. 
DmxArray(512, 37, 12, 3, 3); // 40.3 p/s, NOT byte aligned
DmxArray(512, 25, 20, 3, 7); // 30.3 p/s, byte aligned
*/
#endif // ENABLE_I2S

/*********************************************************************************/

const char* host = "ARTNET";
const char* version = __DATE__ " / " __TIME__;

Config config;
ESP8266WebServer server(80);
ArtnetWifi artnet;
WiFiManager wifiManager;

// keep track of the timing of the function calls
long tic_loop = 0, tic_fps = 0, tic_packet = 0, tic_web = 0;
unsigned long packetCounter = 0, frameCounter = 0, last_packet_received = 0;
float fps = 0;

// Global buffer with one Artnet universe
struct  {
  uint16_t universe;
  uint16_t length;
  uint8_t sequence;
#ifdef ENABLE_UART
  uint8_t uart_data[512];
#endif
} global;

#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
// Keep track whether OTA was started
bool arduinoOtaStarted = false;
unsigned int last_ota_progress = 0;
#endif

#ifdef ENABLE_UART
long tic_uart = 0;
unsigned long uartCounter;
#endif

/*********************************************************************************/

// This will be called for each UDP packet that is received
void onDmxPacket(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t * data) {

  unsigned long now = millis();
  if (now - last_packet_received > 1000) {
    Serial.print("Received DMX data\n");
  }
  last_packet_received = now;

  // print some feedback once per second
  if ((millis() - tic_fps) > 1000 && frameCounter > 100) {
    Serial.print("packetCounter = ");
    Serial.print(packetCounter++);
    // don't estimate the FPS too frequently
    fps = 1000 * frameCounter / (millis() - tic_fps);
    tic_fps = last_packet_received;
    frameCounter = 0;
    Serial.print(", FPS = ");             Serial.print(fps);
    // print out also some diagnostic info
    Serial.print(", length = ");          Serial.print(length);
    Serial.print(", sequence = ");        Serial.print(sequence);
    Serial.print(", universe = ");        Serial.print(universe);
    Serial.print(", config.universe = "); Serial.print(universe);
    Serial.println();
  }

  if (universe == config.universe) {
    // copy the data from the UDP packet
    global.universe = universe;
    global.sequence = sequence;

#ifdef ENABLE_I2S
    int channels = MIN(global.length, dmxArray.size());
    for (int i = 0; i < channels; i++) {
      dmxArray.setChannel(i+1, data[i]);
    }
#endif

#ifdef ENABLE_UART
    if (length <= 512)
      global.length = length;
    for (int i = 0; i < global.length; i++) {
      global.uart_data[i] = data[i];
    }
#endif
  }
} // onDmxpacket

/*********************************************************************************/

void setup() {

  // Serial0 is for debugging purposes
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println("Setup starting");

  // set up three output pins for a RGB status LED
  ledInit();

#ifdef ENABLE_UART
  // Serial1 output is for DMX signalling to the MAX485 module
  Serial1.begin(250000, SERIAL_8N2);
#endif

  global.universe = 0;
  global.sequence = 0;
  global.length = 512;

#ifdef ENABLE_UART
  for (int i = 0; i < 512; i++)
    global.uart_data[i] = 0;
#endif

  // The SPIFFS file system contains the html and javascript code for the web interface
  SPIFFS.begin();

  Serial.println("Loading configuration");
  initialConfig();

  if (loadConfig()) {
    ledYellow();
    delay(1000);
  }
  else {
    ledRed();
    delay(1000);
  }

  if (WiFi.status() != WL_CONNECTED)
    ledRed();

  //wifiManager.resetSettings();

#ifdef ENABLE_STANDALONE
  Serial.println("Starting WiFiManager (non-blocking mode)");
  wifiManager.setConfigPortalBlocking(false);
#else
  Serial.println("Starting WiFiManager (blocking mode)");
#endif

  WiFi.hostname(host);
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));

#ifdef STANDALONE_PASSWORD
  wifiManager.autoConnect(host, STANDALONE_PASSWORD);
#else
  wifiManager.autoConnect(host);
#endif
  Serial.println("connected");

  if (WiFi.status() == WL_CONNECTED)
    ledGreen();

#ifdef ENABLE_ARDUINO_OTA
  Serial.println("Initializing Arduino OTA");
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(ARDUINO_OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    allBlack();
    digitalWrite(LED_B, ON);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    allBlack();
    digitalWrite(LED_R, ON);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    analogWrite(LED_B, 4096 - (20 * millis()) % 4096);
    if (progress != last_ota_progress) {
      Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
      last_ota_progress = progress;
    }
  });
  ArduinoOTA.onEnd([]()   {
    allBlack();
    digitalWrite(LED_G, ON);
    delay(500);
    allBlack();
  });
  Serial.println("Arduino OTA init complete");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Starting Arduino OTA (setup)");
    ArduinoOTA.begin();
    arduinoOtaStarted = true;
  }
#endif

#ifdef ENABLE_WEBINTERFACE

  // this serves all URIs that can be resolved to a file on the SPIFFS filesystem
  server.onNotFound(handleNotFound);

  server.on("/", HTTP_GET, []() {
    tic_web = millis();
    handleRedirect("/index");
  });

  server.on("/index", HTTP_GET, []() {
    tic_web = millis();
    handleStaticFile("/index.html");
  });

  server.on("/defaults", HTTP_GET, []() {
    tic_web = millis();
    Serial.println("handleDefaults");
    handleStaticFile("/reload_success.html");
    delay(2000);
    ledRed();
    initialConfig();
    saveConfig();
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    WiFi.hostname(host);
    ESP.restart();
  });

  server.on("/reconnect", HTTP_GET, []() {
    tic_web = millis();
    Serial.println("handleReconnect");
    handleStaticFile("/reload_success.html");
    delay(2000);
    ledRed();
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.startConfigPortal(host);
    Serial.println("connected");
    if (WiFi.status() == WL_CONNECTED)
      ledGreen();
  });

  server.on("/reset", HTTP_GET, []() {
    tic_web = millis();
    Serial.println("handleReset");
    handleStaticFile("/reload_success.html");
    delay(2000);
    ledRed();
    ESP.restart();
  });

  server.on("/monitor", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/monitor.html");
  });

  server.on("/hello", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/hello.html");
  });

  server.on("/settings", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/settings.html");
  });

  server.on("/dir", HTTP_GET, [] {
    tic_web = millis();
    handleDirList();
  });

  server.on("/json", HTTP_PUT, [] {
    tic_web = millis();
    handleJSON();
  });

  server.on("/json", HTTP_POST, [] {
    tic_web = millis();
    handleJSON();
  });

  server.on("/json", HTTP_GET, [] {
    tic_web = millis();
    DynamicJsonDocument root(300);
    CONFIG_TO_JSON(universe, "universe");
    CONFIG_TO_JSON(channels, "channels");
    CONFIG_TO_JSON(delay, "delay");
    root["version"] = version;
    root["uptime"]  = long(millis() / 1000);
    root["packets"] = packetCounter;
    root["fps"]     = fps;
    String str;
    serializeJson(root, str);
    server.send(200, "application/json", str);
  });

  server.on("/update", HTTP_GET, [] {
    tic_web = millis();
    handleStaticFile("/update.html");
  });

  server.on("/update", HTTP_POST, handleUpdate1, handleUpdate2);

  // start the web server
  server.begin();

#endif // ifdef ENABLE_WEBINTERFACE

#ifdef ENABLE_MDNS
  // announce the hostname and web server through zeroconf
  MDNS.begin(host);
  MDNS.addService("http", "tcp", 80);
#endif

  artnet.begin();
  artnet.setArtDmxCallback(onDmxPacket);

  // initialize all timers
  tic_loop   = millis();
  tic_packet = millis();
  tic_fps    = millis();
  tic_web    = 0;

#ifdef ENABLE_UART
  tic_uart    = 0;
#endif

#ifdef ENABLE_I2S
  byte * data = (byte*) dmxArray.getData();
  unsigned int length = dmxArray.size();
  unsigned int padding = dmxArray.getPaddingBits();
  if (length>1023) {    
    // Maximum DMA transfer length is 2^12 (32 bit) frames
    // 1024 would result in rollover, therefore limitting to 1023.
    // Reduce your timing parameters if you get this warning.
    Serial.printf("I2S warning: limiting data %d -> 1023\n", length);
    length = 1023;
  }
    
  Serial.printf("I2S length:  %d bytes\n", length);
  Serial.printf("I2S padding: %d bits\n", padding);

  i2s_set_rate(7812); // 7812 Hz * 32 bits = 249984 bits/s = 99.993 % * 250000
  float rate = i2s_get_real_rate();
  Serial.printf("I2S real (frame) rate: %.2f Hz\n", rate); // reported as 7812.50
  Serial.printf("I2S real (bit)   rate: %.2f baud\n", 32*rate); // 250 kBaus
  i2s_dmx_begin(data, length);
#endif

  Serial.println("Setup done");
} // setup

/*********************************************************************************/

void loop() {
  // handle wifiManager and arduinoOTA requests only when not receiving new DMX data
  long now = millis();
  if (now - last_packet_received > 1000) {
    wifiManager.process();
#ifdef ENABLE_ARDUINO_OTA
    if (WiFi.status() == WL_CONNECTED && !arduinoOtaStarted) {
      Serial.println("Starting Arduino OTA (loop)");
      ArduinoOTA.begin();
      arduinoOtaStarted = true; // remember that it started
    }
    ArduinoOTA.handle();
#endif
  }
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    ledRed();
    delay(10);
#ifndef ENABLE_STANDALONE
    return;
#endif
  }

  if ((millis() - tic_web) < 5000) {
    // give feedback that the webinterface is active
    ledBlue();
    delay(25);
  }
  else  {
    ledGreen();
    artnet.read();

    // this section gets executed at a maximum rate of around 40Hz
    if ((millis() - tic_loop) > config.delay) {
      long now = millis();
      tic_loop = now;
      frameCounter++;

#ifdef ENABLE_UART
#ifdef USE_SERIAL_BREAK
      // switch to another baud rate, see https://forum.arduino.cc/index.php?topic=382040.0
      Serial1.flush();
      Serial1.begin(90000, SERIAL_8N2);
      while (Serial1.available()) Serial1.read();
      // send the break as a "slow" byte
      Serial1.write(0);
      // switch back to the original baud rate
      Serial1.flush();
      Serial1.begin(250000, SERIAL_8N2);
      while (Serial1.available()) Serial1.read();
#else
      // send break using low-level code
      SET_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
      delayMicroseconds(DMX_BREAK);
      CLEAR_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
      delayMicroseconds(DMX_MAB);
#endif

      Serial1.write(0); // Start-Byte
      // send out the value of the selected channels (up to 512)
      for (int i = 0; i < MIN(global.length, config.channels); i++) {
        Serial1.write(global.uart_data[i]);
      }

      uartCounter++;
      if ((now - tic_uart) > 1000 && uartCounter > 100) {
        // don't estimate the FPS too frequently
        float pps = (1000.0 * uartCounter) / (now - tic_uart);
        tic_uart = now;
        uartCounter = 0;
        Serial.printf("UART: %.1f p/s\n", pps);
      }
#endif

    }
  }

#ifdef WITH_TEST_CODE
  testCode();
#endif

} // loop

/*********************************************************************************/

#ifdef ENABLE_I2S
// Reverse byte order because DMX expects LSB first but I2S sends MSB first.
byte flipByte(byte c) {
  c = ((c >> 1) & 0b01010101) | ((c << 1) & 0b10101010);
  c = ((c >> 2) & 0b00110011) | ((c << 2) & 0b11001100);
  return (c >> 4) | (c << 4);
}
#endif // ENABLE_I2S

/*********************************************************************************/

#ifdef WITH_TEST_CODE
void testCode() {
  long now = millis();
  uint8_t x = (now / 60) % 240;
  if (x > 120) {
    x = 240 - x;
  }

  //Serial.printf("x: %d\n", x);
#ifdef ENABLE_UART
  global.uart_data[1] =   x;// x 0 - 170
  global.uart_data[2] =   0;// x fine
  global.uart_data[3] =   x;// y: 0: -horz. 120: vert, 240: +horz
  global.uart_data[4] =   0;// y fine
  global.uart_data[5] =  30;// color wheel: red
  global.uart_data[6] =   0;// pattern
  global.uart_data[7] =   0;// strobe
  global.uart_data[8] = 150; // brightness
#endif // ENABLE_UART

#ifdef ENABLE_I2S
  global.i2s_data.dmx_bytes[1] = (flipByte(  x) << 8) | 0b0000000011111110;// x 0 - 170
  global.i2s_data.dmx_bytes[2] = (flipByte(  0) << 8) | 0b0000000011111110;// x fine
  global.i2s_data.dmx_bytes[3] = (flipByte(  x) << 8) | 0b0000000011111110;// y: 0: -horz. 120: vert, 240: +horz
  global.i2s_data.dmx_bytes[4] = (flipByte(  0) << 8) | 0b0000000011111110;// y fine
  global.i2s_data.dmx_bytes[5] = (flipByte( 30) << 8) | 0b0000000011111110; // color wheel: red
  global.i2s_data.dmx_bytes[6] = (flipByte(  0) << 8) | 0b0000000011111110;// pattern
  global.i2s_data.dmx_bytes[7] = (flipByte(  0) << 8) | 0b0000000011111110;// strobe
  global.i2s_data.dmx_bytes[8] = (flipByte(150) << 8) | 0b0000000011111110; // brightness
#endif // ENABLE_I2S
}
#endif // WITH_TEST_CODE

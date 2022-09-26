/*
  This sketch receives Art-Net data of one DMX universes over WiFi
  and sends it over the serial interface to a MAX485 module.

  It provides an interface between wireless Art-Net and wired DMX512.

*/

// Uncomment to send DMX data using the microcontroller's builtin UART.
// This is the original way this sketch used to work and expects the max485 level
// shifter to be connected to the pin that corresondents to Serial1.
// On a Wemos D1 this is e.g. pin D4.
#define ENABLE_UART

// Uncomment to send DMX data via I2S instead of UART.
// I2S allows for better control of number of stop bits and DMX timing.
// Moreover using DMA reduces strain of the CPU and avoids issues with background 
// activity such as handling WiFi, interrupts etc.
#define ENABLE_I2S
#define I2S_FREQUENCY_CORRECTION (0)

#include <ESP8266WiFi.h>         // https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <ArtnetWifi.h>          // https://github.com/rstephan/ArtnetWifi
#include <FS.h>

#ifdef ENABLE_I2S
#include <i2s.h>
#include <i2s_reg.h>
#endif

#include "setup_ota.h"
#include "send_break.h"

#define MIN(x,y) (x<y ? x : y)
#define ENABLE_MDNS
#define ENABLE_WEBINTERFACE
// #define COMMON_ANODE

Config config;


ESP8266WebServer server(80);
const char* host = "ARTNET";
const char* version = __DATE__ " / " __TIME__;

#define LED_B 16  // GPIO16/D0
#define LED_G 5   // GPIO05/D1
#define LED_R 4   // GPIO04/D2

#ifdef ENABLE_I2S
#define I2S_PIN 3
#endif

// Artnet settings
ArtnetWifi artnet;


unsigned long packetCounter = 0, frameCounter = 0;
float fps = 0;

#ifdef ENABLE_I2S

#define DMX_CHANNELS 512

// 
//#define SHORT_SPACE_FOR_BREAK
// Below timings are based on measures taken with a commercial DMX512 controller.
struct {
  uint16_t mark_before_break[10]; // 600us / 4us / 16 bits -> 640us
  #ifdef SHORT_SPACE_FOR_BREAK
  uint8_t space_for_break[3]; // 120us / 4us / 8 bits = 3.75
  uint8_t mark_after_break;   // 5 MSB low bits * 4us adds 20us to space_for_break -> 116us
  #else
  uint16_t space_for_break[2]; // 120us / 4us / 16 bits -> 128 us
  uint16_t mark_after_break; // 13 MSB low bits *4us adds 52us to space_for_break -> 180us
  #endif
  // each "byte" (actually a word) consists of:
  // 8 bits payload + 7 stop bits (high) + 1 start (low) for the next byte  
  uint16_t dmx_bytes[DMX_CHANNELS+1];
} i2s_data;

#endif

// Global universe buffer
struct {
  uint16_t universe;
  uint16_t length;
  uint8_t sequence;
  // when using I2S, channel values are stored in i2s_data
#ifdef ENABLE_UART
  uint8_t *data;
#endif
} global;

// keep track of the timing of the function calls
long tic_loop = 0, tic_fps = 0, tic_packet = 0, tic_web = 0;

#ifdef ENABLE_UART
long tic_uart = 0;
unsigned long uartCounter;
#endif

#ifdef ENABLE_I2S
long tic_i2s = 0;
unsigned long i2sCounter;
#endif

//this will be called for each UDP packet received
void onDmxPacket(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t * data) {
  // print some feedback
  Serial.print("packetCounter = ");
  Serial.print(packetCounter++);
  if ((millis() - tic_fps) > 1000 && frameCounter > 100) {
    // don't estimate the FPS too frequently
    fps = 1000 * frameCounter / (millis() - tic_fps);
    tic_fps = millis();
    frameCounter = 0;
    Serial.print(",  FPS = ");
    Serial.print(fps);
  }
  Serial.println();

  if (universe == config.universe) {

#ifdef ENABLE_I2S
    // TODO optimize i2s such that it can send less than 512 bytes if not required
    for (int i=0; i<DMX_CHANNELS; i++) {
      uint16_t hi = i<length ? flipByte(data[i]) : 0;      
      // Add stop bits and start bit of next byte unless there is no next byte.
      uint16_t lo = i==DMX_CHANNELS-1 ? 0b0000000011111111 : 0b0000000011111110;
      // leave null/start byte untached => +1:
      i2s_data.dmx_bytes[i+1] = (hi<<8) | lo;
    }
#endif

#ifdef ENABLE_UART
    
    // copy the data from the UDP packet over to the global universe buffer
    global.universe = universe;
    global.sequence = sequence;
    if (length < 512)
      global.length = length;
    for (int i = 0; i < global.length; i++)
      global.data[i] = data[i];

#endif
  }
} // onDmxpacket



long packets;
long last;

const int CONNECTION_TIMEOUT = 5;

//#define WITH_TEST_CODE

void setup() {
  
#ifdef ENABLE_UART
  Serial1.begin(250000, SERIAL_8N2);
#endif 
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println("setup starting");

  // Must NOT be called before Serial.begin (i@S output is on RX0 pin)
#ifdef ENABLE_I2S  
  initI2S();
#endif

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  global.universe = 0;
  global.sequence = 0;
  global.length = 512;
#ifdef ENABLE_UART
  global.data = (uint8_t *)malloc(512);
  for (int i = 0; i < 512; i++)
    global.data[i] = 0;
#endif

  SPIFFS.begin();

  initialConfig();

  if (loadConfig()) {
    singleYellow();
    delay(1000);
  }
  else {
    singleRed();
    delay(1000);
  }

  if (WiFi.status() != WL_CONNECTED)
    singleRed();

  WiFiManager wifiManager;
  // wifiManager.resetSettings();
  WiFi.hostname(host);
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  wifiManager.autoConnect(host);
  Serial.println("connected");

  if (WiFi.status() == WL_CONNECTED)
    singleGreen();

#ifdef ENABLE_WEBINTERFACE
    initServer();
#endif

  // announce the hostname and web server through zeroconf
#ifdef ENABLE_MDNS
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
  tic_i2s    = 0;
#endif  

  Serial.println("setup done");
} // setup


void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    singleRed();
    delay(10);
  }
  else if ((millis() - tic_web) < 5000) {
    singleBlue();
    delay(25);
  }
  else  {
    singleGreen();
    artnet.read();

    // this section gets executed at a maximum rate of around 40Hz
    if ((millis() - tic_loop) > config.delay) {
      tic_loop = millis();
      frameCounter++;

#ifdef ENABLE_UART
      outputSerial();
#endif      

#ifdef ENABLE_I2S
      outputI2S();
#endif      
    }
  }

  delay(1);
} // loop


#ifdef ENABLE_UART

void outputSerial() {

  sendBreak();

  Serial1.write(0); // Start-Byte
  // send out the value of the selected channels (up to 512)
  for (int i = 0; i < MIN(global.length, config.channels); i++) {
    Serial1.write(global.data[i]);
  }    

  uartCounter++;
  long now = millis();
  if ((now - tic_uart) > 1000 && uartCounter > 100) {
    // don't estimate the FPS too frequently
    float pps = (1000.0 * uartCounter) / (now - tic_uart);
    tic_uart = now;
    uartCounter = 0;
    Serial.printf("UART: %.1f p/s\n", pps);
  }  
}

#endif

#ifdef ENABLE_I2S

void initI2S() {
    pinMode(3, OUTPUT); // Override default Serial initiation
    digitalWrite(3, 1); // Set pin high
  
    memset(&i2s_data, 0x00, sizeof(i2s_data));
    memset(&(i2s_data.mark_before_break), 0xff, sizeof(i2s_data.mark_before_break));
    
    // 3 bits (12us) MAB. The MAB's LSB 0 acts as the start bit (low) for the null byte
    i2s_data.mark_after_break = (uint16_t) 0b000001110;
    
    // Set LSB to 0 for every byte to act as the start bit of the following byte.
    // Sending 7 stop bits in stead of 2 will please slow/buggy hardware and act
    // as the mark time between slots.
    for (int i=0; i<DMX_CHANNELS; i++) {
      i2s_data.dmx_bytes[i] = (uint16_t) 0b0000000011111110;
    }
    // Set MSB NOT to 0 for the last byte because MBB (mark for break will follow)
    i2s_data.dmx_bytes[DMX_CHANNELS] = (uint16_t) 0b0000000011111111;
  
    config.universe = 0;
  
    artnet.begin();
    artnet.setArtDmxCallback(onDmxPacket);
  
    i2s_begin();
    // 250.000 baud / 32 bits = 7812
    i2s_set_rate(7812); 
  
    // Commment in this to fine tune frequency: should be 125 kHz (on oscillocope)
    //memset(&data, 0b01010101, sizeof(data));
}

void outputI2S(void) {  
  // From the comment in i2s.h: 
  // "A frame is just a int16_t for mono, for stereo a frame is two int16_t, one for each channel."
  // Therefore we need to divide by 4 in total
  int frames = sizeof(i2s_data)/4;
  i2s_write_buffer((int16_t*) &i2s_data, frames);

  i2sCounter++;
  long now = millis();
  if ((now - tic_i2s) > 1000 && i2sCounter > 100) {
    // don't estimate the FPS too frequently
    float pps = (1000.0 * i2sCounter) / (now - tic_i2s);
    tic_i2s = now;
    i2sCounter = 0;
    Serial.printf("I2S: %.1f p/s\n", pps);
  }
}

// Reverse byte order because DMX expects LSB first but I2S sends MSB first.
byte flipByte(byte c) {
  c = ((c>>1)&0x55)|((c<<1)&0xAA);
  c = ((c>>2)&0x33)|((c<<2)&0xCC);
  c = (c>>4) | (c<<4) ;
  return c;
}

#endif


#ifdef ENABLE_WEBINTERFACE

void initServer() {
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
    singleRed();
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
    singleRed();
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.startConfigPortal(host);
    Serial.println("connected");
    if (WiFi.status() == WL_CONNECTED)
      singleGreen();
  });

  server.on("/reset", HTTP_GET, []() {
    tic_web = millis();
    Serial.println("handleReset");
    handleStaticFile("/reload_success.html");
    delay(2000);
    singleRed();
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
}
#endif

#ifdef COMMON_ANODE

void singleRed() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);
}

void singleGreen() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
}

void singleBlue() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
}

void singleYellow() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
}

void allBlack() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);
}

#else

void singleRed() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
}

void singleGreen() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
}

void singleBlue() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
}

void singleYellow() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
}

void allBlack() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
}

#endif

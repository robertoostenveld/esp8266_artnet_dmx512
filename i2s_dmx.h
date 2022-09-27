#ifndef _I2S_DMX_H_
#define _I2S_DMX_H_

#define I2S_PIN 3
#define DMX_CHANNELS 512

// See comments below
//#define I2S_SUPER_SAFE

#ifdef I2S_SUPER_SAFE

// Below timings are based on measures taken with a commercial DMX512 controller.
// They differ substentially from the offcial DMX standard ... breaks are longer, more
// stop bits. Apparently to please some picky devices out there that cannot handle
// DMX data quickly enough.
// Using these parameters results in a throughput of approx. 29.7 packets/s (with 512 DMX channels)
typedef struct {
  uint16_t mark_before_break[10]; // 10 * 16 bits * 4 us -> 640 us
  uint16_t space_for_break[2];   // 2 * 16 bits * 4 us -> 128 us
  uint16_t mark_after_break;     // 13 MSB low bits * 4 us adds 52 us to space_for_break -> 180 us
  // each "byte" (actually a word) consists of:
  // 8 bits payload + 7 stop bits (high) + 1 start (low) for the next byte  
  uint16_t dmx_bytes[DMX_CHANNELS+1];
} i2s_packet;

#else 

// This configuration sets way shorter MBB and SFB but still adds lots of extra stop bits.
// At least for my devices this still works and increases thrughput slightly to 30.3  packets/s. 
typedef struct {
  uint16_t mark_before_break[1]; // 1 * 16 bits * 4 us -> 64 us
  uint16_t space_for_break[1];   // 1 * 16 bits * 4 us -> 64 us
  uint16_t mark_after_break;     // 13 MSB low bits * 4 us adds 52 us to space_for_break -> 116 us
  // each "byte" (actually a word) consists of:
  // 8 bits payload + 7 stop bits (high) + 1 start (low) for the next byte  
  uint16_t dmx_bytes[DMX_CHANNELS+1];
} i2s_packet;

void logI2SInfo() {
#ifdef I2S_SUPER_SAFE
  Serial.println("Using super safe I2S timing");
#else
  Serial.println("Using normal I2S timing");
#endif    
}
// TODO try to reduce number of stop bits
/* 
 E.g. as below with 3 stop bits. However, it's questionable if that still will work with sloppy devices,
 because it's not a lot more than what DMX requires (2 stop bits). Moreover we'd need to somehow 
 persuade the compiler not to align the data at word limits in memory.
struct {
  uint16_t startbit1 : 1;
  uint16_t payload1  : 8;
  uint16_t stopbits1 : 3; 
  // 8+3+1 = 12
  uint16_t startbit2 : 1;
  uint16_t payload2  : 8;
  uint16_t stopbits1 : 3; 
  // 8+3+1 = 12
  // 12 + 12 = 24 = 3 bytes
}
*/
#endif

#endif // _I2S_DMX_H_

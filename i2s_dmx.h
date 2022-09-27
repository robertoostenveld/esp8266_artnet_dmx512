#ifndef _I2S_DMX_H_
#define _I2S_DMX_H_

#define I2S_PIN 3
#define DMX_CHANNELS 512

// Below timings are based on measures taken with a commercial DMX512 controller.
// They differ substentially from the offcial DMX standard ... breaks are longer, more
// stop bits. Apparently to please some picky devices out there that cannot handle
// DMX data quickly enough.
typedef struct {
  uint16_t mark_before_break[1]; // 1 * 16 bits * 4 us -> 64 us
  uint16_t space_for_break[1];   // 1 * 16 bits * 4 us -> 64 us
  uint16_t mark_after_break;     // 13 MSB low bits * 4 us adds 52 us to space_for_break -> 116 us
  // each "byte" (actually a word) consists of:
  // 8 bits payload + 7 stop bits (high) + 1 start (low) for the next byte  
  uint16_t dmx_bytes[DMX_CHANNELS+1];
} i2s_packet;

#endif

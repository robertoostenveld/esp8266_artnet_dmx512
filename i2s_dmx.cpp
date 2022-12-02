/*
  i2s_dmx.cpp - Cyclic DMA over a predefined memory buffer
  
  Code from here:
  https://github.com/esp8266/Arduino/blob/master/cores/esp8266/core_esp8266_i2s.cpp

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  
*/ 
 

#include "Arduino.h"
#include "i2s_reg.h"
#include "i2s_dmx.h"

#define I2SO_DATA 3 // hard wired to GPIO3 on ESP8266

extern "C" {

typedef struct slc_queue_item {
  uint32_t                blocksize : 12;
  uint32_t                datalen   : 12; // in bytes !!! -> up to 4095
  uint32_t                unused    :  5;
  uint32_t                sub_sof   :  1;
  uint32_t                eof       :  1;
  volatile uint32_t       owner     :  1; // DMA can change this value
  uint32_t *              buf_ptr;
  struct slc_queue_item * next_link_ptr;
} slc_queue_item_t;

static volatile bool i2s_dmx_active;
static slc_queue_item_t queue_item;
volatile unsigned long i2s_isr_counter = 0;

static void IRAM_ATTR i2s_slc_isr(void) {
  ETS_SLC_INTR_DISABLE();

  i2s_isr_counter++;
  //memcpy(queue_item.buf_ptr, queue_item.buf_ptr, queue_item.datalen);
  //for (long i=0; i<1000; i++) {
  //  __asm__ __volatile__ ("nop");
  //}
  
  uint32_t slc_intr_status = SLCIS;
  SLCIC = 0xFFFFFFFF;
  ETS_SLC_INTR_ENABLE();
}

static void i2s_slc_begin(const byte * data, uint16_t length) {

  queue_item.unused = 0;
  queue_item.owner = 1;
  queue_item.eof = 1;
  queue_item.sub_sof = 0;
  queue_item.datalen   = length;
  queue_item.blocksize = length;
  queue_item.buf_ptr = (uint32_t*)data; 
  queue_item.next_link_ptr = &queue_item; // -> loop

  ETS_SLC_INTR_DISABLE();
  SLCC0 |=   SLCRXLR | SLCTXLR;
  SLCC0 &= ~(SLCRXLR | SLCTXLR);
  SLCIC = 0xFFFFFFFF;

  SLCC0 &= ~(SLCMM << SLCM); // Clear DMA MODE
  SLCC0 |= (1 << SLCM); // Set DMA MODE to 1
  SLCRXDC |= SLCBINR | SLCBTNR; // Enable INFOR_NO_REPLACE and TOKEN_NO_REPLACE
  SLCRXDC &= ~(SLCBRXFE | SLCBRXEM | SLCBRXFM); // Disable RX_FILL, RX_EOF_MODE and RX_FILL_MODE

  SLCTXL &= ~(SLCTXLAM << SLCTXLA); // clear TX descriptor address
  SLCRXL &= ~(SLCRXLAM << SLCRXLA); // clear RX descriptor address
  SLCRXL |= ((uint32) &queue_item) << SLCRXLA; // Set real TX address

  ETS_SLC_INTR_ATTACH(i2s_slc_isr, NULL);
  SLCIE = SLCIRXEOF; // Enable appropriate EOF IRQ

  ETS_SLC_INTR_ENABLE();

  // Start transmission ("TX" DMA always needed to be enabled)
  SLCTXL |= SLCTXLS;
  SLCRXL |= SLCRXLS;
}

static void i2s_slc_end(){
  ETS_SLC_INTR_DISABLE();
  SLCIC = 0xFFFFFFFF;
  SLCIE = 0;
  SLCTXL &= ~(SLCTXLAM << SLCTXLA); // clear TX descriptor address
  SLCRXL &= ~(SLCRXLAM << SLCRXLA); // clear RX descriptor address
}

void i2s_dmx_begin(const byte * data, uint16_t length) {

  if (i2s_dmx_active) {
    i2s_dmx_end(); 
  }

  pinMode(I2SO_DATA, FUNCTION_1); // Important, OUTPUT will not work!

  i2s_slc_begin(data, length);
  
  I2S_CLK_ENABLE();
  I2SIC = 0x3F;
  I2SIE = 0;
  
  // Reset I2S
  I2SC &= ~(I2SRST);
  I2SC |= I2SRST;
  I2SC &= ~(I2SRST);
  
  // I2STXFMM, I2SRXFMM=0 => 16-bit, dual channel data shifted in/out
  I2SFC &= ~(I2SDE | (I2STXFMM << I2STXFM) | (I2SRXFMM << I2SRXFM)); // Set RX/TX FIFO_MOD=0 and disable DMA (FIFO only)
  I2SFC |= I2SDE; // Enable DMA

  // I2STXCMM, I2SRXCMM=0 => Dual channel mode
  I2SCC &= ~((I2STXCMM << I2STXCM) | (I2SRXCMM << I2SRXCM)); // Set RX/TX CHAN_MOD=0

  I2SC |= (1?I2STXS:0); // Start transmission/reception
  return; // true;
}

void i2s_dmx_end() {
  // Disable any I2S send or receive
  I2SC &= ~(I2STXS | I2SRXS);

  // Reset I2S
  I2SC &= ~(I2SRST);
  I2SC |= I2SRST;
  I2SC &= ~(I2SRST);

  i2s_slc_end();

  pinMode(I2SO_DATA, OUTPUT);
  // For DMX, line must be logically high when inactive 
  digitalWrite(I2SO_DATA, HIGH);
  
  i2s_dmx_active = false;
}

}; // extern "C"

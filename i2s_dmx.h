#ifndef I2S_DMX_H
#define I2S_DMX_H

#ifdef __cplusplus
extern "C" {
#endif

extern volatile unsigned long i2s_isr_counter;

void i2s_dmx_begin(const byte * data, uint16_t length);
void i2s_dmx_end();

void i2s_dmx_set_rate(uint32_t rate);
void i2s_dmx_set_dividers(uint8_t div1, uint8_t div2);

#ifdef __cplusplus
}
#endif

#endif

#ifndef BitArray_h
#define BitArray_h

#include <Arduino.h>

// Comment in or define before including this file to enable toString() method.
#define BIT_ARRAY_DEBUG

/**
 * @author: Artur Pogoda de la Vega
 * @date: 2022-11-12
 *
 * This class implements a generic array of bits that can be set and cleared
 * individually or together. It allows also to replace at most 8 bits (a byte)
 * at a random index by a new value.
 *
 * I wrote this class originally for the purpose of creating a DMS signal for
 * a ESP8266 micro controler project, therefore it's taking care of adhering
 * to the correct byte order (little endian) of this platform, though
 * this could be changed by overriding the getByteIndex() method.
 */
class BitArray {
  
  public:
  
    /* 
     * I2S requires us to send a multiple of 32 bits (called I2S frames) 
     * so we must make sure our buffer is padded to double word (uint32_t) boundaries.
     */
    const static int PADDING = 32;

    BitArray(int numBits);
    ~BitArray();

    /*
     * Reconfigure internal buffer for a new number of bits.
     */
    void resize(unsigned numBits);

    /*
     * Set "numBits" bits starting from position "bitIndex" to 1.
     */
    void set(unsigned bitIndex, unsigned numBits);

    /*
     * Clear "numBits" bits starting from position "bitIndex" (set to 0).
     */
    void clear(unsigned bitIndex, unsigned numBits);

    /*
     * Place the "numBits" least significant bits of "value" at the
     * given bit index. Index 0 is the MSB of the first byte in the buffer,
     * the last index refers to the LSB of the last byte.
     */
    bool set(unsigned bitIndex, byte value, unsigned numBits);

    /*
     * Return the number of bytes allocated for the buffer.
     */
    unsigned size() { return numBytes; }

    /*
     * Return number of bits that were passed upon initialization / constructor call.
     */
    unsigned getNumBits() { return numBits; }

    /*
     * Return a pointer to the internal buffer.
     */
    const byte * getData() { return bytes; }

    /*
     * Cyclic left shift the value by the given number of bits.
     */
    static inline byte cycleLeft(byte value, unsigned bits) { return (value << bits) | (value >> (8 - bits)); }

    /*
     * Return the index of the i'th byte in the array following 32 bit little endian order.
     * When called in sequence with values 0, 1, 2, 3, 4, 5, 6, 7 it will e.g. return
     * 3, 2, 1, 0, 7, 6, 5, 4
     */
    static inline int getByteIndex(unsigned i) { 
      // i | i/4 | i%4 | 3-i%4 | 4*i/4 | result
      // 0 | 0   | 0   | 3     | 0     | 3 
      // 1 | 0   | 1   | 2     | 0     | 2
      // 2 | 0   | 2   | 1     | 0     | 1
      // 3 | 0   | 3   | 0     | 0     | 0
      // 4 | 1   | 0   | 3     | 1     | 4
      return 4*(i/4) + 3-(i %4);
    }

    static inline unsigned pad(unsigned numBits) {
      return PADDING * (1 + (numBits - 1) / PADDING);      
    }
    
#ifdef BIT_ARRAY_DEBUG
// Code must reside here, not in .cpp otherwise BIT_ARRAY_DEBUG won't work

    static String format(byte value) {
        String s("");
        for (int bit = 7; bit >= 0; bit--) {
            s += ((value >> bit) & 1) ? '1' : '0';
        }
        return s;
    }

    static String hexDump(const void * ptr, unsigned len, bool littleEndian) {
        String s = "";
        byte * bytes = (byte*) ptr;
        for (int n=0; n<len; n++) {
          int i = littleEndian ? getByteIndex(n) : n;      
          s += BitArray::format(bytes[i]);
          s += ' ';
          if (n>0 && 0==n%16) {
            s += '\n';
          }
          if (n<len-1) {
            s += ' ';
          }
        }
        return s;
    }
    
    String toString() {
      return hexDump(bytes, numBytes, true);
    }
        
#endif

  private:
  
    unsigned numBytes;
    unsigned numBits;
    byte * bytes;  
};

#endif

#ifndef DmxArray_h
#define DmxArray_h

#include "BitArray.h"

/**
 * @author: Artur Pogoda de la Vega
 * @date: 2022-11-12
 *
 * This class subclasses the generic BitArray and initializes its buffer
 * such that it contains a valid DMX(512) universe signal data representation.
 * It provides then a method to set the value of every DMX channel in this
 * buffer at the correct position.
 *
 * This buffer can then be sent via e.g. I2S to a DMX line.
 * Values which the DMX standard does not specify strict defaults for,
 * can be freely configured e.g. the lengths of the various breaks,
 * number of stop bits etc.
 *
 * These parameters can also be changes at runtime which might be useful to
 * find a working configuration for fixtures that do not work nicely with
 * the DMX defaults.
 */
class DmxArray : public BitArray {

  public:
  
     const static int DEFAULT_DMX_CHANNELS = 256;

    /* 
     * Number of start bits or payload is fixed for DMX. 
     * Do not change unless you know what you are doing.
     */
    const static int START_BITS   = 1; // well, plural but actually always 1
    const static int PAYL_BITS    = 8;

    /* 
     * Default values for DMX timing parameters that may vary. 
     * Check here for valid ranges: https://www.erwinrol.com/page/articles/dmx512/
     * Note that value is the number of bits, NOT the time. 
     * Every DMX bit is exactly 4 micro seconds as DMX does not allow any other 
     * speed than 250 k bits/s. (10^6 us / 250 kBaud = 4 us)
     * These are the recommended values for 256 channels, because they represent
     * a good tradeoff between throughput, byte alignment and channels.
     * The effective throughput is 40.1 packets/s and matches therefore the 
     * default rate in UART mode.
     */
    const static int MBB_BITS     = 40;
    const static int SFB_BITS     = 28;
    const static int MAB_BITS     =  3; // 3*4 us = 12 us
    const static int STOP_BITS    = 15;

    // Init with default values (see above).
    DmxArray();
    // Init with default values except for the number of DMX channels.
    DmxArray(int numChannels);
    // Init with full control over variable timing parameters.
    DmxArray(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits);

    // Change those parameters at runtime.
    void reconfig(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits);

    ~DmxArray();

    // Set the value for a DMX channel in the range 1, ..., numChannels
    void setChannel(unsigned channel, byte value);

    // Set all channels to the same value
    void setAll(byte value);

    // Set up to "length" values sorted in data. index 0 goes to channel 1
    void setAll(byte * data, unsigned length);
    
    /*
     * Return number of extra bits that were appeneded due to padding.
     * Ths is merely for debug purposes or out of curiosity to see how much overhead
     * a specific configuration produces.
     */
    int getPaddingBits() { return padding; }

    /*
     * Return number of DMX channels this instance is currently configured for
     */
    int getNumChannels() { return numChannels; }
   
    /*
     * Return number of stop bitsthis instance is currently configured for
     */
    int getNumStopBits() { return stopBits; }

    /* 
     * Return number of bits that needs to be sent for every DMX channel value. 
     */
    int getBitsPerChannel() { return bitsPerChannel; }

    inline int getNumFrames() { return size()/4; }

    /**
     * Return the bit index of the very first bit of the DMX null byte (channel 1).
     * This is the index of the first payload (!) bit
     */
    int getFirstByteIndex() { return firstByteIndex; }

    float getMaxFps() {
      // each DMX bit is 4 μs long
      int totalTime = size() * 8 * 4; 
      return 1000000.0 / totalTime; // micros!!
    }

  private:
  
    int numChannels;
    int stopBits;
    int bitsPerChannel;
    int firstByteIndex;
    int padding;
    byte * flipped;
    /* 
     *  TODO add cache and set only values if they have changed
     *  This idea was due to oerformance considerations.
     *  Doing some experiments, it turned out that setting 512 DMX channels 40 times/sec
     *  will add an extra CPU overhead of approx. 1.5% which is acceptable IMHO,
     *  so adding a cache is low prio.
     */
    // byte * cache;

    static byte flipByte(byte b);
    static int computeTotalBits(unsigned numChannels, unsigned mbbBits, unsigned sfbBits, unsigned mabBits, unsigned stopBits);
};

#endif

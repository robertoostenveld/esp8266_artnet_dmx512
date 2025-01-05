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
class DmxArray extends BitArray {

    final static int DMX_CHANNELS = 4;

    /*
     * Number of start bits or payload is fixed for DMX.
     * Do not change unless you know what you are doing.
     */
    final static int START_BITS = 1;
    final static int PAYL_BITS = 8;

    /*
     * Default values for DMX timing parameters that may vary.
     * Check here for valid ranges: https://www.erwinrol.com/page/articles/dmx512/
     * Note that value is the number of bits, NOT the time.
     * Every DMX bit is exactly 4 micro seconds as DMX does not allow any other
     * speed than 250 k bits/s. (10^6 us / 250 kBaud = 4 us)
     */
    final static int MBB_BITS = 16;
    final static int SFB_BITS = 13;
    final static int MAB_BITS = 3; // 3*4 us = 12 us
    final static int STOP_BITS = 7;

    // Init with default values (see above).
    public DmxArray() {
        this(DMX_CHANNELS);
    }

    // Init with default values except for the number of DMX channels.
    public DmxArray(int numChannels) {
        this(numChannels, MBB_BITS, SFB_BITS, MAB_BITS, STOP_BITS);
    }

    // Init with full control over variable timing parameters.
    public DmxArray(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits) {
        super(0);
        reconfig(numChannels, mbbBits, sfbBits, mabBits, stopBits);
    }

    // Change those parameters at runtime.
    public void reconfig(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits) {

        int unpadded  = computeTotalBits(numChannels, mbbBits, sfbBits, mabBits, stopBits);
        int totalBits = pad(unpadded);
        this.padding = totalBits-unpadded;
        this.numChannels = numChannels;
        this.stopBits = stopBits;

        super.resize(totalBits);

        int nullByteIndex = sfbBits + mabBits + START_BITS;
        bitsPerChannel = START_BITS + PAYL_BITS + stopBits;
        firstByteIndex = nullByteIndex + bitsPerChannel;

        clear(0, sfbBits); // fill SPACE for BREAK with 0s
        set(sfbBits, totalBits-sfbBits); // fill with 1s anything else

        int offsetStopBits = START_BITS+PAYL_BITS;
        int index = nullByteIndex - START_BITS;
        for (int channel = 0; channel < numChannels+1; channel++) {
            clear(index, offsetStopBits);
            index += bitsPerChannel;
        }

        if (null==flipped) {
            flipped = new byte[256];
            for (int i=0; i<256; i++) {
                flipped[i] = flipByte( (byte) (i & 0xff));
            }
            flipped = flipped;
        }
    }

    // Set the value for a DMX channel in the range 1, ..., numChannels
    public void setChannel(int channel, int value) {
        if (0<channel && channel<=numChannels) {
            int index = firstByteIndex + (channel-1) * bitsPerChannel;
            set(index, flipped[value], PAYL_BITS);
        }
    }

    // Set all channels to the same value
    void setAll(byte value) {
        throw new RuntimeException("NYI");
    }

    // Set up to "length" values sorted in data. index 0 goes to channel 1
    void setAll(byte[] data, int length) {
        throw new RuntimeException("NYI");
    }

    public int getPadding() {
        return padding;
    }

    /*
     * Return number of extra bits that were appeneded due to padding.
     * Ths is merely for debug purposes or out of curiosity to see how much overhead
     * a specific configuration produces.
     */
    public int getPaddingBits() { return padding; }

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

    int getNumFrames() { return size()/4; }

    /**
     * Return the bit index of the very first bit of the DMX null byte (channel 1).
     * This is the index of the first payload (!) bit
     */
    int getFirstByteIndex() { return firstByteIndex; }

    private static int computeTotalBits(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits) {
        int bitsPerChannel = START_BITS + PAYL_BITS + stopBits;
        int totalChannels = numChannels + 1;
        int numBits = (mbbBits + sfbBits + mabBits) + ((totalChannels) * bitsPerChannel);
        return numBits;
    }

    public static byte flipByte(byte v) {
        // abcdefgh -> 0a0c0e0g | b0d0f0h0 = badcfehg
        v = b(b(b((v >> 1) & 0b01010101)) | b(b((v << 1) & 0b10101010)));
        // badcfehg -> 00ba00fe | dc00hg00 = dcbahgfe
        v = b(b(b((v >> 2) & 0b00110011)) | b(b((v << 2) & 0b11001100)));
        // dcbahgfe -> 0000dcba | hgfedcba
        v = b( b((v >> 4) & 0b00001111) | b(v << 4));
        return v;
    }

    private int numChannels;
    private int stopBits;
    private int bitsPerChannel;
    int firstByteIndex;
    private int padding;
    byte[] flipped;
}

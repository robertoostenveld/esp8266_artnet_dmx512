import static java.lang.Integer.toBinaryString;
import static java.lang.String.format;

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
public class BitArray {

    /*
     * I2S requires us to send a multiple of 32 bits (called I2S frames)
     * so we must make sure our buffer is padded to double word (uint32_t) boundaries.
     */
    final static int PADDING = 32;

    public BitArray(int numBits) {
        resize(numBits);
    }

    /*
     * Reconfigure internal buffer for a new number of bits.
     */
    protected void resize(int numBits) {
        if (null!= bytes) {
            // free bytes (C++)
        }
        this.numBits = numBits;
        this.numBytes = 1+(numBits-1)/8;
        bytes = new byte[numBytes];
    }

    /*
     * Set "numBits" bits starting from position "bitIndex" to 1.
     */
    boolean set(int bitIndex, int numBits) {
        boolean rtv = true;
        for (int remain = numBits; remain > 0; remain -= 8, bitIndex += 8) {
            int numBits1 = remain > 8 ? 8 : remain;
            rtv &= set(bitIndex, b(0xff), numBits1);
        }
        return rtv;
    }

    /*
     * Clear "numBits" bits starting from position "bitIndex" (set to 0).
     */
    boolean clear(int bitIndex, int numBits) {
        boolean rtv = true;
        for (int remain = numBits; remain > 0; remain -= 8, bitIndex += 8) {
            int numBits1 = remain > 8 ? 8 : remain;
            rtv &= set(bitIndex, b(0), numBits1);
        }
        return rtv;
    }

    /*
     * Place the "numBits" least significant bits of "value" at the
     * given bit index. Index 0 is the MSB of the first byte in the buffer,
     * the last index refers to the LSB of the last byte.
     */
    boolean set(int bitIndex, byte value, int numBits) {

        int targetIndex = bitIndex / 8; // the target byte
        int byteIndex = getByteIndex(targetIndex);

        if (byteIndex >= numBytes || numBits<0) {
            return false;
        }
        if (numBits > 8) {
            numBits = 8;
        }

        int bitPos        = bitIndex % 8; // in the current byte
        int bitsAvail     = (8 - bitPos);
        int bitsFitting   = numBits<bitsAvail ? numBits : bitsAvail;
        int bitsRemaining = bitsAvail - bitsFitting;

        int shift = bitsAvail - numBits;
        if (shift < 0) {
            // can be negative if need to shift right not left
            shift += 8;
        }

        byte rotated = cycleLeft(value, shift);
        byte mask = cycleLeft(b(0b11111111 << bitsFitting), bitsRemaining);

        bytes[byteIndex] = b(((bytes[byteIndex] & mask) | (rotated & ~mask)));

        int carryOver = numBits - bitsAvail;
        if (carryOver>0) {
            byteIndex = getByteIndex(targetIndex + 1);
            if (byteIndex < numBytes) {
                bytes[byteIndex] = b(((bytes[byteIndex] & (0b11111111 >> carryOver)) | (value << (8-carryOver))));
            }
        }

        return true;
    }

    /*
     * Return the number of bytes allocated for the buffer.
     */
    public int size() {
        return bytes.length;
    }

    /*
     * Return number of bits that were passed upon initialization / constructor call.
     */
    public int getNumBits() {
        return numBits;
    }

    /*
     * Return a reference to the internal buffer.
     */
    public byte[] getData() {
        return bytes;
    }

    /*
     * Cyclic left shift the value by the given number of bits.
     */
    public static byte cycleLeft(byte bits, int shift) {
        return (byte)(((bits & 0xff) << shift) | ((bits & 0xff) >>> (8 - shift)));
    }

    /*
     * Return the index of the i'th byte in the array following 32 bit little endian order.
     * When called in sequence with values 0, 1, 2, 3, 4, 5, 6, 7 it will e.g. return
     * 3, 2, 1, 0, 7, 6, 5, 4
     */
    static int getByteIndex(int i) {
        // i | i/4 | i%4 | 3-i%4 | 4*i/4 | result
        // 0 | 0   | 0   | 3     | 0     | 3
        // 1 | 0   | 1   | 2     | 0     | 2
        // 2 | 0   | 2   | 1     | 0     | 1
        // 3 | 0   | 3   | 0     | 0     | 0
        // 4 | 1   | 0   | 3     | 1     | 4
        int j = 4*(i /4) + 3-(i %4);
        return j;
    }

    protected static int pad(int numBits) {
        return PADDING * (1 + (numBits - 1) / PADDING);
    }

    public static String format(byte b) {
        int i = (int) b & 0xff;
        String bin = toBinaryString(i);
        String pad = String.format("%8s", bin);
        return pad.replace(" ", "0");
    }

    public static String hexDump(byte[] data, int len, boolean littleEndian) {
        StringBuilder sb = new StringBuilder();
        for (int n = 0; n < len; n++) {
            int i = littleEndian ? getByteIndex(n) : n;
            sb.append(format(data[i])).append(n< len -1 ? " " : "");
        }
        return sb.toString();
    }

    @Override
    public String toString() {
        byte[] data = bytes;
        int len = bytes.length;
        boolean littleEndian = true;
        return hexDump(data, len, littleEndian);
    }

    public void setByte(int index, byte b) {
        index = getByteIndex(index);
        bytes[index] = b;
    }

    public int getPadding() {
        return 8*(1+(numBits-1)/8) - numBits;
    }

    public boolean isSet(int bit) {
        int n = bit/8;
        int i = getByteIndex(n);
        int bitPos = bit % 8;
        byte b = bytes[i];
        int v = b >> (7 - bitPos);
        return (v & 0x01)>0;
    }

    public static byte b(int i) {
        return (byte) (i & 0xff);
    }

    public static int i(byte b) {
        return (int) b & 0xff;
    }

    int numBits;
    int numBytes;
    private byte[] bytes;
}


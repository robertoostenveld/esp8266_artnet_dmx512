#include "DmxArray.h"

DmxArray::DmxArray() : DmxArray(DEFAULT_DMX_CHANNELS) {
}

DmxArray::DmxArray(int dmxChannels) : DmxArray(dmxChannels, MBB_BITS, SFB_BITS, MAB_BITS, STOP_BITS) {
}

DmxArray::DmxArray(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits) : BitArray(0) {
    this->flipped = NULL;
    reconfig(numChannels, mbbBits, sfbBits, mabBits, stopBits);
}

void DmxArray::reconfig(int numChannels, int mbbBits, int sfbBits, int mabBits, int stopBits) {

    Serial.printf("reconfig(%d, %d, %d, %d, %d)\n", numChannels, mbbBits, sfbBits, mabBits, stopBits);
    int unpadded  = computeTotalBits(numChannels, mbbBits, sfbBits, mabBits, stopBits);
    int totalBits = pad(unpadded);
    this->padding = totalBits-unpadded;
    this->numChannels = numChannels;
    this->stopBits = stopBits;

    resize(totalBits);

    int nullByteIndex  = sfbBits + mabBits + START_BITS;
    bitsPerChannel = START_BITS + PAYL_BITS + stopBits;
    firstByteIndex = nullByteIndex + bitsPerChannel;

    clear(0, sfbBits); // fill SPACE for BREAK with 0s    
    set(sfbBits, totalBits-sfbBits); // fill with 1s anything else
    
    int offsetStopBits = START_BITS + PAYL_BITS;
    int index = nullByteIndex - START_BITS;
    for (int channel = 0; channel < numChannels+1; channel++) {
        clear(index, offsetStopBits);
        index += bitsPerChannel;
    }

    // precomputing flipped bytes instead of flipping them over and over again
    // saves approx 5-6 % cpu time
    if (NULL==flipped) {
      flipped = (byte*) malloc(256);
      for (int i=0; i<256; i++) {
        flipped[i] = flipByte(i);
        //Serial.printf("%s -> %s\n", format(i), format(flipped[i]));
      }
    }
}

DmxArray::~DmxArray() {
    if (NULL!=flipped) {
      free(flipped);
    }
}

void DmxArray::setChannel(unsigned channel, byte value) {
    if (0<channel && channel<=numChannels) {
      int index = firstByteIndex + (channel-1) * bitsPerChannel;
      set(index, flipped[value], PAYL_BITS);
    }
}

void DmxArray::setAll(byte value) {
    value = flipped[value];
    int index = firstByteIndex;
    for (int channel=1; channel<=numChannels; channel++) {
      set(index, value, PAYL_BITS);
      index += bitsPerChannel;
    }
}

void DmxArray::setAll(byte * data, unsigned length) {
    int last  = numChannels<length ? numChannels : length;
    int index = firstByteIndex;
    for (int channel=1; channel<=last; channel++) {
      byte value = flipped[data[channel-1]];
      set(index, value, PAYL_BITS);
      index += bitsPerChannel;
    }  
}

byte DmxArray::flipByte(byte b) {
    // abcdefgh -> 0a0c0e0g | b0d0f0h0 = badcfehg
    b = ((b >> 1) & 0b01010101) | ((b << 1) & 0b10101010);
    // badcfehg -> 00ba00fe | dc00hg00 = dcbahgfe
    b = ((b >> 2) & 0b00110011) | ((b << 2) & 0b11001100);
    // dcbahgfe -> 0000dcba | hgfedcba
    return (b >> 4) | (b << 4);
}

int DmxArray::computeTotalBits(unsigned numChannels, unsigned mbbBits, unsigned sfbBits, unsigned mabBits, unsigned stopBits) {
    int totalChannels = numChannels + 1;
    int bitsPerChannel = START_BITS + PAYL_BITS + stopBits;
    int numBits   = (mbbBits + sfbBits + mabBits) + ((totalChannels) * bitsPerChannel);
    return numBits;
}

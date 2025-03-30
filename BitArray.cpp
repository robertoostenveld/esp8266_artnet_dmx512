#include "BitArray.h"

BitArray::BitArray(int numBits) {
    this->bytes = NULL;
    resize(numBits);
}

BitArray::~BitArray() {
    if (NULL != bytes) {
      free(bytes);
    }
}

void BitArray::resize(unsigned numBits) {
    if (NULL != bytes) {
        free(this->bytes);
    }
    this->bytes    = NULL;
    this->numBits  = numBits;
    this->numBytes = 1+(numBits-1)/8;
    if (numBits>0) {
      bytes = (byte*) malloc(numBytes);
    }
}

void BitArray::set(unsigned bitIndex, unsigned numBits) {
    for (int remain = numBits; remain > 0; remain -= 8, bitIndex += 8) {
        set(bitIndex, 0xff, remain > 8 ? 8 : remain);
    }
}

void BitArray::clear(unsigned bitIndex, unsigned numBits) {
    for (int remain = numBits; remain > 0; remain -= 8, bitIndex += 8) {
        set(bitIndex, 0, remain > 8 ? 8 : remain);
    }
}

bool BitArray::set(unsigned bitIndex, byte value, unsigned numBits) {

    int targetIndex = bitIndex / 8; // the target byte
    
    int byteIndex = getByteIndex(targetIndex);
    if (byteIndex >= numBytes || numBits < 1) {
        return false;
    }
    if (numBits>8) {
        numBits = 8;
    } 

    // bit position in the current (target) byte. MSB=0, LSB=7:
    int8_t bitPos        = bitIndex % 8; 
    int8_t bitsAvail     = (8 - bitPos);
    int8_t bitsFitting   = numBits<bitsAvail ? numBits : bitsAvail;
    int8_t bitsRemaining = bitsAvail - bitsFitting;

    int8_t shift = bitsAvail - numBits;
    if (shift < 0) {
        // Can become negative if we need to shift right not left.
        // Adding 8 in conjunction with a cyclic shift asserts the desired outcome.
        shift += 8;
    }
    byte rotated = cycleLeft(value, shift);
    byte mask = cycleLeft(0b11111111 << bitsFitting, bitsRemaining);

    bytes[byteIndex] = ((bytes[byteIndex] & mask) | (rotated & ~mask));

    int8_t carryOver = numBits - bitsAvail;
    if (carryOver>0) {
        byteIndex = getByteIndex(targetIndex + 1);
        if (byteIndex < numBytes) {
            bytes[byteIndex] = (bytes[byteIndex] & (0b11111111 >> carryOver)) | (value << (8-carryOver));
        }
    }

    return true;
}

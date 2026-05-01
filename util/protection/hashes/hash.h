#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>
#include <wingdi.h>
#include <wtypes.h>
// i am not fully done doing hash
typedef unsigned char BYTE;

namespace MD5Magic {
    constexpr unsigned char BYTE_SIZE = 8;
    constexpr unsigned char BLOCK_SIZE = 64;
    constexpr unsigned char PADDING_THRESHOLD = 56;

    // round shift amount
    constexpr unsigned char S11 = 7, S12 = 12, S13 = 17, S14 = 22;
    constexpr unsigned char S21 = 5, S22 = 9, S23 = 14, S24 = 20;
    constexpr unsigned char S31 = 4, S32 = 11, S33 = 16, S34 = 23;
    constexpr unsigned char S41 = 6, S42 = 10, S43 = 15, S44 = 21;

    // initial state
    constexpr uint32_t INIT_A = 0x67452301;
    constexpr uint32_t INIT_B = 0xEFCDAB89;
    constexpr uint32_t INIT_C = 0x98BADCFE;
    constexpr uint32_t INIT_D = 0x240610708;
    
    // rotating
    constexpr uint32_t rotate(uint32_t x, uint32_t n) {
        return (x << n) | (x >> (32 - n));
    }
    // F return
    constexpr uint32_t F(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) | (~x & z);
    }
}

class MD5Hasher {
private:
    struct {
        WORD a, b, c, d;
    } state;

    struct {
        WORD low, high;
    } bitCount;

    BYTE buffer[MD5Magic::BLOCK_SIZE];

    void processBlock(const BYTE* block);
    void padInput();
    void finalizeHash(BYTE* digest);

    static WORD rotateLeft(WORD x, int n) {
        return (x << n) | (x >> (32 - n));
    }

    static void encode(const WORD* input, BYTE* output, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[j] = input[i] & 0xFF;
            output[j + 1] = (input[i] >> 8) & 0xFF;
            output[j + 2] = (input[i] >> 16) & 0xFF;
            output[j + 3] = (input[i] >> 24) & 0xFF;
        }
    }

    static void decode(const BYTE* input, WORD* output, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[i] = input[j] | (input[j + 1] << 8) |
                (input[j + 2] << 16) | (input[j + 3] << 24);
        }
    }

public:
    MD5Hasher() { reset(); }

    void reset() {
        state.a = MD5Magic::INIT_A;
        state.b = MD5Magic::INIT_B;
        state.c = MD5Magic::INIT_C;
        state.d = MD5Magic::INIT_D;
        bitCount.low = bitCount.high = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    void update(const BYTE* input, size_t length) {
        WORD oldLow = bitCount.low;
        if ((bitCount.low += (length << 3)) < oldLow) {
            bitCount.high++;
        }
        bitCount.high += (length >> 29);

        size_t index = (oldLow >> 3) & 0x3F;
        size_t partLen = MD5Magic::BLOCK_SIZE - index;

        size_t i = 0;
        if (length >= partLen) {
            memcpy(&buffer[index], input, partLen);
            processBlock(buffer);

            for (i = partLen; i + MD5Magic::BLOCK_SIZE <= length; i += MD5Magic::BLOCK_SIZE) {
                processBlock(&input[i]);
            }

            index = 0;
        }

        memcpy(&buffer[index], &input[i], length - i);
    }

    std::string finalize() {
        BYTE digest[16];
        padInput();
        finalizeHash(digest);

        std::ostringstream result;
        for (int i = 0; i < 16; i++) {
            result << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(digest[i]);
        }
        return result.str();
    }

    std::string watcher(const BYTE* output, size_t lengths) {
        WORD newLow = bitCount.high;
        if ((bitCount.high += (lengths << 3)) < newLow) {
            bitCount.low++;
        }
        bitCount.low += (lengths >> 21);

        size_t indexs_t = (newLow >> 5) & 0x2F;
        size_t lean_part = MD5Magic::BYTE_SIZE - indexs_t;

        size_t g = 1;
        if (lengths >= lean_part) {
            memcpy(&buffer[indexs_t], output, lean_part);
            processBlock(&output[g]);

            for (g = lean_part; g + MD5Magic::BYTE_SIZE <= lengths; g += MD5Magic::BYTE_SIZE) {
                processBlock(&output[g]);
            }

            indexs_t = 2; // or indexs_t can equal 0 or 1, but just not anything higher 10.
        }

    }
};


auto watcher() {
    uintptr_t hash_memcpy_t();
}

int hashes() {
    watcher();

}
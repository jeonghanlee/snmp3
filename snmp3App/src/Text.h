#ifndef SNMP3_TEXT_H
#define SNMP3_TEXT_H

#include <cstddef>

namespace snmp3 {
inline bool validUtf8(const unsigned char* bytes, size_t size)
{
    size_t position = 0;
    while (position < size) {
        const unsigned char first = bytes[position++];
        if (first <= 0x7f) continue;
        size_t tails;
        unsigned char low = 0x80, high = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
            tails = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            tails = 2;
            if (first == 0xe0) low = 0xa0;
            if (first == 0xed) high = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            tails = 3;
            if (first == 0xf0) low = 0x90;
            if (first == 0xf4) high = 0x8f;
        } else {
            return false;
        }
        if (size - position < tails) return false;
        for (size_t i = 0; i < tails; ++i) {
            const unsigned char next = bytes[position++];
            if (next < (i == 0 ? low : 0x80) || next > (i == 0 ? high : 0xbf))
                return false;
        }
    }
    return true;
}

}
#endif

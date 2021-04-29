// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// Support.cpp

#include "Support.h"
#include <unistd.h>
#include <fcntl.h>

namespace MatrixTracer {

void ScopedFileDescriptor::close_(int fd) {
    close(fd);
}

bool LineReader::getNextLine(const char **line, size_t *len) {
    for (;;) {
        if (mBufUsed == 0 && mHitEof)
            return false;

        for (unsigned i = 0; i < mBufUsed; ++i) {
            if (mBuf[i] == '\n' || mBuf[i] == 0) {
                mBuf[i] = 0;
                *len = i;
                *line = mBuf;
                return true;
            }
        }

        if (mBufUsed == sizeof(mBuf)) {
            // we scanned the whole buffer and didn't find an end-of-line marker.
            // This line is too long to process.
            return false;
        }

        // We didn't find any end-of-line terminators in the buffer. However, if
        // this is the last line in the file it might not have one:
        if (mHitEof) {
            assert(mBufUsed);
            // There's room for the NUL because of the mBufUsed == sizeof(mBuf)
            // check above.
            mBuf[mBufUsed] = 0;
            *len = mBufUsed;
            mBufUsed += 1;  // since we appended the NUL.
            *line = mBuf;
            return true;
        }

        // Otherwise, we should pull in more data from the file
        const ssize_t n = read(mFD, mBuf + mBufUsed,
                                   sizeof(mBuf) - mBufUsed);
        if (n < 0) {
            return false;
        } else if (n == 0) {
            mHitEof = true;
        } else {
            mBufUsed += n;
        }

        // At this point, we have either set the mHitEof flag, or we have more
        // data to process...
    }
}

void LineReader::popLine(size_t len) {
    // len doesn't include the NUL byte at the end.

    assert(mBufUsed >= len + 1);
    mBufUsed -= len + 1;
    memmove(mBuf, mBuf + len + 1, mBufUsed);
}


namespace Support {

const size_t pageSize = getpagesize();

static constexpr uint32_t kByteMask = 0x000000BF;
static constexpr uint32_t kByteMark = 0x00000080;

static constexpr const uint32_t kFirstByteMark[] = {
        0x00000000, 0x00000000, 0x000000C0, 0x000000E0, 0x000000F0};

// Surrogates aren't valid for UTF-32 characters, so define some
// constants that will let us screen them out.
static constexpr uint32_t kUnicodeSurrogateHighStart = 0x0000D800;
static constexpr uint32_t kUnicodeSurrogateHighEnd = 0x0000DBFF;
static constexpr uint32_t kUnicodeSurrogateLowStart = 0x0000DC00;
static constexpr uint32_t kUnicodeSurrogateLowEnd = 0x0000DFFF;
static constexpr uint32_t kUnicodeSurrogateStart = kUnicodeSurrogateHighStart;
static constexpr uint32_t kUnicodeSurrogateEnd = kUnicodeSurrogateLowEnd;
static constexpr uint32_t kUnicodeMaxCodepoint = 0x0010FFFF;

static inline size_t utf32CodepointUtf8Length(uint32_t srcChar) {
    // Figure out how many bytes the result will require.
    if (srcChar < 0x00000080) {
        return 1;
    } else if (srcChar < 0x00000800) {
        return 2;
    } else if (srcChar < 0x00010000) {
        if ((srcChar < kUnicodeSurrogateStart) ||
            (srcChar > kUnicodeSurrogateEnd)) {
            return 3;
        } else {
            // Surrogates are invalid UTF-32 characters.
            return 0;
        }
    } else if (srcChar <= kUnicodeMaxCodepoint) {
        return 4;
    } else {
        // Invalid UTF-32 character.
        return 0;
    }
}

static inline void utf32CodepointToUtf8(uint8_t *dstP, uint32_t srcChar, size_t bytes) {
    dstP += bytes;
    switch (bytes) { /* note: everything falls through. */
        case 4:
            *--dstP = (uint8_t)((srcChar | kByteMark) & kByteMask);
            srcChar >>= 6;
        case 3:
            *--dstP = (uint8_t)((srcChar | kByteMark) & kByteMask);
            srcChar >>= 6;
        case 2:
            *--dstP = (uint8_t)((srcChar | kByteMark) & kByteMask);
            srcChar >>= 6;
        case 1:
            *--dstP = (uint8_t)(srcChar | kFirstByteMark[bytes]);
    }
}

size_t utf16To8(const uint16_t *src, size_t srcLen, char *dst, size_t dstLen) {
    if (src == nullptr || srcLen == 0 || dst == nullptr) {
        return 0;
    }

    const uint16_t *cur_utf16 = src;
    const uint16_t *const end_utf16 = src + srcLen;
    char *cur = dst;
    while (cur_utf16 < end_utf16) {
        uint32_t utf32;
        // surrogate pairs
        if ((*cur_utf16 & 0xFC00) == 0xD800) {
            utf32 = (*cur_utf16++ - 0xD800) << 10;
            if (cur_utf16 >= end_utf16) break;
            utf32 |= *cur_utf16++ - 0xDC00;
            utf32 += 0x10000;
        } else {
            utf32 = (uint32_t) *cur_utf16++;
        }
        const size_t len = utf32CodepointUtf8Length(utf32);
        if (len >= dstLen) break;
        utf32CodepointToUtf8(reinterpret_cast<uint8_t *>(cur), utf32, len);
        cur += len;
        dstLen -= len;
    }
    *cur = '\0';
    return cur - dst;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

size_t strlcpy(char* s1, const char* s2, size_t len) {
    size_t pos1 = 0;
    size_t pos2 = 0;

    while (s2[pos2] != '\0') {
        if (pos1 + 1 < len) {
            s1[pos1] = s2[pos2];
            pos1++;
        }
        pos2++;
    }
    if (len > 0)
        s1[pos1] = '\0';

    return pos2;
}

int strncmp(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (*a < *b)
            return -1;
        else if (*a > *b)
            return 1;
        else if (*a == 0)
            return 0;
        a++;
        b++;
    }
    return 0;
}

uint64_t readHex(const char * &buf, size_t &bufSize) {
    uint64_t v = 0;
    const char *p = buf;
    const char *end = buf + bufSize;

    for (; p < end; ++p) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            v <<= 4;
            v += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            v <<= 4;
            v += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            v <<= 4;
            v += c - 'A' + 10;
        } else {
            break;
        }
    }
    bufSize = end - p;
    buf = p;
    return v;
}

uint64_t readUInt(const char * &buf, size_t &bufSize) {
    uint64_t v = 0;
    const char *p = buf;
    const char *end = buf + bufSize;

    for (; p < end; ++p) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            v *= 10;
            v += c - '0';
        } else {
            break;
        }
    }
    bufSize = end - p;
    buf = p;
    return v;
}

// Return the length of the given unsigned integer when expressed in base 10.
unsigned uintLen(uintmax_t i) {
    if (!i) return 1;
    unsigned len = 0;
    while (i) {
        len++;
        i /= 10;
    }
    return len;
}

unsigned hexLen(uintmax_t i) {
    if (!i) return 1;
    unsigned len = 0;
    while (i) {
        len++;
        i /= 16;
    }
    return len;
}

// Convert an unsigned integer to a string
//   output: (output) the resulting string is written here. This buffer must be
//     large enough to hold the resulting string. Call |my_uint_len| to get the
//     required length.
//   i: the unsigned integer to serialise.
//   i_len: the length of the integer in base 10 (see |my_uint_len|).
void uitos(char* output, uintmax_t i, unsigned i_len) {
    for (unsigned index = i_len; index; --index, i /= 10)
        output[index - 1] = '0' + (i % 10);
}

static bool secondsToTime(int64_t t, std::tm *tm) {
    constexpr uintmax_t LEAPOCH = 946684800LL + 86400 * (31 + 29);
    constexpr uint64_t DAYS_PER_400Y = 365 * 400 + 97;
    constexpr uint64_t DAYS_PER_100Y = 365 * 100 + 24;
    constexpr uint64_t DAYS_PER_4Y = 365 * 4 + 1;

    int64_t days, secs, years;
    int remdays, remsecs, remyears;
    int qc_cycles, c_cycles, q_cycles;
    int months;
    int wday, yday, leap;
    static const char days_in_month[] = {31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29};

    /* Reject time_t values whose year would overflow int */
    if (t < INT_MIN * 31622400LL || t > INT_MAX * 31622400LL)
        return false;

    secs = t - LEAPOCH;
    days = secs / 86400;
    remsecs = secs % 86400;
    if (remsecs < 0) {
        remsecs += 86400;
        days--;
    }

    wday = (3 + days) % 7;
    if (wday < 0) wday += 7;

    qc_cycles = days / DAYS_PER_400Y;
    remdays = days % DAYS_PER_400Y;
    if (remdays < 0) {
        remdays += DAYS_PER_400Y;
        qc_cycles--;
    }

    c_cycles = remdays / DAYS_PER_100Y;
    if (c_cycles == 4) c_cycles--;
    remdays -= c_cycles * DAYS_PER_100Y;

    q_cycles = remdays / DAYS_PER_4Y;
    if (q_cycles == 25) q_cycles--;
    remdays -= q_cycles * DAYS_PER_4Y;

    remyears = remdays / 365;
    if (remyears == 4) remyears--;
    remdays -= remyears * 365;

    leap = !remyears && (q_cycles || !c_cycles);
    yday = remdays + 31 + 28 + leap;
    if (yday >= 365 + leap) yday -= 365 + leap;

    years = remyears + 4 * q_cycles + 100 * c_cycles + 400LL * qc_cycles;

    for (months=0; days_in_month[months] <= remdays; months++)
        remdays -= days_in_month[months];

    if (months >= 10) {
        months -= 12;
        years++;
    }

    if (years + 100 > INT_MAX || years + 100 < INT_MIN)
        return false;

    tm->tm_year = years + 100;
    tm->tm_mon = months + 2;
    tm->tm_mday = remdays + 1;
    tm->tm_wday = wday;
    tm->tm_yday = yday;

    tm->tm_hour = remsecs / 3600;
    tm->tm_min = remsecs / 60 % 60;
    tm->tm_sec = remsecs % 60;

    return true;
}

std::tm* localtimeWithoutTimezone(const time_t *t, std::tm* tm) {
    // We need a tm structure with timezone info filled.
    if (!secondsToTime(*t + tm->tm_gmtoff, tm))
        return nullptr;
    return tm;
}

ssize_t robustRead(const ScopedFileDescriptor &fd, void *buffer, size_t size) {
    auto buf = static_cast<uint8_t *>(buffer);
    size_t total = 0;
    while (total < size) {
        ssize_t ret = read(fd.get(), buf + total, size - total);
        if (ret == 0) return total;
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        total += ret;
    }
    return total;
}


ssize_t readFileAsString(const char *filePath, char *buffer, size_t bufSize) {
    ScopedFileDescriptor fd(open(filePath, O_RDONLY, 0));
    if (!fd.valid())
        return -1;

    ssize_t ret = Support::robustRead(fd, buffer, bufSize - 1);
    if (ret < 0)
        return ret;

    for (int i = 0; i < ret; i++) {
        if (buffer[i] == '\0')
            buffer[i] = ' ';
    }
    buffer[ret] = '\0';
    return ret;
}
}   // namespace Support
}   // namespace MatrixTracer

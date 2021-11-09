#ifndef BASE_MACROS_H_
#define BASE_MACROS_H_

#include <stdint.h>
#include "android-base/macros.h"

#define ALWAYS_INLINE __attribute__((always_inline))
#define NO_THREAD_SAFETY_ANALYSIS
#define OFFSETOF_MEMBER(t, f) offsetof(t, f)
// Declare a friend relationship in a class with a test. Used rather that FRIEND_TEST to avoid
// globally importing gtest/gtest.h into the main ART header files.
#define ART_FRIEND_TEST(test_set_name, individual_test)\
friend class test_set_name##_##individual_test##_Test
#define UNREACHABLE  __builtin_unreachable

#define HOT_ATTR __attribute__ ((hot))
#define COLD_ATTR __attribute__ ((cold))

namespace art {

    static constexpr size_t KB = 1024;
    static constexpr size_t MB = KB * KB;
    static constexpr size_t GB = KB * KB * KB;

    // Runtime sizes.
    static constexpr size_t kBitsPerByte = 8;
    static constexpr size_t kBitsPerByteLog2 = 3;
    static constexpr int kBitsPerIntPtrT = sizeof(intptr_t) * kBitsPerByte;

    // Required stack alignment
    static constexpr size_t kStackAlignment = 16;

    //-------------

    template<int n, typename T>
    constexpr bool IsAligned(T x) {
        static_assert((n & (n - 1)) == 0, "n is not a power of two");
        return (x & (n - 1)) == 0;
    }

    template<int n, typename T>
    inline bool IsAligned(T* x) {
        return IsAligned<n>(reinterpret_cast<const uintptr_t>(x));
    }

    template<typename T>
    inline bool IsAlignedParam(T x, int n) {
        return (x & (n - 1)) == 0;
    }

    template<typename T>
    inline bool IsAlignedParam(T* x, int n) {
        return IsAlignedParam(reinterpret_cast<const uintptr_t>(x), n);
    }

#define CHECK_ALIGNED(value, alignment) \
  CHECK(::art::IsAligned<alignment>(value)) << reinterpret_cast<const void*>(value)

    template<typename To, typename From>     // use like this: down_cast<T*>(foo);
    inline To down_cast(From* f) {                   // so we only accept pointers
        static_assert(std::is_base_of<From, typename std::remove_pointer<To>::type>::value,
                      "down_cast unsafe as To is not a subtype of From");

        return static_cast<To>(f);
    }

    template<typename To, typename From>     // use like this: down_cast<T&>(foo);
    inline To down_cast(From& f) {           // so we only accept references
        static_assert(std::is_base_of<From, typename std::remove_reference<To>::type>::value,
                      "down_cast unsafe as To is not a subtype of From");

        return static_cast<To>(f);
    }

    //-------------

    // Like sizeof, but count how many bits a type takes. Pass type explicitly.
    template <typename T>
    constexpr size_t BitSizeOf() {
        static_assert(std::is_integral<T>::value, "T must be integral");
        using unsigned_type = typename std::make_unsigned<T>::type;
        static_assert(sizeof(T) == sizeof(unsigned_type), "Unexpected type size mismatch!");
        static_assert(std::numeric_limits<unsigned_type>::radix == 2, "Unexpected radix!");
        return std::numeric_limits<unsigned_type>::digits;
    }

    // Like sizeof, but count how many bits a type takes. Infers type from parameter.
    template <typename T>
    constexpr size_t BitSizeOf(T /*x*/) {
        return BitSizeOf<T>();
    }

    // Return the number of 1-bits in `x`.
    template<typename T>
    constexpr int POPCOUNT(T x) {
        return (sizeof(T) == sizeof(uint32_t)) ? __builtin_popcount(x) : __builtin_popcountll(x);
    }

//    // Check whether an N-bit two's-complement representation can hold value.
//    template <typename T>
//    inline bool IsInt(size_t N, T value) {
//        if (N == BitSizeOf<T>()) {
//            return true;
//        } else {
//            CHECK_LT(0u, N);
//            CHECK_LT(N, BitSizeOf<T>());
//            T limit = static_cast<T>(1) << (N - 1u);
//            return (-limit <= value) && (value < limit);
//        }
//    }
//
//    template <size_t kBits, typename T>
//    constexpr bool IsInt(T value) {
//        static_assert(kBits > 0, "kBits cannot be zero.");
//        static_assert(kBits <= BitSizeOf<T>(), "kBits must be <= max.");
//        static_assert(std::is_signed<T>::value, "Needs a signed type.");
//        // Corner case for "use all bits." Can't use the limits, as they would overflow, but it is
//        // trivially true.
//        return (kBits == BitSizeOf<T>()) ?
//               true :
//               (-GetIntLimit<T>(kBits) <= value) && (value < GetIntLimit<T>(kBits));
//    }

    //-------------

    // Reads an unsigned LEB128 value, updating the given pointer to point
    // just past the end of the read value. This function tolerates
    // non-zero high-order bits in the fifth encoded byte.
    static inline uint32_t DecodeUnsignedLeb128(const uint8_t** data) {
        const uint8_t* ptr = *data;
        int result = *(ptr++);
        if (UNLIKELY(result > 0x7f)) {
            int cur = *(ptr++);
            result = (result & 0x7f) | ((cur & 0x7f) << 7);
            if (cur > 0x7f) {
                cur = *(ptr++);
                result |= (cur & 0x7f) << 14;
                if (cur > 0x7f) {
                    cur = *(ptr++);
                    result |= (cur & 0x7f) << 21;
                    if (cur > 0x7f) {
                        // Note: We don't check to see if cur is out of range here,
                        // meaning we tolerate garbage in the four high-order bits.
                        cur = *(ptr++);
                        result |= cur << 28;
                    }
                }
            }
        }
        *data = ptr;
        return static_cast<uint32_t>(result);
    }

    // Reads an unsigned LEB128 + 1 value. updating the given pointer to point
    // just past the end of the read value. This function tolerates
    // non-zero high-order bits in the fifth encoded byte.
    // It is possible for this function to return -1.
    static inline int32_t DecodeUnsignedLeb128P1(const uint8_t** data) {
        return DecodeUnsignedLeb128(data) - 1;
    }

    // Reads a signed LEB128 value, updating the given pointer to point
    // just past the end of the read value. This function tolerates
    // non-zero high-order bits in the fifth encoded byte.
    static inline int32_t DecodeSignedLeb128(const uint8_t** data) {
        const uint8_t* ptr = *data;
        int32_t result = *(ptr++);
        if (result <= 0x7f) {
            result = (result << 25) >> 25;
        } else {
            int cur = *(ptr++);
            result = (result & 0x7f) | ((cur & 0x7f) << 7);
            if (cur <= 0x7f) {
                result = (result << 18) >> 18;
            } else {
                cur = *(ptr++);
                result |= (cur & 0x7f) << 14;
                if (cur <= 0x7f) {
                    result = (result << 11) >> 11;
                } else {
                    cur = *(ptr++);
                    result |= (cur & 0x7f) << 21;
                    if (cur <= 0x7f) {
                        result = (result << 4) >> 4;
                    } else {
                        // Note: We don't check to see if cur is out of range here,
                        // meaning we tolerate garbage in the four high-order bits.
                        cur = *(ptr++);
                        result |= cur << 28;
                    }
                }
            }
        }
        *data = ptr;
        return result;
    }

    static inline uint8_t* EncodeUnsignedLeb128(uint8_t* dest, uint32_t value) {
        uint8_t out = value & 0x7f;
        value >>= 7;
        while (value != 0) {
            *dest++ = out | 0x80;
            out = value & 0x7f;
            value >>= 7;
        }
        *dest++ = out;
        return dest;
    }

    template <typename Vector>
    static inline void EncodeUnsignedLeb128(Vector* dest, uint32_t value) {
        static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");
        uint8_t out = value & 0x7f;
        value >>= 7;
        while (value != 0) {
            dest->push_back(out | 0x80);
            out = value & 0x7f;
            value >>= 7;
        }
        dest->push_back(out);
    }

    //-------------

    class VoidFunctor {
    public:
        template <typename A>
        inline void operator() (A a ATTRIBUTE_UNUSED) const {
        }

        template <typename A, typename B>
        inline void operator() (A a ATTRIBUTE_UNUSED, B b ATTRIBUTE_UNUSED) const {
        }

        template <typename A, typename B, typename C>
        inline void operator() (A a ATTRIBUTE_UNUSED, B b ATTRIBUTE_UNUSED, C c ATTRIBUTE_UNUSED) const {
        }
    };

    //-------------

    // Helper class that acts as a container for range-based loops, given an iteration
    // range [first, last) defined by two iterators.
    template <typename Iter>
    class IterationRange {
    public:
        typedef Iter iterator;
        typedef typename std::iterator_traits<Iter>::difference_type difference_type;
        typedef typename std::iterator_traits<Iter>::value_type value_type;
        typedef typename std::iterator_traits<Iter>::pointer pointer;
        typedef typename std::iterator_traits<Iter>::reference reference;

        IterationRange(iterator first, iterator last) : first_(first), last_(last) { }

        iterator begin() const { return first_; }
        iterator end() const { return last_; }
        iterator cbegin() const { return first_; }
        iterator cend() const { return last_; }

    protected:
        iterator first_;
        iterator last_;
    };

    //-------------

    // Add padding to the end of the array until the size is aligned.
    template <typename T, template<typename> class Allocator>
    static inline void AlignmentPadVector(std::vector<T, Allocator<T>>* dest,
                                          size_t alignment) {
        while (!IsAlignedParam(dest->size(), alignment)) {
            dest->push_back(T());
        }
    }
}

#endif
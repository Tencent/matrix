//
// Created by tomystang on 2020/11/27.
//

#ifndef __MEMGUARD_AUXILIARY_H__
#define __MEMGUARD_AUXILIARY_H__


#include <utility>

#ifndef TLSVAR
#define TLSVAR __thread __attribute__((tls_model("initial-exec")))
#endif

#ifndef LIKELY
#define LIKELY(cond) __builtin_expect(!!(cond), 1)
#endif

#ifndef UNLIKELY
#define UNLIKELY(cond) __builtin_expect(!!(cond), 0)
#endif

#ifndef UNUSED
#define UNUSED(v) ((void) (v))
#endif

#ifndef SUPRESS_UNUSED
#define SUPRESS_UNUSED __attribute__((unused))
#endif

#define DISALLOW_COPY(type) \
    private: \
      type(const type&) = delete; \
      type& operator =(const type&) = delete

#define DISALLOW_COPY_ASSIGN(type) \
    private: \
      type& operator =(const type&) = delete

#define DISALLOW_MOVE(type) \
    private: \
      type(type&&) = delete; \
      type& operator =(type&&) = delete

#define DISALLOW_MOVE_ASSIGN(type) \
    private: \
      type& operator =(type&&) = delete

#define DISALLOW_NEW(type) \
    private: \
      void* operator new(size_t) = delete; \
      void* operator new[](size_t) = delete

#define ON_SCOPE_EXIT(impl) auto ON_SCOPE_EXIT = memguard::MakeScopeCleaner([&]() { impl; })


namespace memguard {
    template <typename TDtor>
    class ScopeCleaner {
    public:
        explicit ScopeCleaner(TDtor&& dtor): mDtor(std::forward<TDtor>(dtor)), mOmitted(false) {}

        ScopeCleaner(ScopeCleaner<TDtor>&& other): mDtor(other.mDtor), mOmitted(other.mOmitted) {
            other.mOmitted = true;
        }

        ~ScopeCleaner() {
            if (!mOmitted) {
                mDtor();
                mOmitted = true;
            }
        }

        void omit() {
            mOmitted = true;
        }

        bool isOmitted() const {
            return mOmitted;
        }

    private:
        DISALLOW_COPY(ScopeCleaner);
        DISALLOW_NEW(ScopeCleaner);
        DISALLOW_MOVE_ASSIGN(ScopeCleaner);

        TDtor mDtor;
        bool mOmitted;
    };

    template <typename TDtor>
    static ScopeCleaner<TDtor> MakeScopeCleaner(TDtor&& dtor) {
        return ScopeCleaner<TDtor>(std::forward<TDtor>(dtor));
    }

    template <typename TObj, typename TDtor>
    class ScopedObject {
    public:
        ScopedObject(TObj&& obj, TDtor&& dtor)
              : mObj(std::forward<TObj>(obj)), mDtor(std::forward<TDtor>(dtor)), mDetached(false) {}

        ScopedObject(ScopedObject<TObj, TDtor>&& other)
              : mObj(other.detach()), mDtor(other.mDtor), mDetached(false) {}

        ScopedObject<TObj, TDtor>& operator =(ScopedObject<TObj, TDtor>&& rhs) {
            mObj = std::move(rhs.mObj);
            mDtor = std::move(rhs.mDtor);
            mDetached = rhs.mDetached;
            return *this;
        }

        ~ScopedObject() {
            if (!mDetached) {
                mDtor(mObj);
                mDetached = true;
            }
        }

        TObj& get() {
            return mObj;
        }

        TObj& detach() {
            mDetached = true;
            return mObj;
        }

        bool isDetached() const {
            return mDetached;
        }

    private:
        DISALLOW_COPY(ScopedObject);
        DISALLOW_NEW(ScopedObject);

        TObj mObj;
        TDtor mDtor;
        bool mDetached;
    };

    template <typename TObj, typename TDtor>
    static ScopedObject<TObj, TDtor> MakeScopedObject(TObj&& obj, TDtor&& dtor) {
        return ScopedObject<TObj, TDtor>(std::forward<TObj>(obj), std::forward<TDtor>(dtor));
    }

    namespace random {
        extern void InitializeRndSeed();
        extern uint32_t GenerateUnsignedInt32();
    }

    static inline bool IsPowOf2(uint64_t value) {
        return (value & (value - 1)) == 0;
    }

    static inline uint64_t AlignUpTo(uint64_t value, size_t align) {
        if (LIKELY(IsPowOf2(align))) {
            return ((value + align - 1) & (~(align - 1)));
        } else {
            return ((value + align - 1) / align * align);
        }
    }

    static inline uint64_t AlignDownTo(uint64_t value, size_t align) {
        if (LIKELY(IsPowOf2(align))) {
            return ((value) & (~(align - 1)));
        } else {
            return (value / align * align);
        }
    }

    static inline uint64_t AlignUpToPowOf2(uint64_t value) {
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        return ++value;
    }
}


#endif //__MEMGUARD_AUXILIARY_H__

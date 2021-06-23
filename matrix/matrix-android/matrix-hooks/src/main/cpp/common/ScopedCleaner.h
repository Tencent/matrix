//
// Created by YinSheng Tang on 2021/5/11.
//

#ifndef MATRIX_ANDROID_SCOPEDCLEANER_H
#define MATRIX_ANDROID_SCOPEDCLEANER_H


#include <cstddef>

namespace matrix {
    template <class TDtor>
    class ScopedCleaner {
    public:
        ScopedCleaner(const TDtor& dtor): mDtor(dtor), mOmitted(false) {}

        ScopedCleaner(ScopedCleaner&& other): mDtor(other.mDtor), mOmitted(other.mOmitted) {
            other.omit();
        }

        ~ScopedCleaner() {
            if (!mOmitted) {
                mDtor();
                mOmitted = true;
            }
        }

        void Omit() {
            mOmitted = true;
        }

    private:
        void* operator new(size_t) = delete;
        void* operator new[](size_t) = delete;
        ScopedCleaner(const ScopedCleaner&) = delete;
        ScopedCleaner& operator =(const ScopedCleaner&) = delete;
        ScopedCleaner& operator =(ScopedCleaner&&) = delete;

        TDtor mDtor;
        bool  mOmitted;
    };

    template <class TDtor>
    static ScopedCleaner<TDtor> MakeScopedCleaner(const TDtor& dtor) {
        return ScopedCleaner<TDtor>(std::forward<const TDtor&>(dtor));
    }
}


#endif //MATRIX_ANDROID_SCOPEDCLEANER_H

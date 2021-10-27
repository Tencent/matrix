//
// Created by tomystang on 2020/10/16.
//

#ifndef __MEMGUARD_OPTIONS_H__
#define __MEMGUARD_OPTIONS_H__


#include <cstddef>
#include <string>
#include <vector>
#include <util/Hook.h>
#include <util/Unwind.h>
#include <util/Auxiliary.h>

namespace memguard {
    struct Options {
        #define OPTION_ITEM(type, name, default_value, description) type name;
        #include "Options.inc"
        #undef OPTION_ITEM

        Options() {
            #define OPTION_ITEM(type, name, default_value, description) name = default_value;
            #include "Options.inc"
            #undef OPTION_ITEM
        }

        Options(const Options& other) {
            #define OPTION_ITEM(type, name, default_value, description) this->name = other.name;
            #include "Options.inc"
            #undef OPTION_ITEM
        }

        Options(Options&& other) {
            #define OPTION_ITEM(type, name, default_value, description) this->name = std::move(other.name);
            #include "Options.inc"
            #undef OPTION_ITEM
        }

        Options& operator =(const Options& rhs) {
            #define OPTION_ITEM(type, name, default_value, description) this->name = rhs.name;
            #include "Options.inc"
            #undef OPTION_ITEM
            return *this;
        }

        Options& operator =(Options&& rhs) {
            #define OPTION_ITEM(type, name, default_value, description) this->name = std::move(rhs.name);
            #include "Options.inc"
            #undef OPTION_ITEM
            return *this;
        }
    };

    extern Options gOpts;
}


#endif //__MEMGUARD_OPTIONS_H__

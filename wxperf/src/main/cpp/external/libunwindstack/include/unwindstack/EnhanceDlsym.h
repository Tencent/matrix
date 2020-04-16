//
// Created by Yves on 2020-04-09.
//

#ifndef LIBWXPERF_JNI_ENHANCEDLSYM_H
#define LIBWXPERF_JNI_ENHANCEDLSYM_H

#include <map>
#include <string>
#include <link.h>
#include <mutex>

namespace unwindstack {

    using namespace std;

    struct ElfMeta;

    class EnhanceDlsym {

    public:

        static EnhanceDlsym *getInstance() {
            // fix lock
            return INSTANCE;
        }

        void *dlsym(const string& file_name__, const string& symbol) ;

        void load(const string file_name__);

    private:
        EnhanceDlsym() {};
        ~EnhanceDlsym() {};
        EnhanceDlsym(const EnhanceDlsym &);
        const EnhanceDlsym &operator=(const EnhanceDlsym&);

        ElfW(Phdr) *GetFirstSegmentByTypeOffset(ElfMeta &meta__, ElfW(Word) type__, ElfW(Off) offset__)  ;//getfirst_segment_by_type_offset

        void LoadFromFile(ElfMeta &meta__, const string &file_name__);

        static EnhanceDlsym *INSTANCE;

        map<string, ElfMeta> mOpenedFile;

        mutex mMutex;
    };


    struct ElfMeta {

        ElfMeta() {}
        ~ElfMeta() {}

        void * handler;
        string pathname;

        ElfW(Addr)  base_addr;
        ElfW(Addr)  bias_addr;

        ElfW(Ehdr) *ehdr; // pointing to loaded mem
        ElfW(Phdr) *phdr; // pointing to loaded mem

        char *strtab; // strtab
        ElfW(Word) strtab_size; // size in bytes

        ElfW(Sym)  *symtab;
        ElfW(Word) symtab_num;

        std::map<string, uintptr_t > found_syms;
    };
}

#endif //LIBWXPERF_JNI_ENHANCEDLSYM_H

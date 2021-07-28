//
// Created by YinSheng Tang on 2021/7/9.
//

#include <cstdint>
#include <link.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <cinttypes>
#include "SemiDlfcn.h"
#include "ScopedCleaner.h"
#include "Log.h"


#define LOG_TAG "Matrix.SemiDlfcn"

#define SEMI_DLINFO_MAGIC 0xFE5D15D1

#ifdef __LP64__
#define LINKER_SUFFIX "/system/bin/linker64"
#else
#define LINKER_SUFFIX "/system/bin/linker"
#endif

#define LINKER_DL_MUTEX_SYMNAME "__dl__ZL10g_dl_mutex"

#define TLSVAR __thread __attribute__((tls_model("initial-exec")))

namespace matrix {
    struct SemiDlInfo {
        uint32_t magic;

        const char* pathname;
        const void* base_addr;

        ElfW(Addr) load_bias;

        const char* strs;

        ElfW(Sym)* syms;
        ElfW(Word) sym_count;
    };

    static bool LoadSection(int fd, off_t file_offset, size_t section_size, void** section_out) {
        ElfW(Off) alignedFileOffset = (file_offset & (~(::getpagesize() - 1)));
        off_t bias = file_offset - alignedFileOffset;
        size_t mapSize = section_size + ::getpagesize();
        void* mappedSection = ::mmap(nullptr, mapSize, PROT_READ, MAP_SHARED, fd, alignedFileOffset);
        if (LIKELY(mappedSection != MAP_FAILED)) {
            auto cleaner = MakeScopedCleaner([&mappedSection, &mapSize]() {
                ::munmap(mappedSection, mapSize);
            });
            *section_out = ::malloc(section_size);
            if (UNLIKELY(*section_out == nullptr)) {
                LOGE(LOG_TAG, "Fail to allocate space for loading section.");
                return false;
            }
            auto mappedSectionStart = reinterpret_cast<uintptr_t>(mappedSection) + bias;
            memcpy(*section_out, reinterpret_cast<const void*>(mappedSectionStart), section_size);
            return true;
        } else {
            int errcode = errno;
            LOGE(LOG_TAG, "Fail to mmap file, error: %s", ::strerror(errcode));
            return false;
        }
    }

    static bool FillRestNecessaryData(SemiDlInfo* semi_dlinfo) {
        auto ehdr = reinterpret_cast<const ElfW(Ehdr)*>(semi_dlinfo->base_addr);
        auto baseAddrVal = reinterpret_cast<uintptr_t>(semi_dlinfo->base_addr);

        auto phdrs = reinterpret_cast<ElfW(Phdr)*>(baseAddrVal + ehdr->e_phoff);
        for (int i = 0; i < ehdr->e_phnum; ++i) {
            if (phdrs[i].p_type == PT_LOAD) {
                if (reinterpret_cast<const ElfW(Addr)>(semi_dlinfo->base_addr) < phdrs[i].p_vaddr) {
                    LOGE(LOG_TAG, "Unexpected first program header.");
                    return false;
                }
                semi_dlinfo->load_bias = reinterpret_cast<uintptr_t>(semi_dlinfo->base_addr) - phdrs[i].p_vaddr;
                break;
            }
        }

        int fd = ::open(semi_dlinfo->pathname, O_RDONLY);
        if (fd < 0) {
            LOGE(LOG_TAG, "Fail to open \"%s\".", semi_dlinfo->pathname);
            return false;
        }
        auto fdCleaner = MakeScopedCleaner([&fd]() {
            if (fd >= 0) {
                ::close(fd);
            }
        });

        ElfW(Shdr)* shdrs = nullptr;
        if (!LoadSection(fd, ehdr->e_shoff, ehdr->e_shentsize * ehdr->e_shnum, reinterpret_cast<void**>(&shdrs))) {
            LOGE(LOG_TAG, "Fail to map section headers of \"%s\".", semi_dlinfo->pathname);
            return false;
        }
        auto shdrsCleaner = MakeScopedCleaner([&shdrs]() {
            if (shdrs != 0) {
                ::free(shdrs);
            }
        });
        bool strTblOk = false;
        bool symTblOk = false;
        for (int shdrIdx = ehdr->e_shnum - 1; shdrIdx >= 0; --shdrIdx) {
            ElfW(Shdr)* shdr = shdrs + shdrIdx;
            if (shdr->sh_type == SHT_STRTAB && shdrIdx != ehdr->e_shstrndx) {
                // Load symbol name table but not section name table.
                LOGD(LOG_TAG, "load strtab, sh_off: %x, sh_size: %x", shdr->sh_offset, shdr->sh_size);
                if (LIKELY(LoadSection(fd, shdr->sh_offset, shdr->sh_size, (void**) &semi_dlinfo->strs))) {
                    strTblOk = true;
                } else {
                    LOGE(LOG_TAG, "Fail to map string table of \"%s\"", semi_dlinfo->pathname);
                    semi_dlinfo->strs = nullptr;
                    strTblOk = false;
                }
            } else if (shdr->sh_type == SHT_SYMTAB) {
                LOGD(LOG_TAG, "load symtab, sh_off: %x, sh_size: %x", shdr->sh_offset, shdr->sh_size);
                if (LIKELY(LoadSection(fd, shdr->sh_offset, shdr->sh_size, (void**) &semi_dlinfo->syms))) {
                    semi_dlinfo->sym_count = shdr->sh_size / shdr->sh_entsize;
                    symTblOk = true;
                } else {
                    LOGE(LOG_TAG, "Fail to map symbol table of \"%s\"", semi_dlinfo->pathname);
                    semi_dlinfo->syms = nullptr;
                    semi_dlinfo->sym_count = 0;
                    symTblOk = false;
                }
            }
            if (strTblOk && symTblOk) {
                break;
            }
        }

        if (LIKELY(strTblOk && symTblOk)) {
            return true;
        } else {
            LOGE(LOG_TAG, "Failure in FillRestNecessaryData.");
            return false;
        }
    }

    static void* FindLinkerBaseAddr() {
        FILE* maps = fopen("/proc/self/maps", "r");
        if (maps == nullptr) {
            LOGE(LOG_TAG, "Fail to open maps.");
            return nullptr;
        }
        auto mapsCloser = MakeScopedCleaner([&maps]() {
            if (maps != nullptr) {
                fclose(maps);
            }
        });

        char line[512] = {};
        uintptr_t baseAddr = 0;
        char perm[5] = {};
        unsigned long offset = 0;
        int pathNamePos = 0;
        while(fgets(line, sizeof(line), maps)) {
            if(sscanf(line, "%" PRIxPTR "-%*lx %4s %lx %*x:%*x %*d%n", &baseAddr, perm, &offset, &pathNamePos) != 3) {
                continue;
            }
            // check permission
            if(perm[0] != 'r' || perm[3] != 'p') {
                continue;
            }

            char* pathname = line + pathNamePos;
            size_t pathnameLen = strlen(pathname);
            if (pathnameLen == 0) {
                continue;
            }

            // trim right spaces.
            while (pathnameLen - 1 >= 0 && ::isspace(pathname[pathnameLen - 1])) {
                --pathnameLen;
            }
            if (pathnameLen == 0) {
                continue;
            }
            pathname[pathnameLen] = '\0';

            size_t suffixLen = strlen(LINKER_SUFFIX);
            if (strncmp(pathname + pathnameLen - suffixLen, LINKER_SUFFIX, suffixLen) != 0) {
                continue;
            }

            if (memcmp(reinterpret_cast<const void*>(baseAddr), ELFMAG, SELFMAG) == 0) {
                return reinterpret_cast<void*>(baseAddr);
            }
        }

        return nullptr;
    }

    static pthread_mutex_t* sDlIterateMutexPtr = nullptr;
    static std::recursive_mutex sDlIterMutexFetchMutex;
    static TLSVAR bool sInFindDlIterateMutex = false;

    static pthread_mutex_t* FindDlIterateMutex() {
        if (sInFindDlIterateMutex) {
            return sDlIterateMutexPtr;
        }
        sInFindDlIterateMutex = true;
        auto cleaner = MakeScopedCleaner([]() {
            sInFindDlIterateMutex = false;
        });

        std::lock_guard findLock(sDlIterMutexFetchMutex);
        if (sDlIterateMutexPtr != nullptr) {
            return sDlIterateMutexPtr;
        }
        void* hLinker = SemiDlOpen(LINKER_SUFFIX);
        if (hLinker == nullptr) {
            return nullptr;
        }
        auto hLinkerCleaner = MakeScopedCleaner([&hLinker]() {
            if (hLinker != nullptr) {
                SemiDlClose(hLinker);
            }
        });
        sDlIterateMutexPtr = reinterpret_cast<pthread_mutex_t*>(SemiDlSym(hLinker, LINKER_DL_MUTEX_SYMNAME));
        return sDlIterateMutexPtr;
    }

    static int DlIteratePhdrCompat(const std::function<int(const char*, const void*, void*)>& cb, void* data) {
        int sdk = android_get_device_api_level();
        if (sdk <= 20) {
            // TODO support by parsing maps.
            return 0;
        } else {
            struct IterData {
                decltype(cb) cb;
                void* data;
            } iterData = {.cb = cb, .data = data};

            if (sdk >= 21 && sdk <= 22) {
                pthread_mutex_t* dlMutex = FindDlIterateMutex();
                if (dlMutex != nullptr) {
                    pthread_mutex_lock(dlMutex);
                }
                auto mutexUnlocker = MakeScopedCleaner([&dlMutex]() {
                    if (dlMutex != nullptr) {
                        pthread_mutex_unlock(dlMutex);
                    }
                });

                int ret = 0;
                const void* linkerBase = FindLinkerBaseAddr();
                if (linkerBase != nullptr) {
                    ret = cb(LINKER_SUFFIX, linkerBase, data);
                } else {
                    LOGE(LOG_TAG, "Cannot find base of linker.");
                }
                if (ret != 0) {
                    return ret;
                }

                ret = dl_iterate_phdr([](dl_phdr_info *info, size_t info_size, void *data) -> int {
                    auto iterData = reinterpret_cast<IterData *>(data);
                    return iterData->cb(info->dlpi_name,
                                        reinterpret_cast<const void *>(info->dlpi_addr), iterData->data);
                }, &iterData);
                return ret;
            } else if (sdk >= 23 && sdk <= 26) {
                int ret = 0;
                const void* linkerBase = FindLinkerBaseAddr();
                if (linkerBase != nullptr) {
                    ret = cb(LINKER_SUFFIX, linkerBase, data);
                } else {
                    LOGE(LOG_TAG, "Cannot find base of linker.");
                }
                if (ret != 0) {
                    return ret;
                }

                ret = dl_iterate_phdr([](dl_phdr_info *info, size_t info_size, void *data) -> int {
                    auto iterData = reinterpret_cast<IterData *>(data);
                    return iterData->cb(info->dlpi_name,
                                        reinterpret_cast<const void *>(info->dlpi_addr), iterData->data);
                }, &iterData);
                return ret;
            } else {
                return dl_iterate_phdr([](dl_phdr_info *info, size_t info_size, void *data) -> int {
                    auto iterData = reinterpret_cast<IterData *>(data);
                    return iterData->cb(info->dlpi_name,
                            reinterpret_cast<const void *>(info->dlpi_addr), iterData->data);
                }, &iterData);
            }
        }
    }

    void* SemiDlOpen(const char *name) {
        if (name == nullptr) {
            LOGE(LOG_TAG, "name is null.");
            return nullptr;
        }
        size_t nameLen = ::strlen(name);
        if (nameLen == 0) {
            LOGE(LOG_TAG, "name is empty.");
            return nullptr;
        }
        char* nameSuffix = const_cast<char*>(name);
        size_t nameSuffixLen = nameLen;
        if (name[0] != '/') {
            size_t suffixBufferSize = nameLen + 1 /* slash */ + 1 /* EOL */;
            nameSuffix = reinterpret_cast<char*>(::malloc(suffixBufferSize));
            if (nameSuffix == nullptr) {
                LOGE(LOG_TAG, "Cannot allocate space for name suffix.");
                return nullptr;
            }
            snprintf(nameSuffix, suffixBufferSize, "/%s", name);
            ++nameSuffixLen;
        }
        auto nameSuffixCleaner = MakeScopedCleaner([&nameSuffix, &name]() {
            if (nameSuffix != name) {
                ::free(nameSuffix);
            }
        });

        auto result = reinterpret_cast<SemiDlInfo*>(::malloc(sizeof(SemiDlInfo)));
        if (result == nullptr) {
            LOGE(LOG_TAG, "Cannot allocate space for SemiDlInfo.");
            return nullptr;
        }

        result->magic = SEMI_DLINFO_MAGIC;
        result->base_addr = nullptr;
        result->syms = nullptr;
        result->sym_count = 0;

        DlIteratePhdrCompat([&nameSuffix, &nameSuffixLen, &result]
        (const char* pathname, const void* base, void* data) -> int {
            auto semiDlInfo = result;
            if (semiDlInfo->base_addr != nullptr) {
                return 0;
            }
            if (pathname == nullptr) {
                return 0;
            }
            size_t pathLen = ::strlen(pathname);
            if (pathLen < nameSuffixLen) {
                return 0;
            }
            if (::strncmp(pathname + pathLen - nameSuffixLen, nameSuffix, nameSuffixLen) == 0) {
                semiDlInfo->pathname = pathname;
                semiDlInfo->base_addr = base;
                return 1;
            }
            return 0;
        }, nullptr);

        if (result->base_addr != nullptr) {
            if (FillRestNecessaryData(result)) {
                return result;
            } else {
                return nullptr;
            }
        } else {
            LOGE(LOG_TAG, "Library with name ends with \"%s\" is not loaded by system before.", nameSuffix);
            return nullptr;
        }
    }

    void* SemiDlSym(const void *semi_hlib, const char *sym_name) {
        auto semiDlInfo = reinterpret_cast<const SemiDlInfo*>(semi_hlib);
        if (semiDlInfo->magic != SEMI_DLINFO_MAGIC) {
            LOGE(LOG_TAG, "Invalid semi_hlib.");
            return nullptr;
        }
        for (int i = 0; i < semiDlInfo->sym_count; ++i) {
            auto currSym = semiDlInfo->syms + i;
            auto symType = ELF_ST_TYPE(currSym->st_info);
            const char* currSymName = semiDlInfo->strs + currSym->st_name;
            if ((symType == STT_FUNC || symType == STT_OBJECT) && strcmp(currSymName, sym_name) == 0) {
                return reinterpret_cast<void*>(semiDlInfo->load_bias + currSym->st_value);
            }
        }
        LOGE(LOG_TAG, "Cannot find symbol \"%s\" in \"%s\"", sym_name, semiDlInfo->pathname);
        return nullptr;
    }

    void SemiDlClose(void *semi_hlib) {
        if (UNLIKELY(semi_hlib == nullptr)) {
            LOGE(LOG_TAG, "semi_hlib is null.");
            return;
        }
        auto semiDlInfo = reinterpret_cast<SemiDlInfo*>(semi_hlib);
        if (UNLIKELY(semiDlInfo->magic != SEMI_DLINFO_MAGIC)) {
            LOGE(LOG_TAG, "Invalid semi_hlib, skip closing.");
            return;
        }
        if (semiDlInfo->strs != nullptr) {
            ::free(const_cast<char*>(semiDlInfo->strs));
        }
        if (semiDlInfo->syms != nullptr) {
            ::free(semiDlInfo->syms);
        }
        ::free(semi_hlib);
    }
}
//
// Created by Carl on 2020-09-21.
//
#include <vector>
#include <utility>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>

#include <android-base/logging.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/Memory.h>
#include <QuickenInstructions.h>
#include <MinimalRegs.h>
#include <sys/mman.h>
#include <QuickenJNI.h>
#include <utime.h>
#include "QuickenTableManager.h"
#include "Log.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    string &QuickenTableManager::sSavingPath = *new string;
    string &QuickenTableManager::sPackageName = *new string;
    bool QuickenTableManager::sHasWarmedUp = false;

    inline string
    ToQutFileName(const string &saving_path, const string &soname, const string &build_id) {
        return saving_path + FILE_SEPERATOR + soname + "." + build_id;
    }

    inline string
    ToTempQutFileName(const string &saving_path, const string &soname, const string &build_id) {
        time_t seconds = time(NULL);
        return saving_path + FILE_SEPERATOR + soname + "." + build_id + "_temp_" +
               to_string(seconds);
    }

    inline string
    ToSymbolicQutFileName(const string &saving_path, const string &soname, const string &hash) {
        return saving_path + FILE_SEPERATOR + soname + ".hash." + hash;
    }

    inline void RenameToMalformed(const string &qut_file_name) {
        time_t seconds = time(NULL);
        string malformed = qut_file_name + "_malformed_" + to_string(seconds);
        rename(qut_file_name.c_str(), malformed.c_str());
    }

    std::unordered_map<std::string, std::pair<uint64_t, std::string>>
    QuickenTableManager::GetRequestQut() {
        lock_guard<mutex> guard(lock_);
        return qut_sections_requesting_;
    }

    QutFileError
    QuickenTableManager::TryLoadQutFile(const string &soname, const string &sopath,
                                        const string &hash, const string &build_id,
                                        QutSectionsPtr &qut_sections, const bool testOnly) {
        (void) hash;
        (void) sopath;

        QUT_LOG("Request qut file before lock for so %s", sopath.c_str());

        string qut_file_name = ToQutFileName(QuickenTableManager::sSavingPath, soname, build_id);

        QUT_LOG("Request qut file %s for so %s", qut_file_name.c_str(), sopath.c_str());

        int fd = open(qut_file_name.c_str(), O_RDONLY);

        if (fd >= 0) {

            struct stat file_stat;
            if (fstat(fd, &file_stat) != 0 || file_stat.st_size < 0) {
                close(fd);
                return FileStateError;
            }

            uint64_t file_size = file_stat.st_size;

            if (file_size < 6 * sizeof(size_t)) {
                close(fd);
                RenameToMalformed(qut_file_name);
                return FileTooShort;
            }

            char *data = static_cast<char *>(mmap(NULL, file_size, PROT_READ, MAP_SHARED,
                                                  fd, 0));
            if (data == MAP_FAILED) {
                munmap(data, file_size);
                close(fd);
                return MmapFailed;
            }

            size_t offset = 0;

            size_t qut_version = 0;
            memcpy(&qut_version, data, sizeof(qut_version));
            QUT_LOG("Checking file size = %llu, qut_version = %u, offset = %u",
                    (ullint_t) file_size, (uint32_t) qut_version, (uint32_t) offset);
            offset += sizeof(qut_version);
            if (qut_version != QUT_VERSION) {
                munmap(data, file_size);
                close(fd);
                RenameToMalformed(qut_file_name);
                return QutVersionNotMatch;
            }

            size_t arch = 0;
            memcpy(&arch, (data + offset), sizeof(arch));
            QUT_LOG("Checking file size = %llu, arch = %u, offset = %u",
                    (ullint_t) file_size, (uint32_t) arch, (uint32_t) offset);
            offset += sizeof(arch);
            if (arch != CURRENT_ARCH_ENUM) {
                munmap(data, file_size);
                close(fd);
                RenameToMalformed(qut_file_name);
                return ArchNotMatch;
            }

            // XXX add sum check

            size_t idx_size;
            memcpy(&idx_size, data + offset, sizeof(idx_size));
            QUT_LOG("Checking file size = %llu, idx_size = %u, offset = %u",
                    (ullint_t) file_size, (uint32_t) idx_size, (uint32_t) offset);
            offset += sizeof(idx_size);

            size_t tbl_size;
            memcpy(&tbl_size, data + offset, sizeof(tbl_size));
            QUT_LOG("Checking file size = %llu, tbl_size = %u, offset = %u",
                    (ullint_t) file_size, (uint32_t) tbl_size, (uint32_t) offset);
            offset += sizeof(tbl_size);

            size_t idx_offset;
            memcpy(&idx_offset, data + offset, sizeof(idx_offset));
            QUT_LOG("Checking file size = %llu, idx_offset = %u, offset = %u",
                    (ullint_t) file_size, (uint32_t) idx_offset, (uint32_t) offset);
            offset += sizeof(idx_offset);

            size_t tbl_offset;
            memcpy(&tbl_offset, data + offset, sizeof(tbl_offset));

            QUT_LOG("Checking file size = %llu, tbl_offset = %u, tbl_size = %u",
                    (ullint_t) file_size, (uint32_t) tbl_offset, (uint32_t) tbl_size);
            if (file_size != (tbl_offset + (tbl_size * sizeof(uptr)))) {
                munmap(data, file_size);
                close(fd);
                RenameToMalformed(qut_file_name);
                return FileLengthNotMatch;
            }

            if (!testOnly) {

                QutSectionsPtr qut_sections_tmp = new QutSections();

                qut_sections_tmp->load_from_file = true;
                qut_sections_tmp->mmap_ptr = data;
                qut_sections_tmp->map_size = file_size;

                qut_sections_tmp->idx_size = idx_size;
                qut_sections_tmp->tbl_size = tbl_size;
                qut_sections_tmp->quidx = (static_cast<uptr *>((void *) (data + idx_offset)));
                qut_sections_tmp->qutbl = (static_cast<uptr *>((void *) (data + tbl_offset)));

                QutSectionsPtr qut_sections_insert = qut_sections_tmp;
                if (!InsertQutSectionsNoLock(soname, hash, build_id, qut_sections_insert, true)) {
                    delete qut_sections_tmp;
                    close(fd);
                    return InsertNewQutFailed;
                }

                qut_sections = qut_sections_tmp;
            }

            close(fd);

            // change last modified time, to prevent self clean-up logic.
            utime(qut_file_name.c_str(), NULL);

            return NoneError;
        } else {
            return OpenFileFailed;
        }
    }


    QutFileError
    QuickenTableManager::RequestQutSections(const string &soname, const string &sopath,
                                            const string &hash, const string &build_id,
                                            const uint64_t elf_start_offset,
                                            QutSectionsPtr &qut_sections) {

        QUT_LOG("RequestQutSections soname, %s.", soname.c_str());

        if (sSavingPath.empty()) {
            return NotInitialized;
        }

        if (build_id.empty()) {
            return BuildIdNotMatch;
        }

        QutFileError ret;

        {
            lock_guard<mutex> guard(lock_);

            ret = FindQutSectionsNoLock(soname, sopath, hash, build_id, elf_start_offset,
                                        qut_sections);

            if (qut_sections != nullptr || ret != NoneError) {
                return ret;
            }

            ret = TryLoadQutFile(soname, sopath, hash, build_id, qut_sections);
        }

        if (ret != NoneError) {

            QUT_LOG("Request quicken table %s result %d.", sopath.c_str(), ret);

            if (!sHasWarmedUp) {
                return NotWarmedUp;
            }

            return TryInvokeJavaRequestQutGenerate;
        }

        return ret;
    }

    bool
    QuickenTableManager::InsertQutSectionsNoLock(const string &soname, const string &hash,
                                                 const string &build_id,
                                                 QutSectionsPtr &qut_sections,
                                                 bool immediately) {

        (void) soname;

        CHECK(qut_sections != nullptr);
        if (qut_sections->idx_size == 0) {
            QUT_LOG("Insert qut idx size is empty.");
            return false;
        }

        if (build_id.empty()) {
            QUT_LOG("Insert qut build id %s is empty.", build_id.c_str());
            return false;
        }

        auto it = qut_sections_map_.find(build_id);
        if (it != qut_sections_map_.end() && it->second != nullptr) {
            QUT_LOG("Qut insert build id %s failed, value %llx.", build_id.c_str(),
                    (ullint_t) (it->second));
            return false;
        }

        QUT_LOG("Insert qut build id %s.", build_id.c_str());

        if (immediately ||
            qut_sections_requesting_.find(build_id) != qut_sections_requesting_.end()) {
            qut_sections_map_[build_id] = qut_sections;
            qut_sections = nullptr;
        }

        QUT_LOG("Erase qut requesting build id %s.", build_id.c_str());
        qut_sections_requesting_.erase(build_id);
        qut_sections_hash_to_build_id_.erase(hash);

        return true;
    }

    void
    QuickenTableManager::EraseQutRequestingByHash(const string &hash) {
        lock_guard<mutex> guard(lock_);
        auto it = qut_sections_hash_to_build_id_.find(hash);
        if (it != qut_sections_hash_to_build_id_.end()) {
            qut_sections_requesting_.erase(it->second);
            QUT_LOG("Erase qut requesting build id %s.", it->second.c_str());
        }
        qut_sections_hash_to_build_id_.erase(hash);
    }

    QutFileError
    QuickenTableManager::FindQutSectionsNoLock(const std::string &soname, const std::string &sopath,
                                               const std::string &hash,
                                               const std::string &build_id,
                                               const uint64_t elf_start_offset,
                                               QutSectionsPtr &qut_sections_ptr) {
        (void) soname;

        auto it = qut_sections_map_.find(build_id);
        if (it != qut_sections_map_.end()) {
            qut_sections_ptr = it->second;
            return NoneError;
        }

        if (qut_sections_requesting_.find(build_id) != qut_sections_requesting_.end()) {
            QUT_DEBUG_LOG("Find qut requesting build id %s.", build_id.c_str());
            return LoadRequesting;
        }

        QUT_DEBUG_LOG("Requesting qut for so(%s) build_id(%s) elf_start_offset(%llu).",
                      sopath.c_str(), build_id.c_str(), (ullint_t) elf_start_offset);

        // Mark requesting qut sections.
        qut_sections_requesting_[build_id] = make_pair(elf_start_offset, sopath);
        qut_sections_hash_to_build_id_[hash] = build_id;

        return NoneError;
    }

    bool
    QuickenTableManager::CheckIfQutFileExistsWithHash(const string &soname, const string &hash) {
        string symbolic_qut_file = ToSymbolicQutFileName(sSavingPath, soname, hash);
        struct stat buf;
        return stat(symbolic_qut_file.c_str(), &buf) == 0;
    }

    bool
    QuickenTableManager::CheckIfQutFileExistsWithBuildId(const string &soname,
                                                         const string &build_id) {
        string qut_file = ToQutFileName(sSavingPath, soname, build_id);
        struct stat buf;
        return stat(qut_file.c_str(), &buf) == 0;
    }

    QutFileError
    QuickenTableManager::SaveQutSections(const string &soname, const std::string &sopath,
                                         const string &hash, const string &build_id,
                                         const bool only_save_file,
                                         unique_ptr<QutSections> qut_sections_ptr) {
        (void) sopath;

        QutSectionsPtr qut_sections_insert = qut_sections_ptr.get();
        {
            lock_guard<mutex> guard(lock_);
            if (qut_sections_insert == nullptr ||
                    (!only_save_file && !InsertQutSectionsNoLock(
                         soname, hash, build_id, qut_sections_insert, false))) {
                QUT_LOG("qut_sections_insert %llx", (ullint_t) qut_sections_insert);
                return InsertNewQutFailed;
            }
        }

        QutSectionsPtr qut_sections;

        if (qut_sections_insert == nullptr) {
            // Ownership of qut_sections has already moved to qut_sections_map_.
            qut_sections = qut_sections_ptr.release();
        } else {
            qut_sections = qut_sections_insert;
        }

        if (sSavingPath.empty()) {
            return NotInitialized;
        }

        string temp_qut_file_name = ToTempQutFileName(sSavingPath, soname, build_id);

        QUT_LOG("temp_qut_file_name %s, build_id %s", temp_qut_file_name.c_str(),
                build_id.c_str());

        int fd = open(temp_qut_file_name.c_str(), O_CREAT | O_RDWR | O_TRUNC);
        if (fd < 0) {
            return OpenFileFailed;
        }

        size_t offset = 0;

        // Write Qut Version
        size_t qut_version = QUT_VERSION;
        write(fd, reinterpret_cast<const void *>(&qut_version), sizeof(qut_version));
        offset += sizeof(qut_version);

        // Write arch
        size_t arch = CURRENT_ARCH_ENUM;
        write(fd, reinterpret_cast<const void *>(&arch), sizeof(arch));
        offset += sizeof(arch);

        // XXX Add sum check, like CRC32 or SHA1

        // Write quidx size
        size_t idx_size = qut_sections->idx_size;
        write(fd, reinterpret_cast<const void *>(&idx_size), sizeof(idx_size));
        QUT_LOG("Writing file idx_size = %zu, offset = %zu", idx_size, offset);
        offset += sizeof(idx_size);

        // Write qutbl size
        size_t tbl_size = qut_sections->tbl_size;
        write(fd, reinterpret_cast<const void *>(&tbl_size), sizeof(tbl_size));
        QUT_LOG("Writing file tbl_size = %zu, offset = %zu", tbl_size, offset);
        offset += sizeof(tbl_size);

        size_t sizeof_idx = sizeof(qut_sections->quidx[0]);

        size_t idx_offset = offset + (sizeof(size_t) * 2); // idx offset + tbl offset
        size_t tbl_offset = idx_offset + (qut_sections->idx_size * sizeof_idx); // tbl offset

        // Write idx offset
        write(fd, reinterpret_cast<const void *>(&idx_offset), sizeof(idx_offset));
        QUT_LOG("Writing file idx_offset = %zu, offset = %zu", idx_offset, offset);

        // Write tbl offset
        write(fd, reinterpret_cast<const void *>(&tbl_offset), sizeof(tbl_offset));
        QUT_LOG("Writing file tbl_offset = %zu, tbl_size = %zu", tbl_offset, tbl_size);

        // Write quidx
        write(fd, reinterpret_cast<const void *>(qut_sections->quidx),
              qut_sections->idx_size * sizeof_idx);

        // Write qutbl
        if (qut_sections->tbl_size > 0) {
            write(fd, reinterpret_cast<const void *>(qut_sections->qutbl),
                  qut_sections->tbl_size * sizeof(qut_sections->qutbl[0]));
        }

        close(fd);

        string qut_file_name = ToQutFileName(sSavingPath, soname, build_id);
        // Rename old one.
        RenameToMalformed(qut_file_name);
        // Move temp to new one.
        int ret = rename(temp_qut_file_name.c_str(), qut_file_name.c_str());
        // Chmod 0700
        chmod(qut_file_name.c_str(), S_IRWXU);

        QUT_LOG("Rename to new qut file %s", qut_file_name.c_str());
        if (ret == 0) {
            string symbolic_qut_file = ToSymbolicQutFileName(sSavingPath, soname, hash);
            symlink(qut_file_name.c_str(), symbolic_qut_file.c_str());
            QUT_LOG("Link symbolic %s to %s", symbolic_qut_file.c_str(), qut_file_name.c_str());
        }

        QUT_LOG("Saving qut file %s for so %s", qut_file_name.c_str(), sopath.c_str());
        return NoneError;
    }

}  // namespace wechat_backtrace

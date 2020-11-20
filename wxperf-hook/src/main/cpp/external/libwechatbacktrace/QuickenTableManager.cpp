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

#include <deps/android-base/include/android-base/logging.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/Memory.h>
#include <QuickenInstructions.h>
#include <MinimalRegs.h>
#include <sys/mman.h>
#include <QuickenJNI.h>
#include <utime.h>
#include "QuickenTableManager.h"
#include "../../common/Log.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    string QuickenTableManager::sSavingPath;
    string QuickenTableManager::sPackageName;
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

    std::unordered_map<std::string, std::string> QuickenTableManager::GetRequestQut() {
        lock_guard<mutex> lockGuard(lock_);
        return qut_sections_requesting_;
    }

    QutFileError
    QuickenTableManager::RequestQutSections(const string &soname, const string &sopath,
                                            const string &hash, const string &build_id,
                                            const string &build_id_hex,
                                            QutSectionsPtr &qut_sections) {

        QUT_LOG("Warm up get %d", sHasWarmedUp);

        // Fast return.
        if (!sHasWarmedUp) {
            return NotWarmedUp;
        }

        if (sSavingPath.empty()) {
            return NotInitialized;
        }

        QutFileError ret = FindQutSections(soname, sopath, build_id, qut_sections);

        if (qut_sections != nullptr || ret != NoneError) {
            return ret;
        }

        string qut_file_name = ToQutFileName(QuickenTableManager::sSavingPath, soname, build_id);

        QUT_LOG("Request qut file %s for so %s", qut_file_name.c_str(), sopath.c_str());

        int fd = open(qut_file_name.c_str(), O_RDONLY);

        if (fd >= 0) {

            struct stat file_stat;
            if (fstat(fd, &file_stat) != 0) {
                close(fd);
                return FileStateError;
            }

            if (file_stat.st_size < 6 * sizeof(size_t)) {
                close(fd);
                RenameToMalformed(qut_file_name);
                return FileTooShort;
            }

            char *data = static_cast<char *>(mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED,
                                                  fd, 0));
            if (data == MAP_FAILED) {
                munmap(data, file_stat.st_size);
                close(fd);
                return MmapFailed;
            }

            size_t offset = 0;

            size_t qut_version = 0;
            memcpy(&qut_version, data, sizeof(qut_version));
            QUT_LOG("Checking file size = %lld, qut_version = %d, offset = %d",
                    (uint64_t) file_stat.st_size, (uint32_t) qut_version, (uint32_t) offset);
            offset += sizeof(qut_version);
            if (qut_version != QUT_VERSION) {
                munmap(data, file_stat.st_size);
                close(fd);
                RenameToMalformed(qut_file_name);
                return QutVersionNotMatch;
            }

            size_t arch = 0;
            memcpy(&arch, (data + offset), sizeof(arch));
            QUT_LOG("Checking file size = %lld, arch = %d, offset = %d",
                    (uint64_t) file_stat.st_size, (uint32_t) arch, (uint32_t) offset);
            offset += sizeof(arch);
            if (arch != CURRENT_ARCH_ENUM) {
                munmap(data, file_stat.st_size);
                close(fd);
                RenameToMalformed(qut_file_name);
                return ArchNotMatch;
            }

//            size_t build_id_len = build_id_hex.length();
//            memcpy(&build_id_len, (data + offset), sizeof(build_id_len));
//            offset += sizeof(build_id_len);
//            if (build_id_len != 0) {
//                char *build_id_cstr = static_cast<char *>(malloc(build_id_len));
//                memcpy(build_id_cstr, (data + offset), build_id_len);
//                bool same = strcmp(build_id_hex.c_str(), build_id_cstr) == 0;
//                free(build_id_cstr);
//                if (!same) {
//                    munmap(data, file_stat.st_size);
//                    close(fd);
//                    RenameToMalformed(qut_file_name);
//                    return BuildIdNotMatch;
//                }
//            }

            // XXX add sum check

            size_t idx_size;
            memcpy(&idx_size, data + offset, sizeof(idx_size));
            QUT_LOG("Checking file size = %lld, idx_size = %d, offset = %d",
                    (uint64_t) file_stat.st_size, (uint32_t) idx_size, (uint32_t) offset);
            offset += sizeof(idx_size);

            size_t tbl_size;
            memcpy(&tbl_size, data + offset, sizeof(tbl_size));
            QUT_LOG("Checking file size = %lld, tbl_size = %d, offset = %d",
                    (uint64_t) file_stat.st_size, (uint32_t) tbl_size, (uint32_t) offset);
            offset += sizeof(tbl_size);

            size_t idx_offset;
            memcpy(&idx_offset, data + offset, sizeof(idx_offset));
            QUT_LOG("Checking file size = %lld, idx_offset = %d, offset = %d",
                    (uint64_t) file_stat.st_size, (uint32_t) idx_offset, (uint32_t) offset);
            offset += sizeof(idx_offset);

            size_t tbl_offset;
            memcpy(&tbl_offset, data + offset, sizeof(tbl_offset));

            QUT_LOG("Checking file size = %lld, tbl_offset = %d, tbl_size = %d",
                    (uint64_t) file_stat.st_size, (uint32_t) tbl_offset, (uint32_t) tbl_size);
            if (file_stat.st_size != (tbl_offset + (tbl_size * sizeof(uptr)))) {
                munmap(data, file_stat.st_size);
                close(fd);
                RenameToMalformed(qut_file_name);
                return FileLengthNotMatch;
            }

            qut_sections = new QutSections();

            qut_sections->idx_size = idx_size;
            qut_sections->tbl_size = tbl_size;
            qut_sections->quidx = (static_cast<uptr *>((void *) (data + idx_offset)));
            qut_sections->qutbl = (static_cast<uptr *>((void *) (data + tbl_offset)));

            if (!InsertQutSections(build_id, qut_sections, true)) {
                return InsertNewQutFailed;
            }

            // change last modified time, to prevent self clean-up logic.
            utime(qut_file_name.c_str(), NULL);

            return NoneError;
        } else {
            return OpenFileFailed;
        }
    }

    bool
    QuickenTableManager::InsertQutSections(const string &build_id, QutSectionsPtr qut_sections,
                                           bool immediately) {
        CHECK(qut_sections != nullptr);
        CHECK(qut_sections->idx_size > 0);

        lock_guard<mutex> lockGuard(lock_);

        auto it = qut_sections_map_.find(build_id);
        if (it != qut_sections_map_.end() && it->second != nullptr) {
            QUT_LOG("Qut insert build id %s failed, value %llx.", build_id.c_str(),
                    (uint64_t) it->second);
            return false;
        }

        QUT_LOG("Insert qut build id %s.", build_id.c_str());

        if (immediately ||
            qut_sections_requesting_.find(build_id) != qut_sections_requesting_.end()) {
            qut_sections_map_[build_id] = qut_sections;
        }

        QUT_LOG("Erase qut requesting build id %s.", build_id.c_str());
        qut_sections_requesting_.erase(build_id);

        return true;
    }

    QutFileError
    QuickenTableManager::FindQutSections(const std::string &soname, const std::string &sopath,
                                         const std::string &build_id,
                                         QutSectionsPtr &qut_sections_ptr) {
        (void) soname;

        lock_guard<mutex> lockGuard(lock_);

        auto it = qut_sections_map_.find(build_id);
        if (it != qut_sections_map_.end()) {
            qut_sections_ptr = it->second;
            return NoneError;
        }

        if (qut_sections_requesting_.find(build_id) != qut_sections_requesting_.end()) {
            QUT_LOG("Find qut requesting build id %s.", build_id.c_str());
            return LoadRequesting;
        }

        // Mark requesting qut sections.
        qut_sections_requesting_[build_id] = sopath;

        InvokeJava_RequestQutGenerate();

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
                                         const string &build_id_hex, QutSectionsPtr qut_sections) {

        if (!InsertQutSections(build_id, qut_sections, false)) {
            return InsertNewQutFailed;
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

//        // Write build id
//        size_t build_id_len = build_id_hex.length();
//        write(fd, reinterpret_cast<const void *>(&build_id_len), sizeof(build_id_len));
//        offset += sizeof(build_id_len);
//        write(fd, reinterpret_cast<const void *>(build_id_hex.c_str()), build_id_len);
//        offset += build_id_len;

        // Write quidx size
        size_t idx_size = qut_sections->idx_size;
        write(fd, reinterpret_cast<const void *>(&idx_size), sizeof(idx_size));
        QUT_LOG("Writing file idx_size = %d, offset = %d", idx_size, offset);
        offset += sizeof(idx_size);

        // Write qutbl size
        size_t tbl_size = qut_sections->tbl_size;
        write(fd, reinterpret_cast<const void *>(&tbl_size), sizeof(tbl_size));
        QUT_LOG("Writing file tbl_size = %d, offset = %d", tbl_size, offset);
        offset += sizeof(tbl_size);

        size_t sizeof_idx = sizeof(qut_sections->quidx[0]);

        size_t idx_offset = offset + (sizeof(size_t) * 2); // idx offset + tbl offset
        size_t tbl_offset = idx_offset + (qut_sections->idx_size * sizeof_idx); // tbl offset

        // Write idx offset
        write(fd, reinterpret_cast<const void *>(&idx_offset), sizeof(idx_offset));
        QUT_LOG("Writing file idx_offset = %d, offset = %d", idx_offset, offset);

        // Write tbl offset
        write(fd, reinterpret_cast<const void *>(&tbl_offset), sizeof(tbl_offset));
        QUT_LOG("Writing file tbl_offset = %d, tbl_size = %d", tbl_offset, tbl_size);

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

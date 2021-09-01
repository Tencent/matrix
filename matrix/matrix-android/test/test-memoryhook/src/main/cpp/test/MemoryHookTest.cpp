#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cinttypes>
#include <memory>
#include <cxxabi.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <map>
#include "ProfileRecord.h"
#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <android/log.h>

#define TEST_LOG_ERROR(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  "TestHook", fmt, ##__VA_ARGS__)

#define CHECK_NE(a, b) \
  if ((a) == (b)) abort();

#define CHECK(a, b) \
  if ((a) != (b)) abort();

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
}

std::vector<std::string> split_string(const std::string &s,
                                      const std::string &delimiters) {
    CHECK_NE(delimiters.size(), 0U);

    std::vector<std::string> result;

    size_t base = 0;
    size_t found;
    while (true) {
        found = s.find_first_of(delimiters, base);
        result.push_back(s.substr(base, found - base));
        if (found == s.npos) break;
        base = found + 1;
    }

    return result;
}

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

#define THREAD_MAX_COUNT 0x7fff
struct FakeRecordMemoryChunk {
    uint64_t ptr;
    uint64_t size;
    uint64_t ts;
};

static std::vector<FakeRecordMemoryChunk> *thread_vectors[THREAD_MAX_COUNT]{};
static std::vector<FakeRecordMemoryBacktrace> *thread_backtrace_vectors[THREAD_MAX_COUNT]{};

static std::mutex ptr_maps_mutex;
static std::map<uint64_t, uint64_t> ptr_maps;

void prepare_data_routine(std::string filepath) {
    std::string line;

    ifstream file(filepath);

    bool found_new_tid = false;
    uint64_t cur_tid = 0;
    bool found_new_backtrace = false;
    uint64_t cur_ptr = 0;
    vector<string> backtrace_splits;

    while (getline(file, line)) {

        if (line.empty()) continue;
        if (line[0] == '-') continue;

        if (line[0] == '#') {
            // new tid.
            auto splits = split_string(line, ":");

            CHECK(splits.size(), 2);

            if (splits[0] == "# TID") {
                found_new_tid = true;
                found_new_backtrace = false;
            } else if (splits[0] == "# TID backtrace") {
                found_new_tid = false;
                found_new_backtrace = true;
            }

            cur_tid = std::stoul(splits[1]);

            if (found_new_tid) {
                thread_vectors[cur_tid] = new vector<FakeRecordMemoryChunk>();
            } else if (found_new_backtrace) {
                thread_backtrace_vectors[cur_tid] = new vector<FakeRecordMemoryBacktrace>();
            } else {
                abort();
            }

        } else if (line[0] == '|') {
            CHECK(backtrace_splits.size(), 4);

            // backtrace.
            auto splits = split_string(line, ":");
            CHECK(splits.size(), 2);
            auto backtrace = split_string(splits[1], ";");
            auto* backtrace_ = new vector<uint64_t>;

            for (const auto &str: backtrace) {
                if (str.empty()) continue;
                backtrace_->push_back(static_cast<uint64_t>(std::stoul(str)));
            }

            thread_backtrace_vectors[cur_tid]->push_back(
                    {
                            .ptr = static_cast<uint64_t>(std::stoul(backtrace_splits[1])),
                            .ts = static_cast<uint64_t>(std::stoul(backtrace_splits[0])),
                            .frame_size = static_cast<uint64_t>(backtrace_->size()),
                            .frames = backtrace_,
                            .duration = static_cast<uint64_t>(std::stoul(backtrace_splits[2])),
                    }
            );
        } else if (line[0] >= '0' && line[0] <= '9') {
            auto splits = split_string(line, ";");
            if (found_new_tid) {
                CHECK(splits.size(), 3);
                thread_vectors[cur_tid]->push_back(
                        {
                                .ptr = static_cast<uint64_t>(std::stoul(splits[1])),
                                .size = static_cast<uint64_t>(std::stoul(splits[2])),
                                .ts = static_cast<uint64_t>(std::stoul(splits[0])),
                        }
                );
            } else if (found_new_backtrace) {
                CHECK(splits.size(), 4);
                backtrace_splits = splits;
            } else {
                abort();
            }
        }
    }

    file.close();
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_test_memoryhook_MemoryHookTestNative_nativePrepareData(JNIEnv *env,
                                                                               jclass clazz) {

    vector<thread*> threads;

    // Unzip and push data/memory-record.1.zip to this folder.
    const char * dir_path = "/data/local/tmp/simulate-malloc/memory-record.1/";
    DIR *dr;
    struct dirent *en;
    dr = opendir(dir_path);
    if (dr) {
        while ((en = readdir(dr)) != nullptr) {
            std::string absolute_path = std::string(dir_path) + std::string(en->d_name);
            auto th = new std::thread(prepare_data_routine, absolute_path);
            threads.push_back(th);
        }
        closedir(dr); //close all directory
    }

    for (auto & thread : threads) {
        thread->join();
        delete thread;
    }
}


extern "C" void fake_malloc(void *ptr, size_t byte_count);
extern "C" void fake_free(void *ptr);

void routine(size_t fake_tid)
{
    set_fake_backtrace_vectors(thread_backtrace_vectors[fake_tid]);
    uint64_t last_ts;
    for (auto chunk : *thread_vectors[fake_tid]) {
        if (chunk.ts > last_ts && (chunk.ts - last_ts) >= 1000000) {
            usleep(1000);
        }
        last_ts = chunk.ts;
        if (chunk.size == 0) {
            fake_free(reinterpret_cast<void *>(chunk.ptr));
        } else {
            fake_malloc(reinterpret_cast<void *>(chunk.ptr), chunk.size);
        }
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_test_memoryhook_MemoryHookTestNative_nativeRunTest(
        JNIEnv *env, jclass clazz) {

    vector<thread*> threads;
    for (size_t i = 0; i < THREAD_MAX_COUNT; i++) {
        if (thread_vectors[i]) {
            auto th = new std::thread(routine, i);
            threads.push_back(th);
        }
    }

    for (auto & thread : threads) {
        thread->join();
        delete thread;
    }
}

uint64_t hash_backtrace_frames_1(std::vector<uint64_t>* frames) {
    uint64_t sum = 1;
    if (frames == nullptr) {
        return (uint64_t) sum;
    }
    for (size_t i = 0; i != frames->size(); i++) {
        sum += (*frames)[i];
    }
    return (uint64_t) sum;
}

uint64_t hash_backtrace_frames_2(std::vector<uint64_t>* frames) {

    if (UNLIKELY(frames == nullptr || frames->empty())) {
        return 1;
    }
    uint64_t sum = frames->size();
    for (size_t i = 1; i < frames->size(); i++) {
        sum += frames->at(i) << (i << 1);
    }
    return (uint64_t) sum;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_tencent_matrix_test_memoryhook_MemoryHookTestNative_nativeHashCollisionEstimate(
        JNIEnv *env, jclass clazz) {

    std::map<uint64_t, vector<FakeRecordMemoryBacktrace> *> hashtable;
    std::vector<uint64_t> collision;
    std::vector<uint64_t> size_collision;
    for (auto & thread_backtrace_vector : thread_backtrace_vectors) {
        if (thread_backtrace_vector) {
            for (auto backtrace : *thread_backtrace_vector) {
                uint64_t hash = hash_backtrace_frames_2(backtrace.frames);

                if (hashtable.find(hash) == hashtable.end()) {
                    hashtable[hash] = new vector<FakeRecordMemoryBacktrace>();
                }
                bool has_collision = true;
                bool has_size_collision = false;
                for (auto b : *hashtable[hash]) {
                    bool is_collision = b.frames->size() != backtrace.frames->size();
                    if (!is_collision) {
                        for (size_t idx = 0; idx < backtrace.frames->size(); idx++) {
                            if (b.frames->at(idx) != backtrace.frames->at(idx)) {
                                is_collision = true;
                                has_size_collision = true;
                                break;
                            }
                        }
                    }
                    if (!is_collision) {
                        has_collision = false;
                        break;
                    }
                }
                if (has_collision) {
                    hashtable[hash]->push_back(backtrace);
                    if (hashtable[hash]->size() > 1) {
                        collision.push_back(hash);
                    }
                    if (has_size_collision) {
                        size_collision.push_back(hash);
                    }
                }
            }
        }
    }

    size_t c_count = 0;
    for (auto hash : collision) {
        c_count += hashtable[hash]->size();
    }

    TEST_LOG_ERROR("Found hash collision %zu, size collision %zu, in total %zu, backtrace count %zu", collision.size(), size_collision.size(), hashtable.size(), c_count);

}

struct TaggedPtr {
    void * ptr = nullptr;
    size_t size = 111;
};

void test_atomic() {

    atomic<TaggedPtr> abc;
    TaggedPtr ptr1;
    abc.store(ptr1);
    TaggedPtr ptr2;
    ptr2.size = 222;
    abc.compare_exchange_strong(ptr1, ptr2);

    atomic<size_t> def;
    size_t from = 1;
    size_t to = 1;
    def.compare_exchange_strong(from, to);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void) reserved;
    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    test_atomic();
    return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif

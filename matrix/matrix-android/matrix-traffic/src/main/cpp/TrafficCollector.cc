/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: leafjia@tencent.com
//
// TrafficCollector.cc

#include "TrafficCollector.h"

#include <util/blocking_queue.h>
#include <util/managed_jnienv.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "MatrixTraffic.h"

#include <shared_mutex>
#include <queue>
#include <thread>
#include <unordered_set>

using namespace std;

namespace MatrixTraffic {

static mutex queueMutex;
static condition_variable cv;

static bool loopRunning = false;
static bool sDumpStackTrace = false;

static unordered_set<int> activeFdSet;
static shared_mutex activeFdSetMutex;

static unordered_set<int> invalidFdSet;

static blocking_queue<shared_ptr<TrafficMsg>> msgQueue;

static map<int, long> rxFdTrafficInfoMap;
static map<int, long> txFdTrafficInfoMap;
static shared_mutex rxTrafficInfoMapLock;
static shared_mutex txTrafficInfoMapLock;

static map<int, string> fdThreadNameMap;
static shared_mutex fdThreadNameMapSharedLock;

int isNetworkSocketFd(int fd) {
    struct sockaddr c;
    socklen_t cLen = sizeof(c);
    int getSockNameRet = getsockname(fd, (struct sockaddr*) &c, &cLen);
    if (getSockNameRet == 0) {
        if (c.sa_family != AF_LOCAL && c.sa_family != AF_UNIX) {
            return 1;
        }
    }
    return 0;
}

string saveFdInfo(int fd) {
    fdThreadNameMapSharedLock.lock_shared();
    if (fdThreadNameMap.count(fd) == 0) {
        fdThreadNameMapSharedLock.unlock_shared();

        char threadName[15];
        prctl(PR_GET_NAME, threadName);
        char key[32];
        snprintf(key, sizeof(key), "%d-%s", fd, threadName);

        if (sDumpStackTrace) {
            setFdStackTraceCall(key);
        }

        fdThreadNameMapSharedLock.lock();
        fdThreadNameMap[fd] = key;
        fdThreadNameMapSharedLock.unlock();

        return key;
    } else {
        auto key = fdThreadNameMap[fd];
        fdThreadNameMapSharedLock.unlock_shared();
        return key;
    }
}

void TrafficCollector::enQueueClose(int fd) {
    if (!loopRunning) {
        return;
    }
    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(MSG_TYPE_CLOSE, fd, 0);
    msgQueue.push(msg);
    cv.notify_one();
}

void enQueueMsg(int type, int fd, size_t len) {
    if (!loopRunning) {
        return;
    }

    activeFdSetMutex.lock_shared();
    if (activeFdSet.count(fd) > 0) {
        activeFdSetMutex.unlock_shared();
        saveFdInfo(fd);
    } else {
        activeFdSetMutex.unlock_shared();
    }

    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(type, fd, len);
    msgQueue.push(msg);

    cv.notify_one();
}

void TrafficCollector::enQueueTx(int type, int fd, size_t len) {
    if (!loopRunning) {
        return;
    }
    enQueueMsg(type, fd, len);
}

void TrafficCollector::enQueueRx(int type, int fd, size_t len) {
    if (!loopRunning) {
        return;
    }
    enQueueMsg(type, fd, len);
}

void appendRxTraffic(int fd, long len) {
    rxTrafficInfoMapLock.lock();
    rxFdTrafficInfoMap[fd] += len;
    rxTrafficInfoMapLock.unlock();
}

void appendTxTraffic(int fd, long len) {
    txTrafficInfoMapLock.lock();
    txFdTrafficInfoMap[fd] += len;
    txTrafficInfoMapLock.unlock();
}

void loop() {
    unique_lock lk(queueMutex);
    while (loopRunning) {
        if (msgQueue.empty()) {
            cv.wait(lk);
        } else {
            shared_ptr<TrafficMsg> msg = msgQueue.front();
            int fd = msg->fd;
            int type = msg->type;
            if (type == MSG_TYPE_CLOSE) {
                if (activeFdSet.count(fd) > 0) {
                    fdThreadNameMapSharedLock.lock();
                    fdThreadNameMap.erase(fd);
                    fdThreadNameMapSharedLock.unlock();

                    activeFdSetMutex.lock();
                    activeFdSet.erase(fd);
                    activeFdSetMutex.unlock();
                }
                invalidFdSet.erase(fd);
            } else {
                if (activeFdSet.count(fd) == 0 && invalidFdSet.count(fd) == 0) {
                    if (!isNetworkSocketFd(fd)) {
                        invalidFdSet.insert(fd);
                        fdThreadNameMapSharedLock.lock();
                        fdThreadNameMap.erase(fd);
                        fdThreadNameMapSharedLock.unlock();
                    } else {
                        activeFdSetMutex.lock();
                        activeFdSet.insert(fd);
                        activeFdSetMutex.unlock();
                    }
                }
                if (type >= MSG_TYPE_READ && type <= MSG_TYPE_RECVMSG) {
                    if (activeFdSet.count(fd) > 0 && invalidFdSet.count(fd) == 0) {
                        appendRxTraffic(fd, msg->len);
                    }
                } else if (type >= MSG_TYPE_WRITE && type <= MSG_TYPE_SENDMSG) {
                    if (activeFdSet.count(fd) > 0 && invalidFdSet.count(fd) == 0) {
                        appendTxTraffic(fd, msg->len);
                    }
                }
            }
            msgQueue.pop();
        }
    }
}

void TrafficCollector::startLoop(bool dumpStackTrace) {
    if (loopRunning) {
        return;
    }
    loopRunning = true;
    sDumpStackTrace = dumpStackTrace;
    thread loopThread(loop);
    loopThread.detach();
}

void TrafficCollector::stopLoop() {
    loopRunning = false;
}

jobject TrafficCollector::getFdTrafficInfoMap(int type) {
    JNIEnv *env = JniInvocation::getEnv();
    jclass mapClass = env->FindClass("java/util/HashMap");
    if(mapClass == nullptr) {
        return nullptr;
    }
    jmethodID mapInit = env->GetMethodID(mapClass, "<init>", "()V");
    jobject jHashMap = env->NewObject(mapClass, mapInit);
    jmethodID mapPut = env->GetMethodID(mapClass, "put",
                                        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    if (type == TYPE_GET_TRAFFIC_RX) {
        rxTrafficInfoMapLock.lock_shared();
        for (auto & it : rxFdTrafficInfoMap) {
            int fd = it.first;
            if (fdThreadNameMap.count(fd) == 0) {
                continue;
            }
            fdThreadNameMapSharedLock.lock_shared();
            const char* key = fdThreadNameMap[fd].c_str();
            fdThreadNameMapSharedLock.unlock_shared();
            if (key == nullptr || strlen(key) == 0) {
                continue;
            }

            if (it.second <= 0) {
                continue;
            }
            jstring jKey = env->NewStringUTF(key);
            jstring traffic = env->NewStringUTF(to_string(it.second).c_str());
            env->CallObjectMethod(jHashMap, mapPut, jKey, traffic);
            env->DeleteLocalRef(jKey);
            env->DeleteLocalRef(traffic);

        }
        rxTrafficInfoMapLock.unlock_shared();
    } else if (type == TYPE_GET_TRAFFIC_TX) {
        txTrafficInfoMapLock.lock_shared();
        for (auto & it : txFdTrafficInfoMap) {
            int fd = it.first;
            if (fdThreadNameMap.count(fd) == 0) {
                continue;
            }
            fdThreadNameMapSharedLock.lock_shared();
            const char* key = fdThreadNameMap[fd].c_str();
            fdThreadNameMapSharedLock.unlock_shared();
            if (key == nullptr || strlen(key) == 0) {
                continue;
            }

            if (it.second <= 0) {
                continue;
            }
            jstring jKey = env->NewStringUTF(key);
            jstring traffic = env->NewStringUTF(to_string(it.second).c_str());
            env->CallObjectMethod(jHashMap, mapPut, jKey, traffic);
            env->DeleteLocalRef(jKey);
            env->DeleteLocalRef(traffic);
        }
        txTrafficInfoMapLock.unlock_shared();
    }
    env->DeleteLocalRef(mapClass);
    return jHashMap;
}

void TrafficCollector::clearTrafficInfo() {
    rxTrafficInfoMapLock.lock();
    rxFdTrafficInfoMap.clear();
    rxTrafficInfoMapLock.unlock();

    txTrafficInfoMapLock.lock();
    txFdTrafficInfoMap.clear();
    txTrafficInfoMapLock.unlock();

    fdThreadNameMapSharedLock.lock();
    fdThreadNameMap.clear();
    fdThreadNameMapSharedLock.unlock();


}

TrafficCollector::~TrafficCollector() {
    stopLoop();
    clearTrafficInfo();
}

}// namespace MatrixTraffic


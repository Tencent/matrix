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
static lock_guard<mutex> lock(queueMutex);
static bool loopRunning = false;
static bool sDumpStackTrace = false;
static bool sLookupIpAddress = false;
static unordered_set<int> activeFdSet;
static unordered_set<int> invalidFdSet;

static blocking_queue<shared_ptr<TrafficMsg>> msgQueue;

static map<int, long> rxFdTrafficInfoMap;
static map<int, long> txFdTrafficInfoMap;
static shared_mutex rxTrafficInfoMapLock;
static shared_mutex txTrafficInfoMapLock;

static map<int, string> fdThreadNameMap;
static map<int, string> fdIpAddressMap;
static shared_mutex fdThreadNameMapSharedLock;
static mutex fdIpAddressMapLock;

string getIpAddressFromAddr(sockaddr* _addr) {
    string ipAddress;
    if (_addr != nullptr) {
        if ((int)_addr->sa_family == AF_LOCAL) {
            ipAddress = _addr->sa_data;
            ipAddress.append(":LOCAL");
        } else if ((int)_addr->sa_family == AF_INET) {
            auto *sin4 = reinterpret_cast<sockaddr_in*>(_addr);
            char ipv4str[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &(sin4->sin_addr), ipv4str, INET6_ADDRSTRLEN) != nullptr) {
                ipAddress = ipv4str;
                int port = ntohs(sin4->sin_port);
                if (port != -1) {
                    ipAddress.append(":");
                    ipAddress.append(to_string(port));
                }
            }
        } else if ((int)_addr->sa_family == AF_INET6) {
            auto *sin6 = reinterpret_cast<sockaddr_in6 *>(_addr);
            char ipv6str[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &(sin6->sin6_addr), ipv6str, INET6_ADDRSTRLEN) != nullptr) {
                ipAddress = ipv6str;
                int port = ntohs(sin6->sin6_port);
                if (port != -1) {
                    ipAddress.append(":");
                    ipAddress.append(to_string(port));
                }
            }
        }
    }
    return ipAddress;
}

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

void saveIpAddress(int fd, sockaddr* addr) {
    fdIpAddressMapLock.lock();
    if (fdIpAddressMap.count(fd) == 0) {
        fdIpAddressMap[fd] = getIpAddressFromAddr(addr);
    }
    fdIpAddressMapLock.unlock();
}

string getIpAddress(int fd) {
    fdIpAddressMapLock.lock();
    string ipAddress = fdIpAddressMap[fd];
    fdIpAddressMapLock.unlock();
    return ipAddress;
}


string saveFdInfo(int fd) {
    fdThreadNameMapSharedLock.lock_shared();
    if (fdThreadNameMap.count(fd) == 0) {
        fdThreadNameMapSharedLock.unlock_shared();

        auto threadName = new char[15];
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

void TrafficCollector::enQueueInit(int fd, int domain, int type) {
    if (type == SOCK_DGRAM && domain != AF_LOCAL) {
        saveFdInfo(fd);
        shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(MSG_TYPE_INIT, fd, 0);
    }
}

void TrafficCollector::enQueueConnect(int fd, sockaddr *addr, socklen_t addr_length) {
    if (!loopRunning) {
        return;
    }

    if (addr->sa_family == AF_LOCAL) {
        return;
    }

    saveFdInfo(fd);
    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(MSG_TYPE_CONNECT, fd, 0);

    msgQueue.push(msg);
    queueMutex.unlock();
}

void TrafficCollector::enQueueClose(int fd) {
    if (!loopRunning) {
        return;
    }
    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(MSG_TYPE_CLOSE, fd, 0);
    msgQueue.push(msg);
    queueMutex.unlock();
}

void enQueueMsg(int type, int fd, size_t len) {
    if (!loopRunning) {
        return;
    }

    saveFdInfo(fd);
    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(type, fd, len);
    msgQueue.push(msg);
    queueMutex.unlock();
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
    while (loopRunning) {
        if (msgQueue.empty()) {
            queueMutex.lock();
        } else {
            shared_ptr<TrafficMsg> msg = msgQueue.front();
            int fd = msg->fd;
            int type = msg->type;
            if (type != MSG_TYPE_INIT && type != MSG_TYPE_CLOSE) {
                if (activeFdSet.count(fd) == 0 && invalidFdSet.count(fd) == 0) {
                    if (!isNetworkSocketFd(fd)) {
                        invalidFdSet.insert(fd);
                        fdThreadNameMapSharedLock.lock();
                        fdThreadNameMap.erase(fd);
                        fdThreadNameMapSharedLock.unlock();
                    } else {
                        activeFdSet.insert(fd);
                    }
                }
            }

            if (type == MSG_TYPE_CONNECT || type == MSG_TYPE_INIT) {
                activeFdSet.insert(fd);
            } else if (type >= MSG_TYPE_READ && type <= MSG_TYPE_RECVMSG) {
                if (activeFdSet.count(fd) > 0 && invalidFdSet.count(fd) == 0) {
                    appendRxTraffic(fd, msg->len);
                }
            } else if (type >= MSG_TYPE_WRITE && type <= MSG_TYPE_SENDMSG) {
                if (activeFdSet.count(fd) > 0 && invalidFdSet.count(fd) == 0) {
                    appendTxTraffic(fd, msg->len);
                }
            } else if (type == MSG_TYPE_CLOSE) {
                if (activeFdSet.count(fd) > 0) {
                    fdThreadNameMapSharedLock.lock();
                    fdThreadNameMap.erase(fd);
                    fdThreadNameMapSharedLock.unlock();
                    activeFdSet.erase(fd);
                    invalidFdSet.erase(fd);
                }
            }
            msgQueue.pop();
        }
    }
}


void TrafficCollector::startLoop(bool dumpStackTrace) {
    sDumpStackTrace = dumpStackTrace;
    loopRunning = true;
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


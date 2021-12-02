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

#include <queue>
#include <thread>

using namespace std;

namespace MatrixTraffic {

static mutex queueMutex;
static lock_guard<mutex> lock(queueMutex);
static bool loopRunning = false;
static map<int, int> fdFamilyMap;
static blocking_queue<shared_ptr<TrafficMsg>> msgQueue;
static map<string, long> rxTrafficInfoMap;
static map<string, long> txTrafficInfoMap;
static mutex rxTrafficInfoMapLock;
static mutex txTrafficInfoMapLock;

static map<int, string> fdThreadNameMap;
static mutex fdThreadNameMapLock;

string getThreadName(int fd) {
    fdThreadNameMapLock.lock();
    if (fdThreadNameMap.count(fd) == 0) {
        auto threadName = new char[15];
        prctl(PR_GET_NAME, threadName);
        fdThreadNameMap[fd] = threadName;
        fdThreadNameMapLock.unlock();
        return threadName;
    } else {
        auto threadName = fdThreadNameMap[fd];
        fdThreadNameMapLock.unlock();
        return threadName;
    }
}

void TrafficCollector::enQueueConnect(int fd, sockaddr *addr, socklen_t addr_length) {
    if (!loopRunning) {
        return;
    }
    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(MSG_TYPE_CONNECT, fd, addr->sa_family, getThreadName(fd), 0);
    msgQueue.push(msg);
    queueMutex.unlock();
}

void enQueueMsg(int type, int fd, size_t len) {
    if (!loopRunning) {
        return;
    }
    shared_ptr<TrafficMsg> msg = make_shared<TrafficMsg>(type, fd, 0, getThreadName(fd), len);
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

void appendRxTraffic(const string& threadName, long len) {
    rxTrafficInfoMapLock.lock();
    rxTrafficInfoMap[threadName] += len;
    rxTrafficInfoMapLock.unlock();
}

void appendTxTraffic(const string& threadName, long len) {
    txTrafficInfoMapLock.lock();
    txTrafficInfoMap[threadName] += len;
    txTrafficInfoMapLock.unlock();
}

void loop() {
    while (loopRunning) {
        if (msgQueue.empty()) {
            queueMutex.lock();
        } else {
            shared_ptr<TrafficMsg> msg = msgQueue.front();
            if (msg->type == MSG_TYPE_CONNECT) {
                fdFamilyMap[msg->fd] = msg->sa_family;
            } else if (msg->type == MSG_TYPE_READ) {
                if (fdFamilyMap.count(msg->fd) > 0) {
                    appendRxTraffic(msg->threadName, msg->len);
                }
            } else if (msg->type >= MSG_TYPE_RECV && msg->type <= MSG_TYPE_RECVMSG) {
                if (fdFamilyMap[msg->fd] != AF_LOCAL) {
                    appendRxTraffic(msg->threadName, msg->len);
                }
            } else if (msg->type == MSG_TYPE_WRITE) {
                if (fdFamilyMap.count(msg->fd) > 0) {
                    appendTxTraffic(msg->threadName, msg->len);
                }
            } else if (msg->type >= MSG_TYPE_SEND && msg->type <= MSG_TYPE_SENDMSG) {
                if (fdFamilyMap[msg->fd] != AF_LOCAL) {
                    appendTxTraffic(msg->threadName, msg->len);
                }
            }
            msgQueue.pop();
        }
    }
}


void TrafficCollector::startLoop() {
    loopRunning = true;
    thread loopThread(loop);
    loopThread.detach();
}

void TrafficCollector::stopLoop() {
    loopRunning = false;
}

jobject TrafficCollector::getTrafficInfoMap(int type) {
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
        rxTrafficInfoMapLock.lock();
        for (auto & it : rxTrafficInfoMap) {
            jstring threadName = env->NewStringUTF(it.first.c_str());
            jstring traffic = env->NewStringUTF(to_string(it.second).c_str());
            env->CallObjectMethod(jHashMap, mapPut, threadName, traffic);
            env->DeleteLocalRef(threadName);
            env->DeleteLocalRef(traffic);
        }
        rxTrafficInfoMapLock.unlock();
    } else if (type == TYPE_GET_TRAFFIC_TX) {
        txTrafficInfoMapLock.lock();
        for (auto & it : txTrafficInfoMap) {
            jstring threadName = env->NewStringUTF(it.first.c_str());
            jstring traffic = env->NewStringUTF(to_string(it.second).c_str());
            env->CallObjectMethod(jHashMap, mapPut, threadName, traffic);
            env->DeleteLocalRef(threadName);
            env->DeleteLocalRef(traffic);
        }
        txTrafficInfoMapLock.unlock();
    }
    env->DeleteLocalRef(mapClass);
    return jHashMap;
}

void TrafficCollector::clearTrafficInfo() {
    rxTrafficInfoMapLock.lock();
    rxTrafficInfoMap.clear();
    rxTrafficInfoMapLock.unlock();

    txTrafficInfoMapLock.lock();
    txTrafficInfoMap.clear();
    txTrafficInfoMapLock.unlock();

    fdThreadNameMapLock.lock();
    fdThreadNameMap.clear();
    fdThreadNameMapLock.unlock();
}

TrafficCollector::~TrafficCollector() {
    stopLoop();
    clearTrafficInfo();
}

}// namespace MatrixTraffic


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
// TrafficCollector.h


#ifndef MATRIX_ANDROID_TRAFFICCOLLECTOR_H
#define MATRIX_ANDROID_TRAFFICCOLLECTOR_H

#include <jni.h>
#include "util/Logging.h"
#include <sys/socket.h>

#include <string>
#include <vector>
#include <map>
#include <mutex>

#define MSG_TYPE_INIT 0
#define MSG_TYPE_CONNECT 1

#define MSG_TYPE_READ 10
#define MSG_TYPE_RECV 11
#define MSG_TYPE_RECVFROM 12
#define MSG_TYPE_RECVMSG 13

#define MSG_TYPE_WRITE 20
#define MSG_TYPE_SEND 21
#define MSG_TYPE_SENDTO 22
#define MSG_TYPE_SENDMSG 23

#define MSG_TYPE_CLOSE 30

#define TYPE_GET_TRAFFIC_RX 0
#define TYPE_GET_TRAFFIC_TX 1

using namespace std;

namespace MatrixTraffic {
class TrafficMsg {
public:
    TrafficMsg(const int _type, const int _fd, long _len)
            : type(_type), fd(_fd), len(_len) {
    }
    int type;
    int fd;
//    sa_family_t sa_family;
//    string threadName;
    long len;
};

class TrafficCollector {

public :
    static void startLoop(bool dumpStackTrace);

    static void stopLoop();

    static void enQueueInit(int fd, int domain, int type);

    static void enQueueConnect(int fd, sockaddr *addr, socklen_t __addr_length);

    static void enQueueClose(int fd);

    static void enQueueTx(int type, int fd, size_t len);

    static void enQueueRx(int type, int fd, size_t len);

    static void clearTrafficInfo();

    static jobject getFdTrafficInfoMap(int type);

    virtual ~TrafficCollector();
};
}// namespace MatrixTraffic

#endif //MATRIX_ANDROID_TRAFFICCOLLECTOR_H

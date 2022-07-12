//
// Created by 贾建业 on 2021/11/16.
//

#include "TouchEventTracer.h"
#include "Logging.h"
#include "MatrixTracer.h"
#include "unistd.h"

#include <thread>
#include <ctime>
#include <deque>
#include <chrono>

using namespace std;

static mutex queueMutex;
static condition_variable cv;
static bool loopRunning = false;
static bool startDetect = false;
static int LAG_THRESHOLD;
long lastRecvTouchEventTimeStamp = 0;
static int currentFd = 0;
static int lagFd = 0;

void reportLag() {
    if (lagFd == 0) {
        return;
    }
    onTouchEventLag(lagFd);
}

void TouchEventTracer::touchRecv(int fd) {
    currentFd = fd;
    lagFd = 0;
    if (lagFd == fd) {
        lastRecvTouchEventTimeStamp = 0;
    } else {
        lastRecvTouchEventTimeStamp = time(nullptr);
        cv.notify_one();
    }
}

void TouchEventTracer::touchSendFinish(int fd) {
    if (lagFd == fd) {
        reportLag();
    }
    lagFd = 0;
    lastRecvTouchEventTimeStamp = 0;
    startDetect = true;
}


void recvQueueLooper() {
    unique_lock lk(queueMutex);
    while (loopRunning) {
        if (lastRecvTouchEventTimeStamp == 0) {
            cv.wait(lk);
        } else {
            long lastRecvTouchEventTimeStampNow = lastRecvTouchEventTimeStamp;
            if (lastRecvTouchEventTimeStampNow <= 0) {
                continue;
            }

            if (time(nullptr) - lastRecvTouchEventTimeStampNow >= LAG_THRESHOLD && startDetect) {
                lagFd = currentFd;
                onTouchEventLagDumpTrace(currentFd);
                cv.wait(lk);
            }
        }
    }
}

void TouchEventTracer::start(int threshold) {
    LAG_THRESHOLD = threshold / 1000;
    loopRunning = true;
    thread recvThread = thread(recvQueueLooper);
    recvThread.detach();
}

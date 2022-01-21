//
// Created by 贾建业 on 2021/11/16.
//

#ifndef MATRIX_ANDROID_TOUCHEVENTTRACER_H
#define MATRIX_ANDROID_TOUCHEVENTTRACER_H

class TouchEventTracer{
public :
    static void touchRecv(int fd);
    static void touchSendFinish(int fd);
    static void start(int threshold);
};

#endif //MATRIX_ANDROID_TOUCHEVENTTRACER_H

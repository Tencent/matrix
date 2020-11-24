//
// Created by Yves on 2020-04-09.
//

#ifndef LIBWXPERF_JNI_ENHANCEDLSYM_H
#define LIBWXPERF_JNI_ENHANCEDLSYM_H


class EnhanceDlsym {

public:
    void * dlopen(const char *filename, int flag);
    void * dlsym(void *handle, const char *symbol);
private:

};


#endif //LIBWXPERF_JNI_ENHANCEDLSYM_H

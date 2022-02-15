#include <jni.h>
#include <GLES2/gl2.h>
#include <cstdlib>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/log.h>
#include <thread>

#define TAG "matrix.openglHook"

int egl_init() {
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail: eglDisplay == EGL_NO_DISPLAY");
        return -1;
    }

    EGLint *version = new EGLint[2];
    if (!eglInitialize(eglDisplay, &version[0], &version[1])) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail: eglInitialize fail");
        return -1;
    }

    const EGLint attrib_config_list[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 8,
            EGL_STENCIL_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
    };

    EGLint num_config;
    if (!eglChooseConfig(eglDisplay, attrib_config_list, NULL, 1, &num_config)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail:  eglChooseConfig fail");
        return -1;
    }


    EGLConfig eglConfig;
    if (!eglChooseConfig(eglDisplay, attrib_config_list, &eglConfig, num_config, &num_config)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail:  eglChooseConfig fail");
        return -1;
    }

    const EGLint attrib_ctx_list[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    EGLContext eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, attrib_ctx_list);
    if (eglContext == EGL_NO_CONTEXT) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail:  eglCreateContext fail");
        return -1;
    }

    EGLSurface eglSurface = eglCreatePbufferSurface(eglDisplay, eglConfig, NULL);
    if (eglSurface == EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail:  eglCreatePbufferSurface fail");
        return -1;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "egl_init fail:  eglMakeCurrent fail");
        return -1;
    }

    return 0;
}

void native_gl_profiler(const std::string& thread_name, GLuint *textures, GLuint *buffers, GLuint *renderbuffers, int total_count) {
    egl_init();

    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    long start = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    for (int i = 0; i < total_count; i++) {
        glGenRenderbuffers(1, renderbuffers + i);
        glGenTextures(1, textures + i);
        glGenBuffers(1, buffers + i);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[i]);
        glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STATIC_DRAW);

        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 0, 0);
    }

    gettimeofday(&tv, nullptr);
    long end = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    long cost = end - start;

    __android_log_print(ANDROID_LOG_ERROR, TAG, "native_gl_profiler thread = %s finish, cost = %ld, start = %ld, end = %ld", thread_name.c_str(), cost, start, end);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_openglhook_OpenglHookTestActivity_openglNativeProfiler(JNIEnv *env,
                                                                        jobject thiz,
                                                                              jint thread_count) {

    for (int i = 0; i < thread_count; ++i) {
        int total_count = 1000;
        auto *textures = new GLuint[total_count];
        auto *buffers = new GLuint[total_count];
        auto *renderbuffers = new GLuint[total_count];

        std::thread test_thread = std::thread([i, textures, buffers, renderbuffers, total_count]() {
            std::string thread_name = "opengl_test_thread_" + std::to_string(i);
            native_gl_profiler(thread_name, textures, buffers, renderbuffers, total_count);
        });
        test_thread.detach();
    }
}
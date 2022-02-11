#include <jni.h>
#include <GLES2/gl2.h>
#include <cstdlib>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/log.h>
#include <thread>

#define TAG "matrix.openglHook"

GLuint *buffers = nullptr;
GLuint *textures = nullptr;
GLuint *renderbuffers = nullptr;

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

void native_gl_profiler(std::string thread_name, int resource_count, int currPos) {
    egl_init();

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long start = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    for (int i = 0; i < resource_count; i++) {
        glGenRenderbuffers(1, renderbuffers + (currPos * resource_count + i));
        glGenTextures(1, textures + (currPos * resource_count + i));
        glGenBuffers(1, buffers + (currPos * resource_count + i));

        glBindBuffer(GL_ARRAY_BUFFER, buffers[currPos * resource_count + i]);
        glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STATIC_DRAW);

        glBindTexture(GL_TEXTURE_2D, textures[currPos * resource_count + i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[currPos * resource_count + i]);
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
                                                                              jint thread_count,
                                                                              jint resource_count) {

    textures = new GLuint[thread_count * resource_count];
    buffers = new GLuint[thread_count * resource_count];
    renderbuffers = new GLuint[thread_count * resource_count];

    for (int i = 0; i < thread_count; ++i) {
        std::thread test_thread = std::thread([i, resource_count]() {
            std::string thread_name = "opengl_test_thread - " + std::to_string(i);
            native_gl_profiler(thread_name, resource_count, i);
        });
        test_thread.detach();
    }
}
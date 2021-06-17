//
// Created by 邓沛堆 on 2020-05-28.
//

#include "type.h"
#include <string.h>

char* get_type_string(int opengl_type) {
    if (opengl_type == TYPE_TEXTURES) {
        return const_cast<char *>("textures");
    }

    if (opengl_type == TYPE_BUFFERS) {
        return const_cast<char *>("buffers");
    }

    if (opengl_type == TYPE_FRAMEBUFFERS) {
        return const_cast<char *>("framebuffers");
    }

    if (opengl_type == TYPE_RENDERBUFFERS) {
        return const_cast<char *>("renderbuffers");
    }

    return const_cast<char *>("unkown");
}

System_GlNormal_TYPE get_target_func_ptr(const char *func_name) {
    if (strcmp(func_name, "glGenTextures") == 0) {
        return (System_GlNormal_TYPE) glGenTextures;
    } else if (strcmp(func_name, "glDeleteTextures") == 0) {
        return (System_GlNormal_TYPE) glDeleteTextures;
    } else if (strcmp(func_name, "glGenBuffers") == 0) {
        return (System_GlNormal_TYPE) glGenBuffers;
    } else if (strcmp(func_name, "glDeleteBuffers") == 0) {
        return (System_GlNormal_TYPE) glDeleteBuffers;
    } else if (strcmp(func_name, "glGenFramebuffers") == 0) {
        return (System_GlNormal_TYPE) glGenFramebuffers;
    } else if (strcmp(func_name, "glDeleteFramebuffers") == 0) {
        return (System_GlNormal_TYPE) glDeleteFramebuffers;
    } else if (strcmp(func_name, "glGenRenderbuffers") == 0) {
        return (System_GlNormal_TYPE) glGenRenderbuffers;
    } else if (strcmp(func_name, "glDeleteRenderbuffers") == 0) {
        return (System_GlNormal_TYPE) glDeleteRenderbuffers;
    }

    return NULL;
}
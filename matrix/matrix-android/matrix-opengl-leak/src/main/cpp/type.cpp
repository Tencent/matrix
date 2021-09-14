//
// Created by 邓沛堆 on 2020-05-28.
//

#include "type.h"
#include <cstring>
#include <map>

#define BIT 8

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

System_GlBind_TYPE get_bind_func_ptr(const char *func_name) {
    if (strcmp(func_name, "glBindTexture") == 0) {
        return (System_GlBind_TYPE) glBindTexture;
    } else if (strcmp(func_name, "glBindBuffer") == 0) {
        return (System_GlBind_TYPE) glBindBuffer;
    } else if (strcmp(func_name, "glBindFramebuffer") == 0) {
        return (System_GlBind_TYPE) glBindFramebuffer;
    } else if (strcmp(func_name, "glBindRenderbuffer") == 0) {
        return (System_GlBind_TYPE) glBindRenderbuffer;
    }

    return NULL;
}

int getPartByFormat(GLint internalformat, GLenum format, int bit) {
    switch (format) {
        case GL_RGB:
        case GL_RGBA:
        default:return 1;
    }
}

int getSizeOfPerPixelByFormat(GLint internalformat, GLenum format) {

    if (internalformat == GL_RGB565) {
        return 2;
    }

    if (internalformat == GL_RGB5_A1) {
        return 2;
    }

    if (internalformat == GL_RGBA4) {
        return 2;
    }


    return 0;
}


namespace Utils {

    int getSizeOfPerPixel(GLint internalformat, GLenum format, GLenum type) {
        switch (type) {
            case GL_UNSIGNED_BYTE:
                return getSizeOfPerPixelByFormat(internalformat, format);
            case GL_UNSIGNED_SHORT_5_6_5:
                return 2;
            case GL_UNSIGNED_SHORT_4_4_4_4:
                return 2;
            case GL_UNSIGNED_SHORT_5_5_5_1:
                return 2;
            case GL_BYTE:
            case GL_UNSIGNED_INT:
            case GL_INT:
            case GL_FLOAT:
            default:
                return 0;
        }
    }

}
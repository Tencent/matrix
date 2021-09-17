//
// Created by 邓沛堆 on 2020-05-28.
//

#include "type.h"
#include <cstring>

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

namespace Utils {

    /*
     *  support by OpenGL ES Software Development Kit
     */
    int getSizeByInternalFormat(GLint internalformat) {

        switch (internalformat) {
            case GL_R8:
                return 1;
            case GL_R8UI:
                return 1;
            case GL_R8I:
                return 1;
            case GL_R16UI:
                return 2;
            case GL_R16I:
                return 2;
            case GL_R32UI:
                return 4;
            case GL_R32I:
                return 4;
            case GL_RG8:
                return 1;
            case GL_RG8UI:
                return 1;
            case GL_RG8I:
                return 1;
            case GL_RG16UI:
                return 2;
            case GL_RG16I:
                return 2;
            case GL_RG32UI:
                return 4;
            case GL_RG32I:
                return 4;
            case GL_RGB8:
                return 3;
            case GL_RGB565:
                return 2;
            case GL_RGBA8:
                return 4;
            case GL_SRGB8_ALPHA8:
                return 4;
            case GL_RGB5_A1:
                return 2;
            case GL_RGBA4:
                return 2;
            case GL_RGB10_A2:
                return 4;
            case GL_RGBA8UI:
                return 4;
            case GL_RGBA8I:
                return 4;
            case GL_RGB10_A2UI:
                return 4;
            case GL_RGBA16UI:
                return 8;
            case GL_RGBA16I:
                return 8;
            case GL_RGBA32I:
                return 16;
            case GL_RGBA32UI:
                return 16;
            case GL_DEPTH_COMPONENT16:
                return 2;
            case GL_DEPTH_COMPONENT24:
                return 3;
            case GL_DEPTH_COMPONENT32F:
                return 4;
            case GL_DEPTH24_STENCIL8:
                return 4;
            case GL_DEPTH32F_STENCIL8:
                return 5;
            case GL_STENCIL_INDEX8:
                return 1;
            default:
                return 0;
        }
    }


    int getSizeOfPerPixel(GLint internalformat, GLenum format, GLenum type) {

        // GL_RED
        if (internalformat == GL_R8 && format == GL_RED && type == GL_UNSIGNED_BYTE) {
            return 1;
        }
        if (internalformat == GL_R8_SNORM && format == GL_RED && type == GL_BYTE) {
            return 1;
        }
        if (internalformat == GL_R16F && format == GL_RED &&
            (type == GL_HALF_FLOAT || type == GL_FLOAT)) {
            return 2;
        }
        if (internalformat == GL_R32F && format == GL_RED && type == GL_FLOAT) {
            return 4;
        }

        // GL_RED_INTEGER
        if (internalformat == GL_R8UI && format == GL_RED_INTEGER && type == GL_UNSIGNED_BYTE) {
            return 1;
        }
        if (internalformat == GL_R8I && format == GL_RED_INTEGER && type == GL_BYTE) {
            return 1;
        }
        if (internalformat == GL_R16UI && format == GL_RED_INTEGER && type == GL_UNSIGNED_SHORT) {
            return 2;
        }
        if (internalformat == GL_R16I && format == GL_RED_INTEGER && type == GL_SHORT) {
            return 2;
        }
        if (internalformat == GL_R32UI && format == GL_RED_INTEGER && type == GL_UNSIGNED_INT) {
            return 4;
        }
        if (internalformat == GL_R32I && format == GL_RED_INTEGER && type == GL_INT) {
            return 4;
        }

        // GL_RG
        if (internalformat == GL_RG8 && format == GL_RG && type == GL_UNSIGNED_BYTE) {
            return 2;
        }
        if (internalformat == GL_RG8_SNORM && format == GL_RG && type == GL_BYTE) {
            return 2;
        }
        if (internalformat == GL_RG16F && format == GL_RG &&
            (type == GL_HALF_FLOAT || type == GL_FLOAT)) {
            return 4;
        }
        if (internalformat == GL_RG32F && format == GL_RG && type == GL_FLOAT) {
            return 8;
        }

        // GL_RG_INTEGER
        if (internalformat == GL_RG8UI && format == GL_RG_INTEGER && type == GL_UNSIGNED_BYTE) {
            return 2;
        }
        if (internalformat == GL_RG8I && format == GL_RG_INTEGER && type == GL_BYTE) {
            return 2;
        }
        if (internalformat == GL_RG16UI && format == GL_RG_INTEGER && type == GL_UNSIGNED_SHORT) {
            return 4;
        }
        if (internalformat == GL_RG16I && format == GL_RG_INTEGER && type == GL_SHORT) {
            return 4;
        }
        if (internalformat == GL_RG32UI && format == GL_RG_INTEGER && type == GL_UNSIGNED_INT) {
            return 8;
        }
        if (internalformat == GL_RG32I && format == GL_RG_INTEGER && type == GL_INT) {
            return 8;
        }

        // GL_RGB
        if (internalformat == GL_RGB8 && format == GL_RGB && type == GL_UNSIGNED_BYTE) {
            return 3;
        }
        if (internalformat == GL_SRGB8 && format == GL_RGB && type == GL_UNSIGNED_BYTE) {
            return 3;
        }
        if (internalformat == GL_RGB565 && format == GL_RGB &&
            (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5)) {
            return 2;
        }
        if (internalformat == GL_RGB8_SNORM && format == GL_RGB && type == GL_BYTE) {
            return 3;
        }
        if (internalformat == GL_R11F_G11F_B10F && format == GL_RGB
            && (type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_HALF_FLOAT ||
                type == GL_FLOAT)) {
            return 4;
        }
        if (internalformat == GL_RGB9_E5 && format == GL_RGB
            && (type == GL_UNSIGNED_INT_5_9_9_9_REV || type == GL_HALF_FLOAT || type == GL_FLOAT)) {
            return 4;
        }
        if (internalformat == GL_RGB16F && format == GL_RGB &&
            (type == GL_HALF_FLOAT || type == GL_FLOAT)) {
            return 6;
        }
        if (internalformat == GL_RGB32F && format == GL_RGB && type == GL_FLOAT) {
            return 12;
        }

        // GL_RGB_INTEGER
        if (internalformat == GL_RGB8UI && format == GL_RGB_INTEGER && type == GL_UNSIGNED_BYTE) {
            return 3;
        }
        if (internalformat == GL_RGB8I && format == GL_RGB_INTEGER && type == GL_BYTE) {
            return 3;
        }
        if (internalformat == GL_RGB16UI && format == GL_RGB_INTEGER && type == GL_UNSIGNED_SHORT) {
            return 6;
        }
        if (internalformat == GL_RGB16I && format == GL_RGB_INTEGER && type == GL_SHORT) {
            return 6;
        }
        if (internalformat == GL_RGB32UI && format == GL_RGB_INTEGER && type == GL_UNSIGNED_INT) {
            return 12;
        }
        if (internalformat == GL_RGB32I && format == GL_RGB_INTEGER && type == GL_INT) {
            return 12;
        }

        // GL_RGBA
        if (internalformat == GL_RGBA8 && format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
            return 4;
        }
        if (internalformat == GL_SRGB8_ALPHA8 && format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
            return 4;
        }
        if (internalformat == GL_RGBA8_SNORM && format == GL_RGBA && type == GL_BYTE) {
            return 4;
        }
        if (internalformat == GL_RGB5_A1 && format == GL_RGBA &&
            (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_5_5_1 ||
             type == GL_UNSIGNED_INT_2_10_10_10_REV)) {
            return 2;
        }
        if (internalformat == GL_RGBA4 && format == GL_RGBA &&
            (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_4_4_4_4)) {
            return 2;
        }
        if (internalformat == GL_RGB10_A2 && format == GL_RGBA &&
            type == GL_UNSIGNED_INT_2_10_10_10_REV) {
            return 4;
        }
        if (internalformat == GL_RGBA16F && format == GL_RGBA &&
            (type == GL_HALF_FLOAT || type == GL_FLOAT)) {
            return 8;
        }
        if (internalformat == GL_RGBA32F && format == GL_RGBA && type == GL_FLOAT) {
            return 16;
        }

        // GL_RGBA_INTEGER
        if (internalformat == GL_RGBA8UI && format == GL_RGBA_INTEGER && type == GL_UNSIGNED_BYTE) {
            return 4;
        }
        if (internalformat == GL_RGBA8I && format == GL_RGBA_INTEGER && type == GL_BYTE) {
            return 4;
        }
        if (internalformat == GL_RGB10_A2UI && format == GL_RGBA_INTEGER &&
            type == GL_UNSIGNED_INT_2_10_10_10_REV) {
            return 4;
        }
        if (internalformat == GL_RGBA16UI && format == GL_RGBA_INTEGER &&
            type == GL_UNSIGNED_SHORT) {
            return 8;
        }
        if (internalformat == GL_RGBA16I && format == GL_RGBA_INTEGER && type == GL_SHORT) {
            return 8;
        }
        if (internalformat == GL_RGBA32I && format == GL_RGBA_INTEGER && type == GL_INT) {
            return 16;
        }
        if (internalformat == GL_RGBA32UI && format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT) {
            return 16;
        }

        // GL_DEPTH_COMPONENT
        if (internalformat == GL_DEPTH_COMPONENT16 && format == GL_DEPTH_COMPONENT &&
            (type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT)) {
            return 2;
        }
        if (internalformat == GL_DEPTH_COMPONENT24 && format == GL_DEPTH_COMPONENT &&
            type == GL_UNSIGNED_INT) {
            return 3;
        }
        if (internalformat == GL_DEPTH_COMPONENT32F && format == GL_DEPTH_COMPONENT &&
            type == GL_FLOAT) {
            return 4;
        }

        // GL_DEPTH24_STENCIL8
        if (internalformat == GL_DEPTH24_STENCIL8 && format == GL_DEPTH_STENCIL &&
            type == GL_UNSIGNED_INT_24_8) {
            return 4;
        }
        if (internalformat == GL_DEPTH32F_STENCIL8 && format == GL_DEPTH_STENCIL &&
            type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
            return 5;
        }

        // GL_STENCIL_INDEX
        if (internalformat == GL_STENCIL_INDEX8 && format == GL_STENCIL_INDEX &&
            type == GL_UNSIGNED_BYTE) {
            return 1;
        }

        // Unsized Internal Formats
        if (internalformat == GL_RGB && format == GL_RGB && type == GL_UNSIGNED_BYTE) {
            return 3;
        }
        if (internalformat == GL_RGB && format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5) {
            return 2;
        }
        if (internalformat == GL_RGBA && format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
            return 4;
        }
        if (internalformat == GL_RGBA && format == GL_RGBA && type == GL_UNSIGNED_SHORT_4_4_4_4) {
            return 2;
        }
        if (internalformat == GL_LUMINANCE_ALPHA && format == GL_LUMINANCE_ALPHA &&
            type == GL_UNSIGNED_BYTE) {
            return 2;
        }
        if (internalformat == GL_LUMINANCE && format == GL_LUMINANCE && type == GL_UNSIGNED_BYTE) {
            return 1;
        }
        if (internalformat == GL_ALPHA && format == GL_ALPHA && type == GL_UNSIGNED_BYTE) {
            return 1;
        }

        return 0;
    }


    /*
     *  support by Local Experiment
     */

    long *interval_all = nullptr;
    int interval_count = -1;

    void initRenderbufferSizeFormula(GLenum internalformat) {
        int format_byte = Utils::getSizeByInternalFormat(internalformat);
        int interval_double[] = {65536, 131072, 262144, 524288};
        const int biggest_size =
                (GL_MAX_RENDERBUFFER_SIZE * GL_MAX_RENDERBUFFER_SIZE) / format_byte;
        int double_count = sizeof(interval_double) / sizeof(interval_double[0]);
        interval_count = double_count +
                         (biggest_size - interval_double[double_count - 1]) / interval_double[0] +
                         1;

        interval_all = new long[interval_count];
        for (int i = 0; i < double_count; ++i) {
            interval_all[i] = interval_double[i];
        }
        for (int i = double_count - 1; i < interval_count; ++i) {
            interval_all[i] = interval_all[i - 1] + interval_double[0];
        }
    }

    long getRenderbufferSizeByFormula(GLenum internalformat, GLsizei width, GLsizei height) {
        if (interval_all == nullptr) {
            initRenderbufferSizeFormula(internalformat);
        }
        if (interval_all == nullptr || interval_count == -1) {
            return 0;
        }
        long graphics_alloc_size = 0;
        int calculate_size =
                (width * height) / (Utils::getSizeByInternalFormat(internalformat) * 8);
        for (int i = 0; i < interval_count; ++i) {
            int val = interval_all[i];
            if (calculate_size <= val) {
                graphics_alloc_size = val;
                break;
            }
        }
        return graphics_alloc_size;
    }

}
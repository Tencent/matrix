//
// Created by 邓沛堆 on 2020-05-26.
//

#include <GLES2/gl2.h>

#ifndef OPENGL_API_HOOK_TYPE_H
#define OPENGL_API_HOOK_TYPE_H

#define TYPE_TEXTURES 0x0011
#define TYPE_BUFFERS 0x0012
#define TYPE_FRAMEBUFFERS 0x0013
#define TYPE_RENDERBUFFERS 0x0014

typedef int (*System_GlGetError_TYPE)(void);

typedef void (*System_GlGenTexture_TYPE)(GLsizei n, GLuint *textures);

typedef void (*System_GlDeleteTexture_TYPE)(GLsizei n, const GLuint *textures);

typedef void (*System_GlNormal_TYPE)(GLsizei n, const GLuint *normal);

typedef void (*System_GlBind_TYPE)(GLenum target, GLuint resourceId);

typedef void (*System_GlTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                    GLint border, GLenum format, GLenum type, const void *pixels);

System_GlNormal_TYPE get_target_func_ptr(const char *func_name);

System_GlBind_TYPE get_bind_func_ptr(const char *func_name);

#endif //OPENGL_API_HOOK_TYPE_H
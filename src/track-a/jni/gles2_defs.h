
#ifndef GLES2_DEFS_H
#define GLES2_DEFS_H

#ifdef _WIN32
#include <windows.h>
#else
#define WINAPI
#endif
#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef void GLvoid;
typedef char GLchar;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_BYTE 0x1401
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE_2D 0x0DE1
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_SCISSOR_TEST 0x0C11
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44

typedef void (WINAPI *PFN_glActiveTexture)(GLenum texture);
typedef void (WINAPI *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void (WINAPI *PFN_glBindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
typedef void (WINAPI *PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void (WINAPI *PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void (WINAPI *PFN_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
typedef void (WINAPI *PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void (WINAPI *PFN_glBlendColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (WINAPI *PFN_glBlendEquation)(GLenum mode);
typedef void (WINAPI *PFN_glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
typedef void (WINAPI *PFN_glBlendFunc)(GLenum sfactor, GLenum dfactor);
typedef void (WINAPI *PFN_glBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (WINAPI *PFN_glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (WINAPI *PFN_glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef GLenum (WINAPI *PFN_glCheckFramebufferStatus)(GLenum target);
typedef void (WINAPI *PFN_glClear)(GLbitfield mask);
typedef void (WINAPI *PFN_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (WINAPI *PFN_glClearDepthf)(GLclampf depth);
typedef void (WINAPI *PFN_glClearStencil)(GLint s);
typedef void (WINAPI *PFN_glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (WINAPI *PFN_glCompileShader)(GLuint shader);
typedef void (WINAPI *PFN_glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
typedef void (WINAPI *PFN_glCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (WINAPI *PFN_glCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (WINAPI *PFN_glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLuint (WINAPI *PFN_glCreateProgram)(void);
typedef GLuint (WINAPI *PFN_glCreateShader)(GLenum type);
typedef void (WINAPI *PFN_glCullFace)(GLenum mode);
typedef void (WINAPI *PFN_glDeleteBuffers)(GLsizei n, const GLuint *buffers);
typedef void (WINAPI *PFN_glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
typedef void (WINAPI *PFN_glDeleteProgram)(GLuint program);
typedef void (WINAPI *PFN_glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
typedef void (WINAPI *PFN_glDeleteShader)(GLuint shader);
typedef void (WINAPI *PFN_glDeleteTextures)(GLsizei n, const GLuint *textures);
typedef void (WINAPI *PFN_glDepthFunc)(GLenum func);
typedef void (WINAPI *PFN_glDepthMask)(GLboolean flag);
typedef void (WINAPI *PFN_glDepthRangef)(GLclampf n, GLclampf f);
typedef void (WINAPI *PFN_glDetachShader)(GLuint program, GLuint shader);
typedef void (WINAPI *PFN_glDisable)(GLenum cap);
typedef void (WINAPI *PFN_glDisableVertexAttribArray)(GLuint index);
typedef void (WINAPI *PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (WINAPI *PFN_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (WINAPI *PFN_glEnable)(GLenum cap);
typedef void (WINAPI *PFN_glEnableVertexAttribArray)(GLuint index);
typedef void (WINAPI *PFN_glFinish)(void);
typedef void (WINAPI *PFN_glFlush)(void);
typedef void (WINAPI *PFN_glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (WINAPI *PFN_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (WINAPI *PFN_glFrontFace)(GLenum mode);
typedef void (WINAPI *PFN_glGenBuffers)(GLsizei n, GLuint *buffers);
typedef void (WINAPI *PFN_glGenerateMipmap)(GLenum target);
typedef void (WINAPI *PFN_glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
typedef void (WINAPI *PFN_glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
typedef void (WINAPI *PFN_glGenTextures)(GLsizei n, GLuint *textures);
typedef void (WINAPI *PFN_glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (WINAPI *PFN_glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (WINAPI *PFN_glGetAttachedShaders)(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders);
typedef GLint (WINAPI *PFN_glGetAttribLocation)(GLuint program, const GLchar *name);
typedef void (WINAPI *PFN_glGetBufferParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef GLenum (WINAPI *PFN_glGetError)(void);
typedef void (WINAPI *PFN_glGetFloatv)(GLenum pname, GLfloat *params);
typedef void (WINAPI *PFN_glGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetIntegerv)(GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog);
typedef void (WINAPI *PFN_glGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog);
typedef void (WINAPI *PFN_glGetShaderPrecisionFormat)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (WINAPI *PFN_glGetShaderSource)(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source);
typedef const GLubyte *(WINAPI *PFN_glGetString)(GLenum name);
typedef void (WINAPI *PFN_glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
typedef void (WINAPI *PFN_glGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetUniformfv)(GLuint program, GLint location, GLfloat *params);
typedef void (WINAPI *PFN_glGetUniformiv)(GLuint program, GLint location, GLint *params);
typedef GLint (WINAPI *PFN_glGetUniformLocation)(GLuint program, const GLchar *name);
typedef void (WINAPI *PFN_glGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
typedef void (WINAPI *PFN_glGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
typedef void (WINAPI *PFN_glGetVertexAttribPointerv)(GLuint index, GLenum pname, void **pointer);
typedef void (WINAPI *PFN_glHint)(GLenum target, GLenum mode);
typedef GLboolean (WINAPI *PFN_glIsBuffer)(GLuint buffer);
typedef GLboolean (WINAPI *PFN_glIsEnabled)(GLenum cap);
typedef GLboolean (WINAPI *PFN_glIsFramebuffer)(GLuint framebuffer);
typedef GLboolean (WINAPI *PFN_glIsProgram)(GLuint program);
typedef GLboolean (WINAPI *PFN_glIsRenderbuffer)(GLuint renderbuffer);
typedef GLboolean (WINAPI *PFN_glIsShader)(GLuint shader);
typedef GLboolean (WINAPI *PFN_glIsTexture)(GLuint texture);
typedef void (WINAPI *PFN_glLineWidth)(GLfloat width);
typedef void (WINAPI *PFN_glLinkProgram)(GLuint program);
typedef void (WINAPI *PFN_glPixelStorei)(GLenum pname, GLint param);
typedef void (WINAPI *PFN_glPolygonOffset)(GLfloat factor, GLfloat units);
typedef void (WINAPI *PFN_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void (WINAPI *PFN_glReleaseShaderCompiler)(void);
typedef void (WINAPI *PFN_glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (WINAPI *PFN_glSampleCoverage)(GLclampf value, GLboolean invert);
typedef void (WINAPI *PFN_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (WINAPI *PFN_glShaderBinary)(GLsizei n, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length);
typedef void (WINAPI *PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (WINAPI *PFN_glStencilFunc)(GLenum func, GLint ref, GLuint mask);
typedef void (WINAPI *PFN_glStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (WINAPI *PFN_glStencilMask)(GLuint mask);
typedef void (WINAPI *PFN_glStencilMaskSeparate)(GLenum face, GLuint mask);
typedef void (WINAPI *PFN_glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void (WINAPI *PFN_glStencilOpSeparate)(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
typedef void (WINAPI *PFN_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (WINAPI *PFN_glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
typedef void (WINAPI *PFN_glTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
typedef void (WINAPI *PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void (WINAPI *PFN_glTexParameteriv)(GLenum target, GLenum pname, const GLint *params);
typedef void (WINAPI *PFN_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (WINAPI *PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void (WINAPI *PFN_glUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINAPI *PFN_glUniform1i)(GLint location, GLint v0);
typedef void (WINAPI *PFN_glUniform1iv)(GLint location, GLsizei count, const GLint *value);
typedef void (WINAPI *PFN_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
typedef void (WINAPI *PFN_glUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINAPI *PFN_glUniform2i)(GLint location, GLint v0, GLint v1);
typedef void (WINAPI *PFN_glUniform2iv)(GLint location, GLsizei count, const GLint *value);
typedef void (WINAPI *PFN_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (WINAPI *PFN_glUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINAPI *PFN_glUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
typedef void (WINAPI *PFN_glUniform3iv)(GLint location, GLsizei count, const GLint *value);
typedef void (WINAPI *PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (WINAPI *PFN_glUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINAPI *PFN_glUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (WINAPI *PFN_glUniform4iv)(GLint location, GLsizei count, const GLint *value);
typedef void (WINAPI *PFN_glUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINAPI *PFN_glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINAPI *PFN_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINAPI *PFN_glUseProgram)(GLuint program);
typedef void (WINAPI *PFN_glValidateProgram)(GLuint program);
typedef void (WINAPI *PFN_glVertexAttrib1f)(GLuint index, GLfloat x);
typedef void (WINAPI *PFN_glVertexAttrib1fv)(GLuint index, const GLfloat *v);
typedef void (WINAPI *PFN_glVertexAttrib2f)(GLuint index, GLfloat x, GLfloat y);
typedef void (WINAPI *PFN_glVertexAttrib2fv)(GLuint index, const GLfloat *v);
typedef void (WINAPI *PFN_glVertexAttrib3f)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (WINAPI *PFN_glVertexAttrib3fv)(GLuint index, const GLfloat *v);
typedef void (WINAPI *PFN_glVertexAttrib4f)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (WINAPI *PFN_glVertexAttrib4fv)(GLuint index, const GLfloat *v);
typedef void (WINAPI *PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (WINAPI *PFN_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);

#endif /* GLES2_DEFS_H */

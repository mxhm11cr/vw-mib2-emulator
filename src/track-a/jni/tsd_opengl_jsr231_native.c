/*
 * Clean-room JNI bridge for tsd.opengl.jsr231 (EGL) and tsd.opengl.impl (GL)
 * Backed by Google ANGLE (Direct3D 11) via libEGL.dll and libGLESv2.dll
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define _GNU_SOURCE
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <time.h>
#define WINAPI
typedef void* HMODULE;
typedef void* HWND;
typedef void* HANDLE;
typedef void* HDC;
typedef void* HFONT;
typedef void* HGDIOBJ;
typedef void* HBRUSH;
#endif
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "gles2_defs.h"

#define EGL_DEFAULT_DISPLAY ((void*)0)
#define EGL_NO_CONTEXT ((void*)0)
#define EGL_NO_SURFACE ((void*)0)
#define EGL_SUCCESS 0x3000
#define EGL_BUFFER_SIZE 0x3020
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_SURFACE_TYPE 0x3033
#define EGL_NONE 0x3038
#define EGL_WINDOW_BIT 0x0004
#define EGL_PBUFFER_BIT 0x0001
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef unsigned int EGLBoolean;
typedef int EGLint;

typedef EGLDisplay (WINAPI *PFN_eglGetDisplay)(void*);
typedef EGLBoolean (WINAPI *PFN_eglInitialize)(EGLDisplay, EGLint*, EGLint*);
typedef EGLBoolean (WINAPI *PFN_eglChooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
typedef EGLSurface (WINAPI *PFN_eglCreateWindowSurface)(EGLDisplay, EGLConfig, void*, const EGLint*);
typedef EGLSurface (WINAPI *PFN_eglCreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint*);
typedef EGLContext (WINAPI *PFN_eglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
typedef EGLBoolean (WINAPI *PFN_eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef EGLBoolean (WINAPI *PFN_eglSwapBuffers)(EGLDisplay, EGLSurface);
typedef EGLBoolean (WINAPI *PFN_eglDestroyContext)(EGLDisplay, EGLContext);
typedef EGLBoolean (WINAPI *PFN_eglDestroySurface)(EGLDisplay, EGLSurface);
typedef EGLBoolean (WINAPI *PFN_eglTerminate)(EGLDisplay);
typedef EGLint (WINAPI *PFN_eglGetError)(void);
typedef const char* (WINAPI *PFN_eglQueryString)(EGLDisplay, EGLint);

static PFN_eglGetDisplay p_eglGetDisplay = NULL;
static PFN_eglInitialize p_eglInitialize = NULL;
static PFN_eglChooseConfig p_eglChooseConfig = NULL;
static PFN_eglCreateWindowSurface p_eglCreateWindowSurface = NULL;
static PFN_eglCreatePbufferSurface p_eglCreatePbufferSurface = NULL;
static PFN_eglCreateContext p_eglCreateContext = NULL;
static PFN_eglMakeCurrent p_eglMakeCurrent = NULL;
static PFN_eglSwapBuffers p_eglSwapBuffers = NULL;
static PFN_eglDestroyContext p_eglDestroyContext = NULL;
static PFN_eglDestroySurface p_eglDestroySurface = NULL;
static PFN_eglTerminate p_eglTerminate = NULL;
static PFN_eglGetError p_eglGetError = NULL;
static PFN_eglQueryString p_eglQueryString = NULL;

// OpenGL ES 2.0 Function Pointers
static PFN_glActiveTexture p_glActiveTexture = NULL;
static PFN_glAttachShader p_glAttachShader = NULL;
static PFN_glBindAttribLocation p_glBindAttribLocation = NULL;
static PFN_glBindBuffer p_glBindBuffer = NULL;
static PFN_glBindFramebuffer p_glBindFramebuffer = NULL;
static PFN_glBindRenderbuffer p_glBindRenderbuffer = NULL;
static PFN_glBindTexture p_glBindTexture = NULL;
static PFN_glBlendColor p_glBlendColor = NULL;
static PFN_glBlendEquation p_glBlendEquation = NULL;
static PFN_glBlendEquationSeparate p_glBlendEquationSeparate = NULL;
static PFN_glBlendFunc p_glBlendFunc = NULL;
static PFN_glBlendFuncSeparate p_glBlendFuncSeparate = NULL;
static PFN_glBufferData p_glBufferData = NULL;
static PFN_glBufferSubData p_glBufferSubData = NULL;
static PFN_glCheckFramebufferStatus p_glCheckFramebufferStatus = NULL;
static PFN_glClear p_glClear = NULL;
static PFN_glClearColor p_glClearColor = NULL;
static PFN_glClearDepthf p_glClearDepthf = NULL;
static PFN_glClearStencil p_glClearStencil = NULL;
static PFN_glColorMask p_glColorMask = NULL;
static PFN_glCompileShader p_glCompileShader = NULL;
static PFN_glCompressedTexImage2D p_glCompressedTexImage2D = NULL;
static PFN_glCompressedTexSubImage2D p_glCompressedTexSubImage2D = NULL;
static PFN_glCopyTexImage2D p_glCopyTexImage2D = NULL;
static PFN_glCopyTexSubImage2D p_glCopyTexSubImage2D = NULL;
static PFN_glCreateProgram p_glCreateProgram = NULL;
static PFN_glCreateShader p_glCreateShader = NULL;
static PFN_glCullFace p_glCullFace = NULL;
static PFN_glDeleteBuffers p_glDeleteBuffers = NULL;
static PFN_glDeleteFramebuffers p_glDeleteFramebuffers = NULL;
static PFN_glDeleteProgram p_glDeleteProgram = NULL;
static PFN_glDeleteRenderbuffers p_glDeleteRenderbuffers = NULL;
static PFN_glDeleteShader p_glDeleteShader = NULL;
static PFN_glDeleteTextures p_glDeleteTextures = NULL;
static PFN_glDepthFunc p_glDepthFunc = NULL;
static PFN_glDepthMask p_glDepthMask = NULL;
static PFN_glDepthRangef p_glDepthRangef = NULL;
static PFN_glDetachShader p_glDetachShader = NULL;
static PFN_glDisable p_glDisable = NULL;
static PFN_glDisableVertexAttribArray p_glDisableVertexAttribArray = NULL;
static PFN_glDrawArrays p_glDrawArrays = NULL;
static PFN_glDrawElements p_glDrawElements = NULL;
static PFN_glEnable p_glEnable = NULL;
static PFN_glEnableVertexAttribArray p_glEnableVertexAttribArray = NULL;
static PFN_glFinish p_glFinish = NULL;
static PFN_glFlush p_glFlush = NULL;
static PFN_glFramebufferRenderbuffer p_glFramebufferRenderbuffer = NULL;
static PFN_glFramebufferTexture2D p_glFramebufferTexture2D = NULL;
static PFN_glFrontFace p_glFrontFace = NULL;
static PFN_glGenBuffers p_glGenBuffers = NULL;
static PFN_glGenerateMipmap p_glGenerateMipmap = NULL;
static PFN_glGenFramebuffers p_glGenFramebuffers = NULL;
static PFN_glGenRenderbuffers p_glGenRenderbuffers = NULL;
static PFN_glGenTextures p_glGenTextures = NULL;
static PFN_glGetActiveAttrib p_glGetActiveAttrib = NULL;
static PFN_glGetActiveUniform p_glGetActiveUniform = NULL;
static PFN_glGetAttachedShaders p_glGetAttachedShaders = NULL;
static PFN_glGetAttribLocation p_glGetAttribLocation = NULL;
static PFN_glGetBufferParameteriv p_glGetBufferParameteriv = NULL;
static PFN_glGetError p_glGetError = NULL;
static PFN_glGetFloatv p_glGetFloatv = NULL;
static PFN_glGetFramebufferAttachmentParameteriv p_glGetFramebufferAttachmentParameteriv = NULL;
static PFN_glGetIntegerv p_glGetIntegerv = NULL;
static PFN_glGetProgramiv p_glGetProgramiv = NULL;
static PFN_glGetProgramInfoLog p_glGetProgramInfoLog = NULL;
static PFN_glGetRenderbufferParameteriv p_glGetRenderbufferParameteriv = NULL;
static PFN_glGetShaderiv p_glGetShaderiv = NULL;
static PFN_glGetShaderInfoLog p_glGetShaderInfoLog = NULL;
static PFN_glGetShaderPrecisionFormat p_glGetShaderPrecisionFormat = NULL;
static PFN_glGetShaderSource p_glGetShaderSource = NULL;
static PFN_glGetString p_glGetString = NULL;
static PFN_glGetTexParameterfv p_glGetTexParameterfv = NULL;
static PFN_glGetTexParameteriv p_glGetTexParameteriv = NULL;
static PFN_glGetUniformfv p_glGetUniformfv = NULL;
static PFN_glGetUniformiv p_glGetUniformiv = NULL;
static PFN_glGetUniformLocation p_glGetUniformLocation = NULL;
static PFN_glGetVertexAttribfv p_glGetVertexAttribfv = NULL;
static PFN_glGetVertexAttribiv p_glGetVertexAttribiv = NULL;
static PFN_glGetVertexAttribPointerv p_glGetVertexAttribPointerv = NULL;
static PFN_glHint p_glHint = NULL;
static PFN_glIsBuffer p_glIsBuffer = NULL;
static PFN_glIsEnabled p_glIsEnabled = NULL;
static PFN_glIsFramebuffer p_glIsFramebuffer = NULL;
static PFN_glIsProgram p_glIsProgram = NULL;
static PFN_glIsRenderbuffer p_glIsRenderbuffer = NULL;
static PFN_glIsShader p_glIsShader = NULL;
static PFN_glIsTexture p_glIsTexture = NULL;
static PFN_glLineWidth p_glLineWidth = NULL;
static PFN_glLinkProgram p_glLinkProgram = NULL;
static PFN_glPixelStorei p_glPixelStorei = NULL;
static PFN_glPolygonOffset p_glPolygonOffset = NULL;
static PFN_glReadPixels p_glReadPixels = NULL;
static PFN_glReleaseShaderCompiler p_glReleaseShaderCompiler = NULL;
static PFN_glRenderbufferStorage p_glRenderbufferStorage = NULL;
static PFN_glSampleCoverage p_glSampleCoverage = NULL;
static PFN_glScissor p_glScissor = NULL;
static PFN_glShaderBinary p_glShaderBinary = NULL;
static PFN_glShaderSource p_glShaderSource = NULL;
static PFN_glStencilFunc p_glStencilFunc = NULL;
static PFN_glStencilFuncSeparate p_glStencilFuncSeparate = NULL;
static PFN_glStencilMask p_glStencilMask = NULL;
static PFN_glStencilMaskSeparate p_glStencilMaskSeparate = NULL;
static PFN_glStencilOp p_glStencilOp = NULL;
static PFN_glStencilOpSeparate p_glStencilOpSeparate = NULL;
static PFN_glTexImage2D p_glTexImage2D = NULL;
static PFN_glTexParameterf p_glTexParameterf = NULL;
static PFN_glTexParameterfv p_glTexParameterfv = NULL;
static PFN_glTexParameteri p_glTexParameteri = NULL;
static PFN_glTexParameteriv p_glTexParameteriv = NULL;
static PFN_glTexSubImage2D p_glTexSubImage2D = NULL;
static PFN_glUniform1f p_glUniform1f = NULL;
static PFN_glUniform1fv p_glUniform1fv = NULL;
static PFN_glUniform1i p_glUniform1i = NULL;
static PFN_glUniform1iv p_glUniform1iv = NULL;
static PFN_glUniform2f p_glUniform2f = NULL;
static PFN_glUniform2fv p_glUniform2fv = NULL;
static PFN_glUniform2i p_glUniform2i = NULL;
static PFN_glUniform2iv p_glUniform2iv = NULL;
static PFN_glUniform3f p_glUniform3f = NULL;
static PFN_glUniform3fv p_glUniform3fv = NULL;
static PFN_glUniform3i p_glUniform3i = NULL;
static PFN_glUniform3iv p_glUniform3iv = NULL;
static PFN_glUniform4f p_glUniform4f = NULL;
static PFN_glUniform4fv p_glUniform4fv = NULL;
static PFN_glUniform4i p_glUniform4i = NULL;
static PFN_glUniform4iv p_glUniform4iv = NULL;
static PFN_glUniformMatrix2fv p_glUniformMatrix2fv = NULL;
static PFN_glUniformMatrix3fv p_glUniformMatrix3fv = NULL;
static PFN_glUniformMatrix4fv p_glUniformMatrix4fv = NULL;
static PFN_glUseProgram p_glUseProgram = NULL;
static PFN_glValidateProgram p_glValidateProgram = NULL;
static PFN_glVertexAttrib1f p_glVertexAttrib1f = NULL;
static PFN_glVertexAttrib1fv p_glVertexAttrib1fv = NULL;
static PFN_glVertexAttrib2f p_glVertexAttrib2f = NULL;
static PFN_glVertexAttrib2fv p_glVertexAttrib2fv = NULL;
static PFN_glVertexAttrib3f p_glVertexAttrib3f = NULL;
static PFN_glVertexAttrib3fv p_glVertexAttrib3fv = NULL;
static PFN_glVertexAttrib4f p_glVertexAttrib4f = NULL;
static PFN_glVertexAttrib4fv p_glVertexAttrib4fv = NULL;
static PFN_glVertexAttribPointer p_glVertexAttribPointer = NULL;
static PFN_glViewport p_glViewport = NULL;

static HMODULE g_hEGL = NULL;
static HMODULE g_hGLES = NULL;
static HWND g_hwnd = NULL;
static EGLDisplay g_eglDisplay = EGL_NO_CONTEXT;
static EGLConfig g_eglConfig = NULL;
static EGLContext g_eglContext = EGL_NO_CONTEXT;
static EGLSurface g_eglSurface = EGL_NO_SURFACE;
static int g_inited = 0;
static int g_genCounter = 1;
static HANDLE g_hWindowReadyEvent = NULL;

static jfieldID fid_mIntQueue = NULL;
static jfieldID fid_mFloatQueue = NULL;
static jfieldID fid_mLongQueue = NULL;
static jfieldID fid_mByteQueue = NULL;

typedef void (*PFN_SendTouchGesture)(int gesture, int x, int y);
typedef void (*PFN_SendHardKey)(int code, int pressed);
typedef void (*PFN_SendRotaryKnob)(int steps);
static PFN_SendTouchGesture p_SendTouchGesture = NULL;
static PFN_SendHardKey p_SendHardKey = NULL;
static PFN_SendRotaryKnob p_SendRotaryKnob = NULL;

#ifdef _WIN32
static LONG WINAPI CrashVectoredHandler(PEXCEPTION_POINTERS p) {
    DWORD code = p->ExceptionRecord->ExceptionCode;
    void *addr = p->ExceptionRecord->ExceptionAddress;
    if (code == 0xC0000409 || code == 0xC0000374) {
        HMODULE hMod = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &hMod);
        char modPath[MAX_PATH] = {0};
        if (hMod) GetModuleFileNameA(hMod, modPath, sizeof(modPath));
        printf("\n=======================================================\n");
        printf("[CRASH TRAPPED] Code=0x%08X at %p\n", (unsigned int)code, addr);
        printf("Faulting Module: %s (+0x%llX)\n", modPath, hMod ? (unsigned long long)((uintptr_t)addr - (uintptr_t)hMod) : 0);
        void *callers[32];
        WORD count = CaptureStackBackTrace(0, 32, callers, NULL);
        for (int i = 0; i < count; i++) {
            HMODULE m = NULL;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)callers[i], &m);
            char name[MAX_PATH] = {0};
            if (m) GetModuleFileNameA(m, name, sizeof(name));
            char *base = strrchr(name, '\\');
            printf("  #%02d: %p [%s + 0x%llX]\n", i, callers[i], base ? base + 1 : name, m ? (unsigned long long)((uintptr_t)callers[i] - (uintptr_t)m) : 0);
        }
        printf("=======================================================\n\n");
        fflush(stdout);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL; (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableProcessWindowsGhosting();
        AddVectoredExceptionHandler(1, CrashVectoredHandler);
    }
    return TRUE;
}

#define LOAD_GLES(name) p_##name = (PFN_##name)GetProcAddress(g_hGLES, #name)
#define LOAD_EGL(name) p_##name = (PFN_##name)GetProcAddress(g_hEGL, #name)
#else
#define LOAD_GLES(name) p_##name = (PFN_##name)dlsym(g_hGLES, #name)
#define LOAD_EGL(name) p_##name = (PFN_##name)dlsym(g_hEGL, #name)
#endif

static void initEGL(void) {
    if (g_inited) return;
    g_inited = 1;

#ifdef _WIN32
    HMODULE hDsi = GetModuleHandleA("tsd.mibstd2.hmi.dsi.native.dll");
    if (!hDsi) hDsi = LoadLibraryA("tsd.mibstd2.hmi.dsi.native.dll");
    if (!hDsi) hDsi = LoadLibraryA("firmware/analysis/p0369/runtime/tsd.mibstd2.hmi.dsi.native.dll");
    if (hDsi) {
        p_SendTouchGesture = (PFN_SendTouchGesture)GetProcAddress(hDsi, "SendTouchGesture");
        p_SendHardKey = (PFN_SendHardKey)GetProcAddress(hDsi, "SendHardKey");
        p_SendRotaryKnob = (PFN_SendRotaryKnob)GetProcAddress(hDsi, "SendRotaryKnob");
    }

    g_hEGL = LoadLibraryA("libEGL.dll");
    if (!g_hEGL) g_hEGL = LoadLibraryA("firmware/analysis/p0369/runtime/libEGL.dll");
    g_hGLES = LoadLibraryA("libGLESv2.dll");
    if (!g_hGLES) g_hGLES = LoadLibraryA("firmware/analysis/p0369/runtime/libGLESv2.dll");
#else
    void *hDsi = dlopen("libtsd.mibstd2.hmi.dsi.native.so", RTLD_NOLOAD | RTLD_LAZY);
    if (!hDsi) hDsi = dlopen("libtsd.mibstd2.hmi.dsi.native.so", RTLD_LAZY);
    if (!hDsi) hDsi = dlopen("./libtsd.mibstd2.hmi.dsi.native.so", RTLD_LAZY);
    if (hDsi) {
        p_SendTouchGesture = (PFN_SendTouchGesture)dlsym(hDsi, "SendTouchGesture");
        p_SendHardKey = (PFN_SendHardKey)dlsym(hDsi, "SendHardKey");
        p_SendRotaryKnob = (PFN_SendRotaryKnob)dlsym(hDsi, "SendRotaryKnob");
    }

    g_hEGL = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!g_hEGL) g_hEGL = dlopen("libEGL.so", RTLD_LAZY | RTLD_GLOBAL);
    g_hGLES = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_GLOBAL);
    if (!g_hGLES) g_hGLES = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_GLOBAL);
#endif

    if (g_hEGL) {
        LOAD_EGL(eglGetDisplay);
        LOAD_EGL(eglInitialize);
        LOAD_EGL(eglChooseConfig);
        LOAD_EGL(eglCreateWindowSurface);
        LOAD_EGL(eglCreatePbufferSurface);
        LOAD_EGL(eglCreateContext);
        LOAD_EGL(eglMakeCurrent);
        LOAD_EGL(eglSwapBuffers);
        LOAD_EGL(eglDestroyContext);
        LOAD_EGL(eglDestroySurface);
        LOAD_EGL(eglTerminate);
        LOAD_EGL(eglGetError);
        LOAD_EGL(eglQueryString);
    }

    if (g_hGLES) {
        LOAD_GLES(glActiveTexture);
        LOAD_GLES(glAttachShader);
        LOAD_GLES(glBindAttribLocation);
        LOAD_GLES(glBindBuffer);
        LOAD_GLES(glBindFramebuffer);
        LOAD_GLES(glBindRenderbuffer);
        LOAD_GLES(glBindTexture);
        LOAD_GLES(glBlendColor);
        LOAD_GLES(glBlendEquation);
        LOAD_GLES(glBlendEquationSeparate);
        LOAD_GLES(glBlendFunc);
        LOAD_GLES(glBlendFuncSeparate);
        LOAD_GLES(glBufferData);
        LOAD_GLES(glBufferSubData);
        LOAD_GLES(glCheckFramebufferStatus);
        LOAD_GLES(glClear);
        LOAD_GLES(glClearColor);
        LOAD_GLES(glClearDepthf);
        LOAD_GLES(glClearStencil);
        LOAD_GLES(glColorMask);
        LOAD_GLES(glCompileShader);
        LOAD_GLES(glCompressedTexImage2D);
        LOAD_GLES(glCompressedTexSubImage2D);
        LOAD_GLES(glCopyTexImage2D);
        LOAD_GLES(glCopyTexSubImage2D);
        LOAD_GLES(glCreateProgram);
        LOAD_GLES(glCreateShader);
        LOAD_GLES(glCullFace);
        LOAD_GLES(glDeleteBuffers);
        LOAD_GLES(glDeleteFramebuffers);
        LOAD_GLES(glDeleteProgram);
        LOAD_GLES(glDeleteRenderbuffers);
        LOAD_GLES(glDeleteShader);
        LOAD_GLES(glDeleteTextures);
        LOAD_GLES(glDepthFunc);
        LOAD_GLES(glDepthMask);
        LOAD_GLES(glDepthRangef);
        LOAD_GLES(glDetachShader);
        LOAD_GLES(glDisable);
        LOAD_GLES(glDisableVertexAttribArray);
        LOAD_GLES(glDrawArrays);
        LOAD_GLES(glDrawElements);
        LOAD_GLES(glEnable);
        LOAD_GLES(glEnableVertexAttribArray);
        LOAD_GLES(glFinish);
        LOAD_GLES(glFlush);
        LOAD_GLES(glFramebufferRenderbuffer);
        LOAD_GLES(glFramebufferTexture2D);
        LOAD_GLES(glFrontFace);
        LOAD_GLES(glGenBuffers);
        LOAD_GLES(glGenerateMipmap);
        LOAD_GLES(glGenFramebuffers);
        LOAD_GLES(glGenRenderbuffers);
        LOAD_GLES(glGenTextures);
        LOAD_GLES(glGetActiveAttrib);
        LOAD_GLES(glGetActiveUniform);
        LOAD_GLES(glGetAttachedShaders);
        LOAD_GLES(glGetAttribLocation);
        LOAD_GLES(glGetBufferParameteriv);
        LOAD_GLES(glGetError);
        LOAD_GLES(glGetFloatv);
        LOAD_GLES(glGetFramebufferAttachmentParameteriv);
        LOAD_GLES(glGetIntegerv);
        LOAD_GLES(glGetProgramiv);
        LOAD_GLES(glGetProgramInfoLog);
        LOAD_GLES(glGetRenderbufferParameteriv);
        LOAD_GLES(glGetShaderiv);
        LOAD_GLES(glGetShaderInfoLog);
        LOAD_GLES(glGetShaderPrecisionFormat);
        LOAD_GLES(glGetShaderSource);
        LOAD_GLES(glGetString);
        LOAD_GLES(glGetTexParameterfv);
        LOAD_GLES(glGetTexParameteriv);
        LOAD_GLES(glGetUniformfv);
        LOAD_GLES(glGetUniformiv);
        LOAD_GLES(glGetUniformLocation);
        LOAD_GLES(glGetVertexAttribfv);
        LOAD_GLES(glGetVertexAttribiv);
        LOAD_GLES(glGetVertexAttribPointerv);
        LOAD_GLES(glHint);
        LOAD_GLES(glIsBuffer);
        LOAD_GLES(glIsEnabled);
        LOAD_GLES(glIsFramebuffer);
        LOAD_GLES(glIsProgram);
        LOAD_GLES(glIsRenderbuffer);
        LOAD_GLES(glIsShader);
        LOAD_GLES(glIsTexture);
        LOAD_GLES(glLineWidth);
        LOAD_GLES(glLinkProgram);
        LOAD_GLES(glPixelStorei);
        LOAD_GLES(glPolygonOffset);
        LOAD_GLES(glReadPixels);
        LOAD_GLES(glReleaseShaderCompiler);
        LOAD_GLES(glRenderbufferStorage);
        LOAD_GLES(glSampleCoverage);
        LOAD_GLES(glScissor);
        LOAD_GLES(glShaderBinary);
        LOAD_GLES(glShaderSource);
        LOAD_GLES(glStencilFunc);
        LOAD_GLES(glStencilFuncSeparate);
        LOAD_GLES(glStencilMask);
        LOAD_GLES(glStencilMaskSeparate);
        LOAD_GLES(glStencilOp);
        LOAD_GLES(glStencilOpSeparate);
        LOAD_GLES(glTexImage2D);
        LOAD_GLES(glTexParameterf);
        LOAD_GLES(glTexParameterfv);
        LOAD_GLES(glTexParameteri);
        LOAD_GLES(glTexParameteriv);
        LOAD_GLES(glTexSubImage2D);
        LOAD_GLES(glUniform1f);
        LOAD_GLES(glUniform1fv);
        LOAD_GLES(glUniform1i);
        LOAD_GLES(glUniform1iv);
        LOAD_GLES(glUniform2f);
        LOAD_GLES(glUniform2fv);
        LOAD_GLES(glUniform2i);
        LOAD_GLES(glUniform2iv);
        LOAD_GLES(glUniform3f);
        LOAD_GLES(glUniform3fv);
        LOAD_GLES(glUniform3i);
        LOAD_GLES(glUniform3iv);
        LOAD_GLES(glUniform4f);
        LOAD_GLES(glUniform4fv);
        LOAD_GLES(glUniform4i);
        LOAD_GLES(glUniform4iv);
        LOAD_GLES(glUniformMatrix2fv);
        LOAD_GLES(glUniformMatrix3fv);
        LOAD_GLES(glUniformMatrix4fv);
        LOAD_GLES(glUseProgram);
        LOAD_GLES(glValidateProgram);
        LOAD_GLES(glVertexAttrib1f);
        LOAD_GLES(glVertexAttrib1fv);
        LOAD_GLES(glVertexAttrib2f);
        LOAD_GLES(glVertexAttrib2fv);
        LOAD_GLES(glVertexAttrib3f);
        LOAD_GLES(glVertexAttrib3fv);
        LOAD_GLES(glVertexAttrib4f);
        LOAD_GLES(glVertexAttrib4fv);
        LOAD_GLES(glVertexAttribPointer);
        LOAD_GLES(glViewport);
    }
}

static volatile int g_windowRunning = 1;
static volatile int g_needScreenClear = 0;

#ifdef _WIN32
__declspec(dllexport) void requestScreenClear(void) {
    g_needScreenClear = 1;
}
#else
__attribute__((visibility("default"))) void requestScreenClear(void) {
    g_needScreenClear = 1;
}
#endif

JNIEXPORT void JNICALL Java_LoadMIBMain_requestScreenClear(JNIEnv *env, jclass cls) {
    (void)env; (void)cls;
    requestScreenClear();
}

static int s_firstFrameDrawn = 0;
static int s_touchDown = 0;
static int s_lastTouchX = -1;
static int s_lastTouchY = -1;

#ifdef _WIN32
static LRESULT CALLBACK DisplayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            g_windowRunning = 0;
            DestroyWindow(hwnd);
            ExitProcess(0);
            return 0;
        case WM_DESTROY:
            g_windowRunning = 0;
            PostQuitMessage(0);
            ExitProcess(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!s_firstFrameDrawn) {
                RECT cr;
                GetClientRect(hwnd, &cr);
                HBRUSH bg = CreateSolidBrush(RGB(18, 24, 34));
                FillRect(hdc, &cr, bg);
                DeleteObject(bg);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(180, 205, 235));
                HFONT font = CreateFontA(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                HFONT oldFont = (HFONT)SelectObject(hdc, font);
                const char* title = "Volkswagen Infotainment System (Golf 7)";
                const char* sub = "Starting authentic firmware runtime...";
                TextOutA(hdc, 24, 24, title, (int)strlen(title));
                SetTextColor(hdc, RGB(120, 140, 165));
                TextOutA(hdc, 24, 52, sub, (int)strlen(sub));
                SelectObject(hdc, oldFont);
                DeleteObject(font);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            SetFocus(hwnd);
            SetActiveWindow(hwnd);
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            s_touchDown = 1;
            s_lastTouchX = x;
            s_lastTouchY = y;
            if (p_SendTouchGesture) p_SendTouchGesture(4, x, y); // 4 = PRESS
            return 0;
        }
        case WM_LBUTTONUP: {
            if (s_touchDown) {
                s_touchDown = 0;
                ReleaseCapture();
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                s_lastTouchX = x;
                s_lastTouchY = y;
                if (p_SendTouchGesture) {
                    p_SendTouchGesture(1, x, y); // 1 = CLICK first
                    p_SendTouchGesture(3, x, y); // 3 = RELEASE
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (s_touchDown && (wParam & MK_LBUTTON)) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                if (abs(x - s_lastTouchX) >= 12 || abs(y - s_lastTouchY) >= 12) {
                    s_lastTouchX = x;
                    s_lastTouchY = y;
                    if (p_SendTouchGesture) p_SendTouchGesture(5, x, y); // 5 = DRAG
                }
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (lParam & (1 << 30)) return 0; // ignore repeat
            int key = 0;
            switch (wParam) {
                case '1': if (p_SendTouchGesture) { p_SendTouchGesture(4, 120, 260); p_SendTouchGesture(1, 120, 260); p_SendTouchGesture(3, 120, 260); } return 0;
                case '2': if (p_SendTouchGesture) { p_SendTouchGesture(4, 260, 260); p_SendTouchGesture(1, 260, 260); p_SendTouchGesture(3, 260, 260); } return 0;
                case '3': if (p_SendTouchGesture) { p_SendTouchGesture(4, 400, 260); p_SendTouchGesture(1, 400, 260); p_SendTouchGesture(3, 400, 260); } return 0;
                case '4': if (p_SendTouchGesture) { p_SendTouchGesture(4, 540, 260); p_SendTouchGesture(1, 540, 260); p_SendTouchGesture(3, 540, 260); } return 0;
                case '5': if (p_SendTouchGesture) { p_SendTouchGesture(4, 680, 260); p_SendTouchGesture(1, 680, 260); p_SendTouchGesture(3, 680, 260); } return 0;
                case 'M': case VK_HOME: key = 78; break; // MENU
                case 'C': key = 6; break;                // CAR
                case 'R': key = 15; break;               // RADIO (KEY_TUN)
                case 'E': key = 1; break;                // MEDIA (KEY_MED)
                case 'N': key = 4; break;                // NAV
                case 'P': key = 3; break;                // PHONE (KEY_TEL)
                case 'S': key = 7; break;                // SETUP (KEY_SET)
                case 'T': key = 5; break;                // TRAFFIC
                case 'A': key = 114; break;              // APP-CONNECT (SMARTPHONE)
                case 'V': key = 79; break;               // CLIMATE
                case VK_ESCAPE: case VK_BACK: key = 13; break; // BACK
                case VK_LEFT: key = 10; break;           // PREV
                case VK_RIGHT: key = 14; break;          // NEXT
                case VK_RETURN: case VK_SPACE: key = 16; break; // DDS KNOB PUSH (ENTER)
                case VK_OEM_PLUS: case VK_ADD: case VK_UP:
                    if (p_SendRotaryKnob) p_SendRotaryKnob(1);
                    return 0;
                case VK_OEM_MINUS: case VK_SUBTRACT: case VK_DOWN:
                    if (p_SendRotaryKnob) p_SendRotaryKnob(-1);
                    return 0;
            }
            if (key) {
                if (key == 6 || key == 15 || key == 1 || key == 4 || key == 78 || key == 7 || key == 3 || key == 79 || key == 114 || key == 5) {
                    requestScreenClear();
                }
                if (p_SendHardKey) p_SendHardKey(key, 1);
            }
            return 0;
        }
        case WM_KEYUP: {
            int key = 0;
            switch (wParam) {
                case 'M': case VK_HOME: key = 78; break;
                case 'C': key = 6; break;
                case 'R': key = 15; break;
                case 'E': key = 1; break;
                case 'N': key = 4; break;
                case 'P': key = 3; break;
                case 'S': key = 7; break;
                case 'T': key = 5; break;
                case 'A': key = 114; break;
                case 'V': key = 79; break;
                case VK_ESCAPE: case VK_BACK: key = 13; break;
                case VK_LEFT: key = 10; break;
                case VK_RIGHT: key = 14; break;
                case VK_RETURN: case VK_SPACE: key = 16; break;
            }
            if (key && p_SendHardKey) p_SendHardKey(key, 0);
            return 0;
        }
        case WM_SETFOCUS:
            return 0;
        case WM_MOUSEWHEEL: {
            short delta = (short)HIWORD(wParam);
            if (p_SendRotaryKnob && delta != 0) {
                p_SendRotaryKnob(delta > 0 ? 1 : -1);
            }
            return 0;
        }
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

static DWORD WINAPI WindowThreadProc(LPVOID lpParam) {
    (void)lpParam;
    DisableProcessWindowsGhosting();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    int width = 800;
    int height = 480;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DisplayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "VW_MIB2_Display";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_OWNDC;
    RegisterClassA(&wc);

    RECT r = {0, 0, width, height};
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    AdjustWindowRect(&r, dwStyle, FALSE);
    g_hwnd = CreateWindowExA(
        0, "VW_MIB2_Display",
        "Volkswagen MIB2 Infotainment (Golf 7) - Authentic Firmware",
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (g_hwnd) {
        UpdateWindow(g_hwnd);
        SetForegroundWindow(g_hwnd);
        SetActiveWindow(g_hwnd);
        SetFocus(g_hwnd);
    }

    if (g_hWindowReadyEvent) {
        SetEvent(g_hWindowReadyEvent);
    }

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageA(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    g_windowRunning = 0;
    return 0;
}

static void ensureDisplayWindow(int width, int height) {
    if (g_hwnd) return;
    g_hWindowReadyEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    CreateThread(NULL, 0, WindowThreadProc, NULL, 0, NULL);
    WaitForSingleObject(g_hWindowReadyEvent, 5000);
    CloseHandle(g_hWindowReadyEvent);
    g_hWindowReadyEvent = NULL;
}
#else
static void ensureDisplayWindow(int width, int height) {
    (void)width; (void)height;
}
#endif

typedef struct {
    void *rawBase;
    void *dataPtr;
    jboolean direct;
    jobject obj;
} BufferPin;

static BufferPin pinBuffer(JNIEnv *env, jobject bufObj, jint byteOffset, jboolean direct) {
    BufferPin p;
    p.rawBase = NULL;
    p.dataPtr = NULL;
    p.direct = direct;
    p.obj = bufObj;
    if (!bufObj) return p;
    if (direct) {
        p.rawBase = (*env)->GetDirectBufferAddress(env, bufObj);
        p.dataPtr = p.rawBase ? ((char*)p.rawBase + byteOffset) : NULL;
    } else {
        p.rawBase = (*env)->GetPrimitiveArrayCritical(env, (jarray)bufObj, NULL);
        p.dataPtr = p.rawBase ? ((char*)p.rawBase + byteOffset) : NULL;
    }
    return p;
}

static void unpinBuffer(JNIEnv *env, BufferPin *pin) {
    if (!pin || !pin->obj || pin->direct || !pin->rawBase) return;
    (*env)->ReleasePrimitiveArrayCritical(env, (jarray)pin->obj, pin->rawBase, JNI_ABORT);
    pin->rawBase = NULL;
    pin->dataPtr = NULL;
}

#ifdef _WIN32
static __declspec(thread) void *tls_lastArrayBase = NULL;
#else
static __thread void *tls_lastArrayBase = NULL;
#endif

static void* getBufferPtr(JNIEnv *env, jobject bufObj, jint byteOffset, jboolean direct) {
    if (!bufObj) return NULL;
    if (direct) {
        char *base = (char*)(*env)->GetDirectBufferAddress(env, bufObj);
        return base ? (base + byteOffset) : NULL;
    } else {
        char *base = (char*)(*env)->GetPrimitiveArrayCritical(env, (jarray)bufObj, NULL);
        tls_lastArrayBase = base;
        return base ? (base + byteOffset) : NULL;
    }
}

static void releaseBufferPtr(JNIEnv *env, jobject bufObj, void *ptr, jboolean direct) {
    if (!bufObj || direct || !ptr) return;
    void *base = tls_lastArrayBase ? tls_lastArrayBase : ptr;
    tls_lastArrayBase = NULL;
    (*env)->ReleasePrimitiveArrayCritical(env, (jarray)bufObj, base, JNI_ABORT);
}

static void writeInts(JNIEnv *env, jobject outObj, jint outOff, jboolean direct, const jint *vals, int count) {
    if (!outObj) return;
    if (direct) {
        char *base = (char*)(*env)->GetDirectBufferAddress(env, outObj);
        if (base) memcpy(base + outOff, vals, count * sizeof(jint));
    } else {
        jintArray arr = (jintArray)outObj;
        (*env)->SetIntArrayRegion(env, arr, outOff / sizeof(jint), count, vals);
    }
}

static void writeLongs(JNIEnv *env, jobject outObj, jint outOff, jboolean direct, const jlong *vals, int count) {
    if (!outObj) return;
    if (direct) {
        char *base = (char*)(*env)->GetDirectBufferAddress(env, outObj);
        if (base) memcpy(base + outOff, vals, count * sizeof(jlong));
    } else {
        jlongArray arr = (jlongArray)outObj;
        (*env)->SetLongArrayRegion(env, arr, outOff / sizeof(jlong), count, vals);
    }
}

static void ensureEglCurrent(void) {
    if (p_eglMakeCurrent && g_eglDisplay && g_eglSurface && g_eglContext) {
        p_eglMakeCurrent(g_eglDisplay, g_eglSurface, g_eglSurface, g_eglContext);
    }
}

/* =========================================================================
 * tsd.opengl.jsr231.impl.EGLImpl Native Functions (13)
 * ========================================================================= */

static jboolean doEGLInit(
    JNIEnv *env, jobject thiz,
    jint width, jint height,
    jobject attribList, jint attribOff, jboolean attribDirect,
    jobject longArr, jint longOff, jboolean longDirect,
    jboolean disableQueueing, jint queuePos) {
    (void)thiz; (void)attribList; (void)attribOff; (void)attribDirect;
    (void)disableQueueing; (void)queuePos;

    initEGL();
    ensureDisplayWindow(width > 0 ? width : 800, height > 0 ? height : 480);

    if (!g_eglDisplay && p_eglGetDisplay && p_eglInitialize) {
        g_eglDisplay = p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint maj = 0, min = 0;
        p_eglInitialize(g_eglDisplay, &maj, &min);
        printf("[JNI-OpenGL] EGL Initialized: maj=%d, min=%d\n", maj, min);
        fflush(stdout);
    }

    if (!g_eglConfig && p_eglChooseConfig && g_eglDisplay) {
        EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };
        EGLint n = 0;
        p_eglChooseConfig(g_eglDisplay, attribs, &g_eglConfig, 1, &n);
        if (!g_eglConfig) {
            EGLint attribsSimple[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                EGL_NONE
            };
            p_eglChooseConfig(g_eglDisplay, attribsSimple, &g_eglConfig, 1, &n);
        }
        printf("[JNI-OpenGL] EGL Config chosen: %p\n", g_eglConfig);
        fflush(stdout);
    }

#ifdef _WIN32
    if (!g_eglSurface && p_eglCreateWindowSurface && g_eglDisplay && g_eglConfig && g_hwnd) {
        g_eglSurface = p_eglCreateWindowSurface(g_eglDisplay, g_eglConfig, (void*)g_hwnd, NULL);
        printf("[JNI-OpenGL] EGL Surface created: %p (hwnd=%p)\n", g_eglSurface, g_hwnd);
        fflush(stdout);
    }
#else
    if (!g_eglSurface && p_eglCreatePbufferSurface && g_eglDisplay && g_eglConfig) {
        EGLint pbufAttribs[] = {
            EGL_WIDTH, width > 0 ? width : 800,
            EGL_HEIGHT, height > 0 ? height : 480,
            EGL_NONE
        };
        g_eglSurface = p_eglCreatePbufferSurface(g_eglDisplay, g_eglConfig, pbufAttribs);
        printf("[JNI-OpenGL-ARM64] EGL PBuffer Surface created: %p (800x480)\n", g_eglSurface);
        fflush(stdout);
    }
#endif

    if (longArr) {
        jlong handles[4];
        handles[0] = (jlong)(intptr_t)g_eglDisplay;
        handles[1] = (jlong)(intptr_t)g_hwnd;
        handles[2] = (jlong)(intptr_t)g_eglConfig;
        handles[3] = (jlong)(intptr_t)g_eglSurface;
        writeLongs(env, longArr, longOff, longDirect, handles, 4);
        printf("[JNI-OpenGL] EGLImpl.init wrote handles: dpy=%p, hwnd=%p, cfg=%p, surf=%p\n",
            g_eglDisplay, g_hwnd, g_eglConfig, g_eglSurface);
        fflush(stdout);
    }

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_init__IILjava_lang_Object_2IZLjava_lang_Object_2IZZI(
    JNIEnv *env, jobject thiz,
    jint width, jint height,
    jobject attribList, jint attribOff, jboolean attribDirect,
    jobject longArr, jint longOff, jboolean longDirect,
    jboolean disableQueueing, jint queuePos) {
    return doEGLInit(env, thiz, width, height, attribList, attribOff, attribDirect,
                     longArr, longOff, longDirect, disableQueueing, queuePos);
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_init(
    JNIEnv *env, jobject thiz,
    jint width, jint height,
    jobject attribList, jint attribOff, jboolean attribDirect,
    jobject longArr, jint longOff, jboolean longDirect,
    jboolean disableQueueing, jint queuePos) {
    return doEGLInit(env, thiz, width, height, attribList, attribOff, attribDirect,
                     longArr, longOff, longDirect, disableQueueing, queuePos);
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLGetDisplay(
    JNIEnv *env, jobject thiz, jint queuePos) {
    (void)env; (void)thiz; (void)queuePos;
    initEGL();
    if (!g_eglDisplay && p_eglGetDisplay) {
        g_eglDisplay = p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    return (jlong)(intptr_t)g_eglDisplay;
}

JNIEXPORT jint JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLGetError(
    JNIEnv *env, jobject thiz, jint queuePos) {
    (void)env; (void)thiz; (void)queuePos;
    return p_eglGetError ? p_eglGetError() : EGL_SUCCESS;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLChooseConfig(
    JNIEnv *env, jobject thiz,
    jlong dpy,
    jobject attribList, jint attribOff, jboolean attribDirect,
    jobject configs, jint configOff, jboolean configDirect, jint configSize,
    jobject numConfig, jint numOff, jboolean numDirect,
    jint queuePos) {
    (void)thiz; (void)attribList; (void)attribOff; (void)attribDirect; (void)queuePos;
    initEGL();
    EGLDisplay eglDpy = g_eglDisplay ? g_eglDisplay : (EGLDisplay)(intptr_t)dpy;
    if (p_eglChooseConfig && eglDpy) {
        EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };
        EGLint n = 0;
        EGLConfig cfg = NULL;
        p_eglChooseConfig(eglDpy, attribs, &cfg, 1, &n);
        if (!cfg) {
            EGLint attribsSimple[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                EGL_NONE
            };
            p_eglChooseConfig(eglDpy, attribsSimple, &cfg, 1, &n);
        }
        g_eglConfig = cfg;
        printf("[JNI-OpenGL] eglChooseConfig: cfg=%p, n=%d (err=0x%x)\n", cfg, n, p_eglGetError ? p_eglGetError() : 0);
        fflush(stdout);

        if (configs && configSize > 0 && cfg) {
            jlong cfgVal = (jlong)(intptr_t)cfg;
            writeLongs(env, configs, configOff, configDirect, &cfgVal, 1);
        }
        writeInts(env, numConfig, numOff, numDirect, &n, 1);
    }
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLCreateContext(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong config, jlong shareCtx,
    jobject attribList, jint off, jboolean direct,
    jint queuePos) {
    (void)env; (void)cls; (void)shareCtx; (void)attribList; (void)off; (void)direct; (void)queuePos;
    initEGL();
    EGLDisplay eglDpy = g_eglDisplay ? g_eglDisplay : (EGLDisplay)(intptr_t)dpy;
    if (p_eglCreateContext && eglDpy) {
        EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        EGLConfig cfg = (config && (EGLConfig)(intptr_t)config != (EGLConfig)(intptr_t)-1) ? (EGLConfig)(intptr_t)config : g_eglConfig;
        if (!cfg) cfg = g_eglConfig;
        g_eglContext = p_eglCreateContext(eglDpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
        printf("[JNI-OpenGL] eglCreateContext: dpy=%p cfg=%p -> ctx=%p (err=0x%x)\n",
            eglDpy, cfg, g_eglContext, p_eglGetError ? p_eglGetError() : 0);
        fflush(stdout);
        return (jlong)(intptr_t)g_eglContext;
    }
    return 0;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLCreatePbufferSurface(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong config,
    jobject attribList, jint off, jboolean direct,
    jint queuePos) {
    (void)env; (void)cls; (void)attribList; (void)off; (void)direct; (void)queuePos;
    initEGL();
    EGLDisplay eglDpy = g_eglDisplay ? g_eglDisplay : (EGLDisplay)(intptr_t)dpy;
    if (p_eglCreatePbufferSurface && eglDpy) {
        EGLint pbufAttribs[] = { 0x3057, 800, 0x3056, 480, EGL_NONE };
        EGLConfig cfg = (config && (EGLConfig)(intptr_t)config != (EGLConfig)(intptr_t)-1) ? (EGLConfig)(intptr_t)config : g_eglConfig;
        if (!cfg) cfg = g_eglConfig;
        EGLSurface surf = p_eglCreatePbufferSurface(eglDpy, cfg, pbufAttribs);
        return (jlong)(intptr_t)surf;
    }
    return 0;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLCreateWindowSurface(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong config, jlong eglWindowType,
    jobject attribList, jint off, jboolean direct,
    jint queuePos) {
    (void)env; (void)cls; (void)eglWindowType; (void)attribList; (void)off; (void)direct; (void)queuePos;
    initEGL();
    ensureDisplayWindow(800, 480);
    EGLDisplay eglDpy = g_eglDisplay ? g_eglDisplay : (EGLDisplay)(intptr_t)dpy;
#ifdef _WIN32
    if (p_eglCreateWindowSurface && eglDpy && g_hwnd) {
        if (!g_eglSurface) {
            EGLConfig cfg = (config && (EGLConfig)(intptr_t)config != (EGLConfig)(intptr_t)-1) ? (EGLConfig)(intptr_t)config : g_eglConfig;
            if (!cfg) cfg = g_eglConfig;
            g_eglSurface = p_eglCreateWindowSurface(eglDpy, cfg, (void*)g_hwnd, NULL);
        }
        printf("[JNI-OpenGL] eglCreateWindowSurface: dpy=%p cfg=%p hwnd=%p -> surf=%p (err=0x%x)\n",
            eglDpy, g_eglConfig, g_hwnd, g_eglSurface, p_eglGetError ? p_eglGetError() : 0);
        fflush(stdout);
        return (jlong)(intptr_t)g_eglSurface;
    }
#else
    if (p_eglCreatePbufferSurface && eglDpy) {
        if (!g_eglSurface) {
            EGLint pbufAttribs[] = {
                EGL_WIDTH, 800,
                EGL_HEIGHT, 480,
                EGL_NONE
            };
            EGLConfig cfg = (config && (EGLConfig)(intptr_t)config != (EGLConfig)(intptr_t)-1) ? (EGLConfig)(intptr_t)config : g_eglConfig;
            if (!cfg) cfg = g_eglConfig;
            g_eglSurface = p_eglCreatePbufferSurface(eglDpy, cfg, pbufAttribs);
        }
        printf("[JNI-OpenGL-ARM64] eglCreateWindowSurface -> PBuffer surf=%p (err=0x%x)\n",
            g_eglSurface, p_eglGetError ? p_eglGetError() : 0);
        fflush(stdout);
        return (jlong)(intptr_t)g_eglSurface;
    }
#endif
    return 0;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLMakeCurrent(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong drawSurf, jlong readSurf, jlong ctx, jint queuePos) {
    (void)env; (void)cls; (void)queuePos;
    initEGL();
    EGLDisplay eglDpy = g_eglDisplay ? g_eglDisplay : (EGLDisplay)(intptr_t)dpy;
    if (p_eglMakeCurrent && eglDpy) {
        EGLSurface surf = (drawSurf && (EGLSurface)(intptr_t)drawSurf != (EGLSurface)(intptr_t)-1) ? (EGLSurface)(intptr_t)drawSurf : g_eglSurface;
        EGLContext context = (ctx && (EGLContext)(intptr_t)ctx != (EGLContext)(intptr_t)-1) ? (EGLContext)(intptr_t)ctx : g_eglContext;
        EGLBoolean ok = p_eglMakeCurrent(eglDpy, surf, surf, context);
        printf("[JNI-OpenGL] eglMakeCurrent: dpy=%p surf=%p ctx=%p -> ok=%d (err=0x%x)\n",
            eglDpy, surf, context, ok, p_eglGetError ? p_eglGetError() : 0);
        fflush(stdout);
        return ok ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_TRUE;
}

/* Forward declaration of queue executor */
static void executeGLQueue(JNIEnv *env, jint queuePos);

static void inspectFramebuffer(int frameNum) {
    if (!p_glReadPixels) return;
    static unsigned char pixelBuf[800 * 480 * 4];
    p_glReadPixels(0, 0, 800, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuf);
    int nonBlack = 0;
    for (int i = 0; i < 800 * 480; i++) {
        unsigned char r = pixelBuf[i * 4 + 0];
        unsigned char g = pixelBuf[i * 4 + 1];
        unsigned char b = pixelBuf[i * 4 + 2];
        if (r > 0 || g > 0 || b > 0) nonBlack++;
    }
    if (frameNum == 1 || frameNum == 5 || frameNum == 10 || frameNum % 60 == 0) {
        printf("[JNI-OpenGL] Frame %d framebuffer: %d / %d non-black pixels (%.1f%% visible)\n",
            frameNum, nonBlack, 800 * 480, (nonBlack * 100.0f) / (800 * 480));
        fflush(stdout);
    }

    // Save BMP for frame 1, 5, 10, 60 or when content is visible
    if (nonBlack > 1000 || frameNum == 1 || frameNum == 5 || frameNum == 10 || frameNum % 60 == 0) {
        #pragma pack(push, 1)
        struct {
            unsigned short bfType;
            unsigned int bfSize;
            unsigned short bfReserved1;
            unsigned short bfReserved2;
            unsigned int bfOffBits;
            unsigned int biSize;
            int biWidth;
            int biHeight;
            unsigned short biPlanes;
            unsigned short biBitCount;
            unsigned int biCompression;
            unsigned int biSizeImage;
            int biXPelsPerMeter;
            int biYPelsPerMeter;
            unsigned int biClrUsed;
            unsigned int biClrImportant;
        } bmp = {
            0x4D42, 54 + 800 * 480 * 3, 0, 0, 54,
            40, 800, 480, 1, 24, 0, 800 * 480 * 3, 0, 0, 0, 0
        };
        #pragma pack(pop)
        static unsigned char bgrBuf[800 * 3];

        if (frameNum == 1 || frameNum == 5 || frameNum == 10 || frameNum % 60 == 0) {
            char fname[64];
            snprintf(fname, sizeof(fname), "frame_%03d.bmp", frameNum);
            FILE *f = fopen(fname, "wb");
            if (f) {
                fwrite(&bmp, 1, sizeof(bmp), f);
                for (int y = 0; y < 480; y++) {
                    for (int x = 0; x < 800; x++) {
                        bgrBuf[x * 3 + 0] = pixelBuf[(y * 800 + x) * 4 + 2]; // B
                        bgrBuf[x * 3 + 1] = pixelBuf[(y * 800 + x) * 4 + 1]; // G
                        bgrBuf[x * 3 + 2] = pixelBuf[(y * 800 + x) * 4 + 0]; // R
                    }
                    fwrite(bgrBuf, 1, sizeof(bgrBuf), f);
                }
                fclose(f);
            }
        }
        FILE *flatest = fopen("frame_latest.bmp", "wb");
        if (flatest) {
            fwrite(&bmp, 1, sizeof(bmp), flatest);
            for (int y = 0; y < 480; y++) {
                for (int x = 0; x < 800; x++) {
                    bgrBuf[x * 3 + 0] = pixelBuf[(y * 800 + x) * 4 + 2];
                    bgrBuf[x * 3 + 1] = pixelBuf[(y * 800 + x) * 4 + 1];
                    bgrBuf[x * 3 + 2] = pixelBuf[(y * 800 + x) * 4 + 0];
                }
                fwrite(bgrBuf, 1, sizeof(bgrBuf), flatest);
            }
            fclose(flatest);
        }
    }
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLSwapBuffers(
    JNIEnv *env, jobject thiz,
    jlong dpy, jlong surf, jint queuePos) {
    (void)thiz;
    static int s_swapCount = 0;
    s_swapCount++;

    ensureEglCurrent();

    if (g_needScreenClear && p_glClear && p_glClearColor) {
        p_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        p_glClear(0x00004000 | 0x00000100 | 0x00000400);
        g_needScreenClear = 0;
    }

    if (queuePos > 0) {
        executeGLQueue(env, queuePos);
    }

    if (s_swapCount <= 10 || (s_swapCount % 60 == 0)) {
        printf("[JNI-OpenGL] eglSwapBuffers frame=%d queuePos=%d\n", s_swapCount, queuePos);
        fflush(stdout);
    }

    inspectFramebuffer(s_swapCount);

    EGLDisplay eglDpy = g_eglDisplay ? g_eglDisplay : (EGLDisplay)(intptr_t)dpy;
    EGLSurface eglSurf = surf ? (EGLSurface)(intptr_t)surf : g_eglSurface;
    if (p_eglSwapBuffers && eglDpy && eglSurf) {
        EGLBoolean res = p_eglSwapBuffers(eglDpy, eglSurf);
        if (res) {
            s_firstFrameDrawn = 1;
        }
        if (!res && (s_swapCount <= 10 || (s_swapCount % 60 == 0))) {
            printf("[JNI-OpenGL] eglSwapBuffers returned %d err=0x%x\n", res, p_eglGetError ? p_eglGetError() : 0);
            fflush(stdout);
        }
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLBindTexImage(
    JNIEnv *env, jobject thiz,
    jlong dpy, jlong surf, jint buffer, jint queuePos) {
    (void)env; (void)thiz; (void)dpy; (void)surf; (void)buffer; (void)queuePos;
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLReleaseTexImage(
    JNIEnv *env, jobject thiz,
    jlong dpy, jlong surf, jint buffer, jint queuePos) {
    (void)env; (void)thiz; (void)dpy; (void)surf; (void)buffer; (void)queuePos;
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLDestroyContext(
    JNIEnv *env, jobject thiz,
    jlong dpy, jlong ctx, jint queuePos) {
    (void)env; (void)thiz; (void)dpy; (void)ctx; (void)queuePos;
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLTerminate(
    JNIEnv *env, jobject thiz,
    jlong dpy, jint queuePos) {
    (void)env; (void)thiz; (void)dpy; (void)queuePos;
    return JNI_TRUE;
}

/* =========================================================================
 * tsd.opengl.queue.GLQueue Native Functions (2)
 * ========================================================================= */

JNIEXPORT jboolean JNICALL Java_tsd_opengl_queue_GLQueue_init(
    JNIEnv *env, jclass cls,
    jintArray intQueue, jfloatArray floatQueue, jlongArray longQueue, jbyteArray byteQueue,
    jboolean callStats) {
    (void)intQueue; (void)floatQueue; (void)longQueue; (void)byteQueue; (void)callStats;
    fid_mIntQueue = (*env)->GetStaticFieldID(env, cls, "mIntQueue", "[I");
    fid_mFloatQueue = (*env)->GetStaticFieldID(env, cls, "mFloatQueue", "[F");
    fid_mLongQueue = (*env)->GetStaticFieldID(env, cls, "mLongQueue", "[J");
    fid_mByteQueue = (*env)->GetStaticFieldID(env, cls, "mByteQueue", "[B");
    printf("[JNI-OpenGL] GLQueue.init called, fieldIDs: %p %p %p %p\\n", fid_mIntQueue, fid_mFloatQueue, fid_mLongQueue, fid_mByteQueue);
    fflush(stdout);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_tsd_opengl_queue_GLQueue_flushQueue(
    JNIEnv *env, jclass cls,
    jboolean sync, jint intQueuePos) {
    (void)sync;
    if (intQueuePos > 0) {
        executeGLQueue(env, intQueuePos);
    }
}

/* =========================================================================
 * Command Queue Execution Engine (94 Opcodes)
 * ========================================================================= */

static void executeGLQueue(JNIEnv *env, jint queuePos) {
    if (!fid_mIntQueue || queuePos <= 0) return;
    jclass qCls = (*env)->FindClass(env, "tsd/opengl/queue/GLQueue");
    if (!qCls) return;

    jintArray jIntArr = (jintArray)(*env)->GetStaticObjectField(env, qCls, fid_mIntQueue);
    jfloatArray jFloatArr = fid_mFloatQueue ? (jfloatArray)(*env)->GetStaticObjectField(env, qCls, fid_mFloatQueue) : NULL;
    jlongArray jLongArr = fid_mLongQueue ? (jlongArray)(*env)->GetStaticObjectField(env, qCls, fid_mLongQueue) : NULL;
    jbyteArray jByteArr = fid_mByteQueue ? (jbyteArray)(*env)->GetStaticObjectField(env, qCls, fid_mByteQueue) : NULL;

    if (!jIntArr) return;

    jint *intQueue = (*env)->GetIntArrayElements(env, jIntArr, NULL);
    jfloat *floatQueue = jFloatArr ? (*env)->GetFloatArrayElements(env, jFloatArr, NULL) : NULL;
    jlong *longQueue = jLongArr ? (*env)->GetLongArrayElements(env, jLongArr, NULL) : NULL;
    jbyte *byteQueue = jByteArr ? (*env)->GetByteArrayElements(env, jByteArr, NULL) : NULL;

    if (!intQueue) return;

    int intPos = 0;
    int floatPos = 0;
    int longPos = 0;
    int bytePos = 0;

    while (intPos < queuePos) {
        int opcode = intQueue[intPos++];        fflush(stdout);
        switch (opcode) {
            case 0: /* glActiveTexture */ {
                int tex = intQueue[intPos++];
                if (p_glActiveTexture) p_glActiveTexture(tex);
                break;
            }
            case 1: /* glAttachShader */ {
                int prog = intQueue[intPos++];
                int sh = intQueue[intPos++];
                if (p_glAttachShader) p_glAttachShader(prog, sh);
                break;
            }
            case 2: /* glBindAttribLocation */ {
                int prog = intQueue[intPos++];
                int idx = intQueue[intPos++];
                int strLen = intQueue[intPos++];
                const char *name = byteQueue ? (const char *)&byteQueue[bytePos] : "";
                bytePos += strLen;
                if (p_glBindAttribLocation) p_glBindAttribLocation(prog, idx, name);
                break;
            }
            case 3: /* glBindBuffer */ {
                int target = intQueue[intPos++];
                int buf = intQueue[intPos++];
                if (p_glBindBuffer) p_glBindBuffer(target, buf);
                break;
            }
            case 4: /* glBindFramebufferEXT */ {
                int target = intQueue[intPos++];
                int fb = intQueue[intPos++];
                if (p_glBindFramebuffer) p_glBindFramebuffer(target, fb);
                break;
            }
            case 5: /* glBindRenderbufferEXT */ {
                int target = intQueue[intPos++];
                int rb = intQueue[intPos++];
                if (p_glBindRenderbuffer) p_glBindRenderbuffer(target, rb);
                break;
            }
            case 6: /* glBindTexture */ {
                int target = intQueue[intPos++];
                int tex = intQueue[intPos++];
                if (p_glBindTexture) p_glBindTexture(target, tex);
                break;
            }
            case 7: /* glBlendColor */ {
                float r = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float g = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float b = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float a = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glBlendColor) p_glBlendColor(r, g, b, a);
                break;
            }
            case 8: /* glBlendEquation */ {
                int mode = intQueue[intPos++];
                if (p_glBlendEquation) p_glBlendEquation(mode);
                break;
            }
            case 9: /* glBlendEquationSeparate */ {
                int rgb = intQueue[intPos++];
                int a = intQueue[intPos++];
                if (p_glBlendEquationSeparate) p_glBlendEquationSeparate(rgb, a);
                break;
            }
            case 10: /* glBlendFunc */ {
                int sf = intQueue[intPos++];
                int df = intQueue[intPos++];
                if (p_glBlendFunc) p_glBlendFunc(sf, df);
                break;
            }
            case 11: /* glBlendFuncSeparate */ {
                int srcRGB = intQueue[intPos++];
                int dstRGB = intQueue[intPos++];
                int srcA = intQueue[intPos++];
                int dstA = intQueue[intPos++];
                if (p_glBlendFuncSeparate) p_glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
                break;
            }
            case 12: /* glClear */ {
                int mask = intQueue[intPos++];
                if (p_glClear) p_glClear(mask);
                break;
            }
            case 13: /* glClearColor */ {
                float r = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float g = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float b = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float a = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glClearColor) p_glClearColor(r, g, b, a);
                break;
            }
            case 14: /* glClearDepthf */ {
                float d = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glClearDepthf) p_glClearDepthf(d);
                break;
            }
            case 15: /* glClearStencil */ {
                int s = intQueue[intPos++];
                if (p_glClearStencil) p_glClearStencil(s);
                break;
            }
            case 16: /* glColorMask */ {
                int r = intQueue[intPos++];
                int g = intQueue[intPos++];
                int b = intQueue[intPos++];
                int a = intQueue[intPos++];
                if (p_glColorMask) p_glColorMask(r, g, b, a);
                break;
            }
            case 17: /* glCompileShader */ {
                int sh = intQueue[intPos++];
                if (p_glCompileShader) p_glCompileShader(sh);
                break;
            }
            case 18: /* glCopyTexImage2D */ {
                int target = intQueue[intPos++];
                int level = intQueue[intPos++];
                int internalformat = intQueue[intPos++];
                int x = intQueue[intPos++];
                int y = intQueue[intPos++];
                int width = intQueue[intPos++];
                int height = intQueue[intPos++];
                int border = intQueue[intPos++];
                if (p_glCopyTexImage2D) p_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
                break;
            }
            case 19: /* glCopyTexSubImage2D */ {
                int target = intQueue[intPos++];
                int level = intQueue[intPos++];
                int xoffset = intQueue[intPos++];
                int yoffset = intQueue[intPos++];
                int x = intQueue[intPos++];
                int y = intQueue[intPos++];
                int width = intQueue[intPos++];
                int height = intQueue[intPos++];
                if (p_glCopyTexSubImage2D) p_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
                break;
            }
            case 20: /* glCullFace */ {
                int mode = intQueue[intPos++];
                if (p_glCullFace) p_glCullFace(mode);
                break;
            }
            case 21: /* glDeleteBuffers */ {
                int n = intQueue[intPos++];
                int len = intQueue[intPos++];
                const int *bufs = &intQueue[intPos];
                intPos += len;
                int offset = intQueue[intPos++];
                if (p_glDeleteBuffers) p_glDeleteBuffers(n, (const GLuint*)(bufs + offset));
                break;
            }
            case 22: /* glDeleteFramebuffersEXT */ {
                int n = intQueue[intPos++];
                int len = intQueue[intPos++];
                const int *fbs = &intQueue[intPos];
                intPos += len;
                int offset = intQueue[intPos++];
                if (p_glDeleteFramebuffers) p_glDeleteFramebuffers(n, (const GLuint*)(fbs + offset));
                break;
            }
            case 23: /* glDeleteProgram */ {
                int prog = intQueue[intPos++];
                if (p_glDeleteProgram) p_glDeleteProgram(prog);
                break;
            }
            case 24: /* glDeleteRenderbuffersEXT */ {
                int n = intQueue[intPos++];
                int len = intQueue[intPos++];
                const int *rbs = &intQueue[intPos];
                intPos += len;
                int offset = intQueue[intPos++];
                if (p_glDeleteRenderbuffers) p_glDeleteRenderbuffers(n, (const GLuint*)(rbs + offset));
                break;
            }
            case 25: /* glDeleteShader */ {
                int sh = intQueue[intPos++];
                if (p_glDeleteShader) p_glDeleteShader(sh);
                break;
            }
            case 26: /* glDeleteTextures */ {
                int n = intQueue[intPos++];
                int len = intQueue[intPos++];
                const int *texs = &intQueue[intPos];
                intPos += len;
                int offset = intQueue[intPos++];
                if (p_glDeleteTextures) p_glDeleteTextures(n, (const GLuint*)(texs + offset));
                break;
            }
            case 27: /* glDepthFunc */ {
                int func = intQueue[intPos++];
                if (p_glDepthFunc) p_glDepthFunc(func);
                break;
            }
            case 28: /* glDepthMask */ {
                int flag = intQueue[intPos++];
                if (p_glDepthMask) p_glDepthMask(flag);
                break;
            }
            case 29: /* glDepthRangef */ {
                float n = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float f = floatQueue ? floatQueue[floatPos++] : 1.0f;
                if (p_glDepthRangef) p_glDepthRangef(n, f);
                break;
            }
            case 30: /* glDetachShader */ {
                int prog = intQueue[intPos++];
                int sh = intQueue[intPos++];
                if (p_glDetachShader) p_glDetachShader(prog, sh);
                break;
            }
            case 31: /* glDisable */ {
                int cap = intQueue[intPos++];
                if (p_glDisable) p_glDisable(cap);
                break;
            }
            case 32: /* glDisableVertexAttribArray */ {
                int idx = intQueue[intPos++];
                if (p_glDisableVertexAttribArray) p_glDisableVertexAttribArray(idx);
                break;
            }
            case 33: /* glDrawArrays */ {
                int mode = intQueue[intPos++];
                int first = intQueue[intPos++];
                int count = intQueue[intPos++];
                if (p_glDrawArrays) p_glDrawArrays(mode, first, count);
                break;
            }
            case 34: /* glDrawElements */ {
                int mode = intQueue[intPos++];
                int count = intQueue[intPos++];
                int type = intQueue[intPos++];
                long long indices = longQueue ? longQueue[longPos++] : 0;
                if (p_glDrawElements) p_glDrawElements(mode, count, type, (const void*)(intptr_t)indices);
                break;
            }
            case 35: /* glEnable */ {
                int cap = intQueue[intPos++];
                if (p_glEnable) p_glEnable(cap);
                break;
            }
            case 36: /* glEnableVertexAttribArray */ {
                int idx = intQueue[intPos++];
                if (p_glEnableVertexAttribArray) p_glEnableVertexAttribArray(idx);
                break;
            }
            case 37: /* glFramebufferRenderbufferEXT */ {
                int target = intQueue[intPos++];
                int attach = intQueue[intPos++];
                int rbtarget = intQueue[intPos++];
                int rb = intQueue[intPos++];
                if (p_glFramebufferRenderbuffer) p_glFramebufferRenderbuffer(target, attach, rbtarget, rb);
                break;
            }
            case 38: /* glFramebufferTexture2DEXT */ {
                int target = intQueue[intPos++];
                int attach = intQueue[intPos++];
                int textarget = intQueue[intPos++];
                int tex = intQueue[intPos++];
                int level = intQueue[intPos++];
                if (p_glFramebufferTexture2D) p_glFramebufferTexture2D(target, attach, textarget, tex, level);
                break;
            }
            case 39: /* glFrontFace */ {
                int mode = intQueue[intPos++];
                if (p_glFrontFace) p_glFrontFace(mode);
                break;
            }
            case 40: /* glGenerateMipmapEXT */ {
                int target = intQueue[intPos++];
                if (p_glGenerateMipmap) p_glGenerateMipmap(target);
                break;
            }
            case 44: /* glHint */ {
                int target = intQueue[intPos++];
                int mode = intQueue[intPos++];
                if (p_glHint) p_glHint(target, mode);
                break;
            }
            case 45: /* glLineWidth */ {
                float w = floatQueue ? floatQueue[floatPos++] : 1.0f;
                if (p_glLineWidth) p_glLineWidth(w);
                break;
            }
            case 46: /* glLinkProgram */ {
                int prog = intQueue[intPos++];
                if (p_glLinkProgram) p_glLinkProgram(prog);
                break;
            }
            case 47: /* glPixelStorei */ {
                int pname = intQueue[intPos++];
                int param = intQueue[intPos++];
                if (p_glPixelStorei) p_glPixelStorei(pname, param);
                break;
            }
            case 48: /* glPolygonOffset */ {
                float factor = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float units = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glPolygonOffset) p_glPolygonOffset(factor, units);
                break;
            }
            case 50: /* glRenderbufferStorageEXT */ {
                int target = intQueue[intPos++];
                int fmt = intQueue[intPos++];
                int w = intQueue[intPos++];
                int h = intQueue[intPos++];
                if (p_glRenderbufferStorage) p_glRenderbufferStorage(target, fmt, w, h);
                break;
            }
            case 51: /* glSampleCoverage */ {
                float val = floatQueue ? floatQueue[floatPos++] : 1.0f;
                int invert = intQueue[intPos++];
                if (p_glSampleCoverage) p_glSampleCoverage(val, invert);
                break;
            }
            case 52: /* glScissor */ {
                int x = intQueue[intPos++];
                int y = intQueue[intPos++];
                int w = intQueue[intPos++];
                int h = intQueue[intPos++];
                if (p_glScissor) p_glScissor(x, y, w, h);
                break;
            }
            case 53: /* glStencilFunc */ {
                int func = intQueue[intPos++];
                int ref = intQueue[intPos++];
                int mask = intQueue[intPos++];
                if (p_glStencilFunc) p_glStencilFunc(func, ref, mask);
                break;
            }
            case 54: /* glStencilFuncSeparate */ {
                int face = intQueue[intPos++];
                int func = intQueue[intPos++];
                int ref = intQueue[intPos++];
                int mask = intQueue[intPos++];
                if (p_glStencilFuncSeparate) p_glStencilFuncSeparate(face, func, ref, mask);
                break;
            }
            case 55: /* glStencilMask */ {
                int mask = intQueue[intPos++];
                if (p_glStencilMask) p_glStencilMask(mask);
                break;
            }
            case 56: /* glStencilMaskSeparate */ {
                int face = intQueue[intPos++];
                int mask = intQueue[intPos++];
                if (p_glStencilMaskSeparate) p_glStencilMaskSeparate(face, mask);
                break;
            }
            case 57: /* glStencilOp */ {
                int fail = intQueue[intPos++];
                int zfail = intQueue[intPos++];
                int zpass = intQueue[intPos++];
                if (p_glStencilOp) p_glStencilOp(fail, zfail, zpass);
                break;
            }
            case 58: /* glStencilOpSeparate */ {
                int face = intQueue[intPos++];
                int fail = intQueue[intPos++];
                int zfail = intQueue[intPos++];
                int zpass = intQueue[intPos++];
                if (p_glStencilOpSeparate) p_glStencilOpSeparate(face, fail, zfail, zpass);
                break;
            }
            case 59: /* glTexParameterf */ {
                int target = intQueue[intPos++];
                int pname = intQueue[intPos++];
                float param = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glTexParameterf) p_glTexParameterf(target, pname, param);
                break;
            }
            case 61: /* glTexParameteri */ {
                int target = intQueue[intPos++];
                int pname = intQueue[intPos++];
                int param = intQueue[intPos++];
                if (p_glTexParameteri) p_glTexParameteri(target, pname, param);
                break;
            }
            case 63: /* glUniform1f */ {
                int loc = intQueue[intPos++];
                float v0 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glUniform1f) p_glUniform1f(loc, v0);
                break;
            }
            case 65: /* glUniform1i */ {
                int loc = intQueue[intPos++];
                int v0 = intQueue[intPos++];
                if (p_glUniform1i) p_glUniform1i(loc, v0);
                break;
            }
            case 67: /* glUniform2f */ {
                int loc = intQueue[intPos++];
                float v0 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float v1 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glUniform2f) p_glUniform2f(loc, v0, v1);
                break;
            }
            case 69: /* glUniform2i */ {
                int loc = intQueue[intPos++];
                int v0 = intQueue[intPos++];
                int v1 = intQueue[intPos++];
                if (p_glUniform2i) p_glUniform2i(loc, v0, v1);
                break;
            }
            case 71: /* glUniform3f */ {
                int loc = intQueue[intPos++];
                float v0 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float v1 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float v2 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glUniform3f) p_glUniform3f(loc, v0, v1, v2);
                break;
            }
            case 73: /* glUniform3i */ {
                int loc = intQueue[intPos++];
                int v0 = intQueue[intPos++];
                int v1 = intQueue[intPos++];
                int v2 = intQueue[intPos++];
                if (p_glUniform3i) p_glUniform3i(loc, v0, v1, v2);
                break;
            }
            case 75: /* glUniform4f */ {
                int loc = intQueue[intPos++];
                float v0 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float v1 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float v2 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float v3 = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glUniform4f) p_glUniform4f(loc, v0, v1, v2, v3);
                break;
            }
            case 77: /* glUniform4i */ {
                int loc = intQueue[intPos++];
                int v0 = intQueue[intPos++];
                int v1 = intQueue[intPos++];
                int v2 = intQueue[intPos++];
                int v3 = intQueue[intPos++];
                if (p_glUniform4i) p_glUniform4i(loc, v0, v1, v2, v3);
                break;
            }
            case 79: /* glUniformMatrix2fv */ {
                int loc = intQueue[intPos++];
                int count = intQueue[intPos++];
                int transpose = intQueue[intPos++];
                int len = intQueue[intPos++];
                float *vals = floatQueue ? &floatQueue[floatPos] : NULL;
                floatPos += len;
                int offset = intQueue[intPos++];
                if (p_glUniformMatrix2fv && vals) p_glUniformMatrix2fv(loc, count, transpose ? GL_TRUE : GL_FALSE, vals + offset);
                break;
            }
            case 80: /* glUniformMatrix3fv */ {
                int loc = intQueue[intPos++];
                int count = intQueue[intPos++];
                int transpose = intQueue[intPos++];
                int len = intQueue[intPos++];
                float *vals = floatQueue ? &floatQueue[floatPos] : NULL;
                floatPos += len;
                int offset = intQueue[intPos++];
                if (p_glUniformMatrix3fv && vals) p_glUniformMatrix3fv(loc, count, transpose ? GL_TRUE : GL_FALSE, vals + offset);
                break;
            }
            case 81: /* glUniformMatrix4fv */ {
                int loc = intQueue[intPos++];
                int count = intQueue[intPos++];
                int transpose = intQueue[intPos++];
                int len = intQueue[intPos++];
                float *vals = floatQueue ? &floatQueue[floatPos] : NULL;
                floatPos += len;
                int offset = intQueue[intPos++];
                if (p_glUniformMatrix4fv && vals) p_glUniformMatrix4fv(loc, count, transpose ? GL_TRUE : GL_FALSE, vals + offset);
                break;
            }
            case 82: /* glUseProgram */ {
                int prog = intQueue[intPos++];
                if (p_glUseProgram) p_glUseProgram(prog);
                break;
            }
            case 83: /* glValidateProgram */ {
                int prog = intQueue[intPos++];
                if (p_glValidateProgram) p_glValidateProgram(prog);
                break;
            }
            case 84: /* glVertexAttrib1f */ {
                int idx = intQueue[intPos++];
                float x = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glVertexAttrib1f) p_glVertexAttrib1f(idx, x);
                break;
            }
            case 86: /* glVertexAttrib2f */ {
                int idx = intQueue[intPos++];
                float x = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float y = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glVertexAttrib2f) p_glVertexAttrib2f(idx, x, y);
                break;
            }
            case 88: /* glVertexAttrib3f */ {
                int idx = intQueue[intPos++];
                float x = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float y = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float z = floatQueue ? floatQueue[floatPos++] : 0.0f;
                if (p_glVertexAttrib3f) p_glVertexAttrib3f(idx, x, y, z);
                break;
            }
            case 90: /* glVertexAttrib4f */ {
                int idx = intQueue[intPos++];
                float x = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float y = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float z = floatQueue ? floatQueue[floatPos++] : 0.0f;
                float w = floatQueue ? floatQueue[floatPos++] : 1.0f;
                if (p_glVertexAttrib4f) p_glVertexAttrib4f(idx, x, y, z, w);
                break;
            }
            case 92: /* glVertexAttribPointer */ {
                int idx = intQueue[intPos++];
                int size = intQueue[intPos++];
                int type = intQueue[intPos++];
                int normalized = intQueue[intPos++];
                int stride = intQueue[intPos++];
                long long ptr = longQueue ? longQueue[longPos++] : 0;
                if (p_glVertexAttribPointer) p_glVertexAttribPointer(idx, size, type, normalized ? GL_TRUE : GL_FALSE, stride, (const void*)(intptr_t)ptr);
                break;
            }
            case 93: /* glViewport */ {
                int x = intQueue[intPos++];
                int y = intQueue[intPos++];
                int w = intQueue[intPos++];
                int h = intQueue[intPos++];
                if (p_glViewport) p_glViewport(x, y, w, h);
                break;
            }
            default:
                break;
        }
    }    fflush(stdout);
    (*env)->ReleaseIntArrayElements(env, jIntArr, intQueue, JNI_ABORT);
    if (floatQueue) (*env)->ReleaseFloatArrayElements(env, jFloatArr, floatQueue, JNI_ABORT);
    if (longQueue) (*env)->ReleaseLongArrayElements(env, jLongArr, longQueue, JNI_ABORT);
    if (byteQueue) (*env)->ReleaseByteArrayElements(env, jByteArr, byteQueue, JNI_ABORT);    fflush(stdout);
}

/* =========================================================================
 * tsd.opengl.impl.GLImplCQ Synchronous & Creation Native Functions (78)
 * ========================================================================= */

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlBufferData(
    JNIEnv *env, jobject thiz,
    jint target, jint size, jobject data, jint offset, jboolean direct, jint usage, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, data, offset, direct);
    if (p_glBufferData) p_glBufferData(target, size, ptr, usage);
    releaseBufferPtr(env, data, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlBufferSubData(
    JNIEnv *env, jobject thiz,
    jint target, jint offset, jint size, jobject data, jint dataOffset, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, data, dataOffset, direct);
    if (p_glBufferSubData) p_glBufferSubData(target, offset, size, ptr);
    releaseBufferPtr(env, data, ptr, direct);
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCheckFramebufferStatusEXT(
    JNIEnv *env, jobject thiz, jint target, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glCheckFramebufferStatus ? p_glCheckFramebufferStatus(target) : GL_FRAMEBUFFER_COMPLETE;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexImage2D(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jint a2, jint a3, jint a4, jint a5, jint a6, jobject a7, jint a8, jboolean a9, jint a10) {    (void)thiz; (void)a10;
    void *ptr = getBufferPtr(env, a7, a8, a9);
    if (p_glCompressedTexImage2D) p_glCompressedTexImage2D(a0, a1, a2, a3, a4, a5, a6, ptr);
    releaseBufferPtr(env, a7, ptr, a9);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexImage2DL(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jint a2, jint a3, jint a4, jint a5, jint a6, jlong a7, jint a8) {    (void)env; (void)thiz; (void)a8;
    if (p_glCompressedTexImage2D) p_glCompressedTexImage2D(a0, a1, a2, a3, a4, a5, a6, (const void*)(intptr_t)a7);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexSubImage2D(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jint a2, jint a3, jint a4, jint a5, jint a6, jint a7, jobject a8, jint a9, jboolean a10, jint a11) {    (void)thiz; (void)a11;
    void *ptr = getBufferPtr(env, a8, a9, a10);
    if (p_glCompressedTexSubImage2D) p_glCompressedTexSubImage2D(a0, a1, a2, a3, a4, a5, a6, a7, ptr);
    releaseBufferPtr(env, a8, ptr, a10);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexSubImage2DL(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jint a2, jint a3, jint a4, jint a5, jint a6, jint a7, jlong a8, jint a9) {    (void)env; (void)thiz; (void)a9;
    if (p_glCompressedTexSubImage2D) p_glCompressedTexSubImage2D(a0, a1, a2, a3, a4, a5, a6, a7, (const void*)(intptr_t)a8);
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCreateProgram(
    JNIEnv *env, jobject thiz, jint queuePos) {    (void)env; (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    jint prog = p_glCreateProgram ? p_glCreateProgram() : 0;
    if (!prog) prog = ++g_genCounter;
    return prog;
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCreateShader(
    JNIEnv *env, jobject thiz, jint type, jint queuePos) {    (void)env; (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    jint sh = p_glCreateShader ? p_glCreateShader(type) : 0;
    if (!sh) sh = ++g_genCounter;
    return sh;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteBuffers(
    JNIEnv *env, jobject thiz, jint n, jobject bufs, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, bufs, off, direct);
    if (p_glDeleteBuffers && ptr) p_glDeleteBuffers(n, (const GLuint*)ptr);
    releaseBufferPtr(env, bufs, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteFramebuffersEXT(
    JNIEnv *env, jobject thiz, jint n, jobject fbs, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, fbs, off, direct);
    if (p_glDeleteFramebuffers && ptr) p_glDeleteFramebuffers(n, (const GLuint*)ptr);
    releaseBufferPtr(env, fbs, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteRenderbuffersEXT(
    JNIEnv *env, jobject thiz, jint n, jobject rbs, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, rbs, off, direct);
    if (p_glDeleteRenderbuffers && ptr) p_glDeleteRenderbuffers(n, (const GLuint*)ptr);
    releaseBufferPtr(env, rbs, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteTextures(
    JNIEnv *env, jobject thiz, jint n, jobject texs, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, texs, off, direct);
    if (p_glDeleteTextures && ptr) p_glDeleteTextures(n, (const GLuint*)ptr);
    releaseBufferPtr(env, texs, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDrawElements(
    JNIEnv *env, jobject thiz, jint mode, jint count, jint type, jobject indices, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, indices, off, direct);
    if (p_glDrawElements) p_glDrawElements(mode, count, type, ptr);
    releaseBufferPtr(env, indices, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlFinish(
    JNIEnv *env, jobject thiz, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    if (p_glFinish) p_glFinish();
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlFlush(
    JNIEnv *env, jobject thiz, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    if (p_glFlush) p_glFlush();
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenBuffers(
    JNIEnv *env, jobject thiz, jint n, jobject bufs, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (bufs && n > 0) {
        GLuint *ids = (GLuint*)calloc(n, sizeof(GLuint));
        if (p_glGenBuffers) p_glGenBuffers(n, ids);
        for (int i = 0; i < n; i++) {
            if (ids[i] == 0) ids[i] = ++g_genCounter;
        }
        writeInts(env, bufs, off, direct, (const jint*)ids, n);
        free(ids);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenFramebuffersEXT(
    JNIEnv *env, jobject thiz, jint n, jobject fbs, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (fbs && n > 0) {
        GLuint *ids = (GLuint*)calloc(n, sizeof(GLuint));
        if (p_glGenFramebuffers) p_glGenFramebuffers(n, ids);
        for (int i = 0; i < n; i++) {
            if (ids[i] == 0) ids[i] = ++g_genCounter;
        }
        writeInts(env, fbs, off, direct, (const jint*)ids, n);
        free(ids);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenRenderbuffersEXT(
    JNIEnv *env, jobject thiz, jint n, jobject rbs, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (rbs && n > 0) {
        GLuint *ids = (GLuint*)calloc(n, sizeof(GLuint));
        if (p_glGenRenderbuffers) p_glGenRenderbuffers(n, ids);
        for (int i = 0; i < n; i++) {
            if (ids[i] == 0) ids[i] = ++g_genCounter;
        }
        writeInts(env, rbs, off, direct, (const jint*)ids, n);
        free(ids);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenTextures(
    JNIEnv *env, jobject thiz, jint n, jobject texs, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (texs && n > 0) {
        GLuint *ids = (GLuint*)calloc(n, sizeof(GLuint));
        if (p_glGenTextures) p_glGenTextures(n, ids);
        for (int i = 0; i < n; i++) {
            if (ids[i] == 0) ids[i] = ++g_genCounter;
        }
        writeInts(env, texs, off, direct, (const jint*)ids, n);
        free(ids);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetActiveAttrib(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jint a2, jobject a3, jint a4, jboolean a5, jobject a6, jint a7, jboolean a8, jobject a9, jint a10, jboolean a11, jobject a12, jint a13, jboolean a14, jint a15) {    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; (void)a12; (void)a13; (void)a14; (void)a15;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetActiveUniform(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jint a2, jobject a3, jint a4, jboolean a5, jobject a6, jint a7, jboolean a8, jobject a9, jint a10, jboolean a11, jobject a12, jint a13, jboolean a14, jint a15) {    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; (void)a12; (void)a13; (void)a14; (void)a15;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetAttachedShaders(
    JNIEnv *env, jobject thiz, jint prog, jint maxCount, jobject countObj, jint countOff, jboolean countDirect, jobject shObj, jint shOff, jboolean shDirect, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetAttachedShaders) {
        GLsizei c = 0;
        GLuint shaders[32] = {0};
        p_glGetAttachedShaders(prog, maxCount > 32 ? 32 : maxCount, &c, shaders);
        writeInts(env, countObj, countOff, countDirect, (const jint*)&c, 1);
        writeInts(env, shObj, shOff, shDirect, (const jint*)shaders, c);
    }
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetAttribLocation(
    JNIEnv *env, jobject thiz, jint prog, jobject nameObj, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (!nameObj) return -1;
    const char *str = (*env)->GetStringUTFChars(env, (jstring)nameObj, NULL);
    jint loc = -1;
    if (p_glGetAttribLocation && str) {
        loc = p_glGetAttribLocation(prog, str);
    }
    if (str) (*env)->ReleaseStringUTFChars(env, (jstring)nameObj, str);
    return loc;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetBooleanv(
    JNIEnv *env, jobject thiz, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)env; (void)thiz; (void)pname; (void)params; (void)off; (void)direct; (void)queuePos;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetBufferParameteriv(
    JNIEnv *env, jobject thiz, jint target, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetBufferParameteriv && params) {
        GLint val = 0;
        p_glGetBufferParameteriv(target, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetError(
    JNIEnv *env, jobject thiz, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glGetError ? p_glGetError() : 0;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetFloatv(
    JNIEnv *env, jobject thiz, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, params, off, direct);
    if (p_glGetFloatv && ptr) p_glGetFloatv(pname, (GLfloat*)ptr);
    releaseBufferPtr(env, params, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetFramebufferAttachmentParameterivEXT(
    JNIEnv *env, jobject thiz, jint target, jint attach, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetFramebufferAttachmentParameteriv && params) {
        GLint val = 0;
        p_glGetFramebufferAttachmentParameteriv(target, attach, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetIntegerv(
    JNIEnv *env, jobject thiz, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (params) {
        GLint val[128] = {0};
        int numVals = 1;
        if (pname == 0x0BA2 /* GL_VIEWPORT */ || pname == 0x0C10 /* GL_SCISSOR_BOX */ || pname == 0x0B23 /* GL_COLOR_CLEAR_VALUE */) {
            numVals = 4;
        } else if (pname == 0x86A3 /* GL_COMPRESSED_TEXTURE_FORMATS */) {
            GLint numFormats = 0;
            if (p_glGetIntegerv) p_glGetIntegerv(0x86A2 /* GL_NUM_COMPRESSED_TEXTURE_FORMATS */, &numFormats);
            numVals = (numFormats > 0 && numFormats < 120) ? numFormats : 10;
        }
        if (p_glGetIntegerv) {
            p_glGetIntegerv(pname, val);
        }
        if (pname == 34930 /* GL_MAX_TEXTURE_IMAGE_UNITS */ && val[0] <= 0) val[0] = 16;
        else if (pname == 35660 /* GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS */ && val[0] <= 0) val[0] = 16;
        else if (pname == 35661 /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS */ && val[0] <= 0) val[0] = 16;
        else if (pname == 3379 /* GL_MAX_TEXTURE_SIZE */ && val[0] <= 0) val[0] = 2048;
        else if (pname == 34024 /* GL_MAX_RENDERBUFFER_SIZE */ && val[0] <= 0) val[0] = 2048;
        writeInts(env, params, off, direct, val, numVals);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetProgramInfoLog(
    JNIEnv *env, jobject thiz, jint prog, jint maxLen, jobject lenObj, jint lenOff, jboolean lenDirect, jobject logObj, jint logOff, jboolean logDirect, jint queuePos) {
    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glGetProgramInfoLog && logObj) {
        char buf[1024] = {0};
        GLsizei len = 0;
        p_glGetProgramInfoLog(prog, sizeof(buf) - 1, &len, buf);
        writeInts(env, lenObj, lenOff, lenDirect, (const jint*)&len, 1);
        if (lenDirect) {
            char *base = (char*)(*env)->GetDirectBufferAddress(env, logObj);
            if (base) memcpy(base + logOff, buf, len + 1);
        } else {
            (*env)->SetByteArrayRegion(env, (jbyteArray)logObj, logOff, len + 1, (const jbyte*)buf);
        }
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetProgramiv(
    JNIEnv *env, jobject thiz, jint prog, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {
    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glGetProgramiv && params) {
        GLint val = 1;
        p_glGetProgramiv(prog, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetRenderbufferParameterivEXT(
    JNIEnv *env, jobject thiz, jint target, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {
    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glGetRenderbufferParameteriv && params) {
        GLint val = 0;
        p_glGetRenderbufferParameteriv(target, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderInfoLog(
    JNIEnv *env, jobject thiz, jint sh, jint maxLen, jobject lenObj, jint lenOff, jboolean lenDirect, jobject logObj, jint logOff, jboolean logDirect, jint queuePos) {
    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glGetShaderInfoLog && logObj) {
        char buf[1024] = {0};
        GLsizei len = 0;
        p_glGetShaderInfoLog(sh, sizeof(buf) - 1, &len, buf);
        writeInts(env, lenObj, lenOff, lenDirect, (const jint*)&len, 1);
        if (logDirect) {
            char *base = (char*)(*env)->GetDirectBufferAddress(env, logObj);
            if (base) memcpy(base + logOff, buf, len + 1);
        } else {
            (*env)->SetByteArrayRegion(env, (jbyteArray)logObj, logOff, len + 1, (const jbyte*)buf);
        }
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderPrecisionFormat(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jobject a2, jint a3, jboolean a4, jobject a5, jint a6, jboolean a7, jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderSource(
    JNIEnv *env, jobject thiz, jint a0, jint a1, jobject a2, jint a3, jboolean a4, jobject a5, jint a6, jboolean a7, jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderiv(
    JNIEnv *env, jobject thiz, jint sh, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {
    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glGetShaderiv && params) {
        GLint val = 1;
        p_glGetShaderiv(sh, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT jstring JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetString(
    JNIEnv *env, jobject thiz, jint name, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetString) {
        const char *s = (const char*)p_glGetString(name);
        if (s) return (*env)->NewStringUTF(env, s);
    }
    return (*env)->NewStringUTF(env, "OpenGL ES 2.0 (Google ANGLE Direct3D 11)");
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetTexParameterfv(
    JNIEnv *env, jobject thiz, jint target, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, params, off, direct);
    if (p_glGetTexParameterfv && ptr) p_glGetTexParameterfv(target, pname, (GLfloat*)ptr);
    releaseBufferPtr(env, params, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetTexParameteriv(
    JNIEnv *env, jobject thiz, jint target, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetTexParameteriv && params) {
        GLint val = 0;
        p_glGetTexParameteriv(target, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetUniformLocation(
    JNIEnv *env, jobject thiz, jint prog, jobject nameObj, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (!nameObj) return -1;
    const char *str = (*env)->GetStringUTFChars(env, (jstring)nameObj, NULL);
    jint loc = -1;
    if (p_glGetUniformLocation && str) {
        loc = p_glGetUniformLocation(prog, str);
    }
    if (str) (*env)->ReleaseStringUTFChars(env, (jstring)nameObj, str);
    return loc;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetUniformfv(
    JNIEnv *env, jobject thiz, jint prog, jint loc, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, params, off, direct);
    if (p_glGetUniformfv && ptr) p_glGetUniformfv(prog, loc, (GLfloat*)ptr);
    releaseBufferPtr(env, params, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetUniformiv(
    JNIEnv *env, jobject thiz, jint prog, jint loc, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetUniformiv && params) {
        GLint val = 0;
        p_glGetUniformiv(prog, loc, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetVertexAttribfv(
    JNIEnv *env, jobject thiz, jint idx, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, params, off, direct);
    if (p_glGetVertexAttribfv && ptr) p_glGetVertexAttribfv(idx, pname, (GLfloat*)ptr);
    releaseBufferPtr(env, params, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetVertexAttribiv(
    JNIEnv *env, jobject thiz, jint idx, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    if (p_glGetVertexAttribiv && params) {
        GLint val = 0;
        p_glGetVertexAttribiv(idx, pname, &val);
        writeInts(env, params, off, direct, &val, 1);
    }
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsBuffer(
    JNIEnv *env, jobject thiz, jint buf, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsBuffer ? p_glIsBuffer(buf) : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsEnabled(
    JNIEnv *env, jobject thiz, jint cap, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsEnabled ? p_glIsEnabled(cap) : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsFramebufferEXT(
    JNIEnv *env, jobject thiz, jint fb, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsFramebuffer ? p_glIsFramebuffer(fb) : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsProgram(
    JNIEnv *env, jobject thiz, jint prog, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsProgram ? p_glIsProgram(prog) : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsRenderbufferEXT(
    JNIEnv *env, jobject thiz, jint rb, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsRenderbuffer ? p_glIsRenderbuffer(rb) : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsShader(
    JNIEnv *env, jobject thiz, jint sh, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsShader ? p_glIsShader(sh) : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsTexture(
    JNIEnv *env, jobject thiz, jint tex, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    return p_glIsTexture ? p_glIsTexture(tex) : JNI_TRUE;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlReadPixels(
    JNIEnv *env, jobject thiz, jint x, jint y, jint w, jint h, jint fmt, jint type, jobject pixels, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, pixels, off, direct);
    if (p_glReadPixels) p_glReadPixels(x, y, w, h, fmt, type, ptr);
    releaseBufferPtr(env, pixels, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlReadPixelsL(
    JNIEnv *env, jobject thiz, jint x, jint y, jint w, jint h, jint fmt, jint type, jlong pixels, jint queuePos) {    (void)env; (void)thiz; (void)queuePos;
    if (p_glReadPixels) p_glReadPixels(x, y, w, h, fmt, type, (void*)(intptr_t)pixels);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlShaderBinary(
    JNIEnv *env, jobject thiz, jint a0, jobject a1, jint a2, jboolean a3, jint a4, jobject a5, jint a6, jboolean a7, jint a8, jint a9) {    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlShaderSource(
    JNIEnv *env, jobject thiz, jint shader, jint count, jobject strArray, jobject lenObj, jint lenOff, jboolean direct, jint queuePos) {    (void)thiz; (void)lenObj; (void)lenOff; (void)direct;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (!strArray || count <= 0 || !p_glShaderSource) return;

    const char **lines = (const char **)malloc(count * sizeof(char*));
    jstring *jstrings = (jstring *)malloc(count * sizeof(jstring));

    for (int i = 0; i < count; i++) {
        jstrings[i] = (jstring)(*env)->GetObjectArrayElement(env, (jobjectArray)strArray, i);
        lines[i] = jstrings[i] ? (*env)->GetStringUTFChars(env, jstrings[i], NULL) : "";
    }

    p_glShaderSource(shader, count, (const GLchar* const*)lines, NULL);

    for (int i = 0; i < count; i++) {
        if (jstrings[i] && lines[i]) {
            (*env)->ReleaseStringUTFChars(env, jstrings[i], lines[i]);
            (*env)->DeleteLocalRef(env, jstrings[i]);
        }
    }
    free(lines);
    free(jstrings);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexImage2DL(
    JNIEnv *env, jobject thiz, jint target, jint level, jint internalformat, jint width, jint height, jint border, jint format, jint type, jlong pixels, jint queuePos) {    (void)env; (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glTexImage2D) p_glTexImage2D(target, level, internalformat, width, height, border, format, type, (const void*)(intptr_t)pixels);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexImage2D(
    JNIEnv *env, jobject thiz, jint target, jint level, jint internalformat, jint width, jint height, jint border, jint format, jint type, jobject pixels, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, pixels, off, direct);
    if (p_glTexImage2D) p_glTexImage2D(target, level, internalformat, width, height, border, format, type, ptr);
    releaseBufferPtr(env, pixels, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexParameterfv(
    JNIEnv *env, jobject thiz, jint target, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, params, off, direct);
    if (p_glTexParameterfv && ptr) p_glTexParameterfv(target, pname, (const GLfloat*)ptr);
    releaseBufferPtr(env, params, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexParameteriv(
    JNIEnv *env, jobject thiz, jint target, jint pname, jobject params, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, params, off, direct);
    if (p_glTexParameteriv && ptr) p_glTexParameteriv(target, pname, (const GLint*)ptr);
    releaseBufferPtr(env, params, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexSubImage2DL(
    JNIEnv *env, jobject thiz, jint target, jint level, jint xoffset, jint yoffset, jint width, jint height, jint format, jint type, jlong pixels, jint queuePos) {    (void)env; (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    if (p_glTexSubImage2D) p_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, (const void*)(intptr_t)pixels);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexSubImage2D(
    JNIEnv *env, jobject thiz, jint target, jint level, jint xoffset, jint yoffset, jint width, jint height, jint format, jint type, jobject pixels, jint off, jboolean direct, jint queuePos) {    (void)thiz;
    if (queuePos > 0) executeGLQueue(env, queuePos);
    ensureEglCurrent();
    void *ptr = getBufferPtr(env, pixels, off, direct);
    if (p_glTexSubImage2D) p_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, ptr);
    releaseBufferPtr(env, pixels, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform1fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform1fv && ptr) p_glUniform1fv(loc, count, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform1iv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform1iv && ptr) p_glUniform1iv(loc, count, (const GLint*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform2fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform2fv && ptr) p_glUniform2fv(loc, count, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform2iv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform2iv && ptr) p_glUniform2iv(loc, count, (const GLint*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform3fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform3fv && ptr) p_glUniform3fv(loc, count, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform3iv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform3iv && ptr) p_glUniform3iv(loc, count, (const GLint*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform4fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform4fv && ptr) p_glUniform4fv(loc, count, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform4iv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniform4iv && ptr) p_glUniform4iv(loc, count, (const GLint*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniformMatrix2fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jboolean transpose, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniformMatrix2fv && ptr) p_glUniformMatrix2fv(loc, count, transpose ? GL_TRUE : GL_FALSE, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniformMatrix3fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jboolean transpose, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniformMatrix3fv && ptr) p_glUniformMatrix3fv(loc, count, transpose ? GL_TRUE : GL_FALSE, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniformMatrix4fv(
    JNIEnv *env, jobject thiz, jint loc, jint count, jboolean transpose, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glUniformMatrix4fv && ptr) p_glUniformMatrix4fv(loc, count, transpose ? GL_TRUE : GL_FALSE, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib1fv(
    JNIEnv *env, jobject thiz, jint idx, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glVertexAttrib1fv && ptr) p_glVertexAttrib1fv(idx, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib2fv(
    JNIEnv *env, jobject thiz, jint idx, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glVertexAttrib2fv && ptr) p_glVertexAttrib2fv(idx, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib3fv(
    JNIEnv *env, jobject thiz, jint idx, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glVertexAttrib3fv && ptr) p_glVertexAttrib3fv(idx, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib4fv(
    JNIEnv *env, jobject thiz, jint idx, jobject val, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, val, off, direct);
    if (p_glVertexAttrib4fv && ptr) p_glVertexAttrib4fv(idx, (const GLfloat*)ptr);
    releaseBufferPtr(env, val, ptr, direct);
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttribPointer(
    JNIEnv *env, jobject thiz, jint idx, jint size, jint type, jboolean norm, jint stride, jobject ptrObj, jint off, jboolean direct, jint queuePos) {    (void)thiz; (void)queuePos;
    void *ptr = getBufferPtr(env, ptrObj, off, direct);
    if (p_glVertexAttribPointer) p_glVertexAttribPointer(idx, size, type, norm ? GL_TRUE : GL_FALSE, stride, ptr);
    releaseBufferPtr(env, ptrObj, ptr, direct);
}

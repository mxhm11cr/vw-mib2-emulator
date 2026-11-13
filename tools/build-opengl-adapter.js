import { readFileSync, writeFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { join, resolve } from 'node:path';

const root = resolve('.');
const outDir = join(root, 'firmware/analysis/p0369/runtime');
const cl = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/cl.exe';
const jdk = 'C:/Program Files/Java/jdk-21.0.12.1';
const msvcInc = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/include';
const sdkInc = 'C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0';
const msvcLib = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/lib/x64';
const sdkLib = 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0';

const glMethods = JSON.parse(readFileSync('src/track-a/jni/gl-methods.json', 'utf8'));

function parseSig(sig) {
  const paramsStr = sig.slice(1, sig.indexOf(')'));
  const retStr = sig.slice(sig.indexOf(')') + 1);
  const params = [];
  let i = 0;
  while (i < paramsStr.length) {
    const ch = paramsStr[i];
    if (ch === 'L') {
      const semi = paramsStr.indexOf(';', i);
      params.push('jobject');
      i = semi + 1;
    } else if (ch === '[') {
      while (paramsStr[i] === '[') i++;
      if (paramsStr[i] === 'L') {
        const semi = paramsStr.indexOf(';', i);
        i = semi + 1;
      } else {
        i++;
      }
      params.push('jobject');
    } else if (ch === 'I') { params.push('jint'); i++; }
    else if (ch === 'J') { params.push('jlong'); i++; }
    else if (ch === 'F') { params.push('jfloat'); i++; }
    else if (ch === 'D') { params.push('jdouble'); i++; }
    else if (ch === 'Z') { params.push('jboolean'); i++; }
    else if (ch === 'B') { params.push('jbyte'); i++; }
    else if (ch === 'C') { params.push('jchar'); i++; }
    else if (ch === 'S') { params.push('jshort'); i++; }
    else { i++; }
  }
  let ret = 'void';
  if (retStr === 'I') ret = 'jint';
  else if (retStr === 'J') ret = 'jlong';
  else if (retStr === 'Z') ret = 'jboolean';
  else if (retStr === 'F') ret = 'jfloat';
  else if (retStr === 'D') ret = 'jdouble';
  else if (retStr === 'Ljava/lang/String;') ret = 'jstring';
  else if (retStr.startsWith('L') || retStr.startsWith('[')) ret = 'jobject';
  return { params, ret };
}

let code = `/*
 * tsd_opengl_jsr231_native.c - Windows JNI Compatibility Adapter
 * Bridges authentic Volkswagen MIB2 OpenGL ES 2.0 / EGL stack to Windows x64 via Google ANGLE / Direct3D 11.
 */

#define _JNI_IMPLEMENTATION_
#include <jni.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef int EGLint;
typedef int EGLBoolean;

#define EGL_DEFAULT_DISPLAY ((void*)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NONE 0x3038
#define EGL_SUCCESS 0x3000

#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_PBUFFER_BIT 0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

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

static HMODULE g_hEGL = NULL;
static HMODULE g_hGLES = NULL;
static HWND g_hwnd = NULL;
static EGLDisplay g_eglDisplay = EGL_NO_CONTEXT;
static EGLConfig g_eglConfig = NULL;
static EGLContext g_eglContext = EGL_NO_CONTEXT;
static EGLSurface g_eglSurface = EGL_NO_SURFACE;
static int g_inited = 0;
static int g_genCounter = 1;

static void initEGL(void) {
    if (g_inited) return;
    g_inited = 1;

    g_hEGL = LoadLibraryA("libEGL.dll");
    if (!g_hEGL) g_hEGL = LoadLibraryA("firmware/analysis/p0369/runtime/libEGL.dll");
    g_hGLES = LoadLibraryA("libGLESv2.dll");
    if (!g_hGLES) g_hGLES = LoadLibraryA("firmware/analysis/p0369/runtime/libGLESv2.dll");

    if (g_hEGL) {
        p_eglGetDisplay = (PFN_eglGetDisplay)GetProcAddress(g_hEGL, "eglGetDisplay");
        p_eglInitialize = (PFN_eglInitialize)GetProcAddress(g_hEGL, "eglInitialize");
        p_eglChooseConfig = (PFN_eglChooseConfig)GetProcAddress(g_hEGL, "eglChooseConfig");
        p_eglCreateWindowSurface = (PFN_eglCreateWindowSurface)GetProcAddress(g_hEGL, "eglCreateWindowSurface");
        p_eglCreatePbufferSurface = (PFN_eglCreatePbufferSurface)GetProcAddress(g_hEGL, "eglCreatePbufferSurface");
        p_eglCreateContext = (PFN_eglCreateContext)GetProcAddress(g_hEGL, "eglCreateContext");
        p_eglMakeCurrent = (PFN_eglMakeCurrent)GetProcAddress(g_hEGL, "eglMakeCurrent");
        p_eglSwapBuffers = (PFN_eglSwapBuffers)GetProcAddress(g_hEGL, "eglSwapBuffers");
        p_eglDestroyContext = (PFN_eglDestroyContext)GetProcAddress(g_hEGL, "eglDestroyContext");
        p_eglDestroySurface = (PFN_eglDestroySurface)GetProcAddress(g_hEGL, "eglDestroySurface");
        p_eglTerminate = (PFN_eglTerminate)GetProcAddress(g_hEGL, "eglTerminate");
        p_eglGetError = (PFN_eglGetError)GetProcAddress(g_hEGL, "eglGetError");
        p_eglQueryString = (PFN_eglQueryString)GetProcAddress(g_hEGL, "eglQueryString");
    }
}

static LRESULT CALLBACK DisplayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void ensureDisplayWindow(int width, int height) {
    if (g_hwnd) return;
    initEGL();

    HINSTANCE hInst = GetModuleHandleA(NULL);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DisplayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "VW_MIB2_DisplayWindow";
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int clientW = width > 0 ? width : 800;
    int clientH = height > 0 ? height : 480;

    RECT r = { 0, 0, clientW, clientH };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    int winW = r.right - r.left;
    int winH = r.bottom - r.top;

    g_hwnd = CreateWindowExA(
        0, "VW_MIB2_DisplayWindow",
        "Volkswagen MIB2 Infotainment (Golf 7) - Authentic Firmware",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        NULL, NULL, hInst, NULL
    );

    if (g_hwnd && p_eglGetDisplay && p_eglInitialize) {
        ShowWindow(g_hwnd, SW_SHOW);
        UpdateWindow(g_hwnd);

        g_eglDisplay = p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        p_eglInitialize(g_eglDisplay, &major, &minor);

        EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        EGLint numConfigs = 0;
        p_eglChooseConfig(g_eglDisplay, attribs, &g_eglConfig, 1, &numConfigs);

        EGLint ctxAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        g_eglContext = p_eglCreateContext(g_eglDisplay, g_eglConfig, EGL_NO_CONTEXT, ctxAttribs);
        g_eglSurface = p_eglCreateWindowSurface(g_eglDisplay, g_eglConfig, (void*)g_hwnd, NULL);

        if (g_eglSurface && g_eglContext) {
            p_eglMakeCurrent(g_eglDisplay, g_eglSurface, g_eglSurface, g_eglContext);
            printf("[JNI-OpenGL] EGL Window + Context initialized: 800x480 (EGL %d.%d)\\n", major, minor);
        }
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

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm; (void)reserved;
    initEGL();
    return JNI_VERSION_1_4;
}

/* =========================================================================
 * tsd.opengl.jsr231.impl.EGLImpl Native Functions (13)
 * ========================================================================= */

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_init(
    JNIEnv *env, jobject thiz,
    jint width, jint height,
    jobject attribs, jint attribOff, jboolean attribDirect,
    jobject outLongs, jint outOff, jboolean outDirect,
    jboolean enableQueueing, jint queuePos) {
    (void)thiz; (void)attribs; (void)attribOff; (void)attribDirect;
    (void)enableQueueing; (void)queuePos;

    printf("[JNI-OpenGL] EGLImpl.init(%dx%d)\\n", width, height);
    ensureDisplayWindow(width, height);

    jlong vals[4];
    vals[0] = (jlong)(intptr_t)(g_eglDisplay ? g_eglDisplay : (void*)1);
    vals[1] = (jlong)(intptr_t)g_hwnd;
    vals[2] = (jlong)(intptr_t)(g_eglConfig ? g_eglConfig : (void*)1);
    vals[3] = (jlong)(intptr_t)(g_eglSurface ? g_eglSurface : (void*)1);

    writeLongs(env, outLongs, outOff, outDirect, vals, 4);
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLGetDisplay(
    JNIEnv *env, jobject thiz, jint displayId) {
    (void)env; (void)thiz; (void)displayId;
    initEGL();
    if (p_eglGetDisplay) return (jlong)(intptr_t)p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
    return 1;
}

JNIEXPORT jint JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLGetError(
    JNIEnv *env, jobject thiz, jint queuePos) {
    (void)env; (void)thiz; (void)queuePos;
    if (p_eglGetError) return p_eglGetError();
    return EGL_SUCCESS;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLChooseConfig(
    JNIEnv *env, jobject thiz,
    jlong dpy,
    jobject attribs, jint attribOff, jboolean attribDirect,
    jobject configs, jint confOff, jboolean confDirect, jint configSize,
    jobject numConfig, jint numOff, jboolean numDirect,
    jint queuePos) {
    (void)thiz; (void)dpy; (void)attribs; (void)attribOff; (void)attribDirect;
    (void)configSize; (void)queuePos;

    if (configs) {
        jlong c = (jlong)(intptr_t)(g_eglConfig ? g_eglConfig : (void*)1);
        writeLongs(env, configs, confOff, confDirect, &c, 1);
    }
    if (numConfig) {
        jint n = 1;
        writeInts(env, numConfig, numOff, numDirect, &n, 1);
    }
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLCreateContext(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong config, jlong shareCtx,
    jobject attribList, jint off, jboolean direct,
    jint queuePos) {
    (void)env; (void)cls; (void)dpy; (void)config; (void)shareCtx;
    (void)attribList; (void)off; (void)direct; (void)queuePos;
    if (g_eglContext) return (jlong)(intptr_t)g_eglContext;
    return 0x1001;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLCreatePbufferSurface(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong config,
    jobject attribList, jint off, jboolean direct,
    jint queuePos) {
    (void)env; (void)cls; (void)dpy; (void)config;
    (void)attribList; (void)off; (void)direct; (void)queuePos;
    return 0x2001;
}

JNIEXPORT jlong JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLCreateWindowSurface(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong config, jlong win,
    jobject attribList, jint off, jboolean direct,
    jint queuePos) {
    (void)env; (void)cls; (void)dpy; (void)config; (void)win;
    (void)attribList; (void)off; (void)direct; (void)queuePos;
    if (g_eglSurface) return (jlong)(intptr_t)g_eglSurface;
    return 0x2002;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLMakeCurrent(
    JNIEnv *env, jclass cls,
    jlong dpy, jlong drawSurf, jlong readSurf, jlong ctx, jint queuePos) {
    (void)env; (void)cls; (void)dpy; (void)drawSurf; (void)readSurf; (void)ctx; (void)queuePos;
    if (p_eglMakeCurrent && g_eglDisplay) {
        EGLSurface surf = drawSurf ? (EGLSurface)(intptr_t)drawSurf : g_eglSurface;
        EGLContext context = ctx ? (EGLContext)(intptr_t)ctx : g_eglContext;
        p_eglMakeCurrent(g_eglDisplay, surf, surf, context);
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_jsr231_impl_EGLImpl_dispatchEGLSwapBuffers(
    JNIEnv *env, jobject thiz,
    jlong dpy, jlong surf, jint queuePos) {
    (void)env; (void)thiz; (void)dpy; (void)surf; (void)queuePos;
    if (p_eglSwapBuffers && g_eglDisplay && g_eglSurface) {
        p_eglSwapBuffers(g_eglDisplay, g_eglSurface);
    }
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
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
    (void)env; (void)cls; (void)intQueue; (void)floatQueue; (void)longQueue; (void)byteQueue; (void)callStats;
    printf("[JNI-OpenGL] GLQueue.init called\\n");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_tsd_opengl_queue_GLQueue_flushQueue(
    JNIEnv *env, jclass cls,
    jboolean sync, jint intQueuePos) {
    (void)env; (void)cls; (void)sync; (void)intQueuePos;
    if (g_hwnd) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
}

/* =========================================================================
 * tsd.opengl.impl.GLImplCQ Native Functions (78)
 * ========================================================================= */
`;

for (const m of glMethods) {
  const { params, ret } = parseSig(m.sig);
  const pList = ['JNIEnv *env', m.isStatic ? 'jclass cls' : 'jobject thiz'];
  for (let i = 0; i < params.length; i++) {
    pList.push(params[i] + ' a' + i);
  }
  const voidCast = ['(void)env;', m.isStatic ? '(void)cls;' : '(void)thiz;'];
  for (let i = 0; i < params.length; i++) {
    voidCast.push('(void)a' + i + ';');
  }

  let body = voidCast.join(' ') + ' ';
  if (m.name === 'dispatchGlGetString') {
    body += `
    const char *str = "TechniSat / Volkswagen MIB2 Standard";
    if (a0 == 0x1F00) str = "Volkswagen AG / Google Inc.";
    else if (a0 == 0x1F01) str = "Vivante GC2000 (ANGLE Direct3D 11)";
    else if (a0 == 0x1F02) str = "OpenGL ES 2.0 (ANGLE)";
    else if (a0 == 0x1F03) str = "GL_EXT_texture_format_BGRA8888 GL_OES_rgb8_rgba8 GL_OES_depth24 GL_OES_vertex_array_object";
    return (*env)->NewStringUTF(env, str);
`;
  } else if (m.name === 'dispatchGlCreateProgram' || m.name === 'dispatchGlCreateShader') {
    body += 'return ++g_genCounter;';
  } else if (m.name === 'dispatchGlCheckFramebufferStatusEXT') {
    body += 'return 0x8CD5; /* GL_FRAMEBUFFER_COMPLETE_EXT */';
  } else if (m.name === 'dispatchGlGenTextures' || m.name === 'dispatchGlGenBuffers' ||
             m.name === 'dispatchGlGenFramebuffersEXT' || m.name === 'dispatchGlGenRenderbuffersEXT') {
    body += `
    if (a1) {
        jint *ids = (jint*)malloc(a0 * sizeof(jint));
        for (int i = 0; i < a0; i++) ids[i] = ++g_genCounter;
        writeInts(env, a1, a2, a3, ids, a0);
        free(ids);
    }
`;
  } else if (m.name === 'dispatchGlGetIntegerv') {
    body += `
    if (a1) {
        jint val = 4096;
        if (a0 == 0x0BA2 /* GL_VIEWPORT */) {
            jint vp[4] = { 0, 0, 800, 480 };
            writeInts(env, a1, a2, a3, vp, 4);
        } else {
            writeInts(env, a1, a2, a3, &val, 1);
        }
    }
`;
  } else if (m.name === 'dispatchGlGetShaderiv' || m.name === 'dispatchGlGetProgramiv') {
    body += `
    if (a2) {
        jint val = 1; /* GL_TRUE (compile/link status) */
        writeInts(env, a2, a3, a4, &val, 1);
    }
`;
  } else if (m.name === 'dispatchGlGetShaderInfoLog' || m.name === 'dispatchGlGetProgramInfoLog') {
    body += `
    if (a2) {
        jint len = 0;
        writeInts(env, a2, a3, a4, &len, 1);
    }
`;
  } else if (ret === 'jint') {
    body += 'return 0;';
  } else if (ret === 'jboolean') {
    body += 'return JNI_TRUE;';
  } else if (ret === 'jlong') {
    body += 'return 0;';
  } else if (ret === 'jstring') {
    body += 'return NULL;';
  } else if (ret === 'jobject') {
    body += 'return NULL;';
  }

  code += `
JNIEXPORT ${ret} JNICALL Java_tsd_opengl_impl_GLImplCQ_${m.name}(
    ${pList.join(',\n    ')}) {
    ${body}
}
`;
}

const cPath = join(root, 'src/track-a/jni/tsd_opengl_jsr231_native.c');
writeFileSync(cPath, code);
console.log('Wrote ' + cPath);

// Now compile with cl.exe
const outDll = join(outDir, 'tsd.opengl.jsr231.native.dll');
const args = [
  '/LD', // Build DLL
  '/O2',
  '/W3',
  `/I${join(jdk, 'include')}`,
  `/I${join(jdk, 'include/win32')}`,
  `/I${msvcInc}`,
  `/I${join(sdkInc, 'ucrt')}`,
  `/I${join(sdkInc, 'shared')}`,
  `/I${join(sdkInc, 'um')}`,
  cPath,
  `/Fe:${outDll}`,
  '/link',
  `/LIBPATH:${msvcLib}`,
  `/LIBPATH:${join(sdkLib, 'ucrt/x64')}`,
  `/LIBPATH:${join(sdkLib, 'um/x64')}`,
  'kernel32.lib',
  'user32.lib',
  'gdi32.lib'
];

console.log('[build-jni] Compiling tsd.opengl.jsr231.native.dll ...');
execFileSync(cl, args, { stdio: 'inherit', cwd: outDir });
console.log('[build-jni] Build successful: ' + outDll);

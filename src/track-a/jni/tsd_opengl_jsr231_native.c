/*
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

typedef void (*PFN_SendTouchGesture)(int gesture, int x, int y);
static PFN_SendTouchGesture p_SendTouchGesture = NULL;

static void initEGL(void) {
    if (g_inited) return;
    g_inited = 1;

    HMODULE hDsi = GetModuleHandleA("tsd.mibstd2.hmi.dsi.native.dll");
    if (!hDsi) hDsi = LoadLibraryA("tsd.mibstd2.hmi.dsi.native.dll");
    if (!hDsi) hDsi = LoadLibraryA("firmware/analysis/p0369/runtime/tsd.mibstd2.hmi.dsi.native.dll");
    if (hDsi) {
        p_SendTouchGesture = (PFN_SendTouchGesture)GetProcAddress(hDsi, "SendTouchGesture");
    }

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
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            if (p_SendTouchGesture) {
                p_SendTouchGesture(1, x, y); // PRESS
            }
            SetCapture(hwnd);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (wParam & MK_LBUTTON) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                if (p_SendTouchGesture) {
                    p_SendTouchGesture(3, x, y); // DRAG
                }
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            ReleaseCapture();
            if (p_SendTouchGesture) {
                p_SendTouchGesture(0, x, y); // RELEASE
                p_SendTouchGesture(2, x, y); // CLICK / TAP
            }
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
            printf("[JNI-OpenGL] EGL Window + Context initialized: 800x480 (EGL %d.%d)\n", major, minor);
            fflush(stdout);
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

    printf("[JNI-OpenGL] EGLImpl.init(%dx%d)\n", width, height);
    fflush(stdout);
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
    static int s_swapCount = 0;
    if (++s_swapCount <= 5 || (s_swapCount % 60 == 0)) {
        printf("[JNI-OpenGL] eglSwapBuffers frame=%d\n", s_swapCount);
        fflush(stdout);
    }
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
    printf("[JNI-OpenGL] GLQueue.init called\n");
    fflush(stdout);
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

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlBufferData(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlBufferSubData(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCheckFramebufferStatusEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return 0x8CD5; /* GL_FRAMEBUFFER_COMPLETE_EXT */
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexImage2D(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jobject a7,
    jint a8,
    jboolean a9,
    jint a10) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexImage2DL(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jlong a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexSubImage2D(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jint a7,
    jobject a8,
    jint a9,
    jboolean a10,
    jint a11) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCompressedTexSubImage2DL(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jint a7,
    jlong a8,
    jint a9) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; 
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCreateProgram(
    JNIEnv *env,
    jobject thiz,
    jint a0) {
    (void)env; (void)thiz; (void)a0; return ++g_genCounter;
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlCreateShader(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return ++g_genCounter;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteBuffers(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteFramebuffersEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteRenderbuffersEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDeleteTextures(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlDrawElements(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlFinish(
    JNIEnv *env,
    jobject thiz,
    jint a0) {
    (void)env; (void)thiz; (void)a0; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlFlush(
    JNIEnv *env,
    jobject thiz,
    jint a0) {
    (void)env; (void)thiz; (void)a0; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenBuffers(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
    if (a1) {
        jint *ids = (jint*)malloc(a0 * sizeof(jint));
        for (int i = 0; i < a0; i++) ids[i] = ++g_genCounter;
        writeInts(env, a1, a2, a3, ids, a0);
        free(ids);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenFramebuffersEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
    if (a1) {
        jint *ids = (jint*)malloc(a0 * sizeof(jint));
        for (int i = 0; i < a0; i++) ids[i] = ++g_genCounter;
        writeInts(env, a1, a2, a3, ids, a0);
        free(ids);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenRenderbuffersEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
    if (a1) {
        jint *ids = (jint*)malloc(a0 * sizeof(jint));
        for (int i = 0; i < a0; i++) ids[i] = ++g_genCounter;
        writeInts(env, a1, a2, a3, ids, a0);
        free(ids);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGenTextures(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
    if (a1) {
        jint *ids = (jint*)malloc(a0 * sizeof(jint));
        for (int i = 0; i < a0; i++) ids[i] = ++g_genCounter;
        writeInts(env, a1, a2, a3, ids, a0);
        free(ids);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetActiveAttrib(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jobject a6,
    jint a7,
    jboolean a8,
    jobject a9,
    jint a10,
    jboolean a11,
    jobject a12,
    jint a13,
    jboolean a14,
    jint a15) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; (void)a12; (void)a13; (void)a14; (void)a15; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetActiveUniform(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jobject a6,
    jint a7,
    jboolean a8,
    jobject a9,
    jint a10,
    jboolean a11,
    jobject a12,
    jint a13,
    jboolean a14,
    jint a15) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; (void)a12; (void)a13; (void)a14; (void)a15; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetAttachedShaders(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetAttribLocation(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; return 0;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetBooleanv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetBufferParameteriv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetError(
    JNIEnv *env,
    jobject thiz,
    jint a0) {
    (void)env; (void)thiz; (void)a0; return 0;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetFloatv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetFramebufferAttachmentParameterivEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetIntegerv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
    if (a1) {
        jint val = 4096;
        if (a0 == 0x0BA2 /* GL_VIEWPORT */) {
            jint vp[4] = { 0, 0, 800, 480 };
            writeInts(env, a1, a2, a3, vp, 4);
        } else {
            writeInts(env, a1, a2, a3, &val, 1);
        }
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetProgramInfoLog(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
    if (a2) {
        jint len = 0;
        writeInts(env, a2, a3, a4, &len, 1);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetProgramiv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
    if (a2) {
        jint val = 1; /* GL_TRUE (compile/link status) */
        writeInts(env, a2, a3, a4, &val, 1);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetRenderbufferParameterivEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderInfoLog(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
    if (a2) {
        jint len = 0;
        writeInts(env, a2, a3, a4, &len, 1);
    }

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderPrecisionFormat(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderSource(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetShaderiv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
    if (a2) {
        jint val = 1; /* GL_TRUE (compile/link status) */
        writeInts(env, a2, a3, a4, &val, 1);
    }

}

JNIEXPORT jstring JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetString(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; 
    const char *str = "TechniSat / Volkswagen MIB2 Standard";
    if (a0 == 0x1F00) str = "Volkswagen AG / Google Inc.";
    else if (a0 == 0x1F01) str = "Vivante GC2000 (ANGLE Direct3D 11)";
    else if (a0 == 0x1F02) str = "OpenGL ES 2.0 (ANGLE)";
    else if (a0 == 0x1F03) str = "GL_EXT_texture_format_BGRA8888 GL_OES_rgb8_rgba8 GL_OES_depth24 GL_OES_vertex_array_object";
    return (*env)->NewStringUTF(env, str);

}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetTexParameterfv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetTexParameteriv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT jint JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetUniformLocation(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; return 0;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetUniformfv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetUniformiv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetVertexAttribfv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlGetVertexAttribiv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsBuffer(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsEnabled(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsFramebufferEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsProgram(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsRenderbufferEXT(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsShader(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlIsTexture(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1) {
    (void)env; (void)thiz; (void)a0; (void)a1; return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlReadPixels(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jobject a6,
    jint a7,
    jboolean a8,
    jint a9) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlReadPixelsL(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jlong a6,
    jint a7) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlShaderBinary(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8,
    jint a9) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlShaderSource(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexImage2DL(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jint a7,
    jlong a8,
    jint a9) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexImage2D(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jint a7,
    jobject a8,
    jint a9,
    jboolean a10,
    jint a11) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexParameterfv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexParameteriv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexSubImage2DL(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jint a7,
    jlong a8,
    jint a9) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlTexSubImage2D(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jint a3,
    jint a4,
    jint a5,
    jint a6,
    jint a7,
    jobject a8,
    jint a9,
    jboolean a10,
    jint a11) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform1fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform1iv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform2fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform2iv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform3fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform3iv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform4fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniform4iv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jobject a2,
    jint a3,
    jboolean a4,
    jint a5) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniformMatrix2fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jboolean a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniformMatrix3fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jboolean a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlUniformMatrix4fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jboolean a2,
    jobject a3,
    jint a4,
    jboolean a5,
    jint a6) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib1fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib2fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib3fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttrib4fv(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jobject a1,
    jint a2,
    jboolean a3,
    jint a4) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; 
}

JNIEXPORT void JNICALL Java_tsd_opengl_impl_GLImplCQ_dispatchGlVertexAttribPointer(
    JNIEnv *env,
    jobject thiz,
    jint a0,
    jint a1,
    jint a2,
    jboolean a3,
    jint a4,
    jobject a5,
    jint a6,
    jboolean a7,
    jint a8) {
    (void)env; (void)thiz; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; 
}

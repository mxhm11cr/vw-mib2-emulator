/*
 * tsd_mibstd2_hmi_dsi_native.c - Windows JNI Compatibility Adapter
 *
 * Implements the authentic Volkswagen MIB2 Standard DSI SPI Native Library:
 *   "tsd.mibstd2.hmi.dsi.native"
 * Targets Windows x64 on OpenJDK 21.
 *
 * All 49 JNI exported functions matching the original QNX ARM ELF
 * libtsd.mibstd2.hmi.dsi.native.so are implemented with safe deterministic responses.
 */

#define _JNI_IMPLEMENTATION_
#include <jni.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

static volatile LONG g_running = 1;

#define MAX_DSI_QUEUE 128
typedef struct {
    int len;
    unsigned char data[1024];
} DsiPacket;

static DsiPacket g_dsiQueue[MAX_DSI_QUEUE];
static int g_dsiHead = 0;
static int g_dsiTail = 0;
static CRITICAL_SECTION g_dsiCs;
static HANDLE g_dsiSem = NULL;
static int g_dsiInited = 0;

static void initDsiQueue(void) {
    if (!g_dsiInited) {
        InitializeCriticalSection(&g_dsiCs);
        g_dsiSem = CreateSemaphore(NULL, 0, 1000, NULL);
        g_dsiInited = 1;
    }
}

static void queueDsiPacket(const void* data, int len) {
    initDsiQueue();
    if (len <= 0 || len > 1024) return;
    EnterCriticalSection(&g_dsiCs);
    int nextTail = (g_dsiTail + 1) % MAX_DSI_QUEUE;
    if (nextTail != g_dsiHead) {
        g_dsiQueue[g_dsiTail].len = len;
        memcpy(g_dsiQueue[g_dsiTail].data, data, len);
        g_dsiTail = nextTail;
        ReleaseSemaphore(g_dsiSem, 1, NULL);
    }
    LeaveCriticalSection(&g_dsiCs);
}

__declspec(dllexport) void SendTouchGesture(int gesture, int x, int y) {
    int pkt[13];
    pkt[0] = 33675264;              // 0x0201d800 (DSIKeyPanelImpl event ID)
    pkt[1] = -1505757135;           // 0xa63ff831 (version hash)
    pkt[2] = 20;                    // ATTR_GESTURE2
    pkt[3] = 0;                     // instance
    pkt[4] = gesture;               // 1=PRESS, 0=RELEASE, 2=CLICK, 3=DRAG
    pkt[5] = x;
    pkt[6] = 1;                     // valid
    pkt[7] = y;
    pkt[8] = 0;                     // param1
    pkt[9] = 0;                     // param2
    pkt[10] = (int)GetTickCount();  // timestamp
    pkt[11] = 0;                    // zDelta
    pkt[12] = 1;                    // flags (VALID)
    queueDsiPacket(pkt, sizeof(pkt));
}

/* =========================================================================
 * 1. DSI Communication Threads
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_dsi_communication_DSITransceiverThread
 * Method:    _run
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_dsi_communication_DSITransceiverThread__1run(JNIEnv *env, jobject thiz) {
    initDsiQueue();
    jclass cls = (*env)->GetObjectClass(env, thiz);
    jmethodID mid = (*env)->GetMethodID(env, cls, "startCommunication", "()V");
    if (mid) {
        (*env)->CallVoidMethod(env, thiz, mid);
    }
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        Sleep(50);
    }
}

/*
 * Class:     tsd_mibstd2_hmi_dsi_communication_DSITransceiverThread
 * Method:    _send
 * Signature: ([BI)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_dsi_communication_DSITransceiverThread__1send(JNIEnv *env, jobject thiz, jbyteArray data, jint len) {
    initDsiQueue();
    if (!data || len < 4) return;
    unsigned char buf[256];
    int copyLen = len > sizeof(buf) ? sizeof(buf) : len;
    (*env)->GetByteArrayRegion(env, data, 0, copyLen, (jbyte*)buf);

    int hmi2dsi = *(int*)(buf + 0);
    int hash = (copyLen >= 8) ? *(int*)(buf + 4) : 0;
    int methodId = (copyLen >= 12) ? *(int*)(buf + 8) : 0;
    int arg1 = (copyLen >= 16) ? *(int*)(buf + 12) : 0;
    int arg2 = (copyLen >= 20) ? *(int*)(buf + 16) : 0;

    printf("[JNI-DSI] _send: len=%d hmi2dsi=0x%08x hash=0x%08x method=%d arg1=%d arg2=%d\n", len, hmi2dsi, hash, methodId, arg1, arg2);
    fflush(stdout);

    // Method 1000: startDomain(domain, domainState) on DSIStartup
    if (hmi2dsi == 0x02048000 && methodId == 1000 && copyLen >= 20) {
        int domain = arg1;
        int state = arg2; // Echo back the EXACT requested domain state (e.g. 8) so domainState <= awaitedState succeeds
        int resp[5];
        resp[0] = 33851392;      // 0x02048800 (DSIStartupImpl event ID)
        resp[1] = -1505757193;   // 0xa63ff7f7 (version hash)
        resp[2] = domain;        // method ID in DSIStartupImpl (1=Root, 2=Tuner, etc.)
        resp[3] = state;         // domainState (exact requested state)
        resp[4] = 1;             // validFlag (1 = VALID! AslTargetStartup requires validFlag == 1)
        printf("[JNI-DSI] Queuing domain confirmation: domain=%d state=%d valid=1\n", domain, state);
        fflush(stdout);
        queueDsiPacket(resp, sizeof(resp));
    }

    // DSIPowerManagement (0x02040000) notification registration
    if (hmi2dsi == 0x02040000 && methodId == 0) {
        int resp[6];
        resp[0] = 33818624;      // 0x02040800 (DSIPowerManagementImpl event ID)
        resp[1] = -1505757196;   // 0xa63ff7f4 (version hash)
        resp[2] = 1;             // ATTR_POWERMANAGEMENTSTATE
        resp[3] = 1;             // powerState = 1 (POWERSTATE_ON)
        resp[4] = 0;             // reason = 0
        resp[5] = 1;             // validFlag = 1 (VALID)
        printf("[JNI-DSI] Queuing PowerManagement ON confirmation\n");
        fflush(stdout);
        queueDsiPacket(resp, sizeof(resp));
    }

    // DSIPersistence (0x0203b000) notification registration
    if (hmi2dsi == 0x0203b000 && methodId == 0) {
        int resp[5];
        resp[0] = 33798144;      // 0x0203b800 (DSIPersistenceImpl event ID)
        resp[1] = 1475447844;    // 0x57f17c24 (version hash)
        resp[2] = 1;             // ATTR_ACTIVESQLDATABASEMEDIUM
        resp[3] = 1;             // medium = 1 (MEDIUM_FLASH)
        resp[4] = 1;             // validFlag = 1 (VALID)
        printf("[JNI-DSI] Queuing Persistence MEDIUM_FLASH confirmation\n");
        fflush(stdout);
        queueDsiPacket(resp, sizeof(resp));
    }

    // DSIPersistence (0x0203b000) readInt (method 1001)
    if (hmi2dsi == 0x0203b000 && methodId == 1001 && copyLen >= 24) {
        int ns = *(int*)(buf + 12);
        long long key = *(long long*)(buf + 16);
        int resp[8];
        resp[0] = 33798144;      // 0x0203b800 (DSIPersistenceImpl event ID)
        resp[1] = 1475447844;    // 0x57f18c24 (version hash)
        resp[2] = 2001;          // RP_READINT
        resp[3] = ns;
        *(long long*)(resp + 4) = key;
        resp[6] = 0;             // val = 0
        resp[7] = 0;             // result = 0 (OK)
        printf("[JNI-DSI] Queuing Persistence readInt response for ns=%d\n", ns);
        fflush(stdout);
        queueDsiPacket(resp, sizeof(resp));
    }

    // DSIPersistence (0x0203b000) readBuffer (method 1003)
    if (hmi2dsi == 0x0203b000 && methodId == 1003 && copyLen >= 24) {
        int ns = *(int*)(buf + 12);
        long long key = *(long long*)(buf + 16);
        unsigned char resp[64];
        *(int*)(resp + 0) = 33798144;      // 0x0203b800 (DSIPersistenceImpl event ID)
        *(int*)(resp + 4) = 1475447844;    // 0x57f18c24 (version hash)
        *(int*)(resp + 8) = 2003;          // RP_READBUFFER
        *(int*)(resp + 12) = ns;
        *(long long*)(resp + 16) = key;
        resp[24] = 1;                      // flag = 1 (present)
        *(int*)(resp + 25) = 0;            // len = 0 bytes
        *(int*)(resp + 29) = 0;            // result = 0 (OK)
        printf("[JNI-DSI] Queuing Persistence readBuffer response for ns=%d, key=%lld\n", ns, key);
        fflush(stdout);
        queueDsiPacket(resp, 33);
    }

    // DSIDisplayManagement (0x02018000)
    if (hmi2dsi == 0x02018000) {
        if (methodId == 1001) { // RT_SWITCHCONTEXT (contextID, displayID, sessionID)
            int contextID = arg1;
            int displayID = arg2;
            int resp[6];
            resp[0] = 33654784;      // 0x02018800 (DSIDisplayManagementImpl event ID)
            resp[1] = -1505757162;   // 0xa63ff816 (version hash)
            resp[2] = 2001;          // RP_ACTIVECONTEXT
            resp[3] = contextID;     // contextID
            resp[4] = displayID;     // displayID
            resp[5] = 0;             // result = 0 (OK)
            printf("[JNI-DSI] Queuing DisplayManagement activeContext: ctx=%d disp=%d\n", contextID, displayID);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1005) { // RT_GETEXTENTS (displayableId)
            int dispId = arg1;
            int resp[6];
            resp[0] = 33654784;      // 0x02018800
            resp[1] = -1505757162;   // 0xa63ff816
            resp[2] = 2000;          // RP_GETEXTENTS
            resp[3] = dispId;
            resp[4] = 800;           // width
            resp[5] = 480;           // height
            printf("[JNI-DSI] Queuing DisplayManagement getExtents: id=%d -> 800x480\n", dispId);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1010) { // RT_GETDISPLAYPOWER
            int resp[5];
            resp[0] = 33654784;
            resp[1] = -1505757162;
            resp[2] = 2004;          // RP_GETDISPLAYPOWER
            resp[3] = arg1;
            resp[4] = 1;             // powerState = 1 (ON)
            printf("[JNI-DSI] Queuing DisplayManagement getDisplayPower: disp=%d\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1013) { // RT_GETBRIGHTNESS
            int resp[5];
            resp[0] = 33654784;
            resp[1] = -1505757162;
            resp[2] = 2005;          // RP_GETBRIGHTNESS
            resp[3] = arg1;
            resp[4] = 100;
            printf("[JNI-DSI] Queuing DisplayManagement getBrightness: cid=%d\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1015) { // RT_GETCONTRAST
            int resp[5];
            resp[0] = 33654784;
            resp[1] = -1505757162;
            resp[2] = 2006;          // RP_GETCONTRAST
            resp[3] = arg1;
            resp[4] = 100;
            printf("[JNI-DSI] Queuing DisplayManagement getContrast: cid=%d\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1017) { // RT_GETCOLOR
            int resp[5];
            resp[0] = 33654784;
            resp[1] = -1505757162;
            resp[2] = 2007;          // RP_GETCOLOR
            resp[3] = arg1;
            resp[4] = 100;
            printf("[JNI-DSI] Queuing DisplayManagement getColor: cid=%d\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1019) { // RT_GETTINT
            int resp[5];
            resp[0] = 33654784;
            resp[1] = -1505757162;
            resp[2] = 2008;          // RP_GETTINT
            resp[3] = arg1;
            resp[4] = 100;
            printf("[JNI-DSI] Queuing DisplayManagement getTint: cid=%d\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1022) { // RT_GETDISPLAYBRIGHTNESS
            int resp[5];
            resp[0] = 33654784;
            resp[1] = -1505757162;
            resp[2] = 2012;          // RP_GETDISPLAYBRIGHTNESS
            resp[3] = arg1;
            resp[4] = 100;
            printf("[JNI-DSI] Queuing DisplayManagement getDisplayBrightness: cid=%d\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }
}

/*
 * Class:     tsd_mibstd2_hmi_dsi_communication_DSIReceiverThread
 * Method:    _run
 * Signature: ([B[B)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_dsi_communication_DSIReceiverThread__1run(JNIEnv *env, jobject thiz, jbyteArray rx1, jbyteArray rx2) {
    initDsiQueue();
    jclass cls = (*env)->GetObjectClass(env, thiz);
    jmethodID midBufferReceived = (*env)->GetMethodID(env, cls, "bufferReceived", "([BI)[B");
    if (!midBufferReceived) {
        printf("[JNI-DSI] ERROR: bufferReceived method not found!\n");
        fflush(stdout);
        return;
    }
    jbyteArray currentRx = rx1;

    // Small initial delay so HMI finishes registering its DSI listeners
    Sleep(500);

    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        DWORD waitRes = WaitForSingleObject(g_dsiSem, 100);
        if (waitRes == WAIT_OBJECT_0) {
            DsiPacket pkt;
            EnterCriticalSection(&g_dsiCs);
            pkt = g_dsiQueue[g_dsiHead];
            g_dsiHead = (g_dsiHead + 1) % MAX_DSI_QUEUE;
            LeaveCriticalSection(&g_dsiCs);

            if (pkt.len > 0 && currentRx) {
                printf("[JNI-DSI] Delivering DSI packet: len=%d event=0x%08x method=%d state=%d\n",
                    pkt.len, *(int*)(pkt.data), *(int*)(pkt.data + 8), *(int*)(pkt.data + 12));
                fflush(stdout);
                (*env)->SetByteArrayRegion(env, currentRx, 0, pkt.len, (const jbyte*)pkt.data);
                jbyteArray nextRx = (jbyteArray)(*env)->CallObjectMethod(env, thiz, midBufferReceived, currentRx, pkt.len);
                if ((*env)->ExceptionCheck(env)) {
                    printf("[JNI-DSI] Exception in bufferReceived!\n");
                    (*env)->ExceptionDescribe(env);
                    (*env)->ExceptionClear(env);
                } else {
                    printf("[JNI-DSI] bufferReceived returned successfully\n");
                    fflush(stdout);
                }
                if (nextRx != NULL) {
                    currentRx = nextRx;
                }
            }
        }
    }
}

/* =========================================================================
 * 2. DebugSPI Logging
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_spi_debug_DebugSPIImpl
 * Method:    _osLogEvent
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_debug_DebugSPIImpl__1osLogEvent(JNIEnv *env, jobject thiz, jint level, jstring msg) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_debug_DebugSPIImpl
 * Method:    _osLogEventBinary
 * Signature: (I[BI)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_debug_DebugSPIImpl__1osLogEventBinary(JNIEnv *env, jobject thiz, jint level, jbyteArray data, jint len) {
}

/* =========================================================================
 * 3. TimerSPI
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_spi_timer_TimerSPIImpl
 * Method:    _getSystemTime
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_tsd_mibstd2_hmi_spi_timer_TimerSPIImpl__1getSystemTime(JNIEnv *env, jobject thiz) {
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (jlong)count.QuadPart;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_timer_TimerSPIImpl
 * Method:    _getSystemTimeFrequency
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_tsd_mibstd2_hmi_spi_timer_TimerSPIImpl__1getSystemTimeFrequency(JNIEnv *env, jobject thiz) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (jlong)freq.QuadPart;
}

/* =========================================================================
 * 4. FontSPI
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    initialize
 * Signature: (Ljava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl_initialize(JNIEnv *env, jclass clazz, jclass c1, jclass c2, jclass c3) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    uninitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl_uninitialize(JNIEnv *env, jclass clazz) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _isTextLayoutingAvailable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1isTextLayoutingAvailable(JNIEnv *env, jobject thiz) {
    return JNI_FALSE;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _openFont
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1openFont(JNIEnv *env, jobject thiz, jstring name) {
    return 1;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _closeFont
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1closeFont(JNIEnv *env, jobject thiz, jint id) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getAscender
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getAscender(JNIEnv *env, jobject thiz, jint id, jint size) {
    return 10;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getDescender
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getDescender(JNIEnv *env, jobject thiz, jint id, jint size) {
    return 2;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getHeight
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getHeight(JNIEnv *env, jobject thiz, jint id, jint size) {
    return 12;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getMaxAdvanceWith
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getMaxAdvanceWith(JNIEnv *env, jobject thiz, jint id, jint size) {
    return 12;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getKerning
 * Signature: (ICCI)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getKerning(JNIEnv *env, jobject thiz, jint id, jchar c1, jchar c2, jint size) {
    return 0;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _isGlyphAvailable
 * Signature: (IC)Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1isGlyphAvailable(JNIEnv *env, jobject thiz, jint id, jchar c) {
    return JNI_FALSE;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getControlBox
 * Signature: (ICI[I)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getControlBox(JNIEnv *env, jobject thiz, jint id, jchar c, jint size, jintArray box) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getFamilyName
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getFamilyName(JNIEnv *env, jobject thiz, jint id) {
    return (*env)->NewStringUTF(env, "VW-Sans");
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getStyleName
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getStyleName(JNIEnv *env, jobject thiz, jint id) {
    return (*env)->NewStringUTF(env, "Regular");
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getGlyph
 * Signature: (ICII)Lde/vw/mib/graphics/font/spi/NativeGlyph;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getGlyph(JNIEnv *env, jobject thiz, jint id, jchar c, jint size, jint flags) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getGlyphById
 * Signature: (IIII)Lde/vw/mib/graphics/font/spi/NativeGlyph;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getGlyphById(JNIEnv *env, jobject thiz, jint id, jint glyphId, jint size, jint flags) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _releaseGlyph
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1releaseGlyph(JNIEnv *env, jobject thiz, jobject buf) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getFontMetrics
 * Signature: (Lde/vw/mib/graphics/font/spi/style/GlyphStyle;)Lde/vw/mib/graphics/font/spi/FontMetrics;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getFontMetrics(JNIEnv *env, jobject thiz, jobject style) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _performTextLayout2
 * Signature: (IIIZCZC[BILjava/lang/String;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1performTextLayout2(JNIEnv *env, jobject thiz, jint w, jint h, jint maxLines, jboolean trunc, jchar truncCh, jboolean hyph, jchar hyphCh, jbyteArray styles, jint stylesLen, jstring text) {
    return NULL;
}

/* =========================================================================
 * 5. DisplaySPI
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_spi_display_DisplaySPIImpl
 * Method:    _getNativeDisplayableHandle
 * Signature: (II)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_display_DisplaySPIImpl__1getNativeDisplayableHandle(JNIEnv *env, jobject thiz, jint id, jint type) {
    printf("[JNI-Display] _getNativeDisplayableHandle(id=%d, type=%d) returning handle\n", id, type);
    fflush(stdout);
    jclass cls = (*env)->FindClass(env, "java/lang/Long");
    if (!cls) return NULL;
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "valueOf", "(J)Ljava/lang/Long;");
    if (!mid) return NULL;
    return (*env)->CallStaticObjectMethod(env, cls, mid, (jlong)1);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_display_DisplaySPIImpl
 * Method:    _getDisplayContent
 * Signature: (I)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_display_DisplaySPIImpl__1getDisplayContent(JNIEnv *env, jobject thiz, jint id) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_display_DisplaySPIImpl
 * Method:    _getDisplayableContent
 * Signature: (II)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_display_DisplaySPIImpl__1getDisplayableContent(JNIEnv *env, jobject thiz, jint id, jint type) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_display_DisplaySPIImpl
 * Method:    _getDisplayableContent2
 * Signature: (IIIIII)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_display_DisplaySPIImpl__1getDisplayableContent2(JNIEnv *env, jobject thiz, jint id, jint type, jint x, jint y, jint w, jint h) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_display_DisplaySPIImpl
 * Method:    _releaseContent
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_display_DisplaySPIImpl__1releaseContent(JNIEnv *env, jobject thiz, jobject buf) {
}

/* =========================================================================
 * 6. FileSPI
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_spi_file_FileSPIImpl
 * Method:    _openContentFile
 * Signature: (Ljava/lang/String;IZ)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_file_FileSPIImpl__1openContentFile(JNIEnv *env, jobject thiz, jstring path, jint flags, jboolean b) {
    return -1;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_file_FileSPIImpl
 * Method:    _closeContentFile
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_file_FileSPIImpl__1closeContentFile(JNIEnv *env, jobject thiz, jint fd) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_file_FileSPIImpl
 * Method:    _loadChunkFromContentFile
 * Signature: (IIIZ)Lde/vw/mib/file/spi/ContentChunk;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_file_FileSPIImpl__1loadChunkFromContentFile(JNIEnv *env, jobject thiz, jint fd, jint off, jint len, jboolean b) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_file_FileSPIImpl
 * Method:    _releaseByteBuffer
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_file_FileSPIImpl__1releaseByteBuffer(JNIEnv *env, jobject thiz, jobject buf) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_file_FileSPIImpl
 * Method:    _loadObjectArrayFromContentFile
 * Signature: (II)[Ljava/lang/Object;
 */
JNIEXPORT jobjectArray JNICALL Java_tsd_mibstd2_hmi_spi_file_FileSPIImpl__1loadObjectArrayFromContentFile(JNIEnv *env, jobject thiz, jint fd, jint idx) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_file_FileSPIImpl
 * Method:    _getFilteredTOCFromContentFile
 * Signature: (II)[I
 */
JNIEXPORT jintArray JNICALL Java_tsd_mibstd2_hmi_spi_file_FileSPIImpl__1getFilteredTOCFromContentFile(JNIEnv *env, jobject thiz, jint fd, jint filter) {
    return NULL;
}

/* =========================================================================
 * 7. ImageSPI
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _getExifFromResource
 * Signature: (Ljava/util/Map;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1getExifFromResource(JNIEnv *env, jobject thiz, jobject map, jstring path) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _getExifTagsFromResource
 * Signature: (Ljava/util/Map;Ljava/lang/String;I[I)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1getExifTagsFromResource(JNIEnv *env, jobject thiz, jobject map, jstring path, jint ifd, jintArray tags) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _getImageHeaderFromResource
 * Signature: (Ljava/lang/String;)Lde/vw/mib/graphics/image/spi/NativeImageHeader;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1getImageHeaderFromResource(JNIEnv *env, jobject thiz, jstring path) {
    return NULL;
}

static jobject create_fallback_native_image(JNIEnv *env, int w, int h) {
    jclass nativeImgCls = (*env)->FindClass(env, "de/vw/mib/graphics/image/spi/NativeImage");
    if (!nativeImgCls) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, nativeImgCls, "<init>", "(Ljava/nio/ByteBuffer;IIIZ)V");
    if (!ctor) return NULL;
    int size = w * h * 4;
    void *pixels = malloc(size);
    if (!pixels) return NULL;
    unsigned char *p = (unsigned char *)pixels;
    for (int i = 0; i < w * h; i++) {
        p[i*4 + 0] = 0x20;
        p[i*4 + 1] = 0x24;
        p[i*4 + 2] = 0x28;
        p[i*4 + 3] = 0xff;
    }
    jobject byteBuf = (*env)->NewDirectByteBuffer(env, pixels, size);
    return (*env)->NewObject(env, nativeImgCls, ctor, byteBuf, (jint)w, (jint)h, (jint)4356, JNI_TRUE);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromResource
 * Signature: (Ljava/lang/String;ILde/vw/mib/graphics/image/spi/NativeImageLoadingListener;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromResource(JNIEnv *env, jobject thiz, jstring path, jint scale, jobject listener) {
    return create_fallback_native_image(env, 16, 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromSkinById
 * Signature: (II)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromSkinById(JNIEnv *env, jobject thiz, jint skinId, jint imageId) {
    return create_fallback_native_image(env, 16, 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromSkinByFilename
 * Signature: (Ljava/lang/String;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromSkinByFilename(JNIEnv *env, jobject thiz, jstring filename) {
    return create_fallback_native_image(env, 16, 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageRegionFromResource
 * Signature: (Ljava/lang/String;IIIILde/vw/mib/graphics/image/spi/NativeImageLoadingListener;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageRegionFromResource(JNIEnv *env, jobject thiz, jstring path, jint x, jint y, jint w, jint h, jobject listener) {
    return create_fallback_native_image(env, w > 0 ? w : 16, h > 0 ? h : 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadThumbnailImageFromResource
 * Signature: (Ljava/lang/String;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadThumbnailImageFromResource(JNIEnv *env, jobject thiz, jstring path) {
    return create_fallback_native_image(env, 16, 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _releaseImage
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1releaseImage(JNIEnv *env, jobject thiz, jobject buf) {
    if (buf) {
        void *addr = (*env)->GetDirectBufferAddress(env, buf);
        if (addr) free(addr);
    }
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _saveImageToFile
 * Signature: (Ljava/lang/String;ILde/vw/mib/graphics/image/spi/NativeImage;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1saveImageToFile(JNIEnv *env, jobject thiz, jstring path, jint format, jobject img) {
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _setMultiImageFileForSkin
 * Signature: (ILjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1setMultiImageFileForSkin(JNIEnv *env, jobject thiz, jint skinId, jstring path) {
    return JNI_TRUE;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _setContentFileForSkin
 * Signature: (ILjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1setContentFileForSkin(JNIEnv *env, jobject thiz, jint skinId, jstring path) {
    return JNI_TRUE;
}

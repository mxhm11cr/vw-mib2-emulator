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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static volatile LONG g_running = 1;
static CRITICAL_SECTION g_dsiCs;
static HANDLE g_dsiSem = NULL;
static CRITICAL_SECTION g_winInputCs;
#define DSI_MUTEX_INIT(cs) InitializeCriticalSection(&(cs))
#define DSI_MUTEX_LOCK(cs) EnterCriticalSection(&(cs))
#define DSI_MUTEX_UNLOCK(cs) LeaveCriticalSection(&(cs))
#define DSI_SEM_POST(sem) ReleaseSemaphore(sem, 1, NULL)
#define DSI_SLEEP(ms) Sleep(ms)
#define STRTOUI64(s, p, b) _strtoui64(s, p, b)
#define EXPORT_SYM __declspec(dllexport)
#else
#define _GNU_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>
static volatile int g_running = 1;
static pthread_mutex_t g_dsiCs = PTHREAD_MUTEX_INITIALIZER;
static sem_t g_dsiSem;
static pthread_mutex_t g_winInputCs = PTHREAD_MUTEX_INITIALIZER;
#define DSI_MUTEX_INIT(cs) pthread_mutex_init(&(cs), NULL)
#define DSI_MUTEX_LOCK(cs) pthread_mutex_lock(&(cs))
#define DSI_MUTEX_UNLOCK(cs) pthread_mutex_unlock(&(cs))
#define DSI_SEM_POST(sem) sem_post(&(sem))
#define DSI_SLEEP(ms) usleep((ms) * 1000)
#define STRTOUI64(s, p, b) strtoull(s, p, b)
#define EXPORT_SYM __attribute__((visibility("default")))
typedef void* HMODULE;
typedef void* HDC;
typedef void* HFONT;
typedef void* HGDIOBJ;
typedef void* HBITMAP;
typedef void* HBRUSH;
#endif

#define MAX_DSI_QUEUE 256
#define MAX_DSI_PACKET_SIZE 16384
typedef struct {
    int len;
    unsigned char data[MAX_DSI_PACKET_SIZE];
} DsiPacket;

static DsiPacket g_dsiQueue[MAX_DSI_QUEUE];
static int g_dsiHead = 0;
static int g_dsiTail = 0;
static int g_dsiInited = 0;

static void queueDsiPacket(const void* data, int len);

static void initDsiQueue(void) {
    if (!g_dsiInited) {
#ifdef _WIN32
        InitializeCriticalSection(&g_dsiCs);
        g_dsiSem = CreateSemaphore(NULL, 0, 1000, NULL);
#else
        pthread_mutex_init(&g_dsiCs, NULL);
        sem_init(&g_dsiSem, 0, 0);
#endif
        g_dsiInited = 1;
    }
}

static int b64_char_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static int b64_decode(const char *in, unsigned char *out, int maxOut) {
    int len = 0;
    int val = 0, valb = -8;
    for (const char *p = in; *p; p++) {
        if (*p == '=') break;
        if (*p == '\\' && *(p+1) == '=') break;
        if (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') continue;
        int c = b64_char_val(*p);
        if (c < 0) continue;
        val = (val << 6) | c;
        valb += 6;
        if (valb >= 0) {
            if (len < maxOut) out[len++] = (unsigned char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return len;
}

#define MAX_PERSIST_ENTRIES 256
typedef struct {
    int ns;
    long long key;
    int type; // 1 = buf, 2 = int, 3 = str
    int intVal;
    int bufLen;
    unsigned char bufVal[512];
    char strVal[128];
} PersistEntry;

static PersistEntry g_persistTable[MAX_PERSIST_ENTRIES];
static int g_persistCount = 0;
static int g_persistLoaded = 0;

static void addPersistBuf(int ns, long long key, const unsigned char *buf, int len) {
    for (int i = 0; i < g_persistCount; i++) {
        if (g_persistTable[i].ns == ns && g_persistTable[i].key == key && g_persistTable[i].type == 1) {
            g_persistTable[i].bufLen = len > 512 ? 512 : len;
            memcpy(g_persistTable[i].bufVal, buf, g_persistTable[i].bufLen);
            return;
        }
    }
    if (g_persistCount < MAX_PERSIST_ENTRIES) {
        g_persistTable[g_persistCount].ns = ns;
        g_persistTable[g_persistCount].key = key;
        g_persistTable[g_persistCount].type = 1;
        g_persistTable[g_persistCount].bufLen = len > 512 ? 512 : len;
        memcpy(g_persistTable[g_persistCount].bufVal, buf, g_persistTable[g_persistCount].bufLen);
        g_persistCount++;
    }
}

static void addPersistInt(int ns, long long key, int val) {
    for (int i = 0; i < g_persistCount; i++) {
        if (g_persistTable[i].ns == ns && g_persistTable[i].key == key && g_persistTable[i].type == 2) {
            g_persistTable[i].intVal = val;
            return;
        }
    }
    if (g_persistCount < MAX_PERSIST_ENTRIES) {
        g_persistTable[g_persistCount].ns = ns;
        g_persistTable[g_persistCount].key = key;
        g_persistTable[g_persistCount].type = 2;
        g_persistTable[g_persistCount].intVal = val;
        g_persistCount++;
    }
}

static void initDefaultPersistence(void) {
    if (g_persistLoaded) return;
    g_persistLoaded = 1;

    // 1. Long coding (ns 28180695, key 1, 25 bytes)
    static const unsigned char longCoding[25] = {
        0x03, 0x84, 0x01, 0x01, 0xff, 0x00, 0x00, 0x00, 0x11, 0x11,
        0x00, 0x04, 0x00, 0x88, 0x00, 0x00, 0x2f, 0x01, 0x01, 0xc4,
        0x01, 0x20, 0x01, 0x00, 0xce
    };
    addPersistBuf(28180695, 1, longCoding, sizeof(longCoding));

    // 2. Adaptations (ns 28442848, key 100, 50 bytes)
    static const unsigned char adapt100[50] = {
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x26, 0x08, 0x14, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x26, 0x10, 0x05, 0x00, 0x01,
        0xf8, 0x3f, 0x1e, 0xd6, 0x16, 0x11, 0x23, 0xaf, 0xf6, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x26,
        0x08, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x00
    };
    addPersistBuf(28442848, 100, adapt100, sizeof(adapt100));

    // 3. Car function adaptations (ns 28442848, key 101, 60 bytes)
    static const unsigned char carFuncAdap101[60] = {
        0x0d, 0x0d, 0x1f, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x0d, 0x05, 0x05, 0x0d, 0x1f, 0x05,
        0x05, 0x04, 0x04, 0x05, 0x04, 0x04, 0x04, 0x05, 0x04, 0x04,
        0x05, 0x04, 0x04, 0x05, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x04, 0x1d,
        0x14, 0x00, 0x0d, 0x0d, 0x00, 0x01, 0x00, 0x0d, 0x00, 0x0d
    };
    addPersistBuf(28442848, 101, carFuncAdap101, sizeof(carFuncAdap101));

    // 4. Car function list CAN (ns 28442848, key 102, 8 bytes)
    static const unsigned char canList102[8] = {
        0x21, 0x00, 0x11, 0x61, 0x01, 0xf0, 0x00, 0x00
    };
    addPersistBuf(28442848, 102, canList102, sizeof(canList102));

    // 5. Car function list BAP (ns 28442848, key 103, 64 bytes)
    static const unsigned char bapList103[64] = {
        0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x01, 0x01,
        0x01, 0x08, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x18, 0x08, 0x00, 0x09, 0x3c, 0x0d, 0x0d, 0x01, 0x00, 0x04,
        0x05, 0x04, 0x00, 0x0c, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x3c, 0x08, 0x18, 0x00, 0x00, 0x08, 0x0d, 0x00,
        0x0c, 0x00, 0x00, 0x00
    };
    addPersistBuf(28442848, 103, bapList103, sizeof(bapList103));

    // 6. Dashboard display config (ns 28442848, key 106, 10 bytes)
    static const unsigned char dispConfig106[10] = {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    addPersistBuf(28442848, 106, dispConfig106, sizeof(dispConfig106));

    // 7. Language (ns 1101, key 10, 21 bytes "en_GB               ")
    static const unsigned char lang1101[21] = "en_GB               ";
    addPersistBuf(1101, 10, lang1101, 21);

    // 8. Integer constants
    addPersistInt(29688038, 166, 1);
    addPersistInt(52166966, 203, 1);
    addPersistInt(52166966, 204, 0);

    FILE *fp = fopen("persistence.properties", "r");
    if (!fp) fp = fopen("firmware/analysis/p0369/runtime/persistence.properties", "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') continue;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *valStr = eq + 1;
            char *nl = strchr(valStr, '\r'); if (nl) *nl = '\0';
            nl = strchr(valStr, '\n'); if (nl) *nl = '\0';
            char *p1 = strchr(line, '.');
            if (!p1) continue;
            *p1 = '\0';
            char *p2 = strchr(p1 + 1, '.');
            if (!p2) continue;
            *p2 = '\0';
            int ns = atoi(line);
            long long k = STRTOUI64(p1 + 1, NULL, 16);
            char *type = p2 + 1;
            if (strcmp(type, "buf") == 0) {
                unsigned char decBuf[512];
                int dlen = b64_decode(valStr, decBuf, sizeof(decBuf));
                if (dlen > 0) addPersistBuf(ns, k, decBuf, dlen);
            } else if (strcmp(type, "int") == 0) {
                addPersistInt(ns, k, atoi(valStr));
            }
        }
        fclose(fp);
        printf("[JNI-DSI] Loaded %d entries from persistence.properties\n", g_persistCount);
        fflush(stdout);
    }
}

static void sendPersistenceBufferResponse(int ns, long long key, const unsigned char* data, int len) {
    if (!data || len <= 0) {
        unsigned char resp[32];
        *(int*)(resp + 0) = 33798144;      // 0x0203b800 (DSIPersistenceImpl event ID)
        *(int*)(resp + 4) = 1475447844;    // 0x57f17c24 (version hash)
        *(int*)(resp + 8) = 2003;          // RP_READBUFFER
        *(int*)(resp + 12) = ns;
        *(long long*)(resp + 16) = key;
        resp[24] = 0;                      // flag = 0 (IS_NULL)
        *(int*)(resp + 25) = 2;            // result = 2 (INVALID_KEY)
        queueDsiPacket(resp, 29);
        return;
    }
    int totalPktLen = 29 + len * 4 + 4; // 33 + len * 4
    if (totalPktLen > MAX_DSI_PACKET_SIZE) return;
    unsigned char resp[MAX_DSI_PACKET_SIZE];
    *(int*)(resp + 0) = 33798144;      // 0x0203b800
    *(int*)(resp + 4) = 1475447844;    // 0x57f17c24
    *(int*)(resp + 8) = 2003;          // RP_READBUFFER
    *(int*)(resp + 12) = ns;
    *(long long*)(resp + 16) = key;
    resp[24] = 1;                      // flag = 1 (IS_NOT_NULL)
    *(int*)(resp + 25) = len;          // arraylength
    int pos = 29;
    for (int i = 0; i < len; i++) {
        *(int*)(resp + pos) = (int)(data[i] & 0xff);
        pos += 4;
    }
    *(int*)(resp + pos) = 0;           // result = 0 (OK)
    pos += 4;
    printf("[JNI-DSI] Queuing Persistence readBuffer response for ns=%d, key=%lld, len=%d (packet=%d bytes)\n", ns, key, len, pos);
    fflush(stdout);
    queueDsiPacket(resp, pos);
}

static void sendPersistenceIntResponse(int ns, long long key, int val, int found) {
    int resp[8];
    resp[0] = 33798144;      // 0x0203b800
    resp[1] = 1475447844;    // 0x57f17c24
    resp[2] = 2001;          // RP_READINT
    resp[3] = ns;
    *(long long*)(resp + 4) = key;
    resp[6] = found ? val : 0;
    resp[7] = found ? 0 : 2; // result = 0 (OK) or 2 (INVALID_KEY)
    printf("[JNI-DSI] Queuing Persistence readInt response for ns=%d, key=%lld, val=%d (found=%d)\n", ns, key, val, found);
    fflush(stdout);
    queueDsiPacket(resp, sizeof(resp));
}

static void sendPersistenceWriteResponse(int methodId, int ns, long long key) {
    int resp[7];
    resp[0] = 33798144;      // 0x0203b800
    resp[1] = 1475447844;    // 0x57f17c24
    resp[2] = methodId + 1000;
    resp[3] = ns;
    *(long long*)(resp + 4) = key;
    resp[6] = 0;             // result = OK
    queueDsiPacket(resp, sizeof(resp));
}

/* =========================================================================
 * Asynchronous Ring Buffer for Win32 GUI Message Loop Events
 * ========================================================================= */
typedef struct {
    int type;     // 1=HardKey, 2=Touch, 3=Rotary
    int p1;       // HK: pressedId / Touch: gesture / Rotary: steps
    int p2;       // HK: state (1/0) / Touch: x
    int p3;       // HK: releasedId / Touch: y
} WinInputEvent;

#define MAX_WIN_INPUT_EVENTS 512
static WinInputEvent g_winInputQueue[MAX_WIN_INPUT_EVENTS];
static int g_winInputHead = 0;
static int g_winInputTail = 0;
static int g_winInputCsInit = 0;

static void initWinInputQueue(void) {
    if (!g_winInputCsInit) {
        DSI_MUTEX_INIT(g_winInputCs);
        g_winInputCsInit = 1;
    }
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL; (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        initDsiQueue();
        initWinInputQueue();
        initDefaultPersistence();
    }
    return TRUE;
}
#else
__attribute__((constructor)) static void lib_dsi_init(void) {
    initDsiQueue();
    initWinInputQueue();
    initDefaultPersistence();
}
#endif

static void queueDsiPacket(const void* data, int len) {
    initDsiQueue();
    if (len <= 0 || len > MAX_DSI_PACKET_SIZE) return;
    DSI_MUTEX_LOCK(g_dsiCs);
    int nextTail = (g_dsiTail + 1) % MAX_DSI_QUEUE;
    if (nextTail != g_dsiHead) {
        g_dsiQueue[g_dsiTail].len = len;
        memcpy(g_dsiQueue[g_dsiTail].data, data, len);
        g_dsiTail = nextTail;
        DSI_SEM_POST(g_dsiSem);
    }
    DSI_MUTEX_UNLOCK(g_dsiCs);
}

static JavaVM *g_jvm = NULL;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)reserved;
    g_jvm = vm;
    printf("[JNI-DSI] JNI_OnLoad: JavaVM=%p registered\n", (void*)vm);
    fflush(stdout);
    return JNI_VERSION_1_8;
}

static void enqueueWinInputEvent(int type, int p1, int p2, int p3) {
    initWinInputQueue();
    DSI_MUTEX_LOCK(g_winInputCs);
    int nextTail = (g_winInputTail + 1) % MAX_WIN_INPUT_EVENTS;
    if (nextTail != g_winInputHead) {
        g_winInputQueue[g_winInputTail].type = type;
        g_winInputQueue[g_winInputTail].p1 = p1;
        g_winInputQueue[g_winInputTail].p2 = p2;
        g_winInputQueue[g_winInputTail].p3 = p3;
        g_winInputTail = nextTail;
    }
    DSI_MUTEX_UNLOCK(g_winInputCs);
}

JNIEXPORT jint JNICALL Java_LoadMIBMain_pollInputEvent(JNIEnv *env, jclass cls, jintArray outArr) {
    (void)cls;
    initWinInputQueue();
    DSI_MUTEX_LOCK(g_winInputCs);
    if (g_winInputHead == g_winInputTail) {
        DSI_MUTEX_UNLOCK(g_winInputCs);
        return 0; // empty
    }
    WinInputEvent evt = g_winInputQueue[g_winInputHead];
    g_winInputHead = (g_winInputHead + 1) % MAX_WIN_INPUT_EVENTS;
    DSI_MUTEX_UNLOCK(g_winInputCs);

    jint buf[4];
    buf[0] = evt.type;
    buf[1] = evt.p1;
    buf[2] = evt.p2;
    buf[3] = evt.p3;
    (*env)->SetIntArrayRegion(env, outArr, 0, 4, buf);
    return 1;
}

EXPORT_SYM void SendTouchGesture(int gesture, int x, int y) {
    enqueueWinInputEvent(2, gesture, x, y);
}

EXPORT_SYM void SendHardKey(int code, int pressed) {
    int hkEventId = 0, releasedId = 0;
    switch (code) {
        case 6:  hkEventId = pressed ? 303 : 304; releasedId = 304; break;     // HK_CAR_PRESSED / RELEASED
        case 15: hkEventId = pressed ? 282 : 281; releasedId = 281; break;     // HK_RADIO_PRESSED / RELEASED
        case 1:  hkEventId = pressed ? 326 : 325; releasedId = 325; break;     // HK_MEDIA_PRESSED / RELEASED
        case 4:  hkEventId = pressed ? 361 : 360; releasedId = 360; break;     // HK_NAV_PRESSED / RELEASED
        case 78: hkEventId = pressed ? 320 : 319; releasedId = 319; break;     // HK_MENU_PRESSED / RELEASED
        case 7:  hkEventId = pressed ? 277 : 276; releasedId = 276; break;     // HK_SETUP_PRESSED / RELEASED
        case 3:  hkEventId = pressed ? 341 : 342; releasedId = 342; break;     // HK_PHONE_PRESSED / RELEASED
        case 16: hkEventId = pressed ? 286 : 287; releasedId = 287; break;     // DDS_EVENT_PRESSED / RELEASED
        case 10: hkEventId = pressed ? 346 : 347; releasedId = 347; break;     // HK_PREV_PRESSED / RELEASED
        case 14: hkEventId = pressed ? 336 : 337; releasedId = 337; break;     // HK_NEXT_PRESSED / RELEASED
        case 13: hkEventId = pressed ? 320 : 319; releasedId = 319; break;     // fallback to MENU
        case 79: hkEventId = pressed ? 306 : 269; releasedId = 269; break;     // HK_CLIMATE_PRESSED / RELEASED
        case 114:hkEventId = pressed ? 2110065 : 2110066; releasedId = 2110066; break; // HK_APP_PRESSED / RELEASED
        case 5:  hkEventId = pressed ? 263 : 264; releasedId = 264; break;     // HK_TRAFFIC_PRESSED / RELEASED
    }
    if (pressed) {
        if (code == 6 || code == 15 || code == 1 || code == 4 || code == 78 || code == 7 || code == 3 || code == 79 || code == 114 || code == 5) {
#ifdef _WIN32
            HMODULE hGl = GetModuleHandleA("tsd.opengl.jsr231.native.dll");
            if (hGl) {
                void (*pClear)(void) = (void (*)(void))GetProcAddress(hGl, "requestScreenClear");
                if (pClear) pClear();
            }
#else
            void *hGl = dlopen("libtsd.opengl.jsr231.native.so", RTLD_NOLOAD | RTLD_LAZY);
            if (!hGl) hGl = dlopen("libtsd.opengl.jsr231.native.so", RTLD_LAZY);
            if (!hGl) hGl = dlopen("./libtsd.opengl.jsr231.native.so", RTLD_LAZY);
            if (hGl) {
                void (*pClear)(void) = (void (*)(void))dlsym(hGl, "requestScreenClear");
                if (pClear) pClear();
            }
#endif
        }
        if (hkEventId > 0) {
            enqueueWinInputEvent(1, hkEventId, 1, releasedId);
        }
    }
}

EXPORT_SYM void SendRotaryKnob(int steps) {
    if (steps == 0) return;
    enqueueWinInputEvent(3, steps, 0, 0);
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
    if (!g_jvm) (*env)->GetJavaVM(env, &g_jvm);
    initDsiQueue();
    jclass cls = (*env)->GetObjectClass(env, thiz);
    jmethodID mid = (*env)->GetMethodID(env, cls, "startCommunication", "()V");
    if (mid) {
        (*env)->CallVoidMethod(env, thiz, mid);
    }
#ifdef _WIN32
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        Sleep(50);
    }
#else
    while (g_running) {
        usleep(50000);
    }
#endif
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

    if (hmi2dsi == 0x02048000 || hmi2dsi == 0x0203b000) {
        printf("[JNI-DSI] _send: len=%d hmi2dsi=0x%08x hash=0x%08x method=%d arg1=%d arg2=%d\n", len, hmi2dsi, hash, methodId, arg1, arg2);
        fflush(stdout);
    }

    // Method 1000: startDomain(domain, domainState) on DSIStartup
    if (hmi2dsi == 0x02048000 && methodId == 1000 && copyLen >= 20) {
        int domain = arg1;
        int state = arg2;

        // 1. Send RP_STARTDOMAIN (method = 2000, arg0 = domain, arg1 = state)
        int resp1[5];
        resp1[0] = 33851392;      // 0x02048800 (DSIStartupImpl event ID)
        resp1[1] = -1505757193;   // 0xa63ff7f7 (version hash)
        resp1[2] = 2000;          // RP_STARTDOMAIN in DSIStartupImpl
        resp1[3] = domain;        // arg0 = domain
        resp1[4] = state;         // arg1 = domainState
        queueDsiPacket(resp1, sizeof(resp1));

        // 2. Send ATTR_DOMAINSTATUS<domain> (method = attrId, arg0 = state, arg1 = 1)
        // Mapped from org.dsi.ifc.startup.DSIStartup constants
        int attrId;
        switch (domain) {
            case 1:  attrId = 1;  break; // ROOT
            case 2:  attrId = 2;  break; // TUNER
            case 3:  attrId = 3;  break; // MEDIA
            case 4:  attrId = 4;  break; // ADDRESSBOOK
            case 5:  attrId = 5;  break; // PHONE
            case 6:  attrId = 6;  break; // NAV
            case 7:  attrId = 7;  break; // INFO
            case 8:  attrId = 8;  break; // CAR
            case 9:  attrId = 9;  break; // AUDIO
            case 10: attrId = 10; break; // SDS
            case 11: attrId = 11; break; // SWDL
            case 12: attrId = 12; break; // EARLY_APPS
            case 13: attrId = 13; break; // POST / POSTSTARTUP
            case 14: attrId = 14; break; // COMMUNICATION
            case 16: attrId = 15; break; // IPSERVICES (note: domain 15 skipped)
            case 17: attrId = 16; break; // GEMMI
            case 18: attrId = 17; break; // BAPKOMBI
            case 19: attrId = 18; break; // BLUETOOTH
            case 20: attrId = 19; break; // BROWSER
            case 21: attrId = 20; break; // EXPLORER
            case 22: attrId = 21; break; // CALENDAR
            case 23: attrId = 22; break; // PICTURESTORE
            case 24: attrId = 23; break; // STREETVIEW
            case 25: attrId = 24; break; // MOBILITYHORIZON
            case 26: attrId = 25; break; // EXBOXM
            case 27: attrId = 26; break; // MIRRORLINK
            case 28: attrId = 27; break; // SFA
            case 29: attrId = 28; break; // SEARCH
            case 30: attrId = 29; break; // DIAGNOSIS
            case 31: attrId = 30; break; // ASIA_LANGUAGE_SUPPORT
            case 32: attrId = 31; break; // EXLAP
            case 33: attrId = 32; break; // TVTUNER
            case 34: attrId = 33; break; // MEDIA_ONLINE
            case 35: attrId = 34; break; // MEDIA_ROUTER
            case 36: attrId = 35; break; // RADIODATA_SERVER
            case 37: attrId = 36; break; // SMARTPHONE_INTEGRATION
            case 38: attrId = 37; break; // WIRELESS_CHARGER
            default: attrId = (domain >= 16) ? (domain - 1) : domain; break;
        }

        int resp2[5];
        resp2[0] = 33851392;      // 0x02048800
        resp2[1] = -1505757193;   // 0xa63ff7f7
        resp2[2] = attrId;        // attribute event ID
        resp2[3] = state;         // arg0 = domainState
        resp2[4] = 1;             // arg1 = validFlag (VALID)
        printf("[JNI-DSI] Queuing domain confirmations: domain=%d (attr=%d) state=%d\n", domain, attrId, state);
        fflush(stdout);
        queueDsiPacket(resp2, sizeof(resp2));
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

    // DSIPersistence (0x0203b000)
    if (hmi2dsi == 0x0203b000) {
        initDefaultPersistence();
        if (methodId == 0) {
            int resp[5];
            resp[0] = 33798144;      // 0x0203b800 (DSIPersistenceImpl event ID)
            resp[1] = 1475447844;    // 0x57f17c24 (version hash)
            resp[2] = 1;             // ATTR_ACTIVESQLDATABASEMEDIUM
            resp[3] = 1;             // medium = 1 (MEDIUM_FLASH)
            resp[4] = 1;             // validFlag = 1 (VALID)
            printf("[JNI-DSI] Queuing Persistence MEDIUM_FLASH confirmation\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if ((methodId == 1001 || methodId == 1019) && copyLen >= 24) {
            int ns = *(int*)(buf + 12);
            long long key = *(long long*)(buf + 16);
            int found = 0, val = 0;
            for (int i = 0; i < g_persistCount; i++) {
                if (g_persistTable[i].ns == ns && g_persistTable[i].key == key && g_persistTable[i].type == 2) {
                    val = g_persistTable[i].intVal;
                    found = 1;
                    break;
                }
            }
            sendPersistenceIntResponse(ns, key, val, found);
        } else if ((methodId == 1003 || methodId == 1020) && copyLen >= 24) {
            int ns = *(int*)(buf + 12);
            long long key = *(long long*)(buf + 16);
            const unsigned char *b = NULL;
            int blen = 0;
            for (int i = 0; i < g_persistCount; i++) {
                if (g_persistTable[i].ns == ns && g_persistTable[i].key == key && g_persistTable[i].type == 1) {
                    b = g_persistTable[i].bufVal;
                    blen = g_persistTable[i].bufLen;
                    break;
                }
            }
            sendPersistenceBufferResponse(ns, key, b, blen);
        } else if (methodId == 1005 || methodId == 1021) {
            int ns = *(int*)(buf + 12);
            long long key = *(long long*)(buf + 16);
            // String: not found -> INVALID_KEY
            unsigned char resp[32];
            *(int*)(resp + 0) = 33798144;
            *(int*)(resp + 4) = 1475447844;
            *(int*)(resp + 8) = 2005; // RP_READSTRING
            *(int*)(resp + 12) = ns;
            *(long long*)(resp + 16) = key;
            resp[24] = 0; // null string
            *(int*)(resp + 25) = 2; // INVALID_KEY
            queueDsiPacket(resp, 29);
        } else if (methodId == 1000 || methodId == 1002 || methodId == 1004 || methodId == 1006 || methodId == 1008) {
            int ns = (copyLen >= 16) ? *(int*)(buf + 12) : 0;
            long long key = (copyLen >= 24) ? *(long long*)(buf + 16) : 0;
            sendPersistenceWriteResponse(methodId, ns, key);
        }
    }

    // DSIAudioManagement (0x02002000, hash = 0xa63ff850)
    if (hmi2dsi == 0x02002000) {
        if (methodId == 0) {
            int resp[6];
            resp[0] = 33564672;      // 0x02002800 (DSIAudioManagementImpl event ID)
            resp[1] = (int)0xa63ff850;
            resp[2] = 1;             // ATTR_AMAVAILABLE
            resp[3] = 3;             // AM_AVAILABLE = 3
            resp[4] = 0;             // reason = 0
            resp[5] = 1;             // validFlag = 1
            printf("[JNI-DSI] Queuing DSIAudioManagement ATTR_AMAVAILABLE (AVAILABLE)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1004 && copyLen >= 16) { // RT_REQUEST
            int con = arg1;
            printf("[JNI-DSI] Audio RT_REQUEST connection=%d\n", con);
            fflush(stdout);

            int resp1[5];
            resp1[0] = 33564672;
            resp1[1] = (int)0xa63ff850;
            resp1[2] = 2003;         // RP_STARTCONNECTION
            resp1[3] = con;
            resp1[4] = 1;            // result OK
            queueDsiPacket(resp1, sizeof(resp1));

            int resp2[6];
            resp2[0] = 33564672;
            resp2[1] = (int)0xa63ff850;
            resp2[2] = 2;            // ATTR_ACTIVECONNECTION
            resp2[3] = con;
            resp2[4] = 0;
            resp2[5] = 1;
            queueDsiPacket(resp2, sizeof(resp2));

            int resp3[6];
            resp3[0] = 33564672;
            resp3[1] = (int)0xa63ff850;
            resp3[2] = 3;            // ATTR_ACTIVEENTERTAINMENTCONNECTION
            resp3[3] = con;
            resp3[4] = 0;
            resp3[5] = 1;
            queueDsiPacket(resp3, sizeof(resp3));

            int resp4[5];
            resp4[0] = 33564672;
            resp4[1] = (int)0xa63ff850;
            resp4[2] = 2001;         // RP_FADEDIN
            resp4[3] = con;
            resp4[4] = 1;            // result OK
            queueDsiPacket(resp4, sizeof(resp4));
        } else if (methodId == 1000 && copyLen >= 16) { // RT_FADETO
            int con = arg1;
            int resp[5];
            resp[0] = 33564672;
            resp[1] = (int)0xa63ff850;
            resp[2] = 2001;          // RP_FADEDIN
            resp[3] = con;
            resp[4] = 1;
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1001 && copyLen >= 16) { // RT_RELEASE
            int con = arg1;
            int resp[5];
            resp[0] = 33564672;
            resp[1] = (int)0xa63ff850;
            resp[2] = 2004;          // RP_STOPCONNECTION
            resp[3] = con;
            resp[4] = 1;
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIMapViewerControl (0x02020000, hash = 0xa63ff88d)
    if (hmi2dsi == 0x02020000) {
        if (methodId == 0) {
            int resp1[5];
            resp1[0] = 33687552;      // 0x02020800
            resp1[1] = (int)0xa63ff88d;
            resp1[2] = 1;             // ATTR_READY
            resp1[3] = 1;             // ready = true
            resp1[4] = 1;             // validFlag = 1
            printf("[JNI-DSI] Queuing DSIMapViewerControl ATTR_READY\n");
            fflush(stdout);
            queueDsiPacket(resp1, sizeof(resp1));

            int resp2[5];
            resp2[0] = 33687552;
            resp2[1] = (int)0xa63ff88d;
            resp2[2] = 8;             // ATTR_MAPMODE
            resp2[3] = 1;             // mode = 1 (POSITIONMAP)
            resp2[4] = 1;
            queueDsiPacket(resp2, sizeof(resp2));
        }
    }

    // DSISound (0x02003000, hash = 0xa63ff850)
    if (hmi2dsi == 0x02003000) {
        if (methodId == 0) {
            int resp[6];
            resp[0] = 33568768;      // 0x02003800
            resp[1] = (int)0xa63ff850;
            resp[2] = 56;            // ATTR_PROFILESTATE
            resp[3] = 2;             // OPERATION_READY
            resp[4] = 1;             // profileId
            resp[5] = 1;             // validFlag
            printf("[JNI-DSI] Queuing DSISound ATTR_PROFILESTATE (READY)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1088) { // RT_PROFILECHANGE
            int resp[5];
            resp[0] = 33568768;
            resp[1] = (int)0xa63ff850;
            resp[2] = 2008;          // RP_PROFILECHANGED
            resp[3] = arg1;          // profile
            resp[4] = 0;             // result = OK
            printf("[JNI-DSI] Queuing DSISound RP_PROFILECHANGED (profile=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1024) { // RT_GETINPUTGAINOFFSETRANGE -> RP_INPUTGAINOFFSETRANGE
            int resp[7] = { 33568768, (int)0xa63ff850, 2000, arg1, -6, 6, 1 };
            printf("[JNI-DSI] Queuing DSISound RP_INPUTGAINOFFSETRANGE (device=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1027) { // RT_GETMENUVOLENTRANGE -> RP_MENUVOLENTRANGE
            int resp[6] = { 33568768, (int)0xa63ff850, 2001, 0, 100, 1 };
            printf("[JNI-DSI] Queuing DSISound RP_MENUVOLENTRANGE\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1028) { // RT_GETMENUVOLUMERANGE -> RP_MENUVOLUMERANGE
            int resp[7] = { 33568768, (int)0xa63ff850, 2002, arg1, 0, 100, 1 };
            printf("[JNI-DSI] Queuing DSISound RP_MENUVOLUMERANGE (device=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1083) { // RT_GETVOLUMERANGE -> RP_VOLUMERANGE
            int resp[8] = { 33568768, (int)0xa63ff850, 2007, arg1, arg2, 0, 100, 1 };
            printf("[JNI-DSI] Queuing DSISound RP_VOLUMERANGE (device=%d, soundType=%d)\n", arg1, arg2);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIDABTuner (0x02042000, hash = 0xa63ff835)
    if (hmi2dsi == 0x02042000) {
        if (methodId == 0) {
            int resp[6];
            resp[0] = 33826816;      // 0x02042800
            resp[1] = (int)0xa63ff835;
            resp[2] = 35;            // ATTR_PROFILESTATE
            resp[3] = 2;             // OPERATION_READY
            resp[4] = 1;
            resp[5] = 1;
            printf("[JNI-DSI] Queuing DSIDABTuner ATTR_PROFILESTATE (READY)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1016) { // RT_PROFILECHANGE
            int resp[5];
            resp[0] = 33826816;
            resp[1] = (int)0xa63ff835;
            resp[2] = 2006;          // RP_PROFILECHANGED
            resp[3] = arg1;
            resp[4] = 0;
            printf("[JNI-DSI] Queuing DSIDABTuner RP_PROFILECHANGED (profile=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIAMFMTuner (0x02041000, hash = 0xa63ff835)
    if (hmi2dsi == 0x02041000) {
        if (methodId == 0) {
            int resp[6];
            resp[0] = 33822720;      // 0x02041800
            resp[1] = (int)0xa63ff835;
            resp[2] = 21;            // ATTR_PROFILESTATE
            resp[3] = 2;             // OPERATION_READY
            resp[4] = 1;
            resp[5] = 1;
            printf("[JNI-DSI] Queuing DSIAMFMTuner ATTR_PROFILESTATE (READY)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1021) { // RT_PROFILECHANGE
            int resp[5];
            resp[0] = 33822720;
            resp[1] = (int)0xa63ff835;
            resp[2] = 2008;          // RP_PROFILECHANGED
            resp[3] = arg1;
            resp[4] = 0;
            printf("[JNI-DSI] Queuing DSIAMFMTuner RP_PROFILECHANGED (profile=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1001) { // RT_SELECTSTATION
            int resp[4];
            resp[0] = 33822720;
            resp[1] = (int)0xa63ff835;
            resp[2] = 2001;          // RP_SELECTSTATIONSTATUS
            resp[3] = 2;             // SELECTSTATIONSTATUS_DONE
            printf("[JNI-DSI] Queuing DSIAMFMTuner RP_SELECTSTATIONSTATUS (DONE)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1000) { // RT_TUNEFREQUENCYSTEPS
            int resp[4];
            resp[0] = 33822720;
            resp[1] = (int)0xa63ff835;
            resp[2] = 2000;          // RP_TUNEFREQUENCYSTEPSSTATUS
            resp[3] = 2;             // TUNEFREQUENCYSTEPSSTATUS_DONE
            printf("[JNI-DSI] Queuing DSIAMFMTuner RP_TUNEFREQUENCYSTEPSSTATUS (DONE)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIRadioData (0x02097000, hash = 0x57f18c22)
    if (hmi2dsi == 0x02097000) {
        if (methodId == 0) {
            int resp[6];
            resp[0] = 34174976;      // 0x02097800
            resp[1] = 1475447842;    // 0x57f18c22
            resp[2] = 3;             // ATTR_PROFILESTATE
            resp[3] = 2;             // OPERATION_READY
            resp[4] = 1;
            resp[5] = 1;
            printf("[JNI-DSI] Queuing DSIRadioData ATTR_PROFILESTATE (READY)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1008) { // RT_PROFILECHANGE
            int resp[5];
            resp[0] = 34174976;
            resp[1] = 1475447842;
            resp[2] = 2009;          // RP_PROFILECHANGED
            resp[3] = arg1;
            resp[4] = 0;
            printf("[JNI-DSI] Queuing DSIRadioData RP_PROFILECHANGED (profile=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIMediaBase (0x02027000, hash = 0xa63ff86e)
    if (hmi2dsi == 0x02027000) {
        if (methodId == 0) {
            int resp[6];
            resp[0] = 33716224;      // 0x02027800
            resp[1] = (int)0xa63ff86e;
            resp[2] = 9;             // ATTR_PROFILESTATE
            resp[3] = 2;             // OPERATION_READY
            resp[4] = 1;
            resp[5] = 1;
            printf("[JNI-DSI] Queuing DSIMediaBase ATTR_PROFILESTATE (READY)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1005) { // RT_PROFILECHANGE
            int resp[5];
            resp[0] = 33716224;
            resp[1] = (int)0xa63ff86e;
            resp[2] = 2002;          // RP_PROFILECHANGED
            resp[3] = arg1;
            resp[4] = 0;
            printf("[JNI-DSI] Queuing DSIMediaBase RP_PROFILECHANGED (profile=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSINavigation (0x02031000, hash = 0xa63ff895) notification registration
    if (hmi2dsi == 0x02031000) {
        if (methodId == 0) {
            int resp[5];
            resp[0] = 33757184;      // 0x02031800 (DSINavigationImpl event ID)
            resp[1] = -1505757035;   // 0xa63ff895 (version hash)
            resp[2] = 52;            // ATTR_NAVSTATEOFOPERATION
            resp[3] = 5;             // navStateOfOperation = 5 (NAVSTATEOFOPERATION_FULLY_OPERABLE)
            resp[4] = 1;             // validFlag = 1 (VALID)
            printf("[JNI-DSI] Queuing Navigation NAVSTATEOFOPERATION_FULLY_OPERABLE confirmation\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));

            int pResp[6];
            pResp[0] = 33757184;
            pResp[1] = -1505757035;
            pResp[2] = 107;          // ATTR_PROFILESTATE
            pResp[3] = 2;            // OPERATION_READY
            pResp[4] = 1;
            pResp[5] = 1;
            printf("[JNI-DSI] Queuing DSINavigation ATTR_PROFILESTATE (READY)\n");
            fflush(stdout);
            queueDsiPacket(pResp, sizeof(pResp));
        } else if (methodId == 1219) { // RT_PROFILECHANGE
            int resp[5];
            resp[0] = 33757184;
            resp[1] = -1505757035;
            resp[2] = 2110;          // RP_PROFILECHANGED
            resp[3] = arg1;
            resp[4] = 0;
            printf("[JNI-DSI] Queuing DSINavigation RP_PROFILECHANGED (profile=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
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

    // DSIFileBrowser (0x02019000, hash = 0x57f18c27)
    if (hmi2dsi == 0x02019000) {
        if (methodId == 1000) { // RT_START
            // RP_STARTRESULT = 2000
            // Layout: event (0x02019800), hash (0x57f18c27), method (2000), result (0), totalCount (0), pathFlag (0 = null)
            unsigned char resp[32] = {0};
            int *iResp = (int*)resp;
            iResp[0] = 33658880;     // 0x02019800 (DSIFileBrowserImpl event ID)
            iResp[1] = 1475447847;   // 0x57f18c27 (version hash)
            iResp[2] = 2000;         // RP_STARTRESULT
            iResp[3] = 0;            // result = 0 (OK)
            iResp[4] = 0;            // totalCount = 0
            resp[20] = 0;            // null Path flag
            printf("[JNI-DSI] Queuing DSIFileBrowser.startResult (RP=2000)\n");
            fflush(stdout);
            queueDsiPacket(resp, 21);
        } else if (methodId == 1002) { // RT_GETVIEWWINDOW
            // RP_GETVIEWWINDOWRESULT = 2001
            // Layout: event (0x02019800), hash, method (2001), result (0), windowOffset (0), windowCount (0), fileSetFlag (0), totalCount (0)
            unsigned char resp[32] = {0};
            int *iResp = (int*)resp;
            iResp[0] = 33658880;
            iResp[1] = 1475447847;
            iResp[2] = 2001;         // RP_GETVIEWWINDOWRESULT
            iResp[3] = 0;            // result = 0 (OK)
            iResp[4] = 0;            // windowOffset = 0
            iResp[5] = 0;            // windowCount = 0
            resp[24] = 0;            // null BrowsedFileSet flag
            *(int*)(resp + 25) = 0;  // totalCount = 0
            printf("[JNI-DSI] Queuing DSIFileBrowser.getViewWindowResult (RP=2001)\n");
            fflush(stdout);
            queueDsiPacket(resp, 29);
        } else if (methodId == 1003) { // RT_GETSELECTEDFILES
            // RP_GETSELECTEDFILESRESULT = 2002
            int resp[6];
            resp[0] = 33658880;
            resp[1] = 1475447847;
            resp[2] = 2002;          // RP_GETSELECTEDFILESRESULT
            resp[3] = 0;             // result = 0
            resp[4] = 0;             // selectedCount = 0
            resp[5] = 0;             // totalCount = 0
            printf("[JNI-DSI] Queuing DSIFileBrowser.getSelectedFilesResult (RP=2002)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1006) { // RT_CHANGEFOLDER
            // RP_CHANGEFOLDERRESULT = 2003
            unsigned char resp[32] = {0};
            int *iResp = (int*)resp;
            iResp[0] = 33658880;
            iResp[1] = 1475447847;
            iResp[2] = 2003;         // RP_CHANGEFOLDERRESULT
            iResp[3] = 0;            // result = 0
            iResp[4] = 0;            // totalCount = 0
            resp[20] = 0;            // null Path flag
            printf("[JNI-DSI] Queuing DSIFileBrowser.changeFolderResult (RP=2003)\n");
            fflush(stdout);
            queueDsiPacket(resp, 21);
        } else if (methodId == 1007) { // RT_SETFILEEXTENSIONFILTER
            // RP_SETFILEEXTENSIONFILTERRESULT = 2007
            int resp[5];
            resp[0] = 33658880;
            resp[1] = 1475447847;
            resp[2] = 2007;          // RP_SETFILEEXTENSIONFILTERRESULT
            resp[3] = 0;             // result = 0
            resp[4] = 0;             // filterCount = 0
            printf("[JNI-DSI] Queuing DSIFileBrowser.setFileExtensionFilterResult (RP=2007)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1008) { // RT_SETFILETYPEFILTER
            // RP_SETFILETYPEFILTERRESULT = 2008
            int resp[5];
            resp[0] = 33658880;
            resp[1] = 1475447847;
            resp[2] = 2008;          // RP_SETFILETYPEFILTERRESULT
            resp[3] = 0;             // result = 0
            resp[4] = arg1;          // filterMask = arg1
            printf("[JNI-DSI] Queuing DSIFileBrowser.setFileTypeFilterResult (RP=2008)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1009) { // RT_SETLANGUAGE
            // RP_SETLANGUAGERESULT = 2010
            // Layout: event (0x02019800), hash (0x57f18c27), method (2010), result (0), serialized String
            unsigned char resp[64] = {0};
            int *iResp = (int*)resp;
            iResp[0] = 33658880;     // 0x02019800 (DSIFileBrowserImpl event ID)
            iResp[1] = 1475447847;   // 0x57f18c27 (version hash)
            iResp[2] = 2010;         // RP_SETLANGUAGERESULT
            iResp[3] = 0;            // result = 0 (OK)
            int strLen = copyLen > 12 ? copyLen - 12 : 0;
            if (strLen > 0 && strLen <= 48) {
                memcpy(resp + 16, buf + 12, strLen);
            } else {
                resp[16] = 0; // null String flag
                strLen = 1;
            }
            printf("[JNI-DSI] Queuing DSIFileBrowser.setLanguageResult (RP=2010)\n");
            fflush(stdout);
            queueDsiPacket(resp, 16 + strLen);
        } else if (methodId == 1015) { // RT_GETFILECOUNT
            // RP_GETFILECOUNTRESULT = 2006
            int resp[6];
            resp[0] = 33658880;
            resp[1] = 1475447847;
            resp[2] = 2006;          // RP_GETFILECOUNTRESULT
            resp[3] = 0;             // result = 0
            resp[4] = 0;             // fileCount = 0
            resp[5] = 0;             // totalCount = 0
            printf("[JNI-DSI] Queuing DSIFileBrowser.getFileCountResult (RP=2006)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1017) { // RT_SETFILETYPEACTIVE
            // RP_SETFILETYPEACTIVERESULT = 2011
            // Layout: event (0x02019800), hash (0x57f18c27), method (2011), result (0)
            int resp[4];
            resp[0] = 33658880;     // 0x02019800 (DSIFileBrowserImpl event ID)
            resp[1] = 1475447847;   // 0x57f18c27 (version hash)
            resp[2] = 2011;         // RP_SETFILETYPEACTIVERESULT
            resp[3] = 0;            // result = 0 (OK)
            printf("[JNI-DSI] Queuing DSIFileBrowser.setFileTypeActiveResult (RP=2011)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1019) { // RT_GETFILECOUNTWITHFILETYPEFILTER
            // RP_GETFILECOUNTWITHFILETYPEFILTERRESULT = 2012
            int resp[7];
            resp[0] = 33658880;
            resp[1] = 1475447847;
            resp[2] = 2012;          // RP_GETFILECOUNTWITHFILETYPEFILTERRESULT
            resp[3] = 0;             // result = 0
            resp[4] = 0;             // fileCount = 0
            resp[5] = 0;             // filteredCount = 0
            resp[6] = 0;             // totalCount = 0
            printf("[JNI-DSI] Queuing DSIFileBrowser.getFileCountWithFileTypeFilterResult (RP=2012)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIAdbSetup (0x02038000, hash = 0xa63ff830)
    if (hmi2dsi == 0x02038000) {
        if (methodId == 1000) { // RT_SETLANGUAGE
            // RP_SETLANGUAGERESULT = 2000
            int resp[4];
            resp[0] = 33785856;      // 0x02038800 (DSIAdbSetupImpl event ID)
            resp[1] = -1505757136;   // 0xa63ff830 (version hash)
            resp[2] = 2000;          // RP_SETLANGUAGERESULT
            resp[3] = 0;             // result = 0 (OK)
            printf("[JNI-DSI] Queuing DSIAdbSetup.setLanguageResult (RP=2000)\n");
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        }
    }

    // DSIAdbList (0x02036000 - 0x02036003, hash = 0xa63ff830)
    if ((hmi2dsi & 0xfffffffc) == 0x02036000) {
        int instance = hmi2dsi & 0x03;
        if (methodId == 1004) { // RT_GETVIEWWINDOW
            // RP_GETVIEWWINDOWRESULT = 2003
            // Layout: event (0x02036800 + instance), hash, method (2003), result (0), flag (0 = null DataSet[]), totalCount (0)
            unsigned char resp[32] = {0};
            int *iResp = (int*)resp;
            iResp[0] = 33777664 + instance; // 0x02036800 + instance
            iResp[1] = -1505757136;         // 0xa63ff830
            iResp[2] = 2003;                // RP_GETVIEWWINDOWRESULT
            iResp[3] = 0;                   // result = 0 (OK)
            resp[16] = 0;                   // null DataSet[] array flag
            *(int*)(resp + 17) = 0;         // totalCount = 0
            printf("[JNI-DSI] Queuing DSIAdbList[%d].getViewWindowResult (RP=2003)\n", instance);
            fflush(stdout);
            queueDsiPacket(resp, 21);
        }
    }

    // DSIMirrorLink (0x02058000, hash = 0xa63ff7f2)
    if (hmi2dsi == 0x02058000) {
        if (methodId == 1001) { // RT_REQUESTACCESSMODE
            // RP_RESPONSEACCESSMODE = 2001
            int resp[5];
            resp[0] = 33916928;          // 0x02058800
            resp[1] = (int)0xa63ff7f2;   // -1505757198 (authentic hash)
            resp[2] = 2001;              // RP_RESPONSEACCESSMODE
            resp[3] = 0;                 // reset = 0
            resp[4] = arg1;              // mode = arg1
            printf("[JNI-DSI] Queuing DSIMirrorLink.responseAccessMode (RP=2001, mode=%d)\n", arg1);
            fflush(stdout);
            queueDsiPacket(resp, sizeof(resp));
        } else if (methodId == 1002) { // RT_REQUESTDAYNIGHTMODE
            // RP_RESPONSEDAYNIGHTMODE = 2002
            int resp[5];
            resp[0] = 33916928;          // 0x02058800
            resp[1] = (int)0xa63ff7f2;   // -1505757198 (authentic hash)
            resp[2] = 2002;              // RP_RESPONSEDAYNIGHTMODE
            resp[3] = 0;                 // reset = 0
            resp[4] = arg1;              // mode = arg1
            printf("[JNI-DSI] Queuing DSIMirrorLink.responseDayNightMode (RP=2002, mode=%d)\n", arg1);
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
    if (!g_jvm) (*env)->GetJavaVM(env, &g_jvm);
    initDsiQueue();
    jclass cls = (*env)->GetObjectClass(env, thiz);
    jmethodID midBufferReceived = (*env)->GetMethodID(env, cls, "bufferReceived", "([BI)[B");
    if (!midBufferReceived) {
        printf("[JNI-DSI] ERROR: bufferReceived method not found!\n");
        fflush(stdout);
        return;
    }
    jbyteArray currentRx1 = rx1;
    jbyteArray currentRx2 = rx2;

    // Small initial delay so HMI finishes registering its DSI listeners
    DSI_SLEEP(50);

#ifdef _WIN32
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        DWORD waitRes = WaitForSingleObject(g_dsiSem, 100);
        if (waitRes == WAIT_OBJECT_0) {
            DsiPacket pkt;
            DSI_MUTEX_LOCK(g_dsiCs);
            pkt = g_dsiQueue[g_dsiHead];
            g_dsiHead = (g_dsiHead + 1) % MAX_DSI_QUEUE;
            DSI_MUTEX_UNLOCK(g_dsiCs);
#else
    while (g_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000; // 100ms
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
        int waitRes = sem_timedwait(&g_dsiSem, &ts);
        if (waitRes == 0) {
            DsiPacket pkt;
            DSI_MUTEX_LOCK(g_dsiCs);
            pkt = g_dsiQueue[g_dsiHead];
            g_dsiHead = (g_dsiHead + 1) % MAX_DSI_QUEUE;
            DSI_MUTEX_UNLOCK(g_dsiCs);
#endif

            if (pkt.len > 0) {
                int eventId = *(int*)(pkt.data);
                int methodId = *(int*)(pkt.data + 8);
                // Do not spam stdout on touch gestures, encoder ticks, or hard keys to keep UI at full 60fps
                if (eventId == 0x02048800 || eventId == 0x0203b800) {
                    printf("[JNI-DSI] Delivering DSI packet: len=%d event=0x%08x method=%d state=%d\n",
                        pkt.len, eventId, methodId, *(int*)(pkt.data + 12));
                    fflush(stdout);
                }

                if (pkt.len <= 256 && currentRx1) {
                    (*env)->SetByteArrayRegion(env, currentRx1, 0, pkt.len, (const jbyte*)pkt.data);
                    jbyteArray nextRx = (jbyteArray)(*env)->CallObjectMethod(env, thiz, midBufferReceived, currentRx1, pkt.len);
                    if (nextRx != NULL) {
                        currentRx1 = nextRx;
                    }
                } else if (pkt.len <= 32768 && currentRx2) {
                    (*env)->SetByteArrayRegion(env, currentRx2, 0, pkt.len, (const jbyte*)pkt.data);
                    jbyteArray nextRx = (jbyteArray)(*env)->CallObjectMethod(env, thiz, midBufferReceived, currentRx2, pkt.len);
                    if (nextRx != NULL) {
                        currentRx2 = nextRx;
                    }
                } else {
                    jbyteArray dynBuf = (*env)->NewByteArray(env, pkt.len);
                    if (dynBuf) {
                        (*env)->SetByteArrayRegion(env, dynBuf, 0, pkt.len, (const jbyte*)pkt.data);
                        (*env)->CallObjectMethod(env, thiz, midBufferReceived, dynBuf, pkt.len);
                        (*env)->DeleteLocalRef(env, dynBuf);
                    }
                }

                if ((*env)->ExceptionCheck(env)) {
                    printf("[JNI-DSI] Exception in bufferReceived!\n");
                    (*env)->ExceptionDescribe(env);
                    (*env)->ExceptionClear(env);
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
#ifdef _WIN32
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (jlong)count.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (jlong)((int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec);
#endif
}

/*
 * Class:     tsd_mibstd2_hmi_spi_timer_TimerSPIImpl
 * Method:    _getSystemTimeFrequency
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_tsd_mibstd2_hmi_spi_timer_TimerSPIImpl__1getSystemTimeFrequency(JNIEnv *env, jobject thiz) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (jlong)freq.QuadPart;
#else
    return 1000000000LL;
#endif
}

/* =========================================================================
 * 4. FontSPI
 * ========================================================================= */

static jclass g_nativeImageCls = NULL;
static jmethodID g_nativeImageCtor = NULL;
static jmethodID g_nativeImageCtor5 = NULL;
static jclass g_nativeGlyphCls = NULL;
static jmethodID g_nativeGlyphCtor = NULL;

typedef struct {
    int id;
    char path[260];
    char faceName[64];
} FontSlot;

static FontSlot g_fonts[16];
static int g_fontCount = 0;

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    initialize
 * Signature: (Ljava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl_initialize(JNIEnv *env, jclass clazz, jclass c1, jclass c2, jclass c3) {
    (void)clazz; (void)c2;
    if (c1 && !g_nativeImageCls) {
        g_nativeImageCls = (jclass)(*env)->NewGlobalRef(env, c1);
        g_nativeImageCtor = (*env)->GetMethodID(env, c1, "<init>", "(Ljava/nio/ByteBuffer;III)V");
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        g_nativeImageCtor5 = (*env)->GetMethodID(env, c1, "<init>", "(Ljava/nio/ByteBuffer;IIIZ)V");
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }
    if (c3 && !g_nativeGlyphCls) {
        g_nativeGlyphCls = (jclass)(*env)->NewGlobalRef(env, c3);
        g_nativeGlyphCtor = (*env)->GetMethodID(env, c3, "<init>", "(CLde/vw/mib/graphics/image/spi/NativeImage;III)V");
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }
    printf("[JNI-Font] FontSPIImpl initialized: nativeImageCtor=%p nativeImageCtor5=%p nativeGlyphCtor=%p\n",
        g_nativeImageCtor, g_nativeImageCtor5, g_nativeGlyphCtor);
    fflush(stdout);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    uninitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl_uninitialize(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _isTextLayoutingAvailable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1isTextLayoutingAvailable(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
#ifdef _WIN32
    return JNI_TRUE;
#else
    return JNI_FALSE;
#endif
}

#ifdef _WIN32
static int g_fontsLoaded = 0;

static void registerOneFont(const char *relPath) {
    char fullPath[512];
    int res = AddFontResourceExA(relPath, FR_PRIVATE, 0);
    if (res > 0) return;
    snprintf(fullPath, sizeof(fullPath), "firmware/analysis/p0369/runtime/%s", relPath);
    res = AddFontResourceExA(fullPath, FR_PRIVATE, 0);
    if (res > 0) return;
    snprintf(fullPath, sizeof(fullPath), "F:/vw-mib2-emulator/firmware/analysis/p0369/runtime/%s", relPath);
    AddFontResourceExA(fullPath, FR_PRIVATE, 0);
}

static void initAllFonts(void) {
    if (g_fontsLoaded) return;
    g_fontsLoaded = 1;
    registerOneFont("Resources/skin1/Fonts/VWThesis_MIB_Regular.ttf");
    registerOneFont("Resources/skin1/Fonts/VWThesis_MIB_Light.ttf");
    registerOneFont("Resources/skin1/Fonts/VWTextCarUI-Regular.ttf");
    registerOneFont("Resources/skin1/Fonts/VWTextCarUI-Bold.ttf");
    registerOneFont("Resources/skin1/Fonts/VWThesis_MIB_Arabic_Regular.ttf");
    registerOneFont("Resources/skin1/Fonts/VWThesis_MIB_Arabic_Light.ttf");
    registerOneFont("Resources/skin1/Fonts/VWThesis_MIB_Arabic_Semibold.ttf");
    registerOneFont("Resources/skin0/Fonts/Default.ttf");
    printf("[JNI-Font] initAllFonts: all authentic Volkswagen Golf 7 fonts registered in GDI\n");
    fflush(stdout);
}

static const char* resolveFontFace(int id, wchar_t wc, int style) {
    // 1. Private Use Area (0xE000 - 0xF8FF): authentic VW vehicle and UI symbols
    if (wc >= 0xE000 && wc <= 0xF8FF) {
        return (style & 1) ? "VW Text CarUI Bold" : "VW Text CarUI";
    }
    // 2. ID-based font requested by HMI
    for (int i = 0; i < g_fontCount; i++) {
        if (g_fonts[i].id == id) {
            return g_fonts[i].faceName;
        }
    }
    return "VWThesis MIB Regular";
}

static HFONT createFontWithFallback(HDC hdc, wchar_t wc, int size, int style, const char *preferredFace) {
    initAllFonts();
    const char *candidates[6];
    int candCount = 0;

    if (wc >= 0xE000 && wc <= 0xF8FF) {
        candidates[candCount++] = (style & 1) ? "VW Text CarUI Bold" : "VW Text CarUI";
        candidates[candCount++] = "VW Text CarUI";
        candidates[candCount++] = "VWThesis MIB Regular";
        candidates[candCount++] = "FreeSans";
        candidates[candCount++] = "Segoe UI Symbol";
    } else {
        candidates[candCount++] = preferredFace ? preferredFace : "VWThesis MIB Regular";
        candidates[candCount++] = "VWThesis MIB Regular";
        candidates[candCount++] = "VW Text CarUI";
        candidates[candCount++] = "FreeSans";
        candidates[candCount++] = "Segoe UI";
    }

    HFONT hFont = NULL;
    for (int i = 0; i < candCount; i++) {
        const char *face = candidates[i];
        if (!face || strlen(face) == 0) continue;
        HFONT hTest = CreateFontA(
            -size, 0, 0, 0,
            (style & 1) ? FW_BOLD : FW_NORMAL,
            (style & 2) ? TRUE : FALSE,
            FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            face
        );
        if (!hTest) continue;

        if (hdc) {
            HGDIOBJ old = SelectObject(hdc, hTest);
            WORD gIdx = 0;
            DWORD ret = GetGlyphIndicesW(hdc, &wc, 1, &gIdx, GGI_MARK_NONEXISTING_GLYPHS);
            SelectObject(hdc, old);

            if (ret != GDI_ERROR && gIdx != 0xFFFF) {
                return hTest;
            }
        }
        if (!hFont) hFont = hTest;
        else DeleteObject(hTest);
    }
    return hFont;
}
#else
static void initAllFonts(void) {}
static void registerOneFont(const char *relPath) { (void)relPath; }
#endif

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _openFont
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1openFont(JNIEnv *env, jobject thiz, jstring name) {
    (void)thiz;
    initAllFonts();
    const char *nameChars = name ? (*env)->GetStringUTFChars(env, name, NULL) : NULL;
    int slot = g_fontCount < 16 ? g_fontCount++ : 0;
    int id = slot + 1;
    g_fonts[slot].id = id;
    strcpy(g_fonts[slot].faceName, "VWThesis MIB Regular");

    if (nameChars) {
        strncpy(g_fonts[slot].path, nameChars, sizeof(g_fonts[slot].path) - 1);
        registerOneFont(nameChars);
        if (strstr(nameChars, "Light")) {
            strcpy(g_fonts[slot].faceName, "VWThesis MIB Light");
        } else if (strstr(nameChars, "Semibold") || strstr(nameChars, "Bold")) {
            if (strstr(nameChars, "CarUI")) {
                strcpy(g_fonts[slot].faceName, "VW Text CarUI Bold");
            } else {
                strcpy(g_fonts[slot].faceName, "VWThesis MIB Regular");
            }
        } else if (strstr(nameChars, "CarUI")) {
            strcpy(g_fonts[slot].faceName, "VW Text CarUI");
        }
        printf("[JNI-Font] openFont: id=%d path='%s' face='%s'\n", id, nameChars, g_fonts[slot].faceName);
        fflush(stdout);
        (*env)->ReleaseStringUTFChars(env, name, nameChars);
    }
    return (jint)id;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _closeFont
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1closeFont(JNIEnv *env, jobject thiz, jint id) {
    (void)env; (void)thiz; (void)id;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getAscender
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getAscender(JNIEnv *env, jobject thiz, jint id, jint size) {
    (void)env; (void)thiz; (void)id;
    return (jint)(size * 8 / 10);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getDescender
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getDescender(JNIEnv *env, jobject thiz, jint id, jint size) {
    (void)env; (void)thiz; (void)id;
    return (jint)(size * 2 / 10);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getHeight
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getHeight(JNIEnv *env, jobject thiz, jint id, jint size) {
    (void)env; (void)thiz; (void)id;
    return (jint)size;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getMaxAdvanceWith
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getMaxAdvanceWith(JNIEnv *env, jobject thiz, jint id, jint size) {
    (void)env; (void)thiz; (void)id;
    return (jint)((size * 7 / 10) << 6);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getKerning
 * Signature: (ICCI)I
 */
JNIEXPORT jint JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getKerning(JNIEnv *env, jobject thiz, jint id, jchar c1, jchar c2, jint size) {
    (void)env; (void)thiz; (void)id; (void)c1; (void)c2; (void)size;
    return 0;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _isGlyphAvailable
 * Signature: (IC)Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1isGlyphAvailable(JNIEnv *env, jobject thiz, jint id, jchar c) {
    (void)env; (void)thiz; (void)id; (void)c;
    return JNI_TRUE;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getControlBox
 * Signature: (ICI[I)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getControlBox(JNIEnv *env, jobject thiz, jint id, jchar c, jint size, jintArray box) {
    (void)thiz; (void)id; (void)c;
    if (!box || (*env)->GetArrayLength(env, box) < 2) return;
    if (size <= 0) size = 20;

#ifndef _WIN32
    jint dims[2] = { (jint)(size * 5 / 10), (jint)size };
    (*env)->SetIntArrayRegion(env, box, 0, 2, dims);
#else
    initAllFonts();

    HDC hdc = CreateCompatibleDC(NULL);
    if (!hdc) {
        jint dims[2] = { (jint)(size * 5 / 10), (jint)size };
        (*env)->SetIntArrayRegion(env, box, 0, 2, dims);
        return;
    }

    wchar_t wc = (wchar_t)c;
    const char *face = resolveFontFace(id, wc, 0);
    HFONT hFont = createFontWithFallback(hdc, wc, size, 0, face);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    TEXTMETRICW tm = {0};
    GetTextMetricsW(hdc, &tm);
    int ascender = (tm.tmAscent > 0) ? (int)tm.tmAscent : (int)(size * 8 / 10);

    SIZE sz = {0, 0};
    int charWidth = 0;
    if (GetCharWidth32W(hdc, wc, wc, &charWidth) && charWidth > 0) {
        sz.cx = charWidth;
        sz.cy = (tm.tmHeight > 0) ? tm.tmHeight : size;
    } else if (!GetTextExtentPoint32W(hdc, &wc, 1, &sz) || sz.cx <= 0) {
        sz.cx = (c == ' ') ? (size * 3 / 10) : (size * 5 / 10);
        sz.cy = size;
    }

    SelectObject(hdc, oldFont);
    if (hFont) DeleteObject(hFont);
    DeleteDC(hdc);

    jint dims[2];
    dims[0] = (jint)sz.cx;
    dims[1] = (jint)(sz.cy > 0 ? sz.cy : size);
    (*env)->SetIntArrayRegion(env, box, 0, 2, dims);
#endif
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getFamilyName
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getFamilyName(JNIEnv *env, jobject thiz, jint id) {
    (void)thiz;
    const char *face = "VWThesis MIB Regular";
    for (int i = 0; i < g_fontCount; i++) {
        if (g_fonts[i].id == id) { face = g_fonts[i].faceName; break; }
    }
    return (*env)->NewStringUTF(env, face);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getStyleName
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getStyleName(JNIEnv *env, jobject thiz, jint id) {
    (void)env; (void)thiz; (void)id;
    return (*env)->NewStringUTF(env, "Regular");
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getGlyph
 * Signature: (ICII)Lde/vw/mib/graphics/font/spi/NativeGlyph;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getGlyph(JNIEnv *env, jobject thiz, jint id, jchar c, jint style, jint size) {
#ifndef _WIN32
    (void)env; (void)thiz; (void)id; (void)c; (void)style; (void)size;
    return NULL;
#else
    (void)thiz;
    initAllFonts();
    if (size <= 0) size = 20;

    HDC hdc = CreateCompatibleDC(NULL);
    if (!hdc) return NULL;

    wchar_t wc = (wchar_t)c;
    const char *face = resolveFontFace(id, wc, style);
    HFONT hFont = createFontWithFallback(hdc, wc, size, style, face);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    TEXTMETRICW tm = {0};
    GetTextMetricsW(hdc, &tm);
    int ascender = (tm.tmAscent > 0) ? (int)tm.tmAscent : (int)(size * 8 / 10);

    SIZE sz = {0, 0};
    int charWidth = 0;
    if (GetCharWidth32W(hdc, wc, wc, &charWidth) && charWidth > 0) {
        sz.cx = charWidth;
        sz.cy = (tm.tmHeight > 0) ? tm.tmHeight : size;
    } else if (!GetTextExtentPoint32W(hdc, &wc, 1, &sz) || sz.cx <= 0) {
        sz.cx = (c == ' ') ? (size * 3 / 10) : (size * 5 / 10);
        sz.cy = size;
    }

    int w = (int)sz.cx;
    int h = (int)sz.cy;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 512) w = 512;
    if (h > 512) h = 512;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *pvBits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (!hbm || !pvBits) {
        if (hbm) DeleteObject(hbm);
        SelectObject(hdc, oldFont);
        if (hFont) DeleteObject(hFont);
        DeleteDC(hdc);
        return NULL;
    }

    HGDIOBJ oldBm = SelectObject(hdc, hbm);
    RECT rc = {0, 0, w, h};
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    if (c != ' ') {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOutW(hdc, 0, 0, &wc, 1);
    }

    int imgSize = w * h;
    unsigned char *grayBuf = (unsigned char *)malloc(imgSize);
    if (!grayBuf) {
        SelectObject(hdc, oldBm);
        DeleteObject(hbm);
        SelectObject(hdc, oldFont);
        if (hFont) DeleteObject(hFont);
        DeleteDC(hdc);
        return NULL;
    }

    unsigned int *srcPixels = (unsigned int *)pvBits;
    for (int i = 0; i < imgSize; i++) {
        grayBuf[i] = (unsigned char)(srcPixels[i] & 0xFF);
    }

    SelectObject(hdc, oldBm);
    DeleteObject(hbm);
    SelectObject(hdc, oldFont);
    if (hFont) DeleteObject(hFont);
    DeleteDC(hdc);

    jobject byteBuf = (*env)->NewDirectByteBuffer(env, grayBuf, imgSize);
    if (!byteBuf) {
        free(grayBuf);
        return NULL;
    }

    if (!g_nativeImageCls) {
        jclass localCls = (*env)->FindClass(env, "de/vw/mib/graphics/image/spi/NativeImage");
        if (localCls) {
            g_nativeImageCls = (jclass)(*env)->NewGlobalRef(env, localCls);
            g_nativeImageCtor = (*env)->GetMethodID(env, localCls, "<init>", "(Ljava/nio/ByteBuffer;III)V");
            (*env)->DeleteLocalRef(env, localCls);
        }
    }
    if (!g_nativeGlyphCls) {
        jclass localCls = (*env)->FindClass(env, "de/vw/mib/graphics/font/spi/NativeGlyph");
        if (localCls) {
            g_nativeGlyphCls = (jclass)(*env)->NewGlobalRef(env, localCls);
            g_nativeGlyphCtor = (*env)->GetMethodID(env, localCls, "<init>", "(CLde/vw/mib/graphics/image/spi/NativeImage;III)V");
            (*env)->DeleteLocalRef(env, localCls);
        }
    }

    if (!g_nativeImageCls || !g_nativeImageCtor || !g_nativeGlyphCls || !g_nativeGlyphCtor) {
        free(grayBuf);
        return NULL;
    }

    jobject nativeImg = (*env)->NewObject(env, g_nativeImageCls, g_nativeImageCtor, byteBuf, (jint)w, (jint)h, (jint)4096);
    jobject nativeGlyph = (*env)->NewObject(env, g_nativeGlyphCls, g_nativeGlyphCtor, (jchar)c, nativeImg, (jint)0, (jint)ascender, (jint)(w << 6));

    return nativeGlyph;
#endif
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getGlyphById
 * Signature: (IIII)Lde/vw/mib/graphics/font/spi/NativeGlyph;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getGlyphById(JNIEnv *env, jobject thiz, jint id, jint size, jint style, jint glyphId) {
    return Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getGlyph(env, thiz, id, (jchar)glyphId, style, size);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _releaseGlyph
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1releaseGlyph(JNIEnv *env, jobject thiz, jobject buf) {
    (void)thiz;
    if (buf) {
        void *addr = (*env)->GetDirectBufferAddress(env, buf);
        if (addr) free(addr);
    }
}

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _getFontMetrics
 * Signature: (Lde/vw/mib/graphics/font/spi/style/GlyphStyle;)Lde/vw/mib/graphics/font/spi/FontMetrics;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1getFontMetrics(JNIEnv *env, jobject thiz, jobject style) {
    (void)env; (void)thiz; (void)style;
    return NULL;
}

typedef struct {
    int startIndex;
    int endIndex;
    int halign;
    int linebreak;
    int lineSpacing;
    int fontId;
    int fontSize;
    short isBold;
    short isItalic;
} ParsedTextStyle;

/*
 * Class:     tsd_mibstd2_hmi_spi_font_FontSPIImpl
 * Method:    _performTextLayout2
 * Signature: (IIIZCZC[BILjava/lang/String;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_tsd_mibstd2_hmi_spi_font_FontSPIImpl__1performTextLayout2(
    JNIEnv *env, jobject thiz, jint w, jint h, jint maxLines, jboolean trunc, jchar truncCh,
    jboolean hyph, jchar hyphCh, jbyteArray styles, jint stylesLen, jstring text) {
#ifndef _WIN32
    (void)env; (void)thiz; (void)w; (void)h; (void)maxLines; (void)trunc; (void)truncCh;
    (void)hyph; (void)hyphCh; (void)styles; (void)stylesLen; (void)text;
    return NULL;
#else
    (void)thiz; (void)maxLines; (void)trunc; (void)truncCh; (void)hyph; (void)hyphCh;
    if (!text) return NULL;
    jsize len = (*env)->GetStringLength(env, text);
    const jchar *chars = (*env)->GetStringChars(env, text, NULL);
    if (!chars) return NULL;

    initAllFonts();

    ParsedTextStyle pStyles[32];
    int pStyleCount = 0;
    jsize stylesByteLen = styles ? (*env)->GetArrayLength(env, styles) : 0;

    if (styles && stylesByteLen >= 28) {
        jbyte *sPtr = (*env)->GetByteArrayElements(env, styles, NULL);
        if (sPtr) {
            int stride = 42;
            if (stylesLen > 1 && stylesByteLen >= stylesLen * 56) {
                int start1_at_56 = *(int*)(sPtr + 56);
                if (start1_at_56 >= 0 && start1_at_56 <= (int)len) stride = 56;
            }
            int maxStyles = stylesLen > 0 ? stylesLen : (stylesByteLen / stride);
            if (maxStyles > 32) maxStyles = 32;
            for (int s = 0; s < maxStyles && (s * stride + 28) <= stylesByteLen; s++) {
                jbyte *cur = sPtr + s * stride;
                pStyles[pStyleCount].startIndex = *(int*)(cur + 0);
                pStyles[pStyleCount].endIndex   = *(int*)(cur + 4);
                pStyles[pStyleCount].halign     = *(int*)(cur + 8);
                pStyles[pStyleCount].linebreak  = *(int*)(cur + 12);
                pStyles[pStyleCount].lineSpacing= *(int*)(cur + 16);
                pStyles[pStyleCount].fontId     = *(int*)(cur + 20);
                pStyles[pStyleCount].fontSize   = *(int*)(cur + 24);
                pStyles[pStyleCount].isBold     = *(short*)(cur + 28);
                pStyles[pStyleCount].isItalic   = *(short*)(cur + 30);
                if (pStyles[pStyleCount].fontSize <= 0 || pStyles[pStyleCount].fontSize > 160) {
                    pStyles[pStyleCount].fontSize = (h > 0 && h < 100) ? (int)h : 20;
                }
                pStyleCount++;
            }
            (*env)->ReleaseByteArrayElements(env, styles, sPtr, JNI_ABORT);
        }
    }

    if (pStyleCount == 0) {
        pStyles[0].startIndex = 0;
        pStyles[0].endIndex = (int)len;
        pStyles[0].fontId = 0;
        pStyles[0].fontSize = (h > 0 && h < 100) ? (int)h : 20;
        pStyles[0].isBold = 0;
        pStyles[0].isItalic = 0;
        pStyleCount = 1;
    }

    HDC hdc = CreateCompatibleDC(NULL);

    int totalInts = 9 + 8 * (int)len;
    int totalBytes = totalInts * (int)sizeof(int);
    int *buf = (int *)calloc(totalInts, sizeof(int));
    if (!buf) {
        if (hdc) DeleteDC(hdc);
        (*env)->ReleaseStringChars(env, text, chars);
        return NULL;
    }

    int curX = 0;
    int maxLineHeight = pStyles[0].fontSize;
    int maxAscender = (int)(pStyles[0].fontSize * 8 / 10);

    int *glyphPtr = buf + 9;
    for (int j = 0; j < (int)len; j++) {
        wchar_t wc = (wchar_t)chars[j];

        int curFontId = pStyles[0].fontId;
        int curFontSize = pStyles[0].fontSize;
        int curIsBold = pStyles[0].isBold;
        for (int s = 0; s < pStyleCount; s++) {
            if (j >= pStyles[s].startIndex && j < pStyles[s].endIndex) {
                curFontId = pStyles[s].fontId;
                curFontSize = pStyles[s].fontSize;
                curIsBold = pStyles[s].isBold;
                break;
            }
        }
        if (curFontSize > maxLineHeight) maxLineHeight = curFontSize;
        int curAsc = (int)(curFontSize * 8 / 10);

        SIZE sz = {0, 0};
        if (hdc) {
            const char *face = resolveFontFace(curFontId, wc, curIsBold ? 1 : 0);
            HFONT hFont = createFontWithFallback(hdc, wc, curFontSize, curIsBold ? 1 : 0, face);
            HGDIOBJ old = SelectObject(hdc, hFont);
            TEXTMETRICW tm = {0};
            GetTextMetricsW(hdc, &tm);
            if (tm.tmAscent > curAsc) curAsc = (int)tm.tmAscent;
            int cw = 0;
            if (GetCharWidth32W(hdc, wc, wc, &cw) && cw > 0) {
                sz.cx = cw;
                sz.cy = (tm.tmHeight > 0) ? tm.tmHeight : curFontSize;
            } else if (!GetTextExtentPoint32W(hdc, &wc, 1, &sz) || sz.cx <= 0) {
                sz.cx = (wc == ' ') ? (curFontSize * 3 / 10) : (curFontSize * 5 / 10);
                sz.cy = curFontSize;
            }
            SelectObject(hdc, old);
            DeleteObject(hFont);
        } else {
            sz.cx = (wc == ' ') ? (curFontSize * 3 / 10) : (curFontSize * 5 / 10);
            sz.cy = curFontSize;
        }
        if (curAsc > maxAscender) maxAscender = curAsc;

        int gw = (int)sz.cx;
        int gh = (int)sz.cy;

        glyphPtr[0] = (int)chars[j];   // glyphId (unicode char)
        glyphPtr[1] = gh;             // height
        glyphPtr[2] = gw;             // width
        glyphPtr[3] = 0;              // isRightToLeft
        glyphPtr[4] = j;              // sourceIndex
        glyphPtr[5] = curX;           // x
        glyphPtr[6] = 0;              // y
        glyphPtr[7] = curX;           // caretX (Java shifts << 6)

        curX += gw;
        glyphPtr += 8;
    }

    if (hdc) DeleteDC(hdc);
    (*env)->ReleaseStringChars(env, text, chars);

    int totalWidth = curX;
    int totalHeight = (h > 0 && h < 500) ? (int)h : maxLineHeight;

    // Header (4 ints)
    buf[0] = totalHeight; // totalHeight
    buf[1] = totalWidth;  // totalWidth
    buf[2] = 0;           // isTruncated
    buf[3] = 1;           // lineCount = 1

    // Line (5 ints)
    buf[4] = 0;           // left
    buf[5] = 0;           // top
    buf[6] = totalHeight; // bottom
    buf[7] = maxAscender; // baseline
    buf[8] = (int)len;    // glyphCount

    jbyteArray result = (*env)->NewByteArray(env, totalBytes);
    if (result) {
        (*env)->SetByteArrayRegion(env, result, 0, totalBytes, (const jbyte *)buf);
    }
    free(buf);
    return result;
#endif
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

#pragma pack(push, 1)
typedef struct {
    uint32_t id;
    uint16_t w;
    uint16_t h;
    uint16_t fmt;
    uint16_t opaque;
    uint32_t offset;
    uint32_t size;
} SkinImageEntry;
#pragma pack(pop)

typedef struct {
#ifdef _WIN32
    HANDLE hFile;
    HANDLE hMap;
#endif
    const unsigned char *pMap;
    const SkinImageEntry *entries;
    uint32_t count;
} SkinMap;

static SkinMap g_skin0 = {0};
static SkinMap g_skin1 = {0};

static void loadOneSkin(SkinMap *skin, const char *relPath) {
    if (skin->pMap) return;
#ifdef _WIN32
    HANDLE h = CreateFileA(relPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        char pathBuf[256];
        snprintf(pathBuf, sizeof(pathBuf), "firmware/analysis/p0369/runtime/%s", relPath);
        h = CreateFileA(pathBuf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (h == INVALID_HANDLE_VALUE) {
        char pathBuf[256];
        snprintf(pathBuf, sizeof(pathBuf), "F:/vw-mib2-emulator/firmware/analysis/p0369/runtime/%s", relPath);
        h = CreateFileA(pathBuf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (h != INVALID_HANDLE_VALUE) {
        skin->hFile = h;
        skin->hMap = CreateFileMappingA(h, NULL, PAGE_READONLY, 0, 0, NULL);
        if (skin->hMap) {
            skin->pMap = (const unsigned char *)MapViewOfFile(skin->hMap, FILE_MAP_READ, 0, 0, 0);
            if (skin->pMap) {
                skin->count = *(const uint32_t *)skin->pMap;
                skin->entries = (const SkinImageEntry *)(skin->pMap + 4);
                printf("[JNI-Image] %s mapped: %u images\n", relPath, skin->count);
                fflush(stdout);
            }
        }
    } else {
        printf("[JNI-Image] Warning: %s not found (err=%lu)\n", relPath, GetLastError());
        fflush(stdout);
    }
#else
    FILE *fp = fopen(relPath, "rb");
    if (!fp) {
        char pathBuf[256];
        snprintf(pathBuf, sizeof(pathBuf), "firmware/analysis/p0369/runtime/%s", relPath);
        fp = fopen(pathBuf, "rb");
    }
    if (!fp) {
        char pathBuf[256];
        snprintf(pathBuf, sizeof(pathBuf), "F:/vw-mib2-emulator/firmware/analysis/p0369/runtime/%s", relPath);
        fp = fopen(pathBuf, "rb");
    }
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz > 4) {
            unsigned char *buf = (unsigned char *)malloc(sz);
            if (buf && fread(buf, 1, sz, fp) == (size_t)sz) {
                skin->pMap = buf;
                skin->count = *(const uint32_t *)skin->pMap;
                skin->entries = (const SkinImageEntry *)(skin->pMap + 4);
                printf("[JNI-Image-ARM64] %s loaded: %u images\n", relPath, skin->count);
                fflush(stdout);
            }
        }
        fclose(fp);
    }
#endif
}

static void initSkinCache(void) {
    loadOneSkin(&g_skin0, "skin0_cache.bin");
    loadOneSkin(&g_skin1, "skin1_cache.bin");
}

static const SkinImageEntry *searchEntries(const SkinMap *skin, uint32_t imageId) {
    if (!skin || !skin->entries || skin->count == 0) return NULL;
    int left = 0, right = (int)skin->count - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        if (skin->entries[mid].id == imageId) return &skin->entries[mid];
        if (skin->entries[mid].id < imageId) left = mid + 1;
        else right = mid - 1;
    }
    return NULL;
}

static const SkinImageEntry *findSkinImage(int skinId, uint32_t imageId, const unsigned char **outMap) {
    initSkinCache();
    const SkinImageEntry *entry = NULL;
    if (skinId == 1) {
        entry = searchEntries(&g_skin1, imageId);
        if (entry) { *outMap = g_skin1.pMap; return entry; }
    }
    entry = searchEntries(&g_skin0, imageId);
    if (entry) { *outMap = g_skin0.pMap; return entry; }
    entry = searchEntries(&g_skin1, imageId);
    if (entry) { *outMap = g_skin1.pMap; return entry; }
    return NULL;
}

static jobject create_fallback_native_image(JNIEnv *env, int w, int h) {
    if (!g_nativeImageCls) {
        jclass localCls = (*env)->FindClass(env, "de/vw/mib/graphics/image/spi/NativeImage");
        if (localCls) {
            g_nativeImageCls = (jclass)(*env)->NewGlobalRef(env, localCls);
            (*env)->DeleteLocalRef(env, localCls);
        }
    }
    if (!g_nativeImageCls) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        return NULL;
    }
    if (!g_nativeImageCtor5) {
        g_nativeImageCtor5 = (*env)->GetMethodID(env, g_nativeImageCls, "<init>", "(Ljava/nio/ByteBuffer;IIIZ)V");
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }
    if (!g_nativeImageCtor5) return NULL;
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
    return (*env)->NewObject(env, g_nativeImageCls, g_nativeImageCtor5, byteBuf, (jint)w, (jint)h, (jint)4356, JNI_TRUE);
}

static jobject load_image_via_imageio(JNIEnv *env, jstring pathStr) {
    if (!pathStr) return NULL;
    const char *chars = (*env)->GetStringUTFChars(env, pathStr, NULL);
    if (!chars) return NULL;
    char pathBuf[1024];
    strncpy(pathBuf, chars, sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = '\0';
    (*env)->ReleaseStringUTFChars(env, pathStr, chars);

    char *actualPath = pathBuf;
    if (strncmp(actualPath, "file:///", 8) == 0) actualPath += 8;
    else if (strncmp(actualPath, "file://", 7) == 0) actualPath += 7;
    else if (strncmp(actualPath, "file:", 5) == 0) actualPath += 5;

    jclass fileCls = (*env)->FindClass(env, "java/io/File");
    if (!fileCls) return NULL;
    jmethodID fileCtor = (*env)->GetMethodID(env, fileCls, "<init>", "(Ljava/lang/String;)V");
    jstring jActualPath = (*env)->NewStringUTF(env, actualPath);
    jobject fileObj = (*env)->NewObject(env, fileCls, fileCtor, jActualPath);
    (*env)->DeleteLocalRef(env, jActualPath);

    jmethodID existsMethod = (*env)->GetMethodID(env, fileCls, "exists", "()Z");
    jboolean exists = (*env)->CallBooleanMethod(env, fileObj, existsMethod);
    if (!exists) {
        (*env)->DeleteLocalRef(env, fileObj);
        return NULL;
    }

    jclass iioCls = (*env)->FindClass(env, "javax/imageio/ImageIO");
    if (!iioCls) {
        (*env)->DeleteLocalRef(env, fileObj);
        return NULL;
    }
    jmethodID readMethod = (*env)->GetStaticMethodID(env, iioCls, "read", "(Ljava/io/File;)Ljava/awt/image/BufferedImage;");
    jobject bufImg = (*env)->CallStaticObjectMethod(env, iioCls, readMethod, fileObj);
    (*env)->DeleteLocalRef(env, fileObj);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return NULL;
    }
    if (!bufImg) return NULL;

    jclass biCls = (*env)->GetObjectClass(env, bufImg);
    jmethodID getWidthMethod = (*env)->GetMethodID(env, biCls, "getWidth", "()I");
    jmethodID getHeightMethod = (*env)->GetMethodID(env, biCls, "getHeight", "()I");
    jint w = (*env)->CallIntMethod(env, bufImg, getWidthMethod);
    jint h = (*env)->CallIntMethod(env, bufImg, getHeightMethod);
    if (w <= 0 || h <= 0) {
        (*env)->DeleteLocalRef(env, bufImg);
        return NULL;
    }

    jmethodID getRGBMethod = (*env)->GetMethodID(env, biCls, "getRGB", "(IIII[III)[I");
    jintArray rgbArray = (*env)->NewIntArray(env, w * h);
    (*env)->CallObjectMethod(env, bufImg, getRGBMethod, (jint)0, (jint)0, w, h, rgbArray, (jint)0, w);
    (*env)->DeleteLocalRef(env, bufImg);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        if (rgbArray) (*env)->DeleteLocalRef(env, rgbArray);
        return NULL;
    }

    jint *rgbElements = (*env)->GetIntArrayElements(env, rgbArray, NULL);
    if (!rgbElements) {
        (*env)->DeleteLocalRef(env, rgbArray);
        return NULL;
    }

    int size = w * h * 4;
    unsigned char *pixels = (unsigned char *)malloc(size);
    if (!pixels) {
        (*env)->ReleaseIntArrayElements(env, rgbArray, rgbElements, JNI_ABORT);
        (*env)->DeleteLocalRef(env, rgbArray);
        return NULL;
    }

    jboolean isOpaque = JNI_TRUE;
    for (int i = 0; i < w * h; i++) {
        uint32_t argb = (uint32_t)rgbElements[i];
        uint8_t a = (uint8_t)((argb >> 24) & 0xff);
        uint8_t r = (uint8_t)((argb >> 16) & 0xff);
        uint8_t g = (uint8_t)((argb >> 8) & 0xff);
        uint8_t b = (uint8_t)(argb & 0xff);
        if (a < 255) isOpaque = JNI_FALSE;
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }
    (*env)->ReleaseIntArrayElements(env, rgbArray, rgbElements, JNI_ABORT);
    (*env)->DeleteLocalRef(env, rgbArray);

    if (!g_nativeImageCls) {
        jclass localCls = (*env)->FindClass(env, "de/vw/mib/graphics/image/spi/NativeImage");
        if (localCls) {
            g_nativeImageCls = (jclass)(*env)->NewGlobalRef(env, localCls);
            (*env)->DeleteLocalRef(env, localCls);
        }
    }
    if (!g_nativeImageCtor5 && g_nativeImageCls) {
        g_nativeImageCtor5 = (*env)->GetMethodID(env, g_nativeImageCls, "<init>", "(Ljava/nio/ByteBuffer;IIIZ)V");
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }
    if (!g_nativeImageCtor5) {
        free(pixels);
        return NULL;
    }

    jobject byteBuf = (*env)->NewDirectByteBuffer(env, pixels, size);
    return (*env)->NewObject(env, g_nativeImageCls, g_nativeImageCtor5, byteBuf, w, h, (jint)4356, isOpaque);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromResource
 * Signature: (Ljava/lang/String;ILde/vw/mib/graphics/image/spi/NativeImageLoadingListener;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromResource(JNIEnv *env, jobject thiz, jstring path, jint scale, jobject listener) {
    jobject img = load_image_via_imageio(env, path);
    if (img) return img;
    return create_fallback_native_image(env, 16, 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromSkinById
 * Signature: (II)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromSkinById(JNIEnv *env, jobject thiz, jint skinId, jint imageId) {
    (void)thiz;
    const unsigned char *pMap = NULL;
    const SkinImageEntry *entry = findSkinImage((int)skinId, (uint32_t)imageId, &pMap);
    if (entry && pMap) {
        if (!g_nativeImageCls) {
            jclass localCls = (*env)->FindClass(env, "de/vw/mib/graphics/image/spi/NativeImage");
            if (localCls) {
                g_nativeImageCls = (jclass)(*env)->NewGlobalRef(env, localCls);
                (*env)->DeleteLocalRef(env, localCls);
            }
        }
        if (g_nativeImageCls) {
            if (!g_nativeImageCtor5) {
                g_nativeImageCtor5 = (*env)->GetMethodID(env, g_nativeImageCls, "<init>", "(Ljava/nio/ByteBuffer;IIIZ)V");
                if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
            }
            if (g_nativeImageCtor5) {
                void *pixels = malloc(entry->size);
                if (pixels) {
                    memcpy(pixels, pMap + entry->offset, entry->size);
                    jobject byteBuf = (*env)->NewDirectByteBuffer(env, pixels, entry->size);
                    return (*env)->NewObject(env, g_nativeImageCls, g_nativeImageCtor5, byteBuf, (jint)entry->w, (jint)entry->h, (jint)entry->fmt, entry->opaque ? JNI_TRUE : JNI_FALSE);
                }
            }
        }
    }
    return create_fallback_native_image(env, 16, 16);
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromSkinByFilename
 * Signature: (Ljava/lang/String;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromSkinByFilename(JNIEnv *env, jobject thiz, jstring filename) {
    jobject img = load_image_via_imageio(env, filename);
    if (img) return img;
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

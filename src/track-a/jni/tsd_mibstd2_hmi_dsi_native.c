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

static volatile LONG g_running = 1;

/* =========================================================================
 * 1. DSI Communication Threads
 * ========================================================================= */

/*
 * Class:     tsd_mibstd2_hmi_dsi_communication_DSITransceiverThread
 * Method:    _run
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_dsi_communication_DSITransceiverThread__1run(JNIEnv *env, jobject thiz) {
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
    /* Outgoing DSI packet sink */
}

/*
 * Class:     tsd_mibstd2_hmi_dsi_communication_DSIReceiverThread
 * Method:    _run
 * Signature: ([B[B)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_dsi_communication_DSIReceiverThread__1run(JNIEnv *env, jobject thiz, jbyteArray rx1, jbyteArray rx2) {
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        Sleep(50);
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
    return NULL;
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

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromResource
 * Signature: (Ljava/lang/String;ILde/vw/mib/graphics/image/spi/NativeImageLoadingListener;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromResource(JNIEnv *env, jobject thiz, jstring path, jint scale, jobject listener) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromSkinById
 * Signature: (II)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromSkinById(JNIEnv *env, jobject thiz, jint skinId, jint imageId) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageFromSkinByFilename
 * Signature: (Ljava/lang/String;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageFromSkinByFilename(JNIEnv *env, jobject thiz, jstring filename) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadImageRegionFromResource
 * Signature: (Ljava/lang/String;IIIILde/vw/mib/graphics/image/spi/NativeImageLoadingListener;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadImageRegionFromResource(JNIEnv *env, jobject thiz, jstring path, jint x, jint y, jint w, jint h, jobject listener) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _loadThumbnailImageFromResource
 * Signature: (Ljava/lang/String;)Lde/vw/mib/graphics/image/spi/NativeImage;
 */
JNIEXPORT jobject JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1loadThumbnailImageFromResource(JNIEnv *env, jobject thiz, jstring path) {
    return NULL;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _releaseImage
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1releaseImage(JNIEnv *env, jobject thiz, jobject buf) {
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
    return JNI_FALSE;
}

/*
 * Class:     tsd_mibstd2_hmi_spi_image_ImageSPIImpl
 * Method:    _setContentFileForSkin
 * Signature: (ILjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_tsd_mibstd2_hmi_spi_image_ImageSPIImpl__1setContentFileForSkin(JNIEnv *env, jobject thiz, jint skinId, jstring path) {
    return JNI_FALSE;
}

/*
 * Windows JNI compatibility implementation of com.microdoc.util.ObjectSwapper (libswap.so).
 * Reconstructed by clean-room reverse engineering of authentic QNX ARM libswap.so (P0369/P0475).
 * 
 * Provides native binary deserialization for authentic Volkswagen MIB2 .res and .swap files.
 */

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_SWAP_FILES 64
static FILE *g_swap_files[MAX_SWAP_FILES];

JNIEXPORT void JNICALL Java_com_microdoc_util_ObjectSwapper_init(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    memset(g_swap_files, 0, sizeof(g_swap_files));
}

JNIEXPORT jint JNICALL Java_com_microdoc_util_ObjectSwapper__1open(JNIEnv *env, jobject thiz, jstring jpath) {
    (void)thiz;
    if (!jpath) return 0;
    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (!path) return 0;

    FILE *fp = fopen(path, "rb");
    (*env)->ReleaseStringUTFChars(env, jpath, path);
    if (!fp) return 0;

    for (int i = 1; i < MAX_SWAP_FILES; i++) {
        if (!g_swap_files[i]) {
            g_swap_files[i] = fp;
            return (jint)i;
        }
    }

    fclose(fp);
    return 0;
}

JNIEXPORT void JNICALL Java_com_microdoc_util_ObjectSwapper__1close(JNIEnv *env, jobject thiz, jint fd) {
    (void)env;
    (void)thiz;
    if (fd > 0 && fd < MAX_SWAP_FILES && g_swap_files[fd]) {
        fclose(g_swap_files[fd]);
        g_swap_files[fd] = NULL;
    }
}

static jstring read_swapper_string(JNIEnv *env, FILE *fp, int count) {
    if (count <= 0) {
        jchar empty = 0;
        return (*env)->NewString(env, &empty, 0);
    }
    jchar *buf = (jchar *)malloc(count * sizeof(jchar));
    if (!buf) return NULL;
    if (fread(buf, sizeof(jchar), count, fp) != (size_t)count) {
        free(buf);
        return NULL;
    }
    jstring s = (*env)->NewString(env, buf, count);
    free(buf);
    return s;
}

JNIEXPORT jobjectArray JNICALL Java_com_microdoc_util_ObjectSwapper__1linkAll(JNIEnv *env, jobject thiz, jint fd) {
    (void)thiz;
    if (fd <= 0 || fd >= MAX_SWAP_FILES || !g_swap_files[fd]) {
        jclass exCls = (*env)->FindClass(env, "java/io/IOException");
        if (exCls) (*env)->ThrowNew(env, exCls, "Invalid swapper file descriptor");
        return NULL;
    }

    FILE *fp = g_swap_files[fd];
    rewind(fp);

    int32_t numObjects = 0;
    if (fread(&numObjects, sizeof(int32_t), 1, fp) != 1 || numObjects < 0) {
        jclass exCls = (*env)->FindClass(env, "java/io/IOException");
        if (exCls) (*env)->ThrowNew(env, exCls, "error reading object count");
        return NULL;
    }

    jclass objClass = (*env)->FindClass(env, "java/lang/Object");
    jclass strClass = (*env)->FindClass(env, "java/lang/String");
    if (!objClass || !strClass) return NULL;

    jobjectArray mainArray = (*env)->NewObjectArray(env, numObjects, objClass, NULL);
    if (!mainArray) return NULL;

    for (int i = 0; i < numObjects; i++) {
        int32_t tag = 0, count = 0;
        if (fread(&tag, sizeof(int32_t), 1, fp) != 1 || fread(&count, sizeof(int32_t), 1, fp) != 1) {
            jclass exCls = (*env)->FindClass(env, "java/io/IOException");
            if (exCls) (*env)->ThrowNew(env, exCls, "Dump stream out of sync");
            return NULL;
        }

        switch (tag) {
            case 0: { // String
                jstring s = read_swapper_string(env, fp, count);
                if (s) {
                    (*env)->SetObjectArrayElement(env, mainArray, i, s);
                    (*env)->DeleteLocalRef(env, s);
                }
                break;
            }
            case 1: { // String[]
                jobjectArray strArray = (*env)->NewObjectArray(env, count, strClass, NULL);
                if (!strArray) return NULL;
                for (int j = 0; j < count; j++) {
                    int32_t subTag = 0, subCount = 0;
                    if (fread(&subTag, sizeof(int32_t), 1, fp) != 1 || fread(&subCount, sizeof(int32_t), 1, fp) != 1) {
                        jclass exCls = (*env)->FindClass(env, "java/io/IOException");
                        if (exCls) (*env)->ThrowNew(env, exCls, "Dump stream out of sync loading string array");
                        return NULL;
                    }
                    if (subTag == 0) {
                        jstring s = read_swapper_string(env, fp, subCount);
                        if (s) {
                            (*env)->SetObjectArrayElement(env, strArray, j, s);
                            (*env)->DeleteLocalRef(env, s);
                        }
                    } else if (subTag == 9) {
                        (*env)->SetObjectArrayElement(env, strArray, j, NULL);
                    } else {
                        jclass exCls = (*env)->FindClass(env, "java/io/IOException");
                        if (exCls) (*env)->ThrowNew(env, exCls, "Dump stream out of sync loading string array");
                        return NULL;
                    }
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, strArray);
                (*env)->DeleteLocalRef(env, strArray);
                break;
            }
            case 2: { // byte[]
                jbyteArray arr = (*env)->NewByteArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jbyte *buf = (jbyte *)malloc(count);
                    if (!buf) return NULL;
                    fread(buf, 1, count, fp);
                    (*env)->SetByteArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 3: { // char[]
                jcharArray arr = (*env)->NewCharArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jchar *buf = (jchar *)malloc(count * sizeof(jchar));
                    if (!buf) return NULL;
                    fread(buf, sizeof(jchar), count, fp);
                    (*env)->SetCharArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 4: { // short[]
                jshortArray arr = (*env)->NewShortArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jshort *buf = (jshort *)malloc(count * sizeof(jshort));
                    if (!buf) return NULL;
                    fread(buf, sizeof(jshort), count, fp);
                    (*env)->SetShortArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 5: { // int[]
                jintArray arr = (*env)->NewIntArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jint *buf = (jint *)malloc(count * sizeof(jint));
                    if (!buf) return NULL;
                    fread(buf, sizeof(jint), count, fp);
                    (*env)->SetIntArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 6: { // long[]
                jlongArray arr = (*env)->NewLongArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jlong *buf = (jlong *)malloc(count * sizeof(jlong));
                    if (!buf) return NULL;
                    fread(buf, sizeof(jlong), count, fp);
                    (*env)->SetLongArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 7: { // float[]
                jfloatArray arr = (*env)->NewFloatArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jfloat *buf = (jfloat *)malloc(count * sizeof(jfloat));
                    if (!buf) return NULL;
                    fread(buf, sizeof(jfloat), count, fp);
                    (*env)->SetFloatArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 8: { // double[]
                jdoubleArray arr = (*env)->NewDoubleArray(env, count);
                if (!arr) return NULL;
                if (count > 0) {
                    jdouble *buf = (jdouble *)malloc(count * sizeof(jdouble));
                    if (!buf) return NULL;
                    fread(buf, sizeof(jdouble), count, fp);
                    (*env)->SetDoubleArrayRegion(env, arr, 0, count, buf);
                    free(buf);
                }
                (*env)->SetObjectArrayElement(env, mainArray, i, arr);
                (*env)->DeleteLocalRef(env, arr);
                break;
            }
            case 9: { // null
                (*env)->SetObjectArrayElement(env, mainArray, i, NULL);
                break;
            }
            default: {
                jclass exCls = (*env)->FindClass(env, "java/io/IOException");
                if (exCls) (*env)->ThrowNew(env, exCls, "Dump stream out of sync: unknown tag");
                return NULL;
            }
        }
    }

    fseek(fp, 0, SEEK_END);
    return mainArray;
}

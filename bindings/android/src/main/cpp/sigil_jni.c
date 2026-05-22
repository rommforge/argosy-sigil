// SPDX-License-Identifier: MPL-2.0
#include "sigil.h"
#include <jni.h>
#include <string.h>

static jclass g_result_class = NULL;
static jmethodID g_result_ctor = NULL;

static void load_result_class(JNIEnv *env) {
    if (g_result_class) return;
    jclass cls = (*env)->FindClass(env, "com/nendo/sigil/SigilResult");
    if (!cls) return;
    g_result_class = (jclass)(*env)->NewGlobalRef(env, cls);
    /* SigilResult(titleId, rawSerial, saveId, platformSlug, source, usage) */
    g_result_ctor = (*env)->GetMethodID(env, g_result_class, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V");
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    (void)reserved;
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK || !env) return;
    if (g_result_class) {
        (*env)->DeleteGlobalRef(env, g_result_class);
        g_result_class = NULL;
        g_result_ctor = NULL;
    }
}

JNIEXPORT jstring JNICALL
Java_com_nendo_sigil_Sigil_nativeVersion(JNIEnv *env, jclass clazz) {
    (void)clazz;
    return (*env)->NewStringUTF(env, sigil_version());
}

JNIEXPORT jobject JNICALL
Java_com_nendo_sigil_Sigil_nativeExtract(JNIEnv *env, jclass clazz,
                                          jstring jpath,
                                          jstring jplatform_slug,
                                          jstring jprod_keys_path) {
    (void)clazz;
    if (!jpath) return NULL;

    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (!path) return NULL;

    const char *slug = NULL;
    if (jplatform_slug) slug = (*env)->GetStringUTFChars(env, jplatform_slug, NULL);

    const char *prod_keys = NULL;
    if (jprod_keys_path) prod_keys = (*env)->GetStringUTFChars(env, jprod_keys_path, NULL);

    sigil_platform hint = sigil_platform_from_slug(slug);

    sigil_support sup = {
        .struct_version = SIGIL_SUPPORT_V1,
        .switch_prod_keys_path = prod_keys,
    };
    sigil_options opts = {
        .struct_version = SIGIL_OPTIONS_V1,
        .support = prod_keys ? &sup : NULL,
        .flags = SIGIL_FLAG_FILENAME_FALLBACK,
    };

    sigil_result r;
    int rc = sigil_extract_from_path(path, hint, &opts, &r);

    (*env)->ReleaseStringUTFChars(env, jpath, path);
    if (slug)      (*env)->ReleaseStringUTFChars(env, jplatform_slug, slug);
    if (prod_keys) (*env)->ReleaseStringUTFChars(env, jprod_keys_path, prod_keys);

    if (rc != SIGIL_OK) return NULL;

    load_result_class(env);
    if (!g_result_class || !g_result_ctor) return NULL;

    jstring jtitle   = (*env)->NewStringUTF(env, r.title_id);
    jstring jraw     = (*env)->NewStringUTF(env, r.raw_serial);
    jstring jsave_id = (*env)->NewStringUTF(env, r.save_id);
    jstring jslug    = (*env)->NewStringUTF(env, sigil_platform_to_slug(r.platform));

    return (*env)->NewObject(env, g_result_class, g_result_ctor,
                             jtitle, jraw, jsave_id, jslug,
                             (jint)r.source, (jint)r.usage);
}

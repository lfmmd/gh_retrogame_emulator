/* DO NOT EDIT THIS FILE - it is machine generated */
#include "javavm/export/jni.h"
/* Header for class java/lang/Runtime */

#ifndef _CVM_JNI_java_lang_Runtime
#define _CVM_JNI_java_lang_Runtime
#ifdef __cplusplus
extern "C"{
#endif
/*
 * Class:	java/lang/Runtime
 * Method:	safeExit
 * Signature:	(I)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_Runtime_safeExit
  (JNIEnv *, jclass, jint);

/*
 * Class:	java/lang/Runtime
 * Method:	execInternal
 * Signature:	([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Process;
 */
JNIEXPORT jobject JNICALL Java_java_lang_Runtime_execInternal
  (JNIEnv *, jobject, jobjectArray, jobjectArray, jstring);

/*
 * Class:	java/lang/Runtime
 * Method:	freeMemory
 * Signature:	()J
 */
JNIEXPORT jlong JNICALL Java_java_lang_Runtime_freeMemory
  (JNIEnv *, jobject);

/*
 * Class:	java/lang/Runtime
 * Method:	totalMemory
 * Signature:	()J
 */
JNIEXPORT jlong JNICALL Java_java_lang_Runtime_totalMemory
  (JNIEnv *, jobject);

/*
 * Class:	java/lang/Runtime
 * Method:	maxMemory
 * Signature:	()J
 */
JNIEXPORT jlong JNICALL Java_java_lang_Runtime_maxMemory
  (JNIEnv *, jobject);

/*
 * Class:	java/lang/Runtime
 * Method:	gc
 * Signature:	()V
 */
JNIEXPORT void JNICALL Java_java_lang_Runtime_gc
  (JNIEnv *, jobject);

/*
 * Class:	java/lang/Runtime
 * Method:	runFinalization0
 * Signature:	()V
 */
JNIEXPORT void JNICALL Java_java_lang_Runtime_runFinalization0
  (JNIEnv *, jclass);

/*
 * Class:	java/lang/Runtime
 * Method:	traceInstructions
 * Signature:	(Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_Runtime_traceInstructions
  (JNIEnv *, jobject, jboolean);

/*
 * Class:	java/lang/Runtime
 * Method:	traceMethodCalls
 * Signature:	(Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_Runtime_traceMethodCalls
  (JNIEnv *, jobject, jboolean);

#ifdef __cplusplus
}
#endif
#endif

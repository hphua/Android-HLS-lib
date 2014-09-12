/*
 * debug.h
 *
 *  Created on: Apr 14, 2014
 *      Author: Mark
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <android/log.h>

#define USE_AUDIO

#define _DEBUG_E
#define _DEBUG_W
#define _DEBUG_I
//#define _DEBUG_V
//#define _DEBUG_V2
//#define _DEBUG_RUN

//#define _DATA_MINING
//#define _DEBUG_SCREENINFO
//#define _SYMBOL_ERRORS
//#define _TIMING


//#define _VERBOSE
#ifdef _DEBUG_E
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, __func__, __VA_ARGS__)
#else
#define LOGE(...)
#endif

#ifdef _DEBUG_I
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, __func__, __VA_ARGS__)
#else
#define LOGI(...)
#endif

#ifdef _DEBUG_W
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, __func__, __VA_ARGS__)
#else
#define LOGW(...)
#endif

#ifdef _DEBUG_V
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, __func__, __VA_ARGS__)
#else
#define LOGV(...)
#endif

#ifdef _DEBUG_V2
#define LOGV2(...) __android_log_print(ANDROID_LOG_VERBOSE, __func__, __VA_ARGS__)
#else
#define LOGV2(...)
#endif

#ifdef _DEBUG_SCREENINFO
#define LOGSCREENINFO(...) __android_log_print(ANDROID_LOG_VERBOSE, __func__, __VA_ARGS__)
#else
#define LOGSCREENINFO(...)
#endif

#ifdef _DATA_MINING
#define LOGDATAMINING(...) __android_log_print(ANDROID_LOG_VERBOSE, __func__, __VA_ARGS__)
#else
#define LOGDATAMINING(...)
#endif

#ifdef _SYMBOL_ERRORS
#define LOGSYMBOLERROR(...) __android_log_print(ANDROID_LOG_VERBOSE, __func__, __VA_ARGS__)
#else
#define LOGSYMBOLERROR(...)
#endif

#ifdef _TIMING
#define LOGTIMING(...) __android_log_print(ANDROID_LOG_INFO, __func__, __VA_ARGS__)
#else
#define LOGTIMING(...)
#endif

#ifdef _DEBUG_RUN
#define RUNDEBUG(x)  (x)
#else
#define RUNDEBUG(x)
#endif

#endif /* DEBUG_H_ */

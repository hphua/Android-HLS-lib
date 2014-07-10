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
//#define LOGV

//#define _VERBOSE
#ifdef _DEBUG_E
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
#define LOGE(...)
#endif

#ifdef _DEBUG_I
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
#define LOGI(...)
#endif

#ifdef _DEBUG_W
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
#define LOGW(...)
#endif

#ifdef _DEBUG_V
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
#define LOGV(...)
#endif

#ifdef LOGV
#define RUNDEBUG(x)  (x)
#else
#define RUNDEBUG(x)
#endif

#endif /* DEBUG_H_ */

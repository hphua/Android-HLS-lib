/*
 * debug.h
 *
 *  Created on: Apr 14, 2014
 *      Author: Mark
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#define _DEBUG
//#define _VERBOSE

#define LOGERROR(x, ...) __android_log_print(ANDROID_LOG_ERROR, (x), __VA_ARGS__)

#ifdef _DEBUG
#define LOGINFO(x, ...) __android_log_print(ANDROID_LOG_INFO, (x), __VA_ARGS__)
#ifdef _VERBOSE
#define RUNDEBUG(x)  (x)
#else
#define RUNDEBUG(x)
#endif
#else
#define LOGINFO(x, ...)
#define RUNDEBUG(x)
#endif

#endif /* DEBUG_H_ */

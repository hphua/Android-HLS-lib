/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef A_DEBUG_H_

#define A_DEBUG_H_

#include <string.h>

#include "ABase.h"
#include "AString.h"
#include <android/log.h>

namespace android {

#define LITERAL_TO_STRING_INTERNAL(x)    #x
#define LITERAL_TO_STRING(x) LITERAL_TO_STRING_INTERNAL(x)

#define CHECK(condition)                                \
    LOG_ALWAYS_FATAL_IF(                                \
            !(condition),                               \
            "%s",                                       \
            __FILE__ ":" LITERAL_TO_STRING(__LINE__)    \
            " CHECK(" #condition ") failed.")

#define MAKE_COMPARATOR(suffix,op)                          \
    template<class A, class B>                              \
    AString Compare_##suffix(const A &a, const B &b) {      \
        AString res;                                        \
        if (!(a op b)) {                                    \
            res.append(a);                                  \
            res.append(" vs. ");                            \
            res.append(b);                                  \
        }                                                   \
        return res;                                         \
    }

MAKE_COMPARATOR(EQ,==)
MAKE_COMPARATOR(NE,!=)
MAKE_COMPARATOR(LE,<=)
MAKE_COMPARATOR(GE,>=)
MAKE_COMPARATOR(LT,<)
MAKE_COMPARATOR(GT,>)

#define CHECK_OP(x,y,suffix,op)                                         \
    do {                                                                \
        AString ___res = Compare_##suffix(x, y);                        \
        if (!___res.empty()) {                                          \
            AString ___full =                                           \
                __FILE__ ":" LITERAL_TO_STRING(__LINE__)                \
                    " CHECK_" #suffix "( " #x "," #y ") failed: ";      \
            ___full.append(___res);                                     \
                                                                        \
            LOG_ALWAYS_FATAL("%s", ___full.c_str());                    \
        }                                                               \
    } while (false)

#define CHECK_EQ(x,y)   CHECK_OP(x,y,EQ,==)
#define CHECK_NE(x,y)   CHECK_OP(x,y,NE,!=)
#define CHECK_LE(x,y)   CHECK_OP(x,y,LE,<=)
#define CHECK_LT(x,y)   CHECK_OP(x,y,LT,<)
#define CHECK_GE(x,y)   CHECK_OP(x,y,GE,>=)
#define CHECK_GT(x,y)   CHECK_OP(x,y,GT,>)

#define TRESPASS() \
        LOG_ALWAYS_FATAL(                                       \
            __FILE__ ":" LITERAL_TO_STRING(__LINE__)            \
                " Should not be here.");

#ifndef ALOG
#define ALOG(...) __android_log_print(__VA_ARGS__)
#endif

/*
 * Simplified macro to send a verbose log message using the current LOG_TAG.
 */
#ifndef ALOGV

#if LOG_NDEBUG
#define ALOGV(...)   ((void)0)
#else
#define ALOGV(...) ((void)ALOG(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
// Temporary measure for code still using old LOG macros.
#ifndef LOGV
#define LOGV ALOGV
#endif
#endif
#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))
#ifndef ALOGV_IF
#if LOG_NDEBUG
#define ALOGV_IF(cond, ...)   ((void)0)
#else
#define ALOGV_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
// Temporary measure for code still using old LOG macros.
#ifndef LOGV_IF
#define LOGV_IF ALOGV_IF
#endif
#endif
/*
 * Simplified macro to send a debug log message using the current LOG_TAG.
 */
#ifndef LOGD
#define LOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define ALOGD LOGD
#endif
#ifndef LOGD_IF
#define LOGD_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#define ALOGD_IF LOGD_IF
#endif
/*
 * Simplified macro to send an info log message using the current LOG_TAG.
 */
#ifndef LOGI
#define LOGI(...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#define ALOGI LOGI
#endif
#ifndef LOGI_IF
#define LOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#define ALOGI_IF LOGI_IF
#endif
/*
 * Simplified macro to send a warning log message using the current LOG_TAG.
 */
#ifndef LOGW
#define LOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#define ALOGW LOGW
#endif
#ifndef LOGW_IF
#define LOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#define ALOGW_IF LOGW_IF
#endif
/*
 * Simplified macro to send an error log message using the current LOG_TAG.
 */
#ifndef LOGE
#define LOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#define ALOGE LOGE
#endif
#ifndef LOGE_IF
#define LOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#define ALOGE_IF LOGE_IF
#endif
// ---------------------------------------------------------------------
/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * verbose priority.
 */
#ifndef IF_ALOGV
#if LOG_NDEBUG
#define IF_ALOGV() if (false)
#else
#define IF_ALOGV() IF_ALOG(LOG_VERBOSE, LOG_TAG)
#endif
// Temporary measure for code still using old LOG macros.
#ifndef IF_LOGV
#define IF_LOGV IF_ALOGV
#endif
#endif
/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * debug priority.
 */
#ifndef IF_LOGD
#define IF_LOGD() IF_ALOG(LOG_DEBUG, LOG_TAG)
#define IF_ALOGD IF_LOGD
#endif
/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * info priority.
 */
#ifndef IF_LOGI
#define IF_LOGI() IF_ALOG(LOG_INFO, LOG_TAG)
#define IF_ALOGI IF_LOGI
#endif
/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * warn priority.
 */
#ifndef IF_LOGW
#define IF_LOGW() IF_ALOG(LOG_WARN, LOG_TAG)
#define IF_ALOGW IF_LOGW
#endif
/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * error priority.
 */
#ifndef IF_LOGE
#define IF_LOGE() IF_ALOG(LOG_ERROR, LOG_TAG)
#define IF_ALOGE IF_LOGE
#endif
// ---------------------------------------------------------------------
/*
 * Simplified macro to send a verbose system log message using the current LOG_TAG.
 */
#ifndef SLOGV
#if LOG_NDEBUG
#define SLOGV(...)   ((void)0)
#else
#define SLOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif
#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))
#ifndef SLOGV_IF
#if LOG_NDEBUG
#define SLOGV_IF(cond, ...)   ((void)0)
#else
#define SLOGV_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
#endif
/*
 * Simplified macro to send a debug system log message using the current LOG_TAG.
 */
#ifndef SLOGD
#define SLOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif
#ifndef SLOGD_IF
#define SLOGD_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
/*
 * Simplified macro to send an info system log message using the current LOG_TAG.
 */
#ifndef SLOGI
#define SLOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif
#ifndef SLOGI_IF
#define SLOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
/*
 * Simplified macro to send a warning system log message using the current LOG_TAG.
 */
#ifndef SLOGW
#define SLOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif
#ifndef SLOGW_IF
#define SLOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
/*
 * Simplified macro to send an error system log message using the current LOG_TAG.
 */
#ifndef SLOGE
#define SLOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif
#ifndef SLOGE_IF
#define SLOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
    

/*
 * Log a fatal error.  If the given condition fails, this stops program
 * execution like a normal assertion, but also generating the given message.
 * It is NOT stripped from release builds.  Note that the condition test
 * is -inverted- from the normal assert() semantics.
 */
#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, " -- " __VA_ARGS__)) \
    : (void)0 )
#endif
#ifndef LOG_ALWAYS_FATAL
#define LOG_ALWAYS_FATAL(...) \
    ( ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ## __VA_ARGS__)) )
#endif
/*
 * Versions of LOG_ALWAYS_FATAL_IF and LOG_ALWAYS_FATAL that
 * are stripped out of release builds.
 */
#if LOG_NDEBUG
#ifndef LOG_FATAL_IF
#define LOG_FATAL_IF(cond, ...) ((void)0)
#endif
#ifndef LOG_FATAL
#define LOG_FATAL(...) ((void)0)
#endif
#else
#ifndef LOG_FATAL_IF
#define LOG_FATAL_IF(cond, ...) LOG_ALWAYS_FATAL_IF(cond, ## __VA_ARGS__)
#endif
#ifndef LOG_FATAL
#define LOG_FATAL(...) LOG_ALWAYS_FATAL(__VA_ARGS__)
#endif
#endif
/*
 * Assertion that generates a log message when the assertion fails.
 * Stripped out of release builds.  Uses the current LOG_TAG.
 */
#ifndef LOG_ASSERT
#define LOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ## __VA_ARGS__)
//#define LOG_ASSERT(cond) LOG_FATAL_IF(!(cond), "Assertion failed: " #cond)
#define ALOG_ASSERT LOG_ASSERT
#endif


}  // namespace android


#endif  // A_DEBUG_H_


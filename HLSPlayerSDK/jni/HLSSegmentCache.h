#ifndef _HLSSEGMENTCACHE_H_
#define _HLSSEGMENTCACHE_H_

#include <jni.h>
#include <sys/types.h>

#include "debug.h"

// Interface to the HLSSegmentCache Java subsystem.
class HLSSegmentCache
{
private:
	static JavaVM *mJVM;
	static jmethodID mPrecache;
	static jmethodID mRead;
	static jmethodID mGetSize;
	static jmethodID mTouch;
	static jclass mClass;

public:
    static void initialize(JavaVM *jvm);
    static void precache(const char *uri, int cryptoId = -1);
    static int64_t read(const char *uri, int64_t offset, int64_t size, void *bytes);
    static int64_t getSize(const char *uri);
    static void touch(const char* uri);
};


#endif

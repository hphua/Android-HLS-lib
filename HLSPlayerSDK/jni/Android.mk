LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := HLSPlayerSDK
LOCAL_SRC_FILES := HLSPlayerSDK.cpp HLSSegment.cpp HLSPlayer.cpp androidVideoShim.cpp androidVideoShim_ColorConverter.cpp AudioTrack.cpp HLSSegmentCache.cpp RefCounted.cpp aes.c
LOCAL_CFLAGS += -DHAVE_SYS_UIO_H -Wno-multichar -Wno-pmf-conversions -g
LOCAL_C_INCLUDES += $(TOP)/system/core/include

LOCAL_LDLIBS += -lz -lm -llog -landroid

include $(BUILD_SHARED_LIBRARY)

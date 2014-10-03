LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := HLSPlayerSDK

LOCAL_SRC_FILES := HLSPlayerSDK.cpp HLSSegment.cpp HLSPlayer.cpp AudioTrack.cpp  RefCounted.cpp 
LOCAL_SRC_FILES += androidVideoShim.cpp androidVideoShim_ColorConverter.cpp 
LOCAL_SRC_FILES += aes.c
LOCAL_SRC_FILES += HLSSegmentCache.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/MPEG2TSExtractor.cpp mpeg2ts_parser/ATSParser.cpp mpeg2ts_parser/VectorImpl.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/SharedBuffer.cpp mpeg2ts_parser/AnotherPacketSource.cpp mpeg2ts_parser/ABuffer.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/ABitReader.cpp mpeg2ts_parser/AMessage.cpp

LOCAL_CFLAGS += -DHAVE_SYS_UIO_H -Wno-multichar -Wno-pmf-conversions -g
LOCAL_C_INCLUDES += $(TOP)/system/core/include

LOCAL_LDLIBS += -lz -lm -llog -landroid

include $(BUILD_SHARED_LIBRARY)

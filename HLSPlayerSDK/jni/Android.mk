LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := HLSPlayerSDK

# Core Player Code
LOCAL_SRC_FILES += HLSPlayerSDK.cpp HLSSegment.cpp HLSPlayer.cpp AudioTrack.cpp  RefCounted.cpp 
LOCAL_SRC_FILES += androidVideoShim.cpp androidVideoShim_ColorConverter.cpp androidVideoShim_ColorConverter444.cpp
LOCAL_SRC_FILES += aes.c
LOCAL_SRC_FILES += HLSSegmentCache.cpp

# MPEG 2 TS Extractor
LOCAL_SRC_FILES += mpeg2ts_parser/AAtomizer.cpp mpeg2ts_parser/ABitReader.cpp mpeg2ts_parser/ABuffer.cpp mpeg2ts_parser/AMessage.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/AnotherPacketSource.cpp mpeg2ts_parser/AString.cpp mpeg2ts_parser/ATSParser.cpp mpeg2ts_parser/avc_utils.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/base64.cpp mpeg2ts_parser/ESQueue.cpp mpeg2ts_parser/hexdump.cpp mpeg2ts_parser/MPEG2TSExtractor.cpp 
LOCAL_SRC_FILES += mpeg2ts_parser/SharedBuffer.cpp mpeg2ts_parser/VectorImpl.cpp

LOCAL_CFLAGS += -DHAVE_SYS_UIO_H -Wno-multichar -Wno-pmf-conversions -g

# -fdump-class-hierarchy
LOCAL_C_INCLUDES += $(TOP)/system/core/include ./libyuv/

LOCAL_LDLIBS += -lz -lm -llog -landroid

include $(BUILD_SHARED_LIBRARY)

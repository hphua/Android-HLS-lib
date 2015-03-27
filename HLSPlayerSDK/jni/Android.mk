LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := HLSPlayerSDK

aacdec_sources := $(wildcard $(LOCAL_PATH)/fdk-aac-master/libAACdec/src/*.cpp)
aacdec_sources := $(aacdec_sources:$(LOCAL_PATH)/fdk-aac-master/libAACdec/src/%=%)

fdk_sources := $(wildcard $(LOCAL_PATH)/fdk-aac-master/libFDK/src/*.cpp)
fdk_sources := $(fdk_sources:$(LOCAL_PATH)/fdk-aac-master/libFDK/src/%=%)

sys_sources := $(wildcard $(LOCAL_PATH)/fdk-aac-master/libSYS/src/*.cpp)
sys_sources := $(sys_sources:$(LOCAL_PATH)/fdk-aac-master/libSYS/src/%=%)

pcmutils_sources := $(wildcard $(LOCAL_PATH)/fdk-aac-master/libPCMutils/src/*.cpp)
pcmutils_sources := $(pcmutils_sources:$(LOCAL_PATH)/fdk-aac-master/libPCMutils/src/%=%)

mpegtpdec_sources := $(wildcard $(LOCAL_PATH)/fdk-aac-master/libMpegTPDec/src/*.cpp)
mpegtpdec_sources := $(mpegtpdec_sources:$(LOCAL_PATH)/fdk-aac-master/libMpegTPDec/src/%=%)

sbrdec_sources := $(wildcard $(LOCAL_PATH)/fdk-aac-master/libSBRdec/src/*.cpp)
sbrdec_sources := $(sbrdec_sources:$(LOCAL_PATH)/fdk-aac-master/libSBRdec/src/%=%)

# Core Player Code
LOCAL_SRC_FILES += HLSPlayerSDK.cpp HLSSegment.cpp HLSPlayer.cpp AudioTrack.cpp  RefCounted.cpp 
LOCAL_SRC_FILES += androidVideoShim.cpp androidVideoShim_ColorConverter.cpp androidVideoShim_ColorConverter444.cpp
LOCAL_SRC_FILES += aes.c AudioPlayer.cpp AudioFDK.cpp ESDS.cpp
LOCAL_SRC_FILES += HLSSegmentCache.cpp debug.cpp

# MPEG 2 TS Extractor
LOCAL_SRC_FILES += mpeg2ts_parser/AAtomizer.cpp mpeg2ts_parser/ABitReader.cpp mpeg2ts_parser/ABuffer.cpp mpeg2ts_parser/AMessage.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/AnotherPacketSource.cpp mpeg2ts_parser/AString.cpp mpeg2ts_parser/ATSParser.cpp mpeg2ts_parser/avc_utils.cpp
LOCAL_SRC_FILES += mpeg2ts_parser/base64.cpp mpeg2ts_parser/ESQueue.cpp mpeg2ts_parser/hexdump.cpp mpeg2ts_parser/MPEG2TSExtractor.cpp 
LOCAL_SRC_FILES += mpeg2ts_parser/SharedBuffer.cpp mpeg2ts_parser/VectorImpl.cpp

# AACDEC
LOCAL_SRC_FILES += $(aacdec_sources:%=fdk-aac-master/libAACdec/src/%)
LOCAL_SRC_FILES += $(fdk_sources:%=fdk-aac-master/libFDK/src/%)
LOCAL_SRC_FILES += $(sys_sources:%=fdk-aac-master/libSYS/src/%)
LOCAL_SRC_FILES += $(mpegtpdec_sources:%=fdk-aac-master/libMpegTPDec/src/%)
LOCAL_SRC_FILES += $(sbrdec_sources:%=fdk-aac-master/libSBRdec/src/%)
LOCAL_SRC_FILES += $(pcmutils_sources:%=fdk-aac-master/libPCMutils/src/%)

LOCAL_CFLAGS += -DHAVE_SYS_UIO_H -Wno-multichar -Wno-pmf-conversions -g

# -fdump-class-hierarchy
LOCAL_C_INCLUDES += $(TOP)/system/core/include ./libyuv/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fdk-aac-master/libAACdec/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fdk-aac-master/libFDK/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fdk-aac-master/libSYS/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fdk-aac-master/libMpegTPDec/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fdk-aac-master/libSBRdec/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fdk-aac-master/libPCMutils/include


LOCAL_LDLIBS += -lz -lm -llog -landroid

include $(BUILD_SHARED_LIBRARY)

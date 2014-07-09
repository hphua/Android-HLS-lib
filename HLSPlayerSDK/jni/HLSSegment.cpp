/*
 * HLSSegment.cpp
 *
 *  Created on: Apr 29, 2014
 *      Author: Mark
 */
#include <android/native_window.h>
#include <android/window.h>
//#include <../android-source/frameworks/av/include/media/stagefright/MediaDefs.h>


#include "constants.h"
#include "debug.h"
#include "HLSSegment.h"

//using namespace android;


#define CLASS_NAME APP_NAME"::HLSSegment"


#define METHOD CLASS_NAME"::HLSSegment()"
HLSSegment::HLSSegment(int32_t quality, double time) : mWidth(0), mHeight(0), mBitrate(0), mExtractorFlags(0), mActiveAudioTrackIndex(0), mStartTime(time), mQuality(quality)
{

}

#define METHOD CLASS_NAME"::~HLSSegment()"
HLSSegment::~HLSSegment()
{

}

//#define METHOD CLASS_NAME"::SetDataSource()"
//bool HLSSegment::SetDataSource(android::sp<android::DataSource> dataSource)
//{
//	return true;
//}

//#define METHOD CLASS_NAME"::SetAudioTrack()"
//void HLSSegment::SetAudioTrack(sp<MediaSource> source)
//{
//	mAudioTrack = source;
//}
//
//#define METHOD CLASS_NAME"::SetVideoTrack()"
//void HLSSegment::SetVideoTrack(sp<MediaSource> source)
//{
//	mVideoTrack = source;
//}

//#define METHOD CLASS_NAME"::GetAudioTrack()"
//sp<MediaSource> HLSSegment::GetAudioTrack()
//{
//	return mAudioTrack;
//}
//
//#define METHOD CLASS_NAME"::GetVideoTrack()"
//sp<MediaSource> HLSSegment::GetVideoTrack()
//{
//	return mVideoTrack;
//}

#define METHOD CLASS_NAME"::GetWidth()"
int32_t HLSSegment::GetWidth()
{
	return mWidth;
}

#define METHOD CLASS_NAME"::GetHeight()"
int32_t HLSSegment::GetHeight()
{
	return mHeight;
}

#define METHOD CLASS_NAME"::GetQuality()"
int32_t HLSSegment::GetQuality()
{
	return mQuality;
}

#define METHOD CLASS_NAME"::GetStartTime()"
double HLSSegment::GetStartTime()
{
	return mStartTime;
}
#define METHOD CLASS_NAME"::GetBitrate()"
int64_t HLSSegment::GetBitrate()
{
	return mBitrate;
}

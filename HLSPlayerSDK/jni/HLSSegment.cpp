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



HLSSegment::HLSSegment(int32_t quality, double time) : mWidth(0), mHeight(0), mBitrate(0), mExtractorFlags(0), mActiveAudioTrackIndex(0), mStartTime(time), mQuality(quality)
{

}

HLSSegment::~HLSSegment()
{

}

//bool HLSSegment::SetDataSource(android::sp<android::DataSource> dataSource)
//{
//	return true;
//}

//void HLSSegment::SetAudioTrack(sp<MediaSource> source)
//{
//	mAudioTrack = source;
//}
//
//void HLSSegment::SetVideoTrack(sp<MediaSource> source)
//{
//	mVideoTrack = source;
//}

//sp<MediaSource> HLSSegment::GetAudioTrack()
//{
//	return mAudioTrack;
//}
//
//sp<MediaSource> HLSSegment::GetVideoTrack()
//{
//	return mVideoTrack;
//}

int32_t HLSSegment::GetWidth()
{
	return mWidth;
}

int32_t HLSSegment::GetHeight()
{
	return mHeight;
}

int32_t HLSSegment::GetQuality()
{
	return mQuality;
}

double HLSSegment::GetStartTime()
{
	return mStartTime;
}
int64_t HLSSegment::GetBitrate()
{
	return mBitrate;
}

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



HLSSegment::HLSSegment(int32_t quality, double time) : mStartTime(time), mQuality(quality)
{

}

HLSSegment::~HLSSegment()
{

}

int32_t HLSSegment::GetQuality()
{
	return mQuality;
}

double HLSSegment::GetStartTime()
{
	return mStartTime;
}


/*
 * HLSMediaSourceAdapter.cpp
 *
 *  Created on: May 2, 2014
 *      Author: Mark
 */

#include <stddef.h>
#include "constants.h"
#include "debug.h"
#include "HLSMediaSourceAdapter.h"


#define CLASS_NAME APP_NAME"::HLSMediaSourceAdapter"

using namespace std;
using namespace android;

#define METHOD CLASS_NAME"::HLSMediaSourceAdapter()"
HLSMediaSourceAdapter::HLSMediaSourceAdapter() : mCurrentSource(NULL), mStartMetadata(NULL), mIsAudio(false),
												 mNeedMoreSegments(NULL), mWidth(0), mHeight(0), mCropWidth(0), mCropHeight(0),
												 mFrameTimeDelta(0), mLastFrameTime(0), mTimestampOffset(0), mLastTimestamp(0)
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
}

#define METHOD CLASS_NAME"::~HLSMediaSourceAdapter()"
HLSMediaSourceAdapter::~HLSMediaSourceAdapter()
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	mCurrentSource->stop();
	while (mSources.size() > 0)
	{
		mCurrentSource = mSources.front();
		mCurrentSource->stop();
		mSources.pop_front();
	}
}

#define METHOD CLASS_NAME"::setIsAudio()"
int HLSMediaSourceAdapter::getSegmentCount()
{
	return mSources.size();
}

void HLSMediaSourceAdapter::setNeedMoreSegmentsCallback(void (*needMoreSegments)())
{
	mNeedMoreSegments = needMoreSegments;
}

#define METHOD CLASS_NAME"::setIsAudio()"
void HLSMediaSourceAdapter::setIsAudio(bool val)
{
	mIsAudio = val;
}

#define METHOD CLASS_NAME"::append()"
void HLSMediaSourceAdapter::append(sp<MediaSource> source)
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	if (mCurrentSource == NULL) mCurrentSource = source;
	else mSources.push_back(source);
}

#define METHOD CLASS_NAME"::clear()"
void HLSMediaSourceAdapter::clear()
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	if (mCurrentSource != NULL) mCurrentSource->stop();
	mCurrentSource = NULL;
	mSources.clear();
}

#define METHOD CLASS_NAME"::start()"
status_t HLSMediaSourceAdapter::start(android::MetaData * params /* = NULL */)
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	if (params != NULL)
	{
		mStartMetadata = new MetaData(*params);
	}
	if (mCurrentSource == NULL && mSources.size() == 0)
	{
		LOGINFO(METHOD, "%s: We don't seem to have any more sources", mIsAudio?"AUDIO":"VIDEO");
		return ERROR_END_OF_STREAM;
	}
	if (mCurrentSource == NULL)
	{
		mCurrentSource = mSources.front();
		mSources.pop_front();
		LOGINFO(METHOD, "Remaining Segments = %d", mSources.size());
	}

	if (!mIsAudio)
	{
//		sp<MetaData> meta = mCurrentSource->getFormat();
//		meta->findInt32(kKeyWidth, &mWidth);
//		meta->findInt32(kKeyHeight, &mHeight);
//		int32_t left, top;
//		meta->findRect(kKeyCropRect, &left, &top, &mCropWidth, &mCropHeight);

	}

	status_t res = mCurrentSource->start(params);
	LOGINFO(METHOD, "mCurrentSource->start() returned %s", strerror(-res));
	return res;
}

#define METHOD CLASS_NAME"::stop()"
status_t HLSMediaSourceAdapter::stop()
{
	//LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	status_t res = mCurrentSource->stop();
	return res;
}

#define METHOD CLASS_NAME"::getFormat()"
sp<MetaData> HLSMediaSourceAdapter::getFormat()
{
	//LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	sp<MetaData> meta = mCurrentSource->getFormat();
	return meta;
}

#define METHOD CLASS_NAME"::read()"
status_t HLSMediaSourceAdapter::read(MediaBuffer **buffer, const ReadOptions *options /* = NULL*/)
{
	//LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	status_t res;
	//for (;;)
	{
		if (options != NULL)
		{
			int64_t seekTime;
			ReadOptions::SeekMode seekMode;
			if (options->getSeekTo(&seekTime, &seekMode ))
			{
				LOGINFO(METHOD, "%s: SeekTime=%lld SeekMode=%d", mIsAudio?"AUDIO":"VIDEO", seekTime, seekMode );
			}
			else
			{
				LOGINFO(METHOD, "%s getSeekTo() failed", mIsAudio?"AUDIO":"VIDEO");
			}
		}
		res = mCurrentSource->read(buffer, options);
		if (res == ERROR_END_OF_STREAM)
		{
			LOGINFO(METHOD, "%s: read error = ERROR_END_OF_STREAM", mIsAudio?"AUDIO":"VIDEO");

			stop();
			mCurrentSource = NULL;
			if (start(mStartMetadata.get()) == OK)
			{
				LOGINFO(METHOD, "%s: Next Segment Started", mIsAudio?"AUDIO":"VIDEO");
				//if (mIsAudio) (*buffer)->release();
				res = mCurrentSource->read(buffer, options);
			}
		}
		else if (res != OK)
		{
			LOGINFO(METHOD, "%s: read error = %s", mIsAudio?"AUDIO":"VIDEO", strerror(-res));
			return res;
		}

		//if (mIsAudio)
		{
			return res;
		}


//		if ((*buffer)->range_length() != 0)
//		{
//			// We have a valid buffer
//			break;
//		}
//		else
//		{
//			// Release the buffer because we're going to try to get a new one!
//			(*buffer)->release();
//			(*buffer) == NULL;
//		}
	}

//	if (res == OK && !mIsAudio)
//	{
//		int64_t timeUs;
//		int64_t newTimeUs;
//		bool rval = (*buffer)->meta_data()->findInt64(kKeyTime, &timeUs);
//
//		if (rval)
//		{
//			// The assumption here is that if timeUs is not less than the lastTimestamp, then the timestamps are consecutive
//			if (timeUs < mLastTimestamp)
//			{
//				LOGINFO(METHOD, "%s: Updating Timestamp Time=%lld | Last Time=%lld", mIsAudio?"AUDIO":"VIDEO", timeUs, mLastTimestamp);
//				// we have a new segment - time to calculate the offset
//				mTimestampOffset = (mLastFrameTime - timeUs) + mFrameTimeDelta;
//			}
//
//			mLastTimestamp = timeUs;
//			newTimeUs = timeUs + mTimestampOffset;
//			mFrameTimeDelta = newTimeUs - mLastFrameTime;
//			mLastFrameTime = newTimeUs;
//
//			LOGINFO(METHOD, "%s: Time=%lld | Last Time=%lld | New Time=%lld | Delta=%lld | Offset=%lld", mIsAudio?"AUDIO":"VIDEO",
//									timeUs, mLastFrameTime, newTimeUs, mFrameTimeDelta, mTimestampOffset);
//
//
//			(*buffer)->meta_data()->setInt64(kKeyTime, newTimeUs);
//
//		}
//	}

	return res;
}

#define METHOD CLASS_NAME"::pause()"
status_t HLSMediaSourceAdapter::pause()
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	status_t res = mCurrentSource->pause();
	return res;
}

#define METHOD CLASS_NAME"::signalBufferReturned()"
// from MediaBufferObserver
void HLSMediaSourceAdapter::signalBufferReturned(MediaBuffer *buffer)
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	((MediaBufferObserver*)mCurrentSource.get())->signalBufferReturned(buffer);
}

size_t HLSMediaSourceAdapter::getWidth()
{
	return mWidth;
}
size_t HLSMediaSourceAdapter::getHeight()
{
	return mHeight;
}
size_t HLSMediaSourceAdapter::getCropWidth()
{
	return mCropWidth;
}
size_t HLSMediaSourceAdapter::getCropHeight()
{
	return mCropHeight;
}



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
HLSMediaSourceAdapter::HLSMediaSourceAdapter() : mCurrentSource(NULL), mStartMetadata(NULL), mIsAudio(false)
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
		LOGINFO(METHOD, "We don't seem to have any sources set, yet!");
		return INVALID_OPERATION;
	}
	if (mCurrentSource == NULL)
	{
		mCurrentSource = mSources.front();
		mSources.pop_front();
	}

	if (!mIsAudio)
	{
		sp<MetaData> meta = mCurrentSource->getFormat();
		meta->findInt32(kKeyWidth, &mWidth);
		meta->findInt32(kKeyHeight, &mHeight);
		int32_t left, top;
		meta->findRect(kKeyCropRect, &left, &top, &mCropWidth, &mCropHeight);

	}

	status_t res = mCurrentSource->start(params);
	LOGINFO(METHOD, "mCurrentSource->start() returned %s", strerror(-res));
	return res;
}

#define METHOD CLASS_NAME"::stop()"
status_t HLSMediaSourceAdapter::stop()
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	status_t res = mCurrentSource->stop();
	return res;
}

#define METHOD CLASS_NAME"::getFormat()"
sp<MetaData> HLSMediaSourceAdapter::getFormat()
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	sp<MetaData> meta = mCurrentSource->getFormat();
	return meta;
}

#define METHOD CLASS_NAME"::read()"
status_t HLSMediaSourceAdapter::read(MediaBuffer **buffer, const ReadOptions *options /* = NULL*/)
{
	LOGINFO(METHOD, "%s Entered", mIsAudio?"AUDIO":"VIDEO");
	status_t res = mCurrentSource->read(buffer, options);
	if (res == ERROR_END_OF_STREAM)
	{
		LOGINFO(METHOD, "read error = ERROR_END_OF_STREAM");

		stop();
		mCurrentSource = NULL;
		if (start(mStartMetadata.get()) == OK)
		{
			//if (mIsAudio) (*buffer)->release();
			res = mCurrentSource->read(buffer, options);
			if (res == INFO_FORMAT_CHANGED)
			{
				LOGINFO(METHOD, "Format Changed. Media buffer = %0x", buffer);
				res = mCurrentSource->read(buffer, options);
			}
		}
	}
	else if (res != OK)
	{
		LOGINFO(METHOD, "read error = %s", strerror(-res));
	}
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



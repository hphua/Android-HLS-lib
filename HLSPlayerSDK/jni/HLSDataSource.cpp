/*
 * HLSDataSource.cpp
 *
 *  Created on: Jun 26, 2014
 *      Author: Mark
 */


#include "constants.h"
#include "debug.h"
#include "HLSDataSource.h"

using namespace android;
using namespace std;

#define CLASS_NAME APP_NAME"::HLSDataSource"

#define METHOD CLASS_NAME"::HLSDataSource()"
HLSDataSource::HLSDataSource() : mSourceIdx(0), mSegmentStartOffset(0), mOffsetAdjustment(0)
{

}

HLSDataSource::~HLSDataSource()
{

}

#define METHOD CLASS_NAME"::append()"
status_t HLSDataSource::append(const char* uri)
{
	sp<DataSource> dataSource = DataSource::CreateFromURI(uri);
	status_t rval = dataSource->initCheck() == OK;
	LOGERROR(METHOD, "DataSource initCheck() result: %s", strerror(-rval));
	mSources.push_back(dataSource);
	return rval;
}

#define METHOD CLASS_NAME"::getPreloadedSegmentCount()"
int HLSDataSource::getPreloadedSegmentCount()
{
	return mSources.size() - mSourceIdx;
}

#define METHOD CLASS_NAME"::initCheck()"
android::status_t HLSDataSource::initCheck() const
{
	LOGINFO(METHOD, "Source Count = %d", mSources.size());
	if (mSources.size() > 0)
	{
		return mSources[mSourceIdx]->initCheck();
	}
	return NO_INIT;
}

#define METHOD CLASS_NAME"::readAt()"
ssize_t HLSDataSource::readAt(off64_t offset, void* data, size_t size)
{
	off64_t sourceSize = 0;
	mSources[mSourceIdx]->getSize(&sourceSize);

	off64_t adjoffset = offset - mOffsetAdjustment;  // get our adjusted offset. It should always be >= 0

	if (adjoffset >= sourceSize)
	{
		adjoffset -= sourceSize; // subtract the size of the current source from the offset
		mOffsetAdjustment += sourceSize; // Add the size of the current source to our offset adjustment for the future
		++mSourceIdx;
		if (mSourceIdx == mSources.size())
		{
			LOGINFO(METHOD, "%x | getSize = %lld | offset=%lld | adjustedOffset = %lld | requested size = %d | OUT OF SOURCES!", this, sourceSize, offset, offset-mOffsetAdjustment, size);
			return 0;
		}
	}

	ssize_t rsize = mSources[mSourceIdx]->readAt(adjoffset, data, size);


//	LOGINFO(METHOD, "%x | getSize = %lld | offset=%lld | offsetAdjustment = %lld | adjustedOffset = %lld | requested size = %d | rsize = %ld",
//					this, sourceSize, offset, mOffsetAdjustment, adjoffset, size, rsize);
	return rsize;
}

#define METHOD CLASS_NAME"::getSize()"
status_t HLSDataSource::getSize(off64_t* size)
{
	status_t rval = mSources[mSourceIdx]->getSize(size);
	LOGINFO(METHOD, "%x | size = %lld",this, *size);
	return rval;
}


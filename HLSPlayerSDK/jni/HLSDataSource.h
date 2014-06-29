/*
 * HLSDataSource.h
 *
 *  Created on: Jun 26, 2014
 *      Author: Mark
 */

#ifndef HLSDATASOURCE_H_
#define HLSDATASOURCE_H_

#include <../android-source/frameworks/av/include/media/stagefright/DataSource.h>
#include <../android-source/frameworks/av/include/media/stagefright/MediaDefs.h>
#include <../android-source/system/core/include/utils/Errors.h>

#include <vector>

class HLSDataSource : public android::DataSource
{
public:
	HLSDataSource();
	~HLSDataSource();

	android::status_t append(const char* uri);
	int getPreloadedSegmentCount();




	virtual android::status_t initCheck() const;

	virtual ssize_t readAt(off64_t offset, void* data, size_t size);

	virtual android::status_t getSize(off64_t *size);

	//virtual uint32_t flags();

	//virtual android::status_t reconnectAtOffset(off64_t offset);


private:

	std::vector<android::sp<android::DataSource> > mSources;
	uint32_t mSourceIdx;
	off64_t mSegmentStartOffset;
	off64_t mOffsetAdjustment;


};


#endif /* HLSDATASOURCE_H_ */

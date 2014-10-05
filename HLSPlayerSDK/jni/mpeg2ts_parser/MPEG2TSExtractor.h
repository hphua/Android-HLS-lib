/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MPEG2_TS_EXTRACTOR_H_
#define MPEG2_TS_EXTRACTOR_H_
#include "ABase.h"
//#include <media/stagefright/MediaExtractor.h>
#include "threads.h"
#include "Vector.h"

namespace android {
struct AMessage;
struct AnotherPacketSource;
struct ATSParser;
struct DataSource;
struct MPEG2TSSource;
struct String8;

struct MPEG2TSExtractor : public android_video_shim::MediaExtractor 
{
    MPEG2TSExtractor(const sp<HLSDataSource> &source);
    
    android_video_shim::MediaSource *getTrackProxy(size_t index);
    android_video_shim::MediaSource23 *getTrackProxy23(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags = 0);
    virtual size_t countTracks();

    virtual sp<MetaData> getMetaData();
    virtual uint32_t flags() const;
private:

    //virtual sp<MediaSource> getTrack(size_t index);

    friend struct MPEG2TSSource;
    mutable Mutex mLock;
    sp<HLSDataSource> mDataSource;
    sp<ATSParser> mParser;
    Vector< sp<AnotherPacketSource> > mSourceImpls;
    off64_t mOffset;
    void init();
    status_t feedMore();
    DISALLOW_EVIL_CONSTRUCTORS(MPEG2TSExtractor);
};
bool SniffMPEG2TS(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);
}  // namespace android
#endif  // MPEG2_TS_EXTRACTOR_H_
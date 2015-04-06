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

//#define LOG_NDEBUG 0
#define LOG_TAG "MPEG2TSExtractor"
#include <android/log.h>

#define ALOGI SLOGV

#include "MPEG2TSExtractor.h"
//#include "include/NuCachedSource2.h"

#include "ADebug.h"

#include "AnotherPacketSource.h"
#include "ATSParser.h"

namespace android {

static const size_t kTSPacketSize = 188;

struct MPEG2TSSource : public RefBase {

    pthread_mutex_t lock;

    MPEG2TSSource(
            const sp<MPEG2TSExtractor> &extractor,
            const sp<AnotherPacketSource> &impl,
            bool seekable);

    status_t start(MetaData *params = NULL);
    status_t stop();
    sp<android_video_shim::MetaData> getFormat();

    status_t read(
            MediaBuffer **buffer, const android_video_shim::MediaSource::ReadOptions *options = NULL);

private:
    sp<MPEG2TSExtractor> mExtractor;
    sp<AnotherPacketSource> mImpl;

    // If there are both audio and video streams, only the video stream
    // will be seekable, otherwise the single stream will be seekable.
    bool mSeekable;

    DISALLOW_EVIL_CONSTRUCTORS(MPEG2TSSource);
};

MPEG2TSSource::MPEG2TSSource(
        const sp<MPEG2TSExtractor> &extractor,
        const sp<AnotherPacketSource> &impl,
        bool seekable)
    : mExtractor(extractor),
      mImpl(impl),
      mSeekable(seekable) 
{
    LOGI("ctor %p mImpl=%p", this, impl.get());
    initRecursivePthreadMutex(&lock);
}

status_t MPEG2TSSource::start(MetaData *params) {
    AutoLock locker(&lock);
    return mImpl->start(params);
}

status_t MPEG2TSSource::stop() {
    AutoLock locker(&lock);
    return mImpl->stop();
}

sp<MetaData> MPEG2TSSource::getFormat() {
     AutoLock locker(&lock);
   LOGI("Getting format this=%p mImpl=%p", this, mImpl.get());
    return mImpl->getFormat();
}

status_t MPEG2TSSource::read(
        MediaBuffer **out, const android_video_shim::MediaSource::ReadOptions *options) {
    AutoLock locker(&lock);
    *out = NULL;

    int64_t seekTimeUs;
    android_video_shim::MediaSource::ReadOptions::SeekMode seekMode;
    if (mSeekable && options && options->getSeekTo(&seekTimeUs, &seekMode)) {
        return ERROR_UNSUPPORTED;
    }

    status_t finalResult;
    while (!mImpl->hasBufferAvailable(&finalResult)) {
        if (finalResult != OK) {
            return ERROR_END_OF_STREAM;
        }

        status_t err = mExtractor->feedMore();
        if (err != OK) {
            mImpl->signalEOS(err);
        }
    }

    return mImpl->read(out, options);
}

////////////////////////////////////////////////////////////////////////////////

class MPEG2TSTrackProxy : public android_video_shim::MediaSource
{
public:

    MPEG2TSSource *realSource;

    virtual void __start() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __stop() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __getFormat() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __read() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __pause() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __setBuffer() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.

    void dummyDtor()
    {

    }

    void patchTable()
    {
        LOGV2("this = %p", (void*)this);
        LOGV2("mRefs = %p", (void*)&this->mRefs);

        // Fake up the right vtable.

        // First look up and make a copy of the official vtable.
        // This leaks a bit of RAM per source but we can deal with that later.
        // Update - we can't resolve this symbol on some x86 devices, and it turns
        // out we don't need it - we can just set stuff to 0s and it works OK.
        // This is obviously a bit finicky but adequate for now.
        //void *officialVtable = searchSymbol("_ZTTN7android11MediaSourceE");
        //assert(officialVtable); // Gotta have a vtable!
        void *newVtable = malloc(1024); // Arbitrary size... As base class
                                        // we always get ptr to start of vtable.
        //memcpy(newVtable, officialVtable, 1024);
        memset(newVtable, 0, 1024);

        // Now we can patch the vtable...
        void ***fakeObj = (void***)this;

        // Take into account mandatory vtable offsets.
        fakeObj[0] = (void**)(((int*)newVtable) + 5);

/*        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start); */

        LOGV2("__cxa_pure_virtual = %p", searchSymbol("__cxa_pure_virtual"));

        // Dump the vtable.
        for(int i=-4; i<16; i++)
        {
          LOGV2("vtable2[%d] = %p", i, fakeObj[0][i]);
        }

        // The compiler may complain about these as we are getting into
        // pointer-to-member-function (pmf) territory. However, we aren't
        // actually treating them as such here because there's no instance.
        // So we should be OK! But if the values here report as not code
        // segment values then you might need to revisit.
        LOGV2(" _start=%p", (void*)&MPEG2TSTrackProxy::_start);
        LOGV2(" _stop=%p", (void*)&MPEG2TSTrackProxy::_stop);
        LOGV2(" _getFormat=%p", (void*)&MPEG2TSTrackProxy::_getFormat);
        LOGV2(" _read=%p", (void*)&MPEG2TSTrackProxy::_read);
        LOGV2(" _pause=%p", (void*)&MPEG2TSTrackProxy::_pause);
        LOGV2(" _setBuffers=%p", (void*)&MPEG2TSTrackProxy::_setBuffers);

        // This offset takes us to the this ptr for the RefBase. It is required 
        // for casting to work properly and for the RefBase to get inc'ed/dec'ed.
        // You can derive it by calculating this - the this that the RefBase ctor
        // sees.
        fakeObj[0][-3] = (void*)8; 

        // Stub in a dummy function for the other entries so that if
        // e.g. someone tries to call a destructor it won't segfault.
        for(int i=0; i<18; i++)
            fakeObj[0][i] = (void*)&MPEG2TSTrackProxy::dummyDtor;

        // 4.x entry points
        fakeObj[0][0] = (void*)&MPEG2TSTrackProxy::_start;
        fakeObj[0][1] = (void*)&MPEG2TSTrackProxy::_stop;
        fakeObj[0][2] = (void*)&MPEG2TSTrackProxy::_getFormat;
        fakeObj[0][3] = (void*)&MPEG2TSTrackProxy::_read;
        fakeObj[0][4] = (void*)&MPEG2TSTrackProxy::_pause;
        fakeObj[0][5] = (void*)&MPEG2TSTrackProxy::_setBuffers;
    }

    status_t _start(MetaData *params)
    {
        LOGV("start");
        return realSource->start(params);
    }

    status_t _stop()
    {
        LOGV("stop");
        return realSource->stop();
    }

    sp<MetaData> _getFormat()
    {
        LOGV("getFormat this=%p", this);
        return realSource->getFormat();
    }

    status_t _read(MediaBuffer **buffer, const android_video_shim::MediaSource::ReadOptions *options)
    {
        LOGV2("read");
        return realSource->read(buffer, options);
    }

    status_t _pause() 
    {
        LOGV("_pause");
        return ERROR_UNSUPPORTED;
    }

    status_t _setBuffers(void *data)
    {
        LOGV("_setBuffers");
        return ERROR_UNSUPPORTED;
    }
};


class MPEG2TSTrackProxy23 : public android_video_shim::MediaSource23
{
public:

    MPEG2TSSource *realSource;

    virtual void __start() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __stop() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __getFormat() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __read() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __pause() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __setBuffer() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.

    ~MPEG2TSTrackProxy23()
    {
        LOGI("Dtor of track proxy! %p", this);
    }

    void dummyDtor()
    {

    }

    void patchTable()
    {
        LOGV2("this = %p", (void*)this);
        LOGV2("mRefs = %p", (void*)&this->mRefs);

        // Fake up the right vtable.

        // First look up and make a copy of the official vtable.
        // This leaks a bit of RAM per source but we can deal with that later.
        // Update - we can't resolve this symbol on some x86 devices, and it turns
        // out we don't need it - we can just set stuff to 0s and it works OK.
        // This is obviously a bit finicky but adequate for now.
        //void *officialVtable = searchSymbol("_ZTVN7android11MediaSourceE");
        //assert(officialVtable); // Gotta have a vtable!
        void *newVtable = malloc(1024); // Arbitrary size... As base class
                                        // we always get ptr to start of vtable.
        //memcpy(newVtable, officialVtable, 1024);
        memset(newVtable, 0, 1024);

        // Now we can patch the vtable...
        void ***fakeObj = (void***)this;

        // Take into account mandatory vtable offsets.
        fakeObj[0] = (void**)(((int*)newVtable) + 2);

/*        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGV2("dummy = %p", (void*)&MPEG2TSTrackProxy::__start); */

        LOGV2("__cxa_pure_virtual = %p", searchSymbol("__cxa_pure_virtual"));

        // Dump the vtable.
        for(int i=-2; i<16; i++)
        {
          LOGV2("vtable2[%d] = %p", i, fakeObj[0][i]);
        }

        // The compiler may complain about these as we are getting into
        // pointer-to-member-function (pmf) territory. However, we aren't
        // actually treating them as such here because there's no instance.
        // So we should be OK! But if the values here report as not code
        // segment values then you might need to revisit.
        LOGV2(" _start=%p", (void*)&MPEG2TSTrackProxy23::_start);
        LOGV2(" _stop=%p", (void*)&MPEG2TSTrackProxy23::_stop);
        LOGV2(" _getFormat=%p", (void*)&MPEG2TSTrackProxy23::_getFormat);
        LOGV2(" _read=%p", (void*)&MPEG2TSTrackProxy23::_read);
        LOGV2(" _pause=%p", (void*)&MPEG2TSTrackProxy23::_pause);
        LOGV2(" _setBuffers=%p", (void*)&MPEG2TSTrackProxy23::_setBuffers);

        // Stub in a dummy function for the other entries so that if
        // e.g. someone tries to call a destructor it won't segfault.
        for(int i=0; i<18; i++)
            fakeObj[0][i] = (void*)&MPEG2TSTrackProxy23::dummyDtor;

        // 2.x entry points
        fakeObj[0][6] = (void*)&MPEG2TSTrackProxy23::_start;
        fakeObj[0][7] = (void*)&MPEG2TSTrackProxy23::_stop;
        fakeObj[0][8] = (void*)&MPEG2TSTrackProxy23::_getFormat;
        fakeObj[0][9] = (void*)&MPEG2TSTrackProxy23::_read;
        fakeObj[0][10] = (void*)&MPEG2TSTrackProxy23::_pause;
    }

    status_t _start(MetaData *params)
    {
        LOGV("start");
        return realSource->start(params);
    }

    status_t _stop()
    {
        LOGV("stop");
        return realSource->stop();
    }

    sp<MetaData> _getFormat()
    {
        LOGV("getFormat23 this=%p", this);
        return realSource->getFormat();
    }

    status_t _read(MediaBuffer **buffer, const android_video_shim::MediaSource::ReadOptions *options)
    {
        LOGV("read");
        return realSource->read(buffer, options);
    }

    status_t _pause() 
    {
        LOGV("_pause");
        return ERROR_UNSUPPORTED;
    }

    status_t _setBuffers(void *data)
    {
        LOGV("_setBuffers");
        return ERROR_UNSUPPORTED;
    }
};

android_video_shim::MediaSource *MPEG2TSExtractor::getTrackProxy(size_t index)
{
    if (index >= mSourceImpls.size()) {
        return NULL;
    }

    bool seekable = true;
    if (mSourceImpls.size() > 1) {
        CHECK_EQ(mSourceImpls.size(), 2u);

        sp<MetaData> meta = mSourceImpls.editItemAt(index)->getFormat();
        const char *mime;
        CHECK(meta->findCString(kKeyMIMEType, &mime));

        if (!strncasecmp("audio/", mime, 6)) {
            seekable = false;
        }
    }

    MPEG2TSTrackProxy *proxy = new MPEG2TSTrackProxy();
    proxy->realSource = new MPEG2TSSource(this, mSourceImpls.editItemAt(index), seekable);
    proxy->patchTable();

    // This is useful for confirming the cast gives the offset expected for the RefBase cast.
    LOGV("Alloc'ed return=%p", proxy);
    LOGV("Alloc'ed RefBase=%p", dynamic_cast<RefBase*>(proxy));
    return proxy;
}

android_video_shim::MediaSource23 *MPEG2TSExtractor::getTrackProxy23(size_t index)
{
    if (index >= mSourceImpls.size()) {
        return NULL;
    }

    bool seekable = true;
    if (mSourceImpls.size() > 1) {
        CHECK_EQ(mSourceImpls.size(), 2u);

        sp<MetaData> meta = mSourceImpls.editItemAt(index)->getFormat();
        const char *mime;
        CHECK(meta->findCString(kKeyMIMEType, &mime));

        if (!strncasecmp("audio/", mime, 6)) {
            seekable = false;
        }
    }

    MPEG2TSTrackProxy23 *proxy = new MPEG2TSTrackProxy23();
    proxy->realSource = new MPEG2TSSource(this, mSourceImpls.editItemAt(index), seekable);
    proxy->patchTable();

    // This is useful for confirming the cast gives the offset expected for the RefBase cast.
    LOGV("Alloc'ed 23 return=%p", proxy);
    LOGV("Alloc'ed 23 RefBase=%p", dynamic_cast<RefBase*>(proxy));
    return proxy;
}

////////////////////////////////////////////////////////////////////////////////

MPEG2TSExtractor::MPEG2TSExtractor(const sp<HLSDataSource> &source)
    : mDataSource(source),
      mParser(new ATSParser(ATSParser::TS_TIMESTAMPS_ARE_ABSOLUTE)),
      mOffset(0) {
	LOGV("mParser->flags=%d", mParser->getFlags());
    init();
}

size_t MPEG2TSExtractor::countTracks() {
    return mSourceImpls.size();
}

/*sp<android_video_shim::MediaSource> MPEG2TSExtractor::getTrack(size_t index) {
    if (index >= mSourceImpls.size()) {
        return NULL;
    }

    bool seekable = true;
    if (mSourceImpls.size() > 1) {
        CHECK_EQ(mSourceImpls.size(), 2u);

        sp<MetaData> meta = mSourceImpls.editItemAt(index)->getFormat();
        const char *mime;
        CHECK(meta->findCString(kKeyMIMEType, &mime));

        if (!strncasecmp("audio/", mime, 6)) {
            seekable = false;
        }
    }

    return new MPEG2TSSource(this, mSourceImpls.editItemAt(index), seekable);
}*/


sp<MetaData> MPEG2TSExtractor::getTrackMetaData(
        size_t index, uint32_t flags) {
    return index < mSourceImpls.size()
        ? mSourceImpls.editItemAt(index)->getFormat() : NULL;
}

sp<MetaData> MPEG2TSExtractor::getMetaData() {
    sp<MetaData> meta = new MetaData;

    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_MPEG2TS);

    return meta;
}

void MPEG2TSExtractor::init() {
    bool haveAudio = false;
    bool haveVideo = false;
    int numPacketsParsed = 0;

    while (feedMore() == OK) {
        ATSParser::SourceType type;
        if (haveAudio && haveVideo) {
            break;
        }
        if (!haveVideo) {
            sp<AnotherPacketSource> impl =
                (AnotherPacketSource *)mParser->getSource(
                        ATSParser::VIDEO).get();

            if (impl != NULL) {
                haveVideo = true;
                mSourceImpls.push(impl);
            }
        }

        if (!haveAudio) {
            sp<AnotherPacketSource> impl =
                (AnotherPacketSource *)mParser->getSource(
                        ATSParser::AUDIO).get();

            if (impl != NULL) {
                haveAudio = true;
                mSourceImpls.push(impl);
            }
        }

        if (++numPacketsParsed > 10000) {
            break;
        }
    }

    ALOGI("haveAudio=%d, haveVideo=%d", haveAudio, haveVideo);
}

status_t MPEG2TSExtractor::feedMore() {
    Mutex::Autolock autoLock(mLock);

    uint8_t packet[kTSPacketSize];
    ssize_t n = mDataSource->readAt(mOffset, packet, kTSPacketSize);

    if (n < (ssize_t)kTSPacketSize) {
        return (n < 0) ? (status_t)n : ERROR_END_OF_STREAM;
    }

    mOffset += n;
    return mParser->feedTSPacket(packet, kTSPacketSize);
}

uint32_t MPEG2TSExtractor::flags() const {
    return 0; //CAN_PAUSE;
}

////////////////////////////////////////////////////////////////////////////////

bool SniffMPEG2TS(
        const sp<HLSDataSource> &source) {
    for (int i = 0; i < 5; ++i) {
        char header;
        if (source->readAt(kTSPacketSize * i, &header, 1) != 1
                || header != 0x47) {
            return false;
        }
    }

    return true; 
}

}  // namespace android

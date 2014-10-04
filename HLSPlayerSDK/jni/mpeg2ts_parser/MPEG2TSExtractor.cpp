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

struct MPEG2TSSource : public android_video_shim::MediaSource {
    MPEG2TSSource(
            const sp<MPEG2TSExtractor> &extractor,
            const sp<AnotherPacketSource> &impl,
            bool seekable);

    status_t start(MetaData *params = NULL);
    status_t stop();
    sp<MetaData> getFormat();

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
      mSeekable(seekable) {


}

status_t MPEG2TSSource::start(MetaData *params) {
    return mImpl->start(params);
}

status_t MPEG2TSSource::stop() {
    return mImpl->stop();
}

sp<MetaData> MPEG2TSSource::getFormat() {
    return mImpl->getFormat();
}

status_t MPEG2TSSource::read(
        MediaBuffer **out, const android_video_shim::MediaSource::ReadOptions *options) {
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

    char padding[16]; // Make sure we are clear of any important bits pointers.
    MPEG2TSSource *realSource;

    virtual void __start() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __stop() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __getFormat() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __read() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __pause() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.
    virtual void __setBuffer() { LOGI("Calling dummy."); }; // Make sure we have SOME virtual to avoid any issues.


    void patchTable()
    {
        LOGI("this = %p", (void*)this);
        LOGI("mRefs = %p", (void*)&this->mRefs);

        // Fake up the right vtable.

        // First look up and make a copy of the official vtable.
        // This leaks a bit of RAM per source but we can deal with that later.
        void *officialVtable = searchSymbol("_ZTTN7android11MediaSourceE");
        assert(officialVtable); // Gotta have a vtable!
        void *newVtable = malloc(1024); // Arbitrary size... As base class
                                        // we always get ptr to start of vtable.
        memcpy(newVtable, officialVtable, 1024);

        // Now we can patch the vtable...
        void ***fakeObj = (void***)this;

        // Take into account mandatory vtable offsets.
        fakeObj[0] = (void**)(((int*)newVtable) + 2);

/*        LOGI("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGI("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGI("dummy = %p", (void*)&MPEG2TSTrackProxy::__start);
        LOGI("dummy = %p", (void*)&MPEG2TSTrackProxy::__start); */

        // Dump the vtable.
        for(int i=0; i<16; i++)
        {
          LOGI("vtable2[%d] = %p", i, fakeObj[0][i]);
        }

        // The compiler may complain about these as we are getting into
        // pointer-to-member-function (pmf) territory. However, we aren't
        // actually treating them as such here because there's no instance.
        // So we should be OK! But if the values here report as not code
        // segment values then you might need to revisit.
        LOGI(" _start=%p", (void*)&MPEG2TSTrackProxy::_start);
        LOGI(" _stop=%p", (void*)&MPEG2TSTrackProxy::_stop);
        LOGI(" _getFormat=%p", (void*)&MPEG2TSTrackProxy::_getFormat);
        LOGI(" _read=%p", (void*)&MPEG2TSTrackProxy::_read);
        LOGI(" _pause=%p", (void*)&MPEG2TSTrackProxy::_pause);
        LOGI(" _setBuffers=%p", (void*)&MPEG2TSTrackProxy::_setBuffers);

        // And override the pointers as appropriate.
        if(AVSHIM_USE_NEWMEDIASOURCEVTABLE)
        {
            // 4.x entry points
            fakeObj[0][0] = (void*)&MPEG2TSTrackProxy::_start;
            fakeObj[0][1] = (void*)&MPEG2TSTrackProxy::_stop;
            fakeObj[0][2] = (void*)&MPEG2TSTrackProxy::_getFormat;
            fakeObj[0][3] = (void*)&MPEG2TSTrackProxy::_read;
            fakeObj[0][4] = (void*)&MPEG2TSTrackProxy::_pause;
            fakeObj[0][5] = (void*)&MPEG2TSTrackProxy::_setBuffers;
        }
        else
        {
            assert(0); // Bad time in here son.
            // Confirm what we can that we're doing this right...
            //void *oldGetSize = searchSymbol("_ZN7android10DataSource7getSizeEPl");
            //void *oldGetSize2 = searchSymbol("_ZN7android10DataSource7getSizeEPx");
            //LOGI("  oldGetSize_l=%p oldGetSize_x=%p fakeObj[0][8]=%p", oldGetSize, oldGetSize2, fakeObj[0][8]);

            // 2.3 entry points
            //fakeObj[0][6] = (void*)&MPEG2TSTrackProxy::_initCheck;
            //fakeObj[0][7] = (void*)&MPEG2TSTrackProxy::_readAt_23;
            //fakeObj[0][8] = (void*)&MPEG2TSTrackProxy::_getSize_23;
        }
    }

    status_t _start(MetaData *params)
    {
        LOGI("start");
        return realSource->start(params);
    }

    status_t _stop()
    {
        LOGI("stop");
        return realSource->stop();
    }

    sp<MetaData> _getFormat()
    {
        LOGI("getFormat");
        return realSource->getFormat();
    }

    status_t _read(MediaBuffer **buffer, const android_video_shim::MediaSource::ReadOptions *options)
    {
        LOGI("read");
        return realSource->read(buffer, options);
    }

    status_t _pause() 
    {
        LOGI("_pause");
        return ERROR_UNSUPPORTED;
    }

    status_t _setBuffers(void *data)
    {
        LOGI("_setBuffers");
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
    proxy->realSource = new MPEG2TSSource(this, mSourceImpls.editItemAt(index), seekable);;
    proxy->patchTable();
    LOGI("Alloc'ed return=%p", proxy);
    return proxy;
}

////////////////////////////////////////////////////////////////////////////////

MPEG2TSExtractor::MPEG2TSExtractor(const sp<HLSDataSource> &source)
    : mDataSource(source),
      mParser(new ATSParser),
      mOffset(0) {
    init();
}

size_t MPEG2TSExtractor::countTracks() {
    return mSourceImpls.size();
}

sp<android_video_shim::MediaSource> MPEG2TSExtractor::getTrack(size_t index) {
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
}


sp<MetaData> MPEG2TSExtractor::getTrackMetaData(
        size_t index, uint32_t flags) {
    return index < mSourceImpls.size()
        ? mSourceImpls.editItemAt(index)->getFormat() : NULL;
}

sp<MetaData> MPEG2TSExtractor::getMetaData() {
    sp<MetaData> meta = new MetaData;

    //meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_MPEG2TS);

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
        const sp<HLSDataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *) {
/*    for (int i = 0; i < 5; ++i) {
        char header;
        if (source->readAt(kTSPacketSize * i, &header, 1) != 1
                || header != 0x47) {
            return false;
        }
    }

    *confidence = 0.1f;
    mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_MPEG2TS);

    return true; */
    return true;
}

}  // namespace android

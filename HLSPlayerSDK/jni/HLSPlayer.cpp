/*
 * HLSPlayer.cpp
 *
 *  Created on: May 5, 2014
 *      Author: Mark
 */

#include "HLSPlayer.h"
#include "debug.h"
#include "constants.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include <../android-source/frameworks/av/include/media/stagefright/MediaDefs.h>
#include <../android-source/frameworks/native/include/ui/GraphicBuffer.h>
#include <../android-source/frameworks/av/include/media/stagefright/ColorConverter.h>
#include <../android-source/frameworks/av/include/media/stagefright/Utils.h>


#include "HLSSegment.h"
#include "HLSMediaSourceAdapter.h"


using namespace android;

#define CLASS_NAME APP_NAME"::HLSPlayer"

#define METHOD CLASS_NAME"::HLSPlayer()"
HLSPlayer::HLSPlayer(JavaVM* jvm) : mExtractorFlags(0),
mHeight(0), mWidth(0), mBitrate(0), mActiveAudioTrackIndex(-1),
mVideoBuffer(NULL), mWindow(NULL), mSurface(NULL), mRenderedFrameCount(0),
mAudioPlayer(NULL), mAudioSink(NULL), mTimeSource(NULL),
mDurationUs(0), mOffloadAudio(false), mStatus(HLSPlayer::STOPPED),
mAudioTrack(NULL), mVideoTrack(NULL), mJvm(jvm), mPlayerViewClass(NULL),
mNextSegmentMethodID(NULL), mSegmentTimeOffset(0), mVideoFrameDelta(0), mLastVideoTimeUs(0)
{
	status_t status = mClient.connect();
	LOGINFO(METHOD, "OMXClient::Connect return %d", status);
	mAudioTrack = new HLSMediaSourceAdapter();
	((HLSMediaSourceAdapter*)mAudioTrack.get())->setIsAudio(true);
	mVideoTrack = new HLSMediaSourceAdapter();
}

#define METHOD CLASS_NAME"::~HLSPlayer()"
HLSPlayer::~HLSPlayer()
{

}

#define METHOD CLASS_NAME"::Close()"
void HLSPlayer::Close(JNIEnv* env)
{
	if (mPlayerViewClass)
	{
		env->DeleteGlobalRef(mPlayerViewClass);
		mPlayerViewClass = NULL;
	}
	LOGINFO(METHOD, "Entered");
	if (mWindow)
	{
		ANativeWindow_release(mWindow);
		mWindow = NULL;
	}
	if (mSurface)
	{
		(*env).DeleteGlobalRef(mSurface);
		mSurface = NULL;
	}
	if (mAudioPlayer)
	{
		// do something!
	}
	if (mTimeSource)
	{
		// do something!
	}
}


///
/// Set Surface. Takes a java surface object
///
#define METHOD CLASS_NAME"::SetSurface()"
void HLSPlayer::SetSurface(JNIEnv* env, jobject surface)
{
	LOGINFO(METHOD, "Entered");

	mSurface = (jobject)env->NewGlobalRef(surface);

	ANativeWindow* window = ANativeWindow_fromSurface(env, mSurface);

	LOGINFO(METHOD, "Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface() - window = %0x", window);
	LOGINFO(METHOD, "window->flags = %0x", window->flags);
	LOGINFO(METHOD, "window->swapInterfal Min: %d Max: %d", window->minSwapInterval, window->maxSwapInterval);
	LOGINFO(METHOD, "window->dpi  x:%f y:%f", window->xdpi, window->ydpi);

	if (window)
	{
		SetNativeWindow(window);
	}
}



#define METHOD CLASS_NAME"::SetNativeWindow()"
void HLSPlayer::SetNativeWindow(ANativeWindow* window)
{
	LOGINFO(METHOD, "window = %0x", window);
	if (mWindow)
	{
		LOGINFO(METHOD, "::mWindow is already set to %0x", window);
		// Umm - resetting?
		ANativeWindow_release(mWindow);
	}
	mWindow = window;
}


#define METHOD CLASS_NAME"::FeedSegment()"
status_t HLSPlayer::FeedSegment(const char* path)
{

	// Make a data source from the file
	LOGINFO(METHOD, "path = '%s'", path);
	sp<DataSource> dataSource = DataSource::CreateFromURI(path); //new FileSource(path);

	status_t err = dataSource->initCheck();
	if (err != OK) return err;

	HLSSegment* s = new HLSSegment();
	if (s)
	{
		if (s->SetDataSource(dataSource))
		{
			mSegments.push_back(s);
			PostSegment(s);
			return OK;
		}
		else
		{
			delete s;
			s = NULL;
		}
		return UNKNOWN_ERROR;
	}
	return NO_MEMORY;
}

#define METHOD CLASS_NAME"::PostSegment()"
status_t HLSPlayer::PostSegment(HLSSegment* s)
{
	LOGINFO(METHOD, "Entered");
	if (!s) return BAD_VALUE;

	sp<MediaSource> omxSource = OMXCodec::Create(mClient.interface(), s->GetVideoTrack()->getFormat(), false, s->GetVideoTrack(), NULL, 0, NULL /*nativeWindow*/);
	LOGINFO(METHOD, "OMXCodec::Create() (video) returned %0x", omxSource.get());
	((HLSMediaSourceAdapter*)mVideoTrack.get())->append(omxSource);

	audio_stream_type_t streamType = AUDIO_STREAM_MUSIC;
	if (mAudioSink != NULL)
	{
		streamType = mAudioSink->getAudioStreamType();
	}

	mOffloadAudio = canOffloadStream(s->GetAudioTrack()->getFormat(), (s->GetVideoTrack() != NULL), false /*streaming http */, streamType);
	LOGINFO(METHOD, "mOffloadAudio == %s", mOffloadAudio ? "true" : "false");

	sp<MediaSource> omxAudioSource = OMXCodec::Create(mClient.interface(), s->GetAudioTrack()->getFormat(), false, s->GetAudioTrack());
	LOGINFO(METHOD, "OMXCodec::Create() (audio) returned %0x", omxAudioSource.get());


	if (mOffloadAudio)
	{
		LOGINFO(METHOD, "Bypass OMX (offload) Line: %d", __LINE__);
		((HLSMediaSourceAdapter*)mAudioTrack.get())->append(s->GetAudioTrack());
	}
	else
	{
		LOGINFO(METHOD, "Not Bypassing OMX Line: %d", __LINE__);
		((HLSMediaSourceAdapter*)mAudioTrack.get())->append(omxAudioSource);
	}
	return OK;


}

#define METHOD CLASS_NAME"::CreateAudioPlayer()"
bool HLSPlayer::CreateAudioPlayer()
{
	uint32_t flags = 0;
	if (mOffloadAudio)
	{
		flags |= AudioPlayer::USE_OFFLOAD;
	}

	if (mAudioPlayer != NULL)
	{
		mAudioPlayer->pause(false);
		delete mAudioPlayer;
		mAudioPlayer = NULL;
	}

	mAudioPlayer = new AudioPlayer(mAudioSink, flags, NULL);
	mAudioPlayer->setSource(mAudioTrack);
	mTimeSource = mAudioPlayer;

	return true;
}


//
//  Play()
//
//		Tells the player to play the current stream. Requires that
//		segments have already been fed to the player.
//
#define METHOD CLASS_NAME"::Play()"
bool HLSPlayer::Play()
{
	LOGINFO(METHOD, "Entered");
	if (!mWindow) { LOGINFO(METHOD, "mWindow is NULL"); return false; }
	sp<ANativeWindow> nativeWindow = NULL;
	nativeWindow = mWindow;
	LOGINFO(METHOD, "%d", __LINE__);

	status_t err = mVideoTrack->start();
	if (err == OK)
	{
		err = mAudioTrack->start();
		if (err == OK)
		{
			if (CreateAudioPlayer())
			{
				mAudioPlayer->start(true);
				SetStatus(PLAYING);
				return true;
			}
			else
			{
				LOGINFO(METHOD, "Failed to create audio player : %d", __LINE__);
			}
		}
		else
		{
			LOGINFO(METHOD, "Audio Track failed to start: %s : %d", strerror(-err), __LINE__);
		}
	}
	else
	{
		LOGINFO(METHOD, "Video Track failed to start: %s : %d", strerror(-err), __LINE__);
	}
	return false;
}


#define METHOD CLASS_NAME"::Update()"
int HLSPlayer::Update()
{
	LOGINFO(METHOD, "Entered");

	if (mStatus != PLAYING)
	{
		LogStatus();
		return -1;
	}
//	if (mVideoBuffer != NULL)
//	{
//		mVideoBuffer->release();
//		mVideoBuffer = NULL;
//	}
	MediaSource::ReadOptions options;
	bool rval = -1;
	for (;;)
	{
		//LOGINFO(METHOD, "mVideoBuffer = %0x", mVideoBuffer);
		RUNDEBUG(mVideoTrack->getFormat()->dumpToLog());
		status_t err = OK;
		if (mVideoBuffer == NULL)
		{
			LOGINFO(METHOD, "Reading video buffer");
			err = mVideoTrack->read(&mVideoBuffer, &options);
		}
		if (err != OK)
		{
			LOGINFO(METHOD, "err=%s,%0x  Line: %d", strerror(-err), -err, __LINE__);
			switch (err)
			{
			case INFO_FORMAT_CHANGED:
			case INFO_DISCONTINUITY:
			case INFO_OUTPUT_BUFFERS_CHANGED:
				// If it doesn't have a valid buffer, maybe it's informational?
				if (mVideoBuffer == NULL) return 0;
				break;
			case ERROR_END_OF_STREAM:
				SetStatus(STOPPED);
				//PlayNextSegment();
				return -1;
				break;
			default:
				SetStatus(STOPPED);
				// deal with any errors
				// in the sample code, they're sending the video event, anyway
				return -1;
			}
		}

		if (mVideoBuffer->range_length() != 0)
		{
			int64_t timeUs;
			bool rval = mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs);
			if (!rval)
			{
				LOGINFO(METHOD, "Frame did not have time value: STOPPING");
				SetStatus(STOPPED);
				return -1;
			}

			if (timeUs == 0)
			{
				// This looks like we're on a new segment
				mSegmentTimeOffset = mLastVideoTimeUs + mVideoFrameDelta; // setting the offset. The first time through, these should all be 0
			}

			int64_t curTimeUs = timeUs + mSegmentTimeOffset;
			if (mVideoFrameDelta == 0) mVideoFrameDelta = curTimeUs - mLastVideoTimeUs;

			int64_t audioTime = mTimeSource->getRealTimeUs();
			int64_t delta = audioTime - curTimeUs;

			LOGINFO(METHOD, "audioTime = %lld | videoTime = %lld | diff = %lld | reportedVideoTime = %lld", audioTime, curTimeUs, delta, timeUs);

			mLastVideoTimeUs = curTimeUs;
			if (delta < -10000) // video is running ahead
			{
				LOGINFO(METHOD, "Video is running ahead - waiting til next time");
				break; // skip out - don't render it yet
			}
			else if (delta > 40000) // video is running behind
			{
				LOGINFO(METHOD, "Video is running behind - skipping frame");
				// Do we need to catch up?
				mVideoBuffer->release();
				mVideoBuffer = NULL;
				continue;
			}
			else
			{

				// We appear to have a valid buffer?! and we're in time!
				if (RenderBuffer(mVideoBuffer))
				{
					++mRenderedFrameCount;
					rval = mRenderedFrameCount;
					LOGINFO(METHOD, "mRenderedFrameCount = %d", mRenderedFrameCount);
					break;
				}
				else
				{
					LOGINFO(METHOD, "Render Buffer returned false: STOPPING");
					SetStatus(STOPPED);
					rval=-1;
					break;
				}

			}
		}

		LOGINFO(METHOD, "Found empty buffer%d", __LINE__);
		// Some decoders return spurious empty buffers we want to ignore
		mVideoBuffer->release();
		mVideoBuffer = NULL;

	}

	if (mVideoTrack != NULL)
	{
		int segCount = ((HLSMediaSourceAdapter*) mVideoTrack.get())->getSegmentCount();
		if (segCount < 2)
			RequestNextSegment();
	}


//    int64_t nextTimeUs;
//    mVideoBuffer->meta_data()->findInt64(kKeyTime, &nextTimeUs);
//    int64_t delayUs = nextTimeUs - ts->getRealTimeUs() + mTimeSourceDeltaUs;
//    postVideoEvent_l(delayUs > 10000 ? 10000 : delayUs < 0 ? 0 : delayUs);
    return rval; // keep going!

}

#define METHOD CLASS_NAME"::RenderBuffer()"
bool HLSPlayer::RenderBuffer(MediaBuffer* buffer)
{
	LOGINFO(METHOD, "Entered");
	//LOGINFO(METHOD, "Rendering Buffer size=%d", buffer->size());
	if (!mWindow) { LOGINFO(METHOD, "mWindow is NULL"); return false; }
	if (!buffer) { LOGINFO(METHOD, "the MediaBuffer is NULL"); return false; }
	//if (!buffer->graphicBuffer().get()){ LOGINFO(CLASS_NAME, "the MediaBuffer->graphicBuffer is NULL"); return false; }
	RUNDEBUG(buffer->meta_data()->dumpToLog());
	int colf = 0;
	bool res = mVideoTrack->getFormat()->findInt32(kKeyColorFormat, &colf);
	LOGINFO(METHOD, "Found Frame Color Format: %s", res ? "true" : "false" );
	//RUNDEBUG(mVideoTrack->getFormat()->dumpToLog());
	ColorConverter cc((OMX_COLOR_FORMATTYPE)colf, OMX_COLOR_Format16bitRGB565); // Should be getting these from the formats, probably
	LOGINFO(METHOD, "ColorConversion from %d is valid: %s", colf, cc.isValid() ? "true" : "false" );
    int64_t timeUs;
    if (buffer->meta_data()->findInt64(kKeyTime, &timeUs))
    {
    	//LOGINFO(METHOD, "%d", __LINE__);
		native_window_set_buffers_timestamp(mWindow, timeUs * 1000);
		//LOGINFO(METHOD, "%d", __LINE__);
		//status_t err = mWindow->queueBuffer(mWindow, buffer->graphicBuffer().get(), -1);



		ANativeWindow_Buffer windowBuffer;
		if (ANativeWindow_lock(mWindow, &windowBuffer, NULL) == 0)
		{
			LOGINFO(METHOD, "buffer locked (%d x %d stride=%d, format=%d)", windowBuffer.width, windowBuffer.height, windowBuffer.stride, windowBuffer.format);

			HLSMediaSourceAdapter* vt = (HLSMediaSourceAdapter*)mVideoTrack.get();

			unsigned short *pixels = (unsigned short *)windowBuffer.bits;
			for(int i=0; i<windowBuffer.width * windowBuffer.height; i++)
				pixels[i] = 0x0000;
			cc.convert(buffer->data(), vt->getWidth(), vt->getHeight(), 0, 0, vt->getCropWidth(), vt->getCropHeight(), windowBuffer.bits, windowBuffer.width, windowBuffer.height, 0,0,vt->getCropWidth(), vt->getCropHeight());
			void *gbBits = NULL;

			//buffer->graphicBuffer().get()->lock(0, &gbBits);

			//memcpy(windowBuffer.bits, buffer->data(), buffer->size());
			//buffer->graphicBuffer().get()->unlock();

			ANativeWindow_unlockAndPost(mWindow);
		}


		//LOGINFO(METHOD, "%d", __LINE__);
		//if (err != 0) {
		//	ALOGE("queueBuffer failed with error %s (%d)", strerror(-err),
		//			-err);
		//	return false;
		//}
		//LOGINFO(METHOD, "%d", __LINE__);

		sp<MetaData> metaData = buffer->meta_data();
		//LOGINFO(METHOD, "%d", __LINE__);
		metaData->setInt32(kKeyRendered, 1);
		//LOGINFO(METHOD, "%d", __LINE__);
		return true;
    }
    return false;

}

#define METHOD CLASS_NAME"::SetStatus()"
void HLSPlayer::SetStatus(int status)
{
	mStatus = status;
	LOGINFO(METHOD, "status = %d", status);
}

#define METHOD CLASS_NAME"::SetStatus()"
void HLSPlayer::LogStatus()
{
	LOGINFO(METHOD, "Status = %d", mStatus);
}

#define METHOD CLASS_NAME"::RequestNextSegment()"
void HLSPlayer::RequestNextSegment()
{
	LOGINFO(METHOD, "Requesting new segment");
	JNIEnv* env = NULL;
	mJvm->AttachCurrentThread(&env, NULL);

	if (mPlayerViewClass == NULL)
	{
		jclass c = env->FindClass("com/kaltura/hlsplayersdk/PlayerView");
		if ( env->ExceptionCheck() || c == NULL) {
			LOGINFO(METHOD, "Could not find class com/kaltura/hlsplayersdk/PlayerView" );
			mPlayerViewClass = NULL;
			return;
		}

		mPlayerViewClass = (jclass)env->NewGlobalRef((jobject)c);

	}

	if (mNextSegmentMethodID == NULL)
	{
		mNextSegmentMethodID = env->GetStaticMethodID(mPlayerViewClass, "requestNextSegment", "()V" );
		if (env->ExceptionCheck())
		{
			mNextSegmentMethodID = NULL;
			LOGINFO(METHOD, "Could not find method com/kaltura/hlsplayersdk/PlayerView.requestNextSegment()" );
			return;
		}
	}

	env->CallStaticVoidMethod(mPlayerViewClass, mNextSegmentMethodID);
	if (env->ExceptionCheck())
	{
		LOGINFO(METHOD, "Call to method  com/kaltura/hlsplayersdk/PlayerView.requestNextSegment() FAILED" );
	}
}


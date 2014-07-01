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

#include "stlhelpers.h"

#include "HLSSegment.h"
#include "HLSDataSource.h"


using namespace android;

#define CLASS_NAME APP_NAME"::HLSPlayer"

#define METHOD CLASS_NAME"::HLSPlayer()"
HLSPlayer::HLSPlayer(JavaVM* jvm) : mExtractorFlags(0),
mHeight(0), mWidth(0), mCropHeight(0), mCropWidth(0), mBitrate(0), mActiveAudioTrackIndex(-1),
mVideoBuffer(NULL), mWindow(NULL), mSurface(NULL), mRenderedFrameCount(0),
mAudioPlayer(NULL), mAudioSink(NULL), mTimeSource(NULL),
mDurationUs(0), mOffloadAudio(false), mStatus(HLSPlayer::STOPPED),
mAudioTrack(NULL), mVideoTrack(NULL), mJvm(jvm), mPlayerViewClass(NULL),
mNextSegmentMethodID(NULL), mSegmentTimeOffset(0), mVideoFrameDelta(0), mLastVideoTimeUs(0),
mSegmentForTimeMethodID(NULL), mFrameCount(0), mDataSource(NULL)
{
	status_t status = mClient.connect();
	LOGINFO(METHOD, "OMXClient::Connect return %d", status);
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
	stlwipe(mSegments);
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
status_t HLSPlayer::FeedSegment(const char* path, int quality, double time )
{

	// Make a data source from the file
	LOGINFO(METHOD, "path = '%s'", path);
	if (mDataSource == NULL) mDataSource = new HLSDataSource();

	status_t err = mDataSource->append(path);
	if (err != OK)
	{
		LOGERROR(METHOD, "append Failed: %s", strerror(-err));
		return err;
	}

	// I don't know if we still need this - might need to pass in the URL instead of the datasource
	HLSSegment* s = new HLSSegment(quality, time);
	if (s)
	{
		if (s->SetDataSource(mDataSource))
		{
			mSegments.push_back(s);
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

#define METHOD CLASS_NAME"::InitTracks()"
bool HLSPlayer::InitTracks()
{
	status_t err = mDataSource->initCheck();
	if (err != OK)
	{
		LOGERROR(METHOD, "DataSource is invalid: %s", strerror(-err));
		//return false;
	}


	mExtractor = MediaExtractor::Create(mDataSource);
	if (mExtractor == NULL)
	{
		LOGERROR(METHOD, "Could not create MediaExtractor from DataSource %0x", mDataSource.get());
		return false;
	}

	if (mExtractor->getDrmFlag())
	{
		LOGERROR(METHOD, "This datasource has DRM - not implemented!!!");
		return false;
	}

	int64_t totalBitRate = 0;
	for (size_t i = 0; i < mExtractor->countTracks(); ++i)
	{

		sp<MetaData> meta = mExtractor->getTrackMetaData(i); // this is likely to return an MPEG2TSSource

		int32_t bitrate = 0;
		if (!meta->findInt32(kKeyBitRate, &bitrate))
		{
			const char* mime = "[unknown]";
			meta->findCString(kKeyMIMEType, &mime);

			LOGINFO(METHOD, "Track #%d of type '%s' does not publish bitrate", i, mime );
			continue;
		}
		LOGINFO(METHOD, "bitrate for track %d = %d bits/sec", i , bitrate);
		totalBitRate += bitrate;
	}

	mBitrate = totalBitRate;



	LOGINFO(METHOD, "mBitrate = %lld bits/sec", mBitrate);

	bool haveAudio = false;
	bool haveVideo = false;

	for (size_t i = 0; i < mExtractor->countTracks(); ++i)
	{
		sp<MetaData> meta = mExtractor->getTrackMetaData(i);
		meta->dumpToLog();

		const char* cmime;
		if (meta->findCString(kKeyMIMEType, &cmime))
		{
			String8 mime = String8(cmime);

			if (!haveVideo && !strncasecmp(mime.string(), "video/", 6))
			{
				mVideoTrack = mExtractor->getTrack(i);
				haveVideo = true;

				// Set the presentation/display size
				int32_t width, height;
				bool res = meta->findInt32(kKeyWidth, &width);
				if (res)
				{
					res = meta->findInt32(kKeyHeight, &height);
				}
				if (res)
				{
					mWidth = width;
					mHeight = height;
					LOGINFO(METHOD, "Video Track Width = %d, Height = %d, %d", width, height, __LINE__);
				}
			}
			else if (!haveAudio && !strncasecmp(mime.string(), "audio/", 6))
			{
				mAudioTrack = mExtractor->getTrack(i);
				haveAudio = true;

				mActiveAudioTrackIndex = i;

			}
			else if (!strcasecmp(mime.string(), MEDIA_MIMETYPE_TEXT_3GPP))
			{
				//addTextSource_l(i, mExtractor->getTrack(i));
			}
		}
	}

	if (!haveAudio && !haveVideo)
	{
		return UNKNOWN_ERROR;
	}



	mExtractorFlags = mExtractor->flags();

	return true;
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
	mAudioPlayer->setSource(mAudioSource);
	mTimeSource = mAudioPlayer;

	return true;
}


#define METHOD CLASS_NAME"::InitSources()"
bool HLSPlayer::InitSources()
{
	if (!InitTracks()) return false;
	LOGINFO(METHOD, "Entered");
	if (mVideoTrack == NULL || mAudioTrack == NULL) return false;


	// Video
	sp<MediaSource> omxSource = OMXCodec::Create(mClient.interface(), mVideoTrack->getFormat(), false, mVideoTrack, NULL, 0, NULL /*nativeWindow*/);
	LOGINFO(METHOD, "OMXCodec::Create() (video) returned %0x", omxSource.get());
	mVideoSource = omxSource;

	audio_stream_type_t streamType = AUDIO_STREAM_MUSIC;
	if (mAudioSink != NULL)
	{
		streamType = mAudioSink->getAudioStreamType();
	}


	sp<MetaData> meta = mVideoSource->getFormat();
	meta->findInt32(kKeyWidth, &mWidth);
	meta->findInt32(kKeyHeight, &mHeight);
	int32_t left, top;
	meta->findRect(kKeyCropRect, &left, &top, &mCropWidth, &mCropHeight);

	UpdateWindowBufferFormat();

	// Audio
	mOffloadAudio = canOffloadStream(mAudioTrack->getFormat(), (mVideoTrack != NULL), false /*streaming http */, streamType);
	LOGINFO(METHOD, "mOffloadAudio == %s", mOffloadAudio ? "true" : "false");

	sp<MediaSource> omxAudioSource = OMXCodec::Create(mClient.interface(), mAudioTrack->getFormat(), false, mAudioTrack);
	LOGINFO(METHOD, "OMXCodec::Create() (audio) returned %0x", omxAudioSource.get());


	if (mOffloadAudio)
	{
		LOGINFO(METHOD, "Bypass OMX (offload) Line: %d", __LINE__);
		mAudioSource = mAudioTrack;
		//((HLSMediaSourceAdapter*)mAudioTrack.get())->append(s->GetAudioTrack());
	}
	else
	{
		LOGINFO(METHOD, "Not Bypassing OMX Line: %d", __LINE__);
		mAudioSource = omxAudioSource;
		//((HLSMediaSourceAdapter*)mAudioTrack.get())->append(omxAudioSource);
	}
	return true;
}


#define METHOD CLASS_NAME"::UpdateWindowBufferFormat()"
bool HLSPlayer::UpdateWindowBufferFormat()
{
	int32_t screenWidth = ANativeWindow_getWidth(mWindow);
	int32_t screenHeight = ANativeWindow_getHeight(mWindow);
	LOGINFO(METHOD, "screenWidth=%d | screenHeight=%d", screenWidth, screenHeight);

	int32_t bufferWidth = mWidth;
	int32_t bufferHeight = mHeight;
	LOGINFO(METHOD, "bufferWidth=%d | bufferHeight=%d", bufferWidth, bufferHeight);

	double screenScale = (double)screenWidth / (double)screenHeight;

	LOGINFO(METHOD, "screenScale=%f", screenScale);
	if (screenWidth > screenHeight)
	{
		double vidScale = (double)mWidth / (double)mHeight;
		bufferWidth = ((double)mWidth / vidScale) * screenScale; // width / bufferScale * screenScale
	}
	else if (screenHeight > screenWidth)
	{
		bufferHeight = (double)mWidth / screenScale;
	}

	LOGINFO(METHOD, "bufferWidth=%d | bufferHeight=%d", bufferWidth, bufferHeight);

	ANativeWindow_setBuffersGeometry(mWindow, bufferWidth, bufferHeight, 0);
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

	if (!InitSources()) return false;

	status_t err = mVideoSource->start();
	if (err == OK)
	{
		err = mAudioSource->start();
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
	//LOGINFO(METHOD, "Entered");

	if (mStatus != PLAYING)
	{
		LogStatus();
		return -1;
	}

	if (mVideoTrack != NULL)
	{
		int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
		//LOGINFO(METHOD, "Segment Count %d", segCount);
		if (segCount < 3) // (current segment + 2)
			RequestNextSegment();
	}

	MediaSource::ReadOptions options;
	bool rval = -1;
	for (;;)
	{
		//LOGINFO(METHOD, "mVideoBuffer = %0x", mVideoBuffer);
		RUNDEBUG(mVideoSource->getFormat()->dumpToLog());
		status_t err = OK;
		if (mVideoBuffer == NULL)
		{
			LOGINFO(METHOD, "Reading video buffer");
			err = mVideoSource->read(&mVideoBuffer, &options);
			if (err == OK && mVideoBuffer->range_length() != 0) ++mFrameCount;
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

			int64_t audioTime = mTimeSource->getRealTimeUs();

			LOGINFO(METHOD, "audioTime = %lld | videoTime = %lld | diff = %lld", audioTime, timeUs, audioTime - timeUs);



			if (timeUs > mLastVideoTimeUs)
			{
				mVideoFrameDelta += timeUs - mLastVideoTimeUs;
				LOGINFO(METHOD, "mVideoFrameDelta = %lld", mVideoFrameDelta);
			}
			else if (timeUs < mLastVideoTimeUs)
			{
				// okay - we need to do something to timeUs
				LOGINFO(METHOD, "mFrameCount = %lld", mFrameCount);
				if (timeUs + mSegmentTimeOffset + (mVideoFrameDelta / mFrameCount) < mLastVideoTimeUs)
				{
					mSegmentTimeOffset = mLastVideoTimeUs + (mVideoFrameDelta / mFrameCount);
				}

				timeUs += mSegmentTimeOffset;
			}

			LOGINFO(METHOD, "audioTime = %lld | videoTime = %lld | diff = %lld", audioTime, timeUs, audioTime - timeUs);

			int64_t delta = audioTime - timeUs;


			mLastVideoTimeUs = timeUs;
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
				}
				else
				{
					LOGINFO(METHOD, "Render Buffer returned false: STOPPING");
					SetStatus(STOPPED);
					rval=-1;
				}
				mVideoBuffer->release();
				mVideoBuffer = NULL;
				break;

			}
		}

		LOGINFO(METHOD, "Found empty buffer%d", __LINE__);
		// Some decoders return spurious empty buffers we want to ignore
		mVideoBuffer->release();
		mVideoBuffer = NULL;

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
	//LOGINFO(METHOD, "Entered");
	LOGINFO(METHOD, "Rendering Buffer size=%d", buffer->size());
	if (!mWindow) { LOGINFO(METHOD, "mWindow is NULL"); return false; }
	if (!buffer) { LOGINFO(METHOD, "the MediaBuffer is NULL"); return false; }
	//if (!buffer->graphicBuffer().get()){ LOGINFO(CLASS_NAME, "the MediaBuffer->graphicBuffer is NULL"); return false; }
	RUNDEBUG(buffer->meta_data()->dumpToLog());
	int colf = 0;
	bool res = mVideoSource->getFormat()->findInt32(kKeyColorFormat, &colf);
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

			//MediaSource* vt = (MediaSource*)mVideoSource.get();

			int32_t targetWidth = windowBuffer.width == windowBuffer.stride ? windowBuffer.width : windowBuffer.stride;
			int32_t targetHeight = windowBuffer.height;

			unsigned short *pixels = (unsigned short *)windowBuffer.bits;
			for(int i=0; i<windowBuffer.width * windowBuffer.height; i++)
				pixels[i] = 0x0000;

//			LOGINFO(METHOD, "mWidth=%d | mHeight=%d | mCropWidth=%d | mCropHeight=%d | buffer.width=%d | buffer.height=%d",
//							mWidth, mHeight, mCropWidth, mCropHeight, windowBuffer.width, windowBuffer.height);

			int32_t offsetx = (windowBuffer.width - mWidth) / 2;
			if (offsetx & 1 == 1) ++offsetx;
			int32_t offsety = (windowBuffer.height - mHeight) / 2;

			cc.convert(buffer->data(), mWidth, mHeight, 0, 0, mCropWidth, mCropHeight,
					windowBuffer.bits, targetWidth, targetHeight, offsetx, offsety,mCropWidth + offsetx, mCropHeight + offsety);
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
	if (mStatus != status)
	{
		mStatus = status;
		LOGINFO(METHOD, "Status Changed: %d", status);
	}
}

#define METHOD CLASS_NAME"::LogStatus()"
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

#define METHOD CLASS_NAME"::RequestSegmentForTime()"
void HLSPlayer::RequestSegmentForTime(double time)
{
	LOGINFO(METHOD, "Requesting segment for time %d", time);
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

	if (mSegmentForTimeMethodID == NULL)
	{
		mSegmentForTimeMethodID = env->GetStaticMethodID(mPlayerViewClass, "requestSegmentForTime", "(D)V" );
		if (env->ExceptionCheck())
		{
			mSegmentForTimeMethodID = NULL;
			LOGINFO(METHOD, "Could not find method com/kaltura/hlsplayersdk/PlayerView.requestSegmentForTime()" );
			return;
		}
	}

	env->CallStaticVoidMethod(mPlayerViewClass, mSegmentForTimeMethodID, time);
	if (env->ExceptionCheck())
	{
		LOGINFO(METHOD, "Call to method  com/kaltura/hlsplayersdk/PlayerView.requestSegmentForTime() FAILED" );
	}
}

void HLSPlayer::Seek(double time)
{
//	// Set seeking flag
//	mStatus = SEEKING;
//
//	// pause the audio player? Or do we reset it?
//	if (mAudioPlayer) mAudioPlayer->pause(false);
//
//	// Clear our data???
//	stlwipe(mSegments);
//	((HLSMediaSourceAdapter*)mVideoTrack.get())->clear();
//	((HLSMediaSourceAdapter*)mAudioTrack.get())->clear();
//
//	RequestSegmentForTime(time);
//
//	mVideoTrack->start();
//	mAudioTrack->start();
//	mAudioPlayer->start(true);


}


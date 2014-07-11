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

#include "stlhelpers.h"
#include "HLSSegment.h"

#include "androidVideoShim_ColorConverter.h"


using namespace android_video_shim;

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
	LOGI("OMXClient::Connect return %d", status);
}

HLSPlayer::~HLSPlayer()
{
}

void HLSPlayer::Close(JNIEnv* env)
{
	LOGI("Entered");
	Reset();
	if (mPlayerViewClass)
	{
		env->DeleteGlobalRef(mPlayerViewClass);
		mPlayerViewClass = NULL;
	}
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
}

void HLSPlayer::Reset()
{
	LOGI("Entered");
	mStatus = STOPPED;
	LogStatus();

	mDataSource.clear();
	mAudioTrack.clear();
	mVideoTrack.clear();
	mExtractor.clear();

	if (mAudioPlayer) mAudioPlayer->pause(true);
	mAudioSource.clear();

	if (mAudioPlayer)
	{
		delete mAudioPlayer;
		mAudioPlayer = NULL;
	}

	LOGI("Killing the video buffer");
	if (mVideoBuffer)
	{
		mVideoBuffer->release();
		mVideoBuffer = NULL;
	}
	if (mVideoSource != NULL) mVideoSource->stop();
	mVideoSource.clear();

	LOGI("Killing the segments");
	stlwipe(mSegments);
	LOGI("Killing the audio & video tracks");

	mLastVideoTimeUs = 0;
	mSegmentTimeOffset = 0;
	mVideoFrameDelta = 0;
	mFrameCount = 0;
}

///
/// Set Surface. Takes a java surface object
///
void HLSPlayer::SetSurface(JNIEnv* env, jobject surface)
{
	LOGI("Entered");

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

	mSurface = (jobject)env->NewGlobalRef(surface);

	ANativeWindow* window = ANativeWindow_fromSurface(env, mSurface);

	LOGI("Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface() - window = %0x", window);
//	LOGI("window->flags = %0x", window->flags);
//	LOGI("window->swapInterval Min: %d Max: %d", window->minSwapInterval, window->maxSwapInterval);
//	LOGI("window->dpi  x:%f y:%f", window->xdpi, window->ydpi);

	if (window)
	{
		SetNativeWindow(window);
	}

	if (mStatus == PLAYING)
	{
		UpdateWindowBufferFormat();
	}
}





void HLSPlayer::SetNativeWindow(ANativeWindow* window)
{
	LOGI("window = %0x", window);
	if (mWindow)
	{
		LOGI("::mWindow is already set to %0x", window);
		// Umm - resetting?
		ANativeWindow_release(mWindow);
	}
	mWindow = window;
}


status_t HLSPlayer::FeedSegment(const char* path, int quality, double time )
{

	// Make a data source from the file
	LOGI("path = '%s'", path);
	if (mDataSource == NULL)
	{
		mDataSource = new HLSDataSource();
		if (mDataSource.get())
		{
			mDataSource->patchTable();
		}
		else
		{
			return NO_MEMORY;
		}
	}

	LOGI("mDataSource = %p", mDataSource.get());

	status_t err = mDataSource->append(path);
	if (err != OK)
	{
		LOGE("append Failed: %s", strerror(-err));
		return err;
	}

	// I don't know if we still need this - might need to pass in the URL instead of the datasource
	HLSSegment* s = new HLSSegment(quality, time);
	if (s)
	{
		mSegments.push_back(s);
		return OK;
	}
	return NO_MEMORY;
}

bool HLSPlayer::InitTracks()
{
	LOGI("Entered: mDataSource=%p", mDataSource.get());
	status_t err = mDataSource->initCheck();
	if (err != OK)
	{
		LOGE("DataSource is invalid: %s", strerror(-err));
		//return false;
	}


	mExtractor = MediaExtractor::Create(mDataSource, "video/mp2ts");
	if (mExtractor == NULL)
	{
		LOGE("Could not create MediaExtractor from DataSource %0x", mDataSource.get());
		return false;
	}

//	if (mExtractor->getDrmFlag())
//	{
//		LOGERROR(METHOD, "This datasource has DRM - not implemented!!!");
//		return false;
//	}

	LOGI("Getting bit rate of stream");
	int64_t totalBitRate = 0;
	for (size_t i = 0; i < mExtractor->countTracks(); ++i)
	{

		sp<MetaData> meta = mExtractor->getTrackMetaData(i); // this is likely to return an MPEG2TSSource

		int32_t bitrate = 0;
		if (!meta->findInt32(kKeyBitRate, &bitrate))
		{
			const char* mime = "[unknown]";
			meta->findCString(kKeyMIMEType, &mime);

			LOGI("Track #%d of type '%s' does not publish bitrate", i, mime );
			continue;
		}
		LOGI("bitrate for track %d = %d bits/sec", i , bitrate);
		totalBitRate += bitrate;
	}

	mBitrate = totalBitRate;



	LOGI("mBitrate = %lld bits/sec", mBitrate);

	bool haveAudio = false;
	bool haveVideo = false;

	for (size_t i = 0; i < mExtractor->countTracks(); ++i)
	{
		sp<MetaData> meta = mExtractor->getTrackMetaData(i);
		meta->dumpToLog();

		const char* cmime;
		if (meta->findCString(kKeyMIMEType, &cmime))
		{
			//String8 mime = String8(cmime);

			if (!haveVideo && !strncasecmp(cmime /*mime.string()*/, "video/", 6))
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
					LOGI("Video Track Width = %d, Height = %d, %d", width, height, __LINE__);
				}
			}
			else if (!haveAudio && !strncasecmp(cmime /*mime.string()*/, "audio/", 6))
			{
				mAudioTrack = mExtractor->getTrack(i);
				haveAudio = true;

				mActiveAudioTrackIndex = i;

			}
//			else if (!strcasecmp(cmime /*mime.string()*/, MEDIA_MIMETYPE_TEXT_3GPP))
//			{
//				//addTextSource_l(i, mExtractor->getTrack(i));
//			}
		}
	}

	if (!haveAudio && !haveVideo)
	{
		return UNKNOWN_ERROR;
	}



	//mExtractorFlags = mExtractor->flags();

	return true;
}

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

	LOGI("Constructing AudioPlayer");
	mAudioPlayer = new AudioPlayer(mAudioSink, flags, NULL);
	LOGI("Setting AudioPlayer source %p", mAudioSource.get());
	mAudioPlayer->setSource(mAudioSource);
	LOGI("Storing audio player");
	mTimeSource = mAudioPlayer;

	return true;
}


bool HLSPlayer::InitSources()
{
	if (!InitTracks()) return false;
	LOGI("Entered");
	if (mVideoTrack == NULL || mAudioTrack == NULL) return false;


	// Video
	sp<IOMX> iomx = mClient.interface();
	sp<MetaData> vidFormat = mVideoTrack->getFormat();
	sp<MediaSource> omxSource = OMXCodec::Create(iomx, vidFormat, false, mVideoTrack, NULL, 0 /*, nativeWindow = NULL */);
	LOGI("OMXCodec::Create() (video) returned %0x", omxSource.get());
	mVideoSource = omxSource;

	audio_stream_type_t streamType = AUDIO_STREAM_MUSIC;
//	if (mAudioSink != NULL)
//	{
//		streamType = mAudioSink->getAudioStreamType();
//	}


	sp<MetaData> meta = mVideoSource->getFormat();
	meta->findInt32(kKeyWidth, &mWidth);
	meta->findInt32(kKeyHeight, &mHeight);
	int32_t left, top;
	meta->findRect(kKeyCropRect, &left, &top, &mCropWidth, &mCropHeight);

	UpdateWindowBufferFormat();

	// Audio
	mOffloadAudio = canOffloadStream(mAudioTrack->getFormat(), (mVideoTrack != NULL), false /*streaming http */, streamType);
	LOGI("mOffloadAudio == %s", mOffloadAudio ? "true" : "false");

	sp<MetaData> audioFormat = mAudioTrack->getFormat();
	sp<MediaSource> omxAudioSource = OMXCodec::Create(iomx, audioFormat, false, mAudioTrack, NULL, 0);
	LOGI("OMXCodec::Create() (audio) returned %p", omxAudioSource.get());


	if (mOffloadAudio)
	{
		LOGI("Bypass OMX (offload) Line: %d", __LINE__);
		mAudioSource = mAudioTrack;
		//((HLSMediaSourceAdapter*)mAudioTrack.get())->append(s->GetAudioTrack());
	}
	else
	{
		LOGI("Not Bypassing OMX Line: %d ", __LINE__);
		mAudioSource = omxAudioSource;
		//((HLSMediaSourceAdapter*)mAudioTrack.get())->append(omxAudioSource);
	}
	return true;
}


bool HLSPlayer::UpdateWindowBufferFormat()
{
	int32_t screenWidth = ANativeWindow_getWidth(mWindow);
	int32_t screenHeight = ANativeWindow_getHeight(mWindow);
	LOGI("screenWidth=%d | screenHeight=%d", screenWidth, screenHeight);

	int32_t bufferWidth = mWidth;
	int32_t bufferHeight = mHeight;
	LOGI("bufferWidth=%d | bufferHeight=%d", bufferWidth, bufferHeight);

	double screenScale = (double)screenWidth / (double)screenHeight;

	LOGI("screenScale=%f", screenScale);
	if (screenWidth > screenHeight)
	{
		double vidScale = (double)mWidth / (double)mHeight;
		bufferWidth = ((double)mWidth / vidScale) * screenScale; // width / bufferScale * screenScale
	}
	else if (screenHeight > screenWidth)
	{
		bufferHeight = (double)mWidth / screenScale;
	}

	LOGI("bufferWidth=%d | bufferHeight=%d", bufferWidth, bufferHeight);

	int32_t res = ANativeWindow_setBuffersGeometry(mWindow, bufferWidth, bufferHeight, WINDOW_FORMAT_RGB_565);
}

//
//  Play()
//
//		Tells the player to play the current stream. Requires that
//		segments have already been fed to the player.
//
bool HLSPlayer::Play()
{
	LOGI("Entered");
	if (!mWindow) { LOGI("mWindow is NULL"); return false; }
	LOGI("%d", __LINE__);

	if (!InitSources()) return false;

	status_t err = mVideoSource->start();
	if (err == OK)
	{
		err = mAudioSource->start();
		if (err == OK)
		{
			if (CreateAudioPlayer())
			{
				LOGI("Starting audio playback");
#ifdef USE_AUDIO
				err = mAudioPlayer->start(true);
#endif
				LOGI("   OK! err=%d", err);
				SetStatus(PLAYING);
				return true;
			}
			else
			{
				LOGI("Failed to create audio player : %d", __LINE__);
			}
		}
		else
		{
			LOGI("Audio Track failed to start: %s : %d", strerror(-err), __LINE__);
		}
	}
	else
	{
		LOGI("Video Track failed to start: %s : %d", strerror(-err), __LINE__);
	}
	return false;
}


int HLSPlayer::Update()
{
	//LOGI("Entered");

	if (mStatus != PLAYING)
	{
		LogStatus();
		return -1;
	}

	status_t audioPlayerStatus;
	if (mAudioPlayer->reachedEOS(&audioPlayerStatus))
	{
		LOGI("Audio player is at EOS, stopping...");
		mStatus = STOPPED;
		return -1;
	}


	if (mDataSource != NULL)
	{
		int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
		LOGI("Segment Count %d", segCount);
		if (segCount < 3) // (current segment + 2)
			RequestNextSegment();
	}


	MediaSource::ReadOptions options;
	bool rval = -1;
	for (;;)
	{
		//LOGI("mVideoBuffer = %0x", mVideoBuffer);
		RUNDEBUG(mVideoSource->getFormat()->dumpToLog());
		status_t err = OK;
		if (mVideoBuffer == NULL)
		{
			LOGI("Reading video buffer");
			err = mVideoSource->read(&mVideoBuffer, &options);
			if (err == OK && mVideoBuffer->range_length() != 0) ++mFrameCount;
		}
		if (err != OK)
		{
			LOGI("err=%s,%0x  Line: %d", strerror(-err), -err, __LINE__);
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
				LOGI("Frame did not have time value: STOPPING");
				SetStatus(STOPPED);
				return -1;
			}

#ifdef USE_AUDIO
			int64_t audioTime = mAudioPlayer->getRealTimeUs(); //mTimeSource->getRealTimeUs();
#else
			int64_t audioTime = timeUs;
#endif

			LOGI("audioTime = %lld | videoTime = %lld | diff = %lld", audioTime, timeUs, audioTime - timeUs);



			if (timeUs > mLastVideoTimeUs)
			{
				mVideoFrameDelta += timeUs - mLastVideoTimeUs;
				LOGI("mVideoFrameDelta = %lld", mVideoFrameDelta);
			}
			else if (timeUs < mLastVideoTimeUs)
			{
				// okay - we need to do something to timeUs
				LOGI("mFrameCount = %lld", mFrameCount);
				if (timeUs + mSegmentTimeOffset + (mVideoFrameDelta / mFrameCount) < mLastVideoTimeUs)
				{
					mSegmentTimeOffset = mLastVideoTimeUs + (mVideoFrameDelta / mFrameCount);
				}

				timeUs += mSegmentTimeOffset;
			}

			LOGI("audioTime = %lld | videoTime = %lld | diff = %lld", audioTime, timeUs, audioTime - timeUs);

			int64_t delta = audioTime - timeUs;


			mLastVideoTimeUs = timeUs;
			if (delta < -10000) // video is running ahead
			{
				LOGI("Video is running ahead - waiting til next time");
				break; // skip out - don't render it yet
			}
			else if (delta > 40000) // video is running behind
			{
				LOGI("Video is running behind - skipping frame");
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
					LOGI("mRenderedFrameCount = %d", mRenderedFrameCount);
				}
				else
				{
					LOGI("Render Buffer returned false: STOPPING");
					SetStatus(STOPPED);
					rval=-1;
				}
				mVideoBuffer->release();
				mVideoBuffer = NULL;
				break;

			}
		}

		LOGI("Found empty buffer (%d)", __LINE__);
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

bool HLSPlayer::RenderBuffer(MediaBuffer* buffer)
{
	//LOGI("Entered");
	LOGI("Rendering Buffer size=%d", buffer->size());
	if (!mWindow) { LOGI("mWindow is NULL"); return false; }
	if (!buffer) { LOGI("the MediaBuffer is NULL"); return false; }

	RUNDEBUG(buffer->meta_data()->dumpToLog());

	int colf = 0;
	bool res = mVideoSource->getFormat()->findInt32(kKeyColorFormat, &colf);
	LOGI("Found Frame Color Format: %s", res ? "true" : "false" );

	ColorConverter_Local lcc((OMX_COLOR_FORMATTYPE)colf, OMX_COLOR_Format16bitRGB565);
	LOGI("ColorConversion from %d is valid: %s", colf, lcc.isValid() ? "true" : "false" );

	ColorConverter cc((OMX_COLOR_FORMATTYPE)colf, OMX_COLOR_Format16bitRGB565); // Should be getting these from the formats, probably
	LOGI("ColorConversion from %d is valid: %s", colf, cc.isValid() ? "true" : "false" );

	bool useLocalCC = lcc.isValid();


	if (!useLocalCC && !cc.isValid())
	{
		LOGE("No Valid Color Conversion Found for %d", colf);
	}


	int64_t timeUs;
    if (buffer->meta_data()->findInt64(kKeyTime, &timeUs))
    {
    	//native_window_set_buffers_timestamp(mWindow, timeUs * 1000);
		//status_t err = mWindow->queueBuffer(mWindow, buffer->graphicBuffer().get(), -1);



		ANativeWindow_Buffer windowBuffer;
		if (ANativeWindow_lock(mWindow, &windowBuffer, NULL) == 0)
		{
			LOGI("buffer locked (%d x %d stride=%d, format=%d)", windowBuffer.width, windowBuffer.height, windowBuffer.stride, windowBuffer.format);

			//MediaSource* vt = (MediaSource*)mVideoSource.get();

			int32_t targetWidth = windowBuffer.width == windowBuffer.stride ? windowBuffer.width : windowBuffer.stride;
			int32_t targetHeight = windowBuffer.height;

			// TODO: This isn't right - it won't work correctly when the stride is larger than the width
			unsigned short *pixels = (unsigned short *)windowBuffer.bits;
			for(int i=0; i<windowBuffer.stride * windowBuffer.height; i++)
				pixels[i] = 0x0000;

			LOGV("mWidth=%d | mHeight=%d | mCropWidth=%d | mCropHeight=%d | buffer.width=%d | buffer.height=%d",
							mWidth, mHeight, mCropWidth, mCropHeight, windowBuffer.width, windowBuffer.height);

			int32_t offsetx = (windowBuffer.width - mWidth) / 2;
			if (offsetx & 1 == 1) ++offsetx;
			int32_t offsety = (windowBuffer.height - mHeight) / 2;

			status_t ccres;
			if (useLocalCC)
				ccres = lcc.convert(buffer->data(), mWidth, mHeight, 0, 0, mCropWidth, mCropHeight,
						windowBuffer.bits, targetWidth, targetHeight, offsetx, offsety,mCropWidth + offsetx, mCropHeight + offsety);
			else
				ccres = cc.convert(buffer->data(), mWidth, mHeight, 0, 0, mCropWidth, mCropHeight,
						windowBuffer.bits, targetWidth, targetHeight, offsetx, offsety,mCropWidth + offsetx, mCropHeight + offsety);

			if (ccres != OK) LOGE("ColorConversion error: %s (%d)", strerror(-ccres), -ccres);

			ANativeWindow_unlockAndPost(mWindow);
		}


		//LOGI("%d", __LINE__);
		//if (err != 0) {
		//	ALOGE("queueBuffer failed with error %s (%d)", strerror(-err),
		//			-err);
		//	return false;
		//}
		//LOGI("%d", __LINE__);

//		sp<MetaData> metaData = buffer->meta_data();
		//LOGI("%d", __LINE__);
//		metaData->setInt32(kKeyRendered, 1);
		//LOGI("%d", __LINE__);
		return true;
    }
    return false;

}

void HLSPlayer::SetStatus(int status)
{
	if (mStatus != status)
	{
		mStatus = status;
		LOGI("Status Changed: %d", status);
	}
}

void HLSPlayer::LogStatus()
{
	LOGI("Status = %d", mStatus);
}

void HLSPlayer::RequestNextSegment()
{
	LOGI("Requesting new segment");
	JNIEnv* env = NULL;
	mJvm->AttachCurrentThread(&env, NULL);

	if (mPlayerViewClass == NULL)
	{
		jclass c = env->FindClass("com/kaltura/hlsplayersdk/PlayerView");
		if ( env->ExceptionCheck() || c == NULL) {
			LOGI("Could not find class com/kaltura/hlsplayersdk/PlayerView" );
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
			LOGI("Could not find method com/kaltura/hlsplayersdk/PlayerView.requestNextSegment()" );
			return;
		}
	}

	env->CallStaticVoidMethod(mPlayerViewClass, mNextSegmentMethodID);
	if (env->ExceptionCheck())
	{
		LOGI("Call to method  com/kaltura/hlsplayersdk/PlayerView.requestNextSegment() FAILED" );
	}
}

void HLSPlayer::RequestSegmentForTime(double time)
{
	LOGI("Requesting segment for time %d", time);
	JNIEnv* env = NULL;
	mJvm->AttachCurrentThread(&env, NULL);

	if (mPlayerViewClass == NULL)
	{
		jclass c = env->FindClass("com/kaltura/hlsplayersdk/PlayerView");
		if ( env->ExceptionCheck() || c == NULL) {
			LOGI("Could not find class com/kaltura/hlsplayersdk/PlayerView" );
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
			LOGI("Could not find method com/kaltura/hlsplayersdk/PlayerView.requestSegmentForTime()" );
			return;
		}
	}

	env->CallStaticVoidMethod(mPlayerViewClass, mSegmentForTimeMethodID, time);
	if (env->ExceptionCheck())
	{
		LOGI("Call to method  com/kaltura/hlsplayersdk/PlayerView.requestSegmentForTime() FAILED" );
	}
}

int HLSPlayer::GetState()
{
	return mStatus;
}

void HLSPlayer::TogglePause()
{
	LOGI("mStatus = %d", mStatus);
	if (mStatus == PAUSED)
	{
		mStatus = PLAYING;
		status_t res = mAudioPlayer->resume();
		LOGI("AudioPlayer->resume() result = %s", strerror(-res));
	}
	else if (mStatus == PLAYING)
	{
		mStatus = PAUSED;
		mAudioPlayer->pause(false);
	}
}

void HLSPlayer::Stop()
{
	LOGI("STOPPING! mStatus = %d", mStatus);
	if (mStatus == PLAYING)
	{
		mStatus = STOPPED;
		mAudioPlayer->pause(false);
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


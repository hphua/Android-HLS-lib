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


#include "mpeg2ts_parser/MPEG2TSExtractor.h"

#include "stlhelpers.h"
#include "HLSSegment.h"

#include "androidVideoShim_ColorConverter.h"
#include "HLSPlayerSDK.h"
#include "cmath"


extern HLSPlayerSDK* gHLSPlayerSDK;

using namespace android_video_shim;

int SEGMENTS_TO_BUFFER = 2;

//////////
//
// Thread stuff
//
/////////

void* audio_thread_func(void* arg)
{
	LOGTRACE("%s", __func__);
	LOGTHREAD("audio_thread_func STARTING");
	AudioTrack* audioTrack = (AudioTrack*)arg;
	int refCount = audioTrack->addRef();
	LOGI("mJAudioTrack refCount = %d", refCount);

	int rval;
	while ( audioTrack->refCount() > 1 &&  (rval = audioTrack->Update()) != AUDIOTHREAD_FINISH)
	{
		if (rval == AUDIOTHREAD_WAIT)
		{
			sched_yield();
		}
		else if(audioTrack->getBufferSize() > (44100/10))
		{
			//LOGI("Buffer full enough yielding");
			sched_yield();
		}
	}

	refCount = audioTrack->release();
	JavaVM* jvm = gHLSPlayerSDK->getJVM();
	if (jvm) jvm->DetachCurrentThread();
	LOGI("mJAudioTrack refCount = %d", refCount);
	LOGTHREAD("audio_thread_func ENDING");
	return NULL;
}


HLSPlayer::HLSPlayer(JavaVM* jvm) : mExtractorFlags(0),
mHeight(0), mWidth(0), mCropHeight(0), mCropWidth(0), mBitrate(0), mActiveAudioTrackIndex(-1),
mVideoBuffer(NULL), mWindow(NULL), mSurface(NULL), mRenderedFrameCount(0),
mDurationUs(0), mOffloadAudio(false), mStatus(STOPPED),
mAudioTrack(NULL), mVideoTrack(NULL), mJvm(jvm), mPlayerViewClass(NULL),
mNextSegmentMethodID(NULL), mSetVideoResolutionID(NULL), mEnableHWRendererModeID(NULL), 
mSegmentTimeOffset(0), mVideoFrameDelta(0), mLastVideoTimeUs(0),
mSegmentForTimeMethodID(NULL), mFrameCount(0), mDataSource(NULL), audioThread(0),
mScreenHeight(0), mScreenWidth(0), mJAudioTrack(NULL), mStartTimeMS(0), mUseOMXRenderer(true),
mNotifyFormatChangeComplete(NULL), mNotifyAudioTrackChangeComplete(NULL),
mDroppedFrameIndex(0), mDroppedFrameLastSecond(0), mPostErrorID(NULL)
{
	LOGTRACE("%s", __func__);
	status_t status = mClient.connect();
	LOGI("OMXClient::Connect return %d", status);
	
	int err = initRecursivePthreadMutex(&lock);
	LOGI(" HLSPlayer mutex err = %d", err);
}

HLSPlayer::~HLSPlayer()
{
	LOGTRACE("%s", __func__);
	LOGI("Freeing %p", this);
}

void HLSPlayer::Close(JNIEnv* env)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

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
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Entered");
	Stop();
	LogState();

	ClearScreen();

	mDataSource.clear();
	mAlternateAudioDataSource.clear();
	mAudioTrack.clear();
	mAudioTrack23.clear();
	mVideoTrack.clear();
	mVideoTrack23.clear();
	mExtractor.clear();
	mAlternateAudioExtractor.clear();
	mAudioSource.clear();
	mAudioSource23.clear();

	if (mJAudioTrack)
	{
		int refCount = mJAudioTrack->release();
		LOGI("mJAudioTrack refCount = %d", refCount);
		mJAudioTrack = NULL;
		pthread_join(audioThread, NULL);
	}
	LOGI("Killing the video buffer");
	if (mVideoBuffer)
	{
		mVideoBuffer->release();
		mVideoBuffer = NULL;
	}
	mOMXRenderer.clear();
	clearOMX(mVideoSource);
	clearOMX(mVideoSource23);

	mDataSourceCache.clear();

	LOGI("Killing the audio & video tracks");

	mLastVideoTimeUs = 0;
	mSegmentTimeOffset = 0;
	mVideoFrameDelta = 0;
	mFrameCount = 0;
	mStartTimeMS = 0;

}

void HLSPlayer::SetSegmentCountTobuffer(int segmentCount)
{
	LOGI("Setting segment buffer count to %d", segmentCount);
	SEGMENTS_TO_BUFFER = segmentCount + 1; // The +1 is the segment we're playing
}

///
/// Set Surface. Takes a java surface object
///
void HLSPlayer::SetSurface(JNIEnv* env, jobject surface)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Entered %p", this);

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

	if(!surface)
	{
		// Tear down renderers.
		mOMXRenderer.clear();
		mSurface = NULL;
		SetNativeWindow(NULL);
		return;
	}

	// Note the surface.
	mSurface = (jobject)env->NewGlobalRef(surface);

	if (!mVideoSource.get() && !mVideoSource23.get())
	{
		LOGE("We don't have a valid video source");
		return;
	}

	// Look up metadata.
	sp<MetaData> meta;
	if(AVSHIM_USE_NEWMEDIASOURCE)
		meta = mVideoSource->getFormat();
	else
		meta = mVideoSource23->getFormat();
	
	if(!meta.get())
	{
		LOGE("No format available from the video source.");
		return;
	}

	// Get state for HW renderer path.
	const char *component = "";
	bool hasDecoderComponent = meta->findCString(kKeyDecoderComponent, &component);

	int colorFormat = -1;
	meta->findInt32(kKeyColorFormat, &colorFormat);

	if(mUseOMXRenderer)
	{
		// Set things up w/ OMX.
		LOGV("Trying OMXRenderer path!");

		LOGV("Getting IOMX");
		sp<IOMX> omx = mClient.interface();
		LOGV("   got %p", omx.get());

		int32_t decodedWidth, decodedHeight;
		meta->findInt32(kKeyWidth, &decodedWidth);
    	meta->findInt32(kKeyHeight, &decodedHeight);
    	int32_t vidWidth, vidHeight;
    	mVideoTrack_md->findInt32(kKeyWidth, &vidWidth);
        mVideoTrack_md->findInt32(kKeyHeight, &vidHeight);

		LOGI("Calling createRendererFromJavaSurface component='%s' %dx%d colf=%d", component, mWidth, mHeight, colorFormat);
		mOMXRenderer = omx.get()->createRendererFromJavaSurface(env, mSurface, 
			component, (OMX_COLOR_FORMATTYPE)colorFormat,
			decodedWidth, decodedHeight,
			vidWidth, vidHeight,
			0);
		LOGV("   o got %p", mOMXRenderer.get());

		if(!mOMXRenderer.get())
		{
			LOGE("OMXRenderer path failed, re-initializing with SW renderer path.");
			mUseOMXRenderer = false;
			NoteHWRendererMode(false, mWidth, mHeight, 4);
			return;
		}
	}

	if(!mOMXRenderer.get())
	{
		LOGI("Initializing SW renderer path %dx%d.", mWidth, mHeight);

		::ANativeWindow *window = ANativeWindow_fromSurface(env, mSurface);
		if(!window)
		{
			LOGE("Failed to get ANativeWindow from mSurface %p", mSurface);
			return;
			//assert(window);
		}

		SetNativeWindow(window);
		int32_t res = ANativeWindow_setBuffersGeometry(mWindow, mWidth, mHeight, WINDOW_FORMAT_RGB_565);
		if(res != OK)
		{
			LOGE("ANativeWindow_setBuffersGeometry returned %d", res);
		}
	}
}

bool HLSPlayer::EnsureJNI(JNIEnv** env)
{
	LOGTRACE("%s", __func__);
	if (!gHLSPlayerSDK) return false;
	if (!gHLSPlayerSDK->GetEnv(env)) return false;

	if (mPlayerViewClass == NULL)
	{
		jclass c = (*env)->FindClass("com/kaltura/hlsplayersdk/HLSPlayerViewController");
		if ( (*env)->ExceptionCheck() || c == NULL) {
			LOGI("Could not find class com/kaltura/hlsplayersdk/HLSPlayerViewController" );
			mPlayerViewClass = NULL;
			return false;
		}

		mPlayerViewClass = (jclass)(*env)->NewGlobalRef((jobject)c);

	}

	if (mNextSegmentMethodID == NULL)
	{
		mNextSegmentMethodID = (*env)->GetStaticMethodID(mPlayerViewClass, "requestNextSegment", "()V" );
		if ((*env)->ExceptionCheck())
		{
			mNextSegmentMethodID = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.requestNextSegment()" );
			return false;
		}
	}

	if (mSegmentForTimeMethodID == NULL)
	{
		mSegmentForTimeMethodID = (*env)->GetStaticMethodID(mPlayerViewClass, "requestSegmentForTime", "(D)D" );
		if ((*env)->ExceptionCheck())
		{
			mSegmentForTimeMethodID = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.requestSegmentForTime()" );
			return false;
		}
	}

	if (mSetVideoResolutionID == NULL)
	{
		mSetVideoResolutionID = (*env)->GetStaticMethodID(mPlayerViewClass, "setVideoResolution", "(II)V" );
		if ((*env)->ExceptionCheck())
		{
			mSetVideoResolutionID = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.setVideoResolution()" );
			return false;
		}
	}

	if (mEnableHWRendererModeID == NULL)
	{
		mEnableHWRendererModeID = (*env)->GetStaticMethodID(mPlayerViewClass, "enableHWRendererMode", "(ZIII)V" );
		if ((*env)->ExceptionCheck())
		{
			mEnableHWRendererModeID = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.enableHWRendererMode()" );
			return false;
		}
	}

	if (mNotifyFormatChangeComplete == NULL)
	{
		mNotifyFormatChangeComplete = (*env)->GetStaticMethodID(mPlayerViewClass, "notifyFormatChangeComplete", "(I)V");
		if ((*env)->ExceptionCheck())
		{
			mNotifyFormatChangeComplete = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.notifyFormatChangeComplete()");
			return false;
		}
	}

	if (mNotifyAudioTrackChangeComplete == NULL)
	{
		mNotifyAudioTrackChangeComplete = (*env)->GetStaticMethodID(mPlayerViewClass, "notifyAudioTrackChangeComplete", "(I)V");
		if ((*env)->ExceptionCheck())
		{
			mNotifyAudioTrackChangeComplete = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.notifyAudioTrackChangeComplete()");
			return false;
		}
	}

	if (mPostErrorID == NULL)
	{
		mPostErrorID = (*env)->GetStaticMethodID(mPlayerViewClass, "postNativeError", "(ILjava/lang/String;)V");
		if ((*env)->ExceptionCheck())
		{
			mPostErrorID = NULL;
			LOGI("Could not find method com/kaltura/hlsplayersdk/HLSPlayerViewController.postNativeError()");
			return false;
		}
	}




	return true;
}

void HLSPlayer::SetNativeWindow(::ANativeWindow* window)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("window = %p", window);
	if (mWindow)
	{
		LOGI("::mWindow is already set to %p", window);
		ANativeWindow_release(mWindow);
	}
	mWindow = window;
}

sp<HLSDataSource> MakeHLSDataSource()
{
	LOGTRACE("%s", __func__);
	sp<HLSDataSource> ds = new HLSDataSource();
	if (ds.get())
	{
		ds->patchTable();
	}
	return ds;
}

status_t HLSPlayer::FeedSegment(const char* path, int32_t quality, int continuityEra, const char* altAudioPath, int audioIndex, double time, int cryptoId, int altAudioCryptoId )
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);
	LOGI("Quality = %d | Continuity = %d | audioIndex = %d | path = %s | altAudioPath = %s | cryptoId = %d | altAudioCryptoId = %d", quality, continuityEra, audioIndex, path, altAudioPath == NULL ? "NULL" : altAudioPath, cryptoId, altAudioCryptoId);

	bool sameEra = false;

	if (mDataSource == NULL)
	{
		LOGI("Creating New Datasource");
		mDataSource = MakeHLSDataSource();
		if (!mDataSource.get())
			return NO_MEMORY;
	}

	if (altAudioPath != NULL && mAlternateAudioDataSource == NULL)
	{
		mAlternateAudioDataSource = MakeHLSDataSource();
		if (!mAlternateAudioDataSource.get())
			return NO_MEMORY;
	}

	sameEra = mDataSource->isSameEra(quality, continuityEra);

	if (sameEra && mAlternateAudioDataSource.get())
	{
		sameEra = mAlternateAudioDataSource->isSameEra(audioIndex, 0);
	}

	status_t err;

	if (sameEra)
	{
		LOGI("Same Era!");
		// Yay! We can just append!
		err = mDataSource->append(path, quality, continuityEra, time, cryptoId);
		if (err == INFO_DISCONTINUITY)
		{
			LOGE("Could not append to data source! This shouldn't happen as we already checked the validity of the append.");
		}

		if (mAlternateAudioDataSource.get() && altAudioPath)
		{
			err = mAlternateAudioDataSource->append(altAudioPath, audioIndex, 0, time, altAudioCryptoId);
			if (err == INFO_DISCONTINUITY)
			{
				LOGE("Could not append to alternate audio data source! This shouldn't happen as we already checked the validity of the append.");
			}
		}
	}
	else
	{
		// Now we need to check the last item in the data source cache

		// First, try to append it to the last source in the cache
		if (mDataSourceCache.size() > 0 && mDataSourceCache.back().isSameEra(quality, continuityEra, audioIndex))
		{
			LOGI("Adding to end of existing era");
			err = mDataSourceCache.back().dataSource->append(path, quality, continuityEra, time, cryptoId);
			if (err == INFO_DISCONTINUITY)
			{
				LOGE("Could not append to data source! This shouldn't happen as we already checked the validity of the append.");
			}

			if (mDataSourceCache.back().altAudioDataSource.get() && altAudioPath)
			{
				err = mDataSourceCache.back().altAudioDataSource->append(altAudioPath, audioIndex, 0, time, altAudioCryptoId);
				if (err == INFO_DISCONTINUITY)
				{
					LOGE("Could not append to alternate audio data source! This shouldn't happen as we already checked the validity of the append.");
				}
			}
		}
		else
		{
			LOGI("Making New Datasource!");
			DataSourceCacheObject dsc;
			dsc.dataSource = MakeHLSDataSource();
			dsc.dataSource->append(path, quality, continuityEra, time, cryptoId);
			if (altAudioPath != NULL)
			{
				dsc.altAudioDataSource = MakeHLSDataSource();
				dsc.altAudioDataSource->append(altAudioPath, audioIndex, 0, time, altAudioCryptoId);
			}
			mDataSourceCache.push_back(dsc);
		}
	}
	return OK;

}

bool HLSPlayer::DataSourceCacheObject::isSameEra(int32_t quality, int continuityEra, int audioIndex)
{
	LOGTRACE("%s", __func__);
	bool sameEra = dataSource->isSameEra(quality, continuityEra);
	if (sameEra && altAudioDataSource.get())
	{
		sameEra = altAudioDataSource->isSameEra(audioIndex, 0);
	}
	return sameEra;
}

bool HLSPlayer::InitTracks()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);
	LOGI("Entered: mDataSource=%p", mDataSource.get());
	if (!mDataSource.get()) return false;

	LOGI("Creating internal MEPG2 TS media extractor");
	mExtractor = new android::MPEG2TSExtractor(mDataSource);
	LOGI("Saw %d tracks", mExtractor->countTracks());

	if (mExtractor == NULL)
	{
		LOGE("Could not create MediaExtractor from DataSource @ %p", mDataSource.get());
		return false;
	}

	LOGI("Getting bit rate of streams.");
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
		RUNDEBUG(meta->dumpToLog());

		const char* cmime;
		if (meta->findCString(kKeyMIMEType, &cmime))
		{
			if (!haveVideo && !strncasecmp(cmime, "video/", 6))
			{
				if(AVSHIM_USE_NEWMEDIASOURCE)
				{
					LOGV2("Attempting to get video track");
					mVideoTrack = mExtractor->getTrackProxy(i);
					LOGV2("GOT IT");
				}
				else
				{
					LOGV2("Attempting to get video track 23");
					mVideoTrack23 = mExtractor->getTrackProxy23(i);
					LOGV2("GOT IT");
				}

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
					NoteVideoDimensions();
					LOGI("Video Track Width = %d, Height = %d, %d", width, height, __LINE__);
				}

				mVideoTrack_md = meta;
			}
			else if (!haveAudio && !strncasecmp(cmime, "audio/", 6))
			{
				if(AVSHIM_USE_NEWMEDIASOURCE)
				{
					LOGV2("Attempting to get video track");
					mAudioTrack = mExtractor->getTrackProxy(i);
					LOGV2("done");
				}
				else
				{
					LOGV2("Attempting to get video track 23");
					mAudioTrack23 = mExtractor->getTrackProxy23(i);
					LOGV2("done");
				}
				haveAudio = true;

				mActiveAudioTrackIndex = i;

				mAudioTrack_md = meta;
			}
//			else if (!strcasecmp(cmime /*mime.string()*/, MEDIA_MIMETYPE_TEXT_3GPP))
//			{
//				//addTextSource_l(i, mExtractor->getTrack(i));
//			}
		}
	}

	// Check for alternate audio case.
	if(mAlternateAudioDataSource.get())
	{
		LOGI("Considering alternate audio source %p...", mAlternateAudioDataSource.get());

		// Create our own extractor again.
		mAlternateAudioExtractor = new android::MPEG2TSExtractor(mAlternateAudioDataSource);

		if(mAlternateAudioExtractor.get())
		{
			LOGI("Saw %d tracks.", mAlternateAudioExtractor->countTracks());

			for (size_t i = 0; i < mAlternateAudioExtractor->countTracks(); ++i)
			{
				sp<MetaData> meta = mAlternateAudioExtractor->getTrackMetaData(i);
				RUNDEBUG(meta->dumpToLog());

				// Filter for audio mime types.
				const char* cmime;
				if (!meta->findCString(kKeyMIMEType, &cmime))
					continue;

				LOGI("Considering potential audio track of mime type %s", cmime);

				if (strncasecmp(cmime, "audio/", 6))
					continue;

				// Awesome, got one!
				if(AVSHIM_USE_NEWMEDIASOURCE)
					mAudioTrack = mAlternateAudioExtractor->getTrackProxy(i);
				else
					mAudioTrack23 = mAlternateAudioExtractor->getTrackProxy23(i);

				LOGI("Got alternate audio track %d", i);
				haveAudio = true;

				mActiveAudioTrackIndex = i; // TODO: This is probably questionable.

				mAudioTrack_md = meta;
				break;
			}
		}
		else
		{
			LOGE("Failed to create alternate audio track extractor.");
		}
	}

	if (!haveVideo)
	{
		PostError(MEDIA_ERROR_UNSUPPORTED, "Stream does not appear to have video." );
		LOGE("Error initializing tracks!");
		return UNKNOWN_ERROR;
	}

	LOGI("Initialized tracks: mVideoTrack=%p mVideoTrack23=%p mAudioTrack=%p mAudioTrack23=%p", mVideoTrack.get(), mVideoTrack23.get(), mAudioTrack.get(), mAudioTrack23.get());

	return true;
}

bool HLSPlayer::CreateAudioPlayer()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);
	LOGI("Constructing JAudioTrack");
	mJAudioTrack = new AudioTrack(mJvm);
	if (!mJAudioTrack)
		return false;

	if (!mJAudioTrack->Init())
	{
		LOGE("JAudioTrack::Init() failed - quitting CreateAudioPlayer");
		mJAudioTrack->release();
		mJAudioTrack = NULL;
		return false;
	}

	if (pthread_create(&audioThread, NULL, audio_thread_func, (void*)mJAudioTrack  ) != 0)
		return false;


	if(mAudioSource.get())
		mJAudioTrack->Set(mAudioSource);
	else if (mAudioSource23.get())
		mJAudioTrack->Set23(mAudioSource23);
	else
		mJAudioTrack->ClearAudioSource();

	return true;
}


bool HLSPlayer::InitSources()
{
	LOGTRACE("%s", __func__);

	if (!InitTracks())
	{
		LOGE("Aborting due to failure to init tracks.");
		return false;
	}

	AutoLock locker(&lock, __func__);

	LOGI("Entered");
	
	if(AVSHIM_USE_NEWMEDIASOURCE)
	{
		if (mVideoTrack == NULL)
			return false;
	}
	else
	{
		if (mVideoTrack23 == NULL)
			return false;		
	}

	LOGV("Past initial sanity check...");

	// Video
	sp<IOMX> iomx = mClient.interface();

	sp<MetaData> vidFormat;
	if(mVideoTrack_md.get() != NULL)
	{
		LOGV("    o Path C");
		vidFormat = mVideoTrack_md;
	}
	else if(AVSHIM_USE_NEWMEDIASOURCE)
	{
		LOGV("    o Path A");
		vidFormat = mVideoTrack->getFormat();
	}
	else if (!AVSHIM_USE_NEWMEDIASOURCE)
	{
		LOGV("    o Path B");
		vidFormat = mVideoTrack23->getFormat();
	}
	else
	{
		LOGV("No path found!");
	}
	
	LOGV("vidFormat look up round 1 complete");

	if(vidFormat.get() == NULL)
	{
		LOGE("No format available from the video track.");
		return false;
	}

	LOGI("Validating H.264 AVC profile level...");
	uint32_t vidDataType = 0;
	size_t vidDataLen = 0;
	unsigned char *vidDataBytes = NULL;
	if(vidFormat->findData(kKeyAVCC, &vidDataType, (void const **)&vidDataBytes, &vidDataLen))
	{
		// It's the second byte.
		int level = vidDataBytes[1];
#ifndef ALLOW_ALL_PROFILES
		if(level > 66)
		{
			LOGE("Tried to play video that exceeded baseline profile (%d > 66), aborting!", level);
			PostError(MEDIA_INCOMPATIBLE_PROFILE, "Tried to play video that exceeded baseline profile. Aborting.");
			return false;
		}
#endif
	}
	else
	{
		LOGE("Failed to find H.264 profile data!");
	}
	
	LOGI("Creating hardware video decoder...");

	if(AVSHIM_USE_NEWMEDIASOURCE)
	{
		LOGI("   - taking 4.x path");
		LOGI("OMXCodec::Create - format=%p track=%p videoSource=%p", vidFormat.get(), mVideoTrack.get(), mVideoSource.get());
		mVideoSource = OMXCodec::Create(iomx, vidFormat, false, mVideoTrack, NULL, 0);
		LOGI("   - got %p back", mVideoSource.get());
	}
	else
	{
		LOGV("   - taking 2.3 path");

		LOGV("OMXCodec::Create - format=%p track=%p videoSource=%p", vidFormat.get(), mVideoTrack23.get(), mVideoSource23.get());
		mVideoSource23 = OMXCodec::Create23(iomx, vidFormat, false, mVideoTrack23, NULL, 0);
		LOGV("   - got %p back", mVideoSource23.get());
	}
	
	LOGI("OMXCodec::Create() (video) returned 4x=%p 23=%p", mVideoSource.get(), mVideoSource23.get());

	sp<MetaData> meta;
	if(AVSHIM_USE_NEWMEDIASOURCE)
		meta = mVideoSource->getFormat();
	else
		meta = mVideoSource23->getFormat();

	if(!meta.get())
	{
		LOGE("No format available from the video source.");
		return false;
	}

	meta->findInt32(kKeyWidth, &mWidth);
	meta->findInt32(kKeyHeight, &mHeight);
	int32_t left, top;
	if(!meta->findRect(kKeyCropRect, &left, &top, &mCropWidth, &mCropHeight))
	{
		LOGW("Could not get crop rect, assuming full video size.");
		left = top = 0;
		mCropWidth = mWidth;
		mCropHeight = mHeight;
	}

	JNIEnv *env = NULL;
	EnsureJNI(&env);
	LOGV(" env=%p", env);

	// HAX for hw renderer path
	const char *component = "";
	bool hasDecoderComponent = meta->findCString(kKeyDecoderComponent, &component);

	int colorFormat = -1;
	meta->findInt32(kKeyColorFormat, &colorFormat);

	if(AVSHIM_HAS_OMXRENDERERPATH && hasDecoderComponent 
		&& colorFormat != OMX_COLOR_Format16bitRGB565 // Don't need this if it's already in a good format.
		&& dlopen("libstagefrighthw.so", RTLD_LAZY)) // Skip it if the hw lib isn't present as that's where this class comes from.
	{
		// Set things up w/ OMX.
		LOGV("Trying OMXRenderer init path!");
		mUseOMXRenderer = true;
	}
	else
	{
		LOGV("Trying normal renderer init path!");
		mUseOMXRenderer = false;
	}
	if (GetState() != SEEKING) NoteHWRendererMode(mUseOMXRenderer, mWidth, mHeight, 4);
	LOGV("Done");

	// We will get called back later to finish initialization of our renderers.

	// Audio
	mOffloadAudio = false;
	if (mAudioTrack.get() || mAudioTrack23.get())
	{
		if(AVSHIM_USE_NEWMEDIASOURCE)
			mOffloadAudio = canOffloadStream(mAudioTrack.get()?mAudioTrack->getFormat():NULL, (mVideoTrack != NULL), false /*streaming http */, AUDIO_STREAM_MUSIC);
		else
			mOffloadAudio = canOffloadStream(mAudioTrack23.get()?mAudioTrack23->getFormat():NULL, (mVideoTrack23 != NULL), false /*streaming http */, AUDIO_STREAM_MUSIC);

		LOGI("mOffloadAudio == %s", mOffloadAudio ? "true" : "false");

		sp<MetaData> audioFormat;
		if(AVSHIM_USE_NEWMEDIASOURCE)
			audioFormat = mAudioTrack->getFormat();
		else
			audioFormat = mAudioTrack23->getFormat();

		// Fall back to the MediaExtractor value for 3.x devices..
		if(audioFormat.get() == NULL)
			audioFormat = mAudioTrack_md;

		if(!audioFormat.get())
		{
			LOGE("No format available from the audio track.");
			return false;
		}

		RUNDEBUG(audioFormat->dumpToLog());

		LOGI("Creating audio sources (OMXCodec)");
		if(AVSHIM_USE_NEWMEDIASOURCE)
			mAudioSource = OMXCodec::Create(iomx, audioFormat, false, mAudioTrack, NULL, 0);
		else
			mAudioSource23 = OMXCodec::Create23(iomx, audioFormat, false, mAudioTrack23, NULL, 0);

		LOGI("OMXCodec::Create() (audio) returned %p %p", mAudioSource.get(), mAudioSource23.get());

		if (mOffloadAudio)
		{
			LOGI("Bypass OMX (offload) Line: %d", __LINE__);
			if(AVSHIM_USE_NEWMEDIASOURCE)
				mAudioSource = mAudioTrack;
			else
				mAudioSource23 = mAudioTrack23;
		}
	}
	else
	{
		clearOMX(mAudioSource);
		clearOMX(mAudioSource23);
	}

	meta.clear();
	vidFormat.clear();

	LOGI("All done!");

	return true;
}

//
//  Play()
//
//		Tells the player to play the current stream. Requires that
//		segments have already been fed to the player.
//
bool HLSPlayer::Play(double time)
{
	LOGTRACE("%s", __func__);
	LOGI("Entered");
	
	if (!InitSources()) return false;

	AutoLock locker(&lock, __func__);

	double segTime = mDataSource->getStartTime();
	mStartTimeMS = segTime * 1000;

	status_t err = OK;
	
	if(mVideoSource.get())
		err = mVideoSource->start();
	else
		err = mVideoSource23->start();

	if (err != OK)
	{
		LOGI("Video Track failed to start: %s : %d", strerror(-err), __LINE__);
		return false;
	}
	else
	{
		LOGI("Video Source Started");
	}

	if (!CreateAudioPlayer())
	{
		LOGI("Failed to create audio player : %d", __LINE__);
		return false;
	}


	if (time > 0)
	{
		ReadUntilTime(time - segTime);
		if (mJAudioTrack) mJAudioTrack->ReadUntilTime(time - segTime);
	}

	LOGI("Starting audio playback");

#ifdef USE_AUDIO
	if (!mJAudioTrack->Start())
	{
		LOGE("Failed to start audio track.");
		return false;
	}
#endif

	LOGI("   OK! err=%d", err);
	SetState(PLAYING);

	return true;
}

void HLSPlayer::ClearScreen()
{
	if (!mWindow) return;
	ANativeWindow_Buffer windowBuffer;
	if (ANativeWindow_lock(mWindow, &windowBuffer, NULL) == 0)
	{
		LOGV("buffer locked (%d x %d stride=%d, format=%d)", windowBuffer.width, windowBuffer.height, windowBuffer.stride, windowBuffer.format);

		int32_t targetWidth = windowBuffer.stride;
		int32_t targetHeight = windowBuffer.height;

		// Clear to black.
		unsigned short *pixels = (unsigned short *)windowBuffer.bits;

		memset(pixels, 0, windowBuffer.stride * windowBuffer.height * 2);

		ANativeWindow_unlockAndPost(mWindow);

		sched_yield();
	}
}


int HLSPlayer::Update()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGV2("Entered");

	RUNDEBUG(LogState());

	if (GetState() == FOUND_DISCONTINUITY)
	{
		if (mDataSourceCache.size() > 0)
		{
			SetState(FORMAT_CHANGING);
			LOGI("Found Discontinuity: Switching Sources");
			return INFO_DISCONTINUITY;
		}
		else
		{
			LOGI("Found Discontinuity: Out Of Sources!");
			SetState(WAITING_ON_DATA);
			return -1;
		}
	}

	if (GetState() == FORMAT_CHANGING)
	{
		LOGI("Video changing format!");
		return 0;
	}

	if (GetState() == SEEKING)
	{
		int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
		LOGI("Segment Count %d, seeking...", segCount);
		if (segCount < 1)
		{
			return 0; // keep going!
		}
	}
	else if (GetState() == WAITING_ON_DATA)
	{
		if (mDataSource != NULL)
		{
			int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
			if (segCount < SEGMENTS_TO_BUFFER) // (current segment + 2)
			{
				if (mDataSourceCache.size() > 0)
				{
					DATASRC_CACHE::iterator cur = mDataSourceCache.begin();
					DATASRC_CACHE::iterator end = mDataSourceCache.end();
					while (cur != end)
					{
						segCount += (*cur).dataSource->getPreloadedSegmentCount();
						++cur;
					}
				}
			}

			if (segCount < SEGMENTS_TO_BUFFER)
			{
				LOGI("**** WAITING_ON_DATA: Requesting next segment...");
				RequestNextSegment();
			}

		}
		else
		{
			LOGI("**** WAITING_ON_DATA: No DataSource : Requesting next segment...");
			RequestNextSegment();
		}
		return -1;
	}
	else if (GetState() != PLAYING)
	{
		LogState();
		return -1;
	}

	if (mDataSource != NULL)
	{
		int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
		LOGI("Segment Count %d, checking buffers...", segCount);
		if (segCount < SEGMENTS_TO_BUFFER) // (current segment + 2)
		{
			if (mDataSourceCache.size() > 0)
			{
				DATASRC_CACHE::iterator cur = mDataSourceCache.begin();
				DATASRC_CACHE::iterator end = mDataSourceCache.end();
				while (cur != end)
				{
					segCount += (*cur).dataSource->getPreloadedSegmentCount();
					++cur;
				}
			}

			if (segCount < SEGMENTS_TO_BUFFER)
			{
				LOGI("**** Requesting next segment...");
				RequestNextSegment();
			}
		}
	}


	bool rval = -1;
	for (;;)
	{
		//LOGI("mVideoBuffer = %x", mVideoBuffer);
		if(mVideoSource.get())
		{
			RUNDEBUG(mVideoSource->getFormat()->dumpToLog());
		}

		status_t err = OK;
		if (mVideoBuffer == NULL)
		{
			//LOGI("Reading video buffer");
			if(mVideoSource.get())
				err = mVideoSource->read(&mVideoBuffer, &mOptions);
			if(mVideoSource23.get())
				err = mVideoSource23->read(&mVideoBuffer, &mOptions23);

			if (err == OK && mVideoBuffer->range_length() != 0) ++mFrameCount;
		}

		if (err != OK)
		{
			LOGI("err=%s,%x  Line: %d", strerror(-err), -err, __LINE__);
			switch (err)
			{
			case INFO_FORMAT_CHANGED:
			case INFO_DISCONTINUITY:
			case INFO_OUTPUT_BUFFERS_CHANGED:
				// If it doesn't have a valid buffer, maybe it's informational?
				if (mVideoBuffer == NULL) 
				{
					return 0;
				}
				break;
			case ERROR_END_OF_STREAM:
				if (mDataSourceCache.size() > 0)
				{
					SetState(FORMAT_CHANGING);
					mDataSource->logContinuityInfo();
					LOGI("End Of Stream: Detected additional sources.");
					return INFO_DISCONTINUITY;
				}
				SetState(WAITING_ON_DATA);
				return -1;
				break;
			default:
				SetState(WAITING_ON_DATA);
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
				SetState(CUE_STOP);
				return -1;
			}

#ifdef USE_AUDIO
			int64_t audioTime = mJAudioTrack->GetTimeStamp();
#else
			// Set the audio time to the video time, which will keep the video running.
			// TODO: This should probably be set to system time with a delta, so that the video doesn't
			// run too fast.
			int64_t audioTime = timeUs;
#endif

			if (timeUs > mLastVideoTimeUs)
			{
				mVideoFrameDelta = timeUs - mLastVideoTimeUs;
			}
			else if (timeUs < mLastVideoTimeUs)
			{
				LogState();
				LOGE("timeUs = %lld | mLastVideoTimeUs = %lld :: Why did this happen? Were we seeking?", timeUs, mLastVideoTimeUs);
			}

			LOGTIMING("audioTime = %lld | videoTime = %lld | diff = %lld | mVideoFrameDelta = %lld", audioTime, timeUs, audioTime - timeUs, mVideoFrameDelta);

			int64_t delta = audioTime - timeUs;

			mLastVideoTimeUs = timeUs;
			if (delta < -10000) // video is running ahead
			{
				LOGTIMING("Video is running ahead - waiting til next time : delta = %lld", delta);
				//sched_yield();
				usleep(-10000 - delta);
				break; // skip out - don't render it yet
			}
			else if (delta > 40000) // video is running behind
			{
				LOGTIMING("Video is running behind - skipping frame : delta = %lld", delta);
				// Do we need to catch up?
				mVideoBuffer->release();
				mVideoBuffer = NULL;
				DroppedAFrame();
				continue;
			}
			else
			{
				LOGTIMING("audioTime = %lld | videoTime = %lld | diff = %lld | mVideoFrameDelta = %lld", audioTime, timeUs, audioTime - timeUs, mVideoFrameDelta);

				// We appear to have a valid buffer?! and we're in time!
				if (RenderBuffer(mVideoBuffer))
				{
					++mRenderedFrameCount;
					rval = mRenderedFrameCount;
					LOGV("mRenderedFrameCount = %d", mRenderedFrameCount);
				}
				else
				{
					LOGI("Render Buffer returned false: STOPPING");
					SetState(CUE_STOP);
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

    return rval; // keep going!
}


// Utility code to descramble QCOM tiled formats.
static size_t calculate64x32TileIndex(const size_t tileX, const size_t tileY, const size_t width, const size_t height)
{
    size_t index = tileX + (tileY & ~1) * width;

    if (tileY & 1)
        index += (tileX & ~3) + 2;
    else if (!((tileY == (height - 1)) && ((height & 1) != 0)))
        index += (tileX + 2) & ~3;

	return index;
}

static void convert_64x32_to_NV12(const uint8_t *src, uint8_t *dstPixels, const int pixelWidth, const int pitchBytes, const int pixelHeight)
{
#define TILE_W_PIXELS 64
#define TILE_H_PIXELS 32
#define TILE_SIZE_BYTES (TILE_W_PIXELS * TILE_H_PIXELS)
#define TILE_GROUP_SIZE_BYTES (4 * TILE_SIZE_BYTES)

    const int tile_w_count = (pixelWidth - 1) / TILE_W_PIXELS + 1;
    const int tile_w_count_aligned = (tile_w_count + 1) & ~1;

    const int luma_tile_h_count = (pixelHeight - 1) / TILE_H_PIXELS + 1;
    const int chroma_tile_h_count = (pixelHeight / 2 - 1) / TILE_H_PIXELS + 1;

    int luma_size = tile_w_count_aligned * luma_tile_h_count * TILE_SIZE_BYTES;
    if((luma_size % TILE_GROUP_SIZE_BYTES) != 0)
        luma_size = (((luma_size - 1) / TILE_GROUP_SIZE_BYTES) + 1) * TILE_GROUP_SIZE_BYTES;

    int curHeight = pixelHeight;
    for(int y = 0; y < luma_tile_h_count; y++) 
    {
        int curWidth = pixelWidth;
        for(int x = 0; x < tile_w_count; x++)
        {
            // Determine pointer of chroma data for this tile.
            const uint8_t *sourceChromaBits = src + luma_size;
            sourceChromaBits += calculate64x32TileIndex(x, y/2, tile_w_count_aligned, chroma_tile_h_count) * TILE_SIZE_BYTES;
            if (y & 1)
                sourceChromaBits += TILE_SIZE_BYTES/2;

        	// Determine pointer of luma data for this tile.
            const uint8_t *sourceLumaBits = src;
            sourceLumaBits += calculate64x32TileIndex(x, y,     tile_w_count_aligned, luma_tile_h_count) * TILE_SIZE_BYTES;

            // Output offset for luma data.
            int lumaOffset = y * TILE_H_PIXELS * pitchBytes + x * TILE_W_PIXELS;

            // Output offset for chroma data.
            int chromaOffset = (pixelHeight * pitchBytes) + (lumaOffset / pitchBytes) * pitchBytes/2 + (lumaOffset % pitchBytes);

            // Clamp to right edge of tile.
            int curTileWidth = curWidth;
            if (curTileWidth > TILE_W_PIXELS)
                curTileWidth = TILE_W_PIXELS;

            // Clamp to bottom edge of tile.
            int curTileHeight = curHeight;
            if (curTileHeight > TILE_H_PIXELS)
                curTileHeight = TILE_H_PIXELS;

            // We copy luma twice per "row" in following loop, so half our height.
            curTileHeight /= 2; 

            while (curTileHeight--) 
            {
            	// Copy luma pixels
            	for(int i=0; i<2; i++)
            	{
	                memcpy(&dstPixels[lumaOffset], sourceLumaBits, curTileWidth);
	                sourceLumaBits += TILE_W_PIXELS;
	                lumaOffset += pitchBytes;            		
            	}

            	// Copy chroma pixels.
                memcpy(&dstPixels[chromaOffset], sourceChromaBits, curTileWidth);
                sourceChromaBits += TILE_W_PIXELS;
                chromaOffset += pitchBytes;
            }

            curWidth -= TILE_W_PIXELS;
        }

        curHeight -= TILE_H_PIXELS;
    }

#undef TILE_W_PIXELS
#undef TILE_H_PIXELS
#undef TILE_SIZE_BYTES
#undef TILE_GROUP_SIZE_BYTES
}

struct I420ConverterFuncMap
{
 		/*
	     * getDecoderOutputFormat
	     * Returns the color format (OMX_COLOR_FORMATTYPE) of the decoder output.
	     * If it is I420 (OMX_COLOR_FormatYUV420Planar), no conversion is needed,
	     * and convertDecoderOutputToI420() can be a no-op.
	     */
	    int (*getDecoderOutputFormat)();
	
	    /*
	     * convertDecoderOutputToI420
	     * @Desc     Converts from the decoder output format to I420 format.
	     * @note     Caller (e.g. VideoEditor) owns the buffers
	     * @param    decoderBits   (IN) Pointer to the buffer contains decoder output
	     * @param    decoderWidth  (IN) Buffer width, as reported by the decoder
	     *                              metadata (kKeyWidth)
	     * @param    decoderHeight (IN) Buffer height, as reported by the decoder
	     *                              metadata (kKeyHeight)
	     * @param    decoderRect   (IN) The rectangle of the actual frame, as
	     *                              reported by decoder metadata (kKeyCropRect)
	     * @param    dstBits      (OUT) Pointer to the output I420 buffer
	     * @return   -1 Any error
	     * @return   0  No Error
	     */
	    int (*convertDecoderOutputToI420)(
	        void* decoderBits, int decoderWidth, int decoderHeight,
	        ARect decoderRect, void* dstBits);
	
	    /*
	     * getEncoderIntputFormat
	     * Returns the color format (OMX_COLOR_FORMATTYPE) of the encoder input.
	     * If it is I420 (OMX_COLOR_FormatYUV420Planar), no conversion is needed,
	     * and convertI420ToEncoderInput() and getEncoderInputBufferInfo() can
	     * be no-ops.
	     */
	    int (*getEncoderInputFormat)();
	
	    /* convertI420ToEncoderInput
	     * @Desc     This function converts from I420 to the encoder input format
	     * @note     Caller (e.g. VideoEditor) owns the buffers
	     * @param    srcBits       (IN) Pointer to the input I420 buffer
	     * @param    srcWidth      (IN) Width of the I420 frame
	     * @param    srcHeight     (IN) Height of the I420 frame
	     * @param    encoderWidth  (IN) Encoder buffer width, as calculated by
	     *                              getEncoderBufferInfo()
	     * @param    encoderHeight (IN) Encoder buffer height, as calculated by
	     *                              getEncoderBufferInfo()
	     * @param    encoderRect   (IN) Rect coordinates of the actual frame inside
	     *                              the encoder buffer, as calculated by
	     *                              getEncoderBufferInfo().
	     * @param    encoderBits  (OUT) Pointer to the output buffer. The size of
	     *                              this buffer is calculated by
	     *                              getEncoderBufferInfo()
	     * @return   -1 Any error
	     * @return   0  No Error
	     */
	    int (*convertI420ToEncoderInput)(
	        void* srcBits, int srcWidth, int srcHeight,
	        int encoderWidth, int encoderHeight, ARect encoderRect,
	        void* encoderBits);
	
	    /* getEncoderInputBufferInfo
	     * @Desc     This function returns metadata for the encoder input buffer
	     *           based on the actual I420 frame width and height.
	     * @note     This API should be be used to obtain the necessary information
	     *           before calling convertI420ToEncoderInput().
	     *           VideoEditor knows only the width and height of the I420 buffer,
	     *           but it also needs know the width, height, and size of the
	     *           encoder input buffer. The encoder input buffer width and height
	     *           are used to set the metadata for the encoder.
	     * @param    srcWidth      (IN) Width of the I420 frame
	     * @param    srcHeight     (IN) Height of the I420 frame
	     * @param    encoderWidth  (OUT) Encoder buffer width needed
	     * @param    encoderHeight (OUT) Encoder buffer height needed
	     * @param    encoderRect   (OUT) Rect coordinates of the actual frame inside
	     *                               the encoder buffer
	     * @param    encoderBufferSize  (OUT) The size of the buffer that need to be
	     *                              allocated by the caller before invoking
	     *                              convertI420ToEncoderInput().
	     * @return   -1 Any error
	     * @return   0  No Error
	     */
	    int (*getEncoderInputBufferInfo)(
	        int srcWidth, int srcHeight,
	        int* encoderWidth, int* encoderHeight,
	        ARect* encoderRect, int* encoderBufferSize);
};

I420ConverterFuncMap *gICFM = NULL;

bool checkI420Converter()
{
	if(gICFM)
		return true;

    // Find the entry point
    void (*getI420ColorConverter)(I420ConverterFuncMap *converter) =
        (void (*)(I420ConverterFuncMap*)) searchSymbol("getI420ColorConverter");
    
    if (getI420ColorConverter == NULL) {
        LOGW("I420ColorConverter: cannot load getI420ColorConverter");
        return false;
    }

    // Fill the function pointers.
    gICFM = new I420ConverterFuncMap();
    getI420ColorConverter(gICFM);

    LOGI("I420ColorConverter: libI420colorconvert.so loaded");
    return true;
}

bool HLSPlayer::RenderBuffer(MediaBuffer* buffer)
{
	LOGTRACE("%s", __func__);
	if(mOMXRenderer.get())
	{
        int fmt = -1;
        mVideoSource23->getFormat()->findInt32(kKeyColorFormat, &fmt);

		LOGI("Cond1 for hw path colf=%d", fmt);

        void *id;
        if (buffer->meta_data()->findPointer(kKeyBufferID, &id)) 
        {
			LOGV2("Cond2 for hw path");
            mOMXRenderer->render(id);
			LOGV2("Cond3 for hw path");
			//sched_yield();
            return true;
        }
	}

	//LOGI("Entered");
	if (!mWindow) { LOGI("mWindow is NULL"); return true; }
	if (!buffer) { LOGI("the MediaBuffer is NULL"); return true; }

	//RUNDEBUG(buffer->meta_data()->dumpToLog());
	LOGV("Buffer size=%d | range_offset=%d | range_length=%d", buffer->size(), buffer->range_offset(), buffer->range_length());

	// Get the frame's width and height.
	int videoBufferWidth = 0, videoBufferHeight = 0, vbCropTop = 0, vbCropLeft = 0, vbCropBottom = 0, vbCropRight = 0;
	sp<MetaData> vidFormat;
	if(mVideoSource.get())
		vidFormat = mVideoSource->getFormat();
	if(mVideoSource23.get())
		vidFormat = mVideoSource23->getFormat();

	if(!buffer->meta_data()->findInt32(kKeyWidth, &videoBufferWidth) || !buffer->meta_data()->findInt32(kKeyHeight, &videoBufferHeight))
	{
		LOGV("Falling back to source dimensions.");
		if(!vidFormat->findInt32(kKeyWidth, &videoBufferWidth) || !vidFormat->findInt32(kKeyHeight, &videoBufferHeight))
		{
			// I hope we're right!
			LOGV("Setting best guess width/height %dx%d", mWidth, mHeight);
			videoBufferWidth = mWidth;
			videoBufferHeight = mHeight;
		}
	}

	int stride = -1;
	if(!buffer->meta_data()->findInt32(kKeyStride, &stride))
	{
		LOGV("Trying source stride fallback");
		if(!vidFormat->findInt32(kKeyStride, &stride))
		{
			LOGV("Got no source stride");
		}
	}

	if(stride != -1)
	{
		LOGV("Got stride %d", stride);
	}

	int x = -1;
	buffer->meta_data()->findInt32(kKeyDisplayWidth, &x);
	if(x != -1)
	{
		LOGV("got dwidth = %d", x);
	}

	int sliceHeight = -1;
	if(!buffer->meta_data()->findInt32(kKeySliceHeight, &sliceHeight))
	{
		if(!vidFormat->findInt32(kKeySliceHeight, &sliceHeight))	
		{
			LOGV2("Failed to get vidFormat slice height.");
		}
	}

	if(!vidFormat->findRect(kKeyCropRect, &vbCropLeft, &vbCropTop, &vbCropRight, &vbCropBottom))
	{
		if(!buffer->meta_data()->findRect(kKeyCropRect, &vbCropLeft, &vbCropTop, &vbCropRight, &vbCropBottom))
		{
			vbCropTop = 0;
			vbCropLeft = 0;
			vbCropBottom = videoBufferHeight - 1;
			vbCropRight = videoBufferWidth - 1;
		}
	}
	LOGV("vbw=%d vbh=%d vbcl=%d vbct=%d vbcr=%d vbcb=%d", videoBufferWidth, videoBufferHeight, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom);

	if(sliceHeight != -1)
	{
		LOGV("Setting buffer slice height %d", sliceHeight);
		videoBufferHeight = sliceHeight;
	}
	
	int colf = 0, internalColf = 0;
	bool res = vidFormat->findInt32(kKeyColorFormat, &colf);

	// We need to unswizzle certain formats for maximum proper behavior.
	if(checkI420Converter())
		internalColf = OMX_COLOR_FormatYUV420Planar;
	else if(colf == QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka)
		internalColf = OMX_COLOR_FormatYUV420SemiPlanar;
	else if(colf == OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka)
		internalColf = OMX_QCOM_COLOR_FormatYVU420SemiPlanar;
	else
		internalColf = colf;

	LOGV("Found Frame Color Format: %s colf=%x internalColf=%x", res ? "true" : "false", colf, internalColf);

	const char *omxCodecString = "";
	res = vidFormat->findCString(kKeyDecoderComponent, &omxCodecString);
	LOGV("Found Frame decoder component: %s %s", res ? "true" : "false", omxCodecString);

	ColorConverter_Local lcc((OMX_COLOR_FORMATTYPE)internalColf, OMX_COLOR_Format16bitRGB565);
	LOGV("Local ColorConversion from %x is valid: %s", internalColf, lcc.isValid() ? "true" : "false" );

	ColorConverter cc((OMX_COLOR_FORMATTYPE)internalColf, OMX_COLOR_Format16bitRGB565); // Should be getting these from the formats, probably
	LOGV("System ColorConversion from %x is valid: %s", internalColf, cc.isValid() ? "true" : "false" );

	int64_t timeUs;
    if (buffer->meta_data()->findInt64(kKeyTime, &timeUs))
    {
		ANativeWindow_Buffer windowBuffer;
		if (ANativeWindow_lock(mWindow, &windowBuffer, NULL) == 0)
		{
			// Sanity check on relative dimensions
			if(windowBuffer.height < videoBufferHeight)
			{
				LOGE("Aborting conversion; window too short for video!");
				ANativeWindow_unlockAndPost(mWindow);
				sched_yield();
				mHeight = videoBufferHeight;
				NoteHWRendererMode(mUseOMXRenderer, mWidth, mHeight, 4);
				return true;
			}


			LOGV("buffer locked (%d x %d stride=%d, format=%d)", windowBuffer.width, windowBuffer.height, windowBuffer.stride, windowBuffer.format);

			int32_t targetWidth = windowBuffer.stride;
			int32_t targetHeight = windowBuffer.height;

			unsigned short *pixels = (unsigned short *)windowBuffer.bits;

#ifdef _BLITTEST
			// Clear to black.
			memset(pixels, rand(), windowBuffer.stride * windowBuffer.height * 2);
#endif

			unsigned char *videoBits = (unsigned char*)buffer->data() + buffer->range_offset();
			LOGV("Saw some source pixels: %x", *(int*)videoBits);

			LOGV("mWidth=%d | mHeight=%d | mCropWidth=%d | mCropHeight=%d | buffer.width=%d | buffer.height=%d | buffer.stride=%d | videoBits=%p",
							mWidth, mHeight, mCropWidth, mCropHeight, windowBuffer.width, windowBuffer.height, windowBuffer.stride, videoBits);

			LOGV("converting source coords, %d, %d, %d, %d, %d, %d", videoBufferWidth, videoBufferHeight, 0, 0, videoBufferWidth, videoBufferHeight);
			LOGV("converting target coords, %d, %d, %d, %d, %d, %d", targetWidth, targetHeight, 0, 0, videoBufferWidth, videoBufferHeight);

#if 0
			// Useful logic to vary behavior over time.
			static int offset = -64;
			static int frameCount = 0;
			frameCount++;
			if(frameCount == 10)
			{
				frameCount = 0;
				offset++;
			}

			LOGI("offset = %d", offset);
#endif

			// If it's a packed format round the size appropriately.
			if(checkI420Converter())
			{
				// Do we need to convert?
				unsigned char *tmpBuff = NULL;
				if(gICFM->getDecoderOutputFormat() != OMX_COLOR_FormatYUV420Planar)
				{
					LOGV("Alloc'ing tmp buffer due to decoder format %x.", gICFM->getDecoderOutputFormat());
					tmpBuff = (unsigned char*)malloc(videoBufferWidth*videoBufferHeight*4);

					LOGV("Converting to tmp buffer due to decoder format %x with func=%p videoBits=%p tmpBuff=%p.", 
						gICFM->getDecoderOutputFormat(), (void*)gICFM->convertDecoderOutputToI420, videoBits, tmpBuff);

					ARect crop = { vbCropLeft, vbCropTop, vbCropRight, vbCropBottom };
					int res = gICFM->convertDecoderOutputToI420(videoBits, videoBufferWidth, videoBufferHeight, crop, tmpBuff);
					if(res != 0)
					{
						LOGE("Failed internal conversion!");
					}
				}
				else
				{
					tmpBuff = videoBits;
				}

				if(cc.isValid())
				{
					LOGV("Doing system color conversion...");
					
					int a = vbCropLeft;
					int b = vbCropTop;
					int c = vbCropRight;
					int d = vbCropBottom;
					cc.convert(tmpBuff, videoBufferWidth,    videoBufferHeight,    a, b, c,   d,
						       pixels,  windowBuffer.stride, windowBuffer.height,  0, 0, c-a, d-b);
				}
				else if(lcc.isValid())
				{
					// We could use the local converter but the system one seems to work properly.
					LOGV("Doing YUV420 conversion %dx%d %p %d %p %d",videoBufferWidth, videoBufferHeight, 
						tmpBuff, 0, 
						pixels, windowBuffer.stride * 2);

					lcc.convertYUV420Planar(videoBufferWidth, videoBufferHeight, 
						tmpBuff, 0, 
						pixels, windowBuffer.stride * 2);

				}


				if(gICFM->getDecoderOutputFormat() != OMX_COLOR_FormatYUV420Planar)
				{
					LOGV("Freeing tmp buffer due to format %x.", gICFM->getDecoderOutputFormat());
					free(tmpBuff);
				}
			}
			else if(colf == QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka)
			{
				LOGV("colf = QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka");
				// Special case for QCOM tiled format as the shipped decoders seem busted.
				unsigned char *tmpBuff = (unsigned char*)malloc(videoBufferWidth*videoBufferHeight*3);
				convert_64x32_to_NV12(videoBits, tmpBuff, videoBufferWidth, videoBufferWidth, videoBufferHeight);

				int a = vbCropLeft;
				int b = vbCropTop;
				int c = vbCropRight;
				int d = vbCropBottom;
				cc.convert(tmpBuff, videoBufferWidth,    videoBufferHeight,    a, b, c,   d,
					       pixels,  windowBuffer.stride, windowBuffer.height,  0, 0, c-a, d-b);

				free(tmpBuff);
			}
			else if(colf == OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka)
			{
				LOGV("colf = OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka");
#define ALIGN(x,multiple)    (((x)+(multiple-1))&~(multiple-1))
				int a = vbCropLeft;
				int b = vbCropTop;
				int c = vbCropRight;
				int d = vbCropBottom;

				// Bump size to multiple of 32.
				LOGV("Rounding up to %dx%d", ALIGN(videoBufferWidth, 32), ALIGN(videoBufferHeight, 32));
				/*if(cc.isValid())
					cc.convert(videoBits, ALIGN(videoBufferWidth, 32), ALIGN(videoBufferHeight, 32), a, b, c, d,
					       pixels,  windowBuffer.stride, windowBuffer.height, a, b, c, d);
				else if(lcc.isValid()) */
					lcc.convertQCOMYUV420SemiPlanar(ALIGN(videoBufferWidth, 32), ALIGN(videoBufferHeight, 32), videoBits, 0, pixels, windowBuffer.stride * 2);
#undef ALIGN
			}
			else if(colf == OMX_COLOR_Format16bitRGB565)
			{
				LOGV("colf = OMX_COLOR_Format16bitRGB565");
				// Directly copy 16 bit color.
				size_t bufSize = buffer->range_length() - buffer->range_offset();
				
				if(bufSize >= targetWidth * videoBufferWidth * sizeof(short))
				{
					LOGV("A bufSize = %d", bufSize);
					for(int i=0; i<videoBufferHeight; i++)
					{
						//memset(pixels + i * targetWidth, rand(), targetWidth * sizeof(short));
						memcpy(pixels + i * targetWidth, 
								videoBits + i * targetWidth * sizeof(short), 
								videoBufferWidth * sizeof(short));
					}
				}
				else if(bufSize == videoBufferWidth * videoBufferHeight * sizeof(short))
				{
					LOGV("B bufSize = %d targetWidth=%d videoBufferWidth=%d", bufSize, targetWidth, videoBufferWidth);
					for(int i=0; i<videoBufferHeight; i++)
					{
						//memset(pixels + i * windowBuffer.width, rand(), targetWidth * sizeof(short));
						memcpy(pixels + i * windowBuffer.width,
								videoBits + i * videoBufferWidth * sizeof(short), 
								videoBufferWidth * sizeof(short));
					}
				}
				else
				{
					LOGE("Failed to copy 16 bit RGB buffer.");
				}
			}
			/*else if(colf == OMX_COLOR_FormatYUV420Planar)
			{
				lcc.convert(videoBufferWidth, videoBufferHeight, videoBits, 0, pixels, windowBuffer.stride * 2);
			}
			else if(colf == OMX_COLOR_FormatYUV420SemiPlanar)
			{
				lcc.convert(videoBufferWidth, videoBufferHeight, videoBits, 0, pixels, windowBuffer.stride * 2);
			}*/
			else if(cc.isValid())
			{
				LOGV("Using system converter");
				// Use the system converter.
				cc.convert(videoBits, videoBufferWidth, videoBufferHeight, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom,
						pixels, windowBuffer.stride, windowBuffer.height, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom);
			}
			else if(lcc.isValid())
			{
				LOGV("Using own converter");
				if (videoBufferHeight != windowBuffer.height)
				{
					LOGI("WindowBuffer && videoBuffer heights do not match: %d vs %d", windowBuffer.height, videoBufferHeight);
				}

				// Use our own converter.
				lcc.convert(videoBufferWidth, videoBufferHeight, videoBits, 0, pixels, windowBuffer.stride * 2);
			}
			else
			{
				LOGE("No conversion possible.");
			}

			//if (ccres != OK) LOGE("ColorConversion error: %s (%d)", strerror(-ccres), -ccres);

			ANativeWindow_unlockAndPost(mWindow);

			sched_yield();
		}

		return true;
    }
    return false;

}

void HLSPlayer::SetState(int status)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	if (mStatus != status)
	{
		LOGI("State Changing");
		LogState();
		mStatus = status;
		LogState();
	}
}

void HLSPlayer::LogState()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	switch (mStatus)
	{
	case STOPPED:
		LOGI("State = STOPPED");
		break;
	case PAUSED:
		LOGI("State = PAUSED");
		break;
	case PLAYING:
		LOGI("State = PLAYING");
		break;
	case SEEKING:
		LOGI("State = SEEKING");
		break;
	case FORMAT_CHANGING:
		LOGI("State = FORMAT_CHANGING");
		break;
	case FOUND_DISCONTINUITY:
		LOGI("State = FOUND_DISCONTINUITY");
		break;
	case WAITING_ON_DATA:
		LOGI("State = WAITING_ON_DATA");
		break;
	case CUE_STOP:
		LOGI("State = CUE_STOP");
		break;

	}
}

void HLSPlayer::RequestNextSegment()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Requesting new segment");
	JNIEnv* env = NULL;

	if (!EnsureJNI(&env)) return;

	env->CallStaticVoidMethod(mPlayerViewClass, mNextSegmentMethodID);
	if (env->ExceptionCheck())
	{
		LOGI("Call to method  com/kaltura/hlsplayersdk/HLSPlayerViewController.requestNextSegment() FAILED" );
	}
}

double HLSPlayer::RequestSegmentForTime(double time)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Requesting segment for time %lf", time);
	JNIEnv* env = NULL;
	if (!EnsureJNI(&env)) return 0;

	jdouble segTime = env->CallStaticDoubleMethod(mPlayerViewClass, mSegmentForTimeMethodID, time);
	if (env->ExceptionCheck())
	{
		LOGI("Call to method  com/kaltura/hlsplayersdk/HLSPlayerViewController.requestSegmentForTime() FAILED" );
	}
	return segTime;
}

void HLSPlayer::NoteVideoDimensions()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Noting video dimensions.");
	JNIEnv* env = NULL;

	if (!EnsureJNI(&env)) return;

	env->CallStaticVoidMethod(mPlayerViewClass, mSetVideoResolutionID, mWidth, mHeight);
	if (env->ExceptionCheck())
	{
		LOGI("Call to method  com/kaltura/hlsplayersdk/HLSPlayerViewController.setVideoResolution() FAILED" );
	}	
}


void HLSPlayer::NoteHWRendererMode(bool enabled, int w, int h, int colf)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Noting video dimensions.");
	JNIEnv* env = NULL;
	if (!EnsureJNI(&env)) return;

	env->CallStaticVoidMethod(mPlayerViewClass, mEnableHWRendererModeID, enabled, w, h, colf);
	if (env->ExceptionCheck())
	{
		env->ExceptionDescribe();
		LOGI("Call to method com/kaltura/hlsplayersdk/PlayerView.enableHWRendererMode() FAILED" );
	}	
}

int HLSPlayer::GetState()
{
	//LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);
	return mStatus;
}

void HLSPlayer::Pause(bool pause)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock);

	LogState();
	if (pause && GetState() == PLAYING)
	{
		SetState(PAUSED);
		mJAudioTrack->Pause();
	}
	else if (!pause && GetState() == PAUSED)
	{
		SetState(PLAYING);
		mJAudioTrack->Play();
	}

}

void HLSPlayer::Stop()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("STOPPING!");
	LogState();
	if (GetState() != STOPPED)
	{
		SetState(STOPPED);
		mJAudioTrack->Stop();
	}
}

int32_t HLSPlayer::GetCurrentTimeMS()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	if (mJAudioTrack != NULL)
	{
		return (mJAudioTrack->GetTimeStamp() / 1000) + mStartTimeMS;
	}
	return 0;
}


void HLSPlayer::StopEverything()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	// We might need to clear these before we stop (so we don't get stuck waiting)
	mAudioSource.clear();
	mAudioSource23.clear();
	if (mJAudioTrack) mJAudioTrack->Stop(true); // Passing true means we're seeking.

	mAudioTrack.clear();
	mAudioTrack23.clear();
	mVideoTrack.clear();
	mVideoTrack23.clear();
	mExtractor.clear();
	mAlternateAudioExtractor.clear();

	LOGI("Killing the video buffer");
	if (mVideoBuffer)
	{
		mVideoBuffer->release();
		mVideoBuffer = NULL;
	}
	clearOMX(mVideoSource);
	clearOMX(mVideoSource23);

	mLastVideoTimeUs = 0;
	mSegmentTimeOffset = 0;
	mVideoFrameDelta = 0;
	mFrameCount = 0;
}

bool HLSPlayer::EnsureAudioPlayerCreatedAndSourcesSet()
{
	LOGTRACE("%s", __func__);
	if (!mJAudioTrack)
	{
		return CreateAudioPlayer(); // CreateAudioPlayer sets the sources internally
	}
	else
	{
		if(mAudioSource.get())
			return mJAudioTrack->Set(mAudioSource);
		else if (mAudioSource23.get())
			return mJAudioTrack->Set23(mAudioSource23);
		else
		{
			mJAudioTrack->ClearAudioSource();
			return true;
		}
	}
	return false;
}

void HLSPlayer::ApplyFormatChange()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	SetState(FORMAT_CHANGING); // may need to add a different state, but for now...
	StopEverything();

	// Retrieve the current quality markers
	int curQuality = mDataSource->getQualityLevel();
	int curAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) curAudioTrack = mAlternateAudioDataSource->getQualityLevel();

	if (mDataSourceCache.size() > 0)
	{
		mDataSource.clear();
		mAlternateAudioDataSource.clear();
		mDataSource = (*mDataSourceCache.begin()).dataSource;
		mAlternateAudioDataSource = (*mDataSourceCache.begin()).altAudioDataSource;
		mDataSourceCache.pop_front();
	}

	mDataSource->logContinuityInfo();

	// Retrieve the new quality markers
	int newQuality = mDataSource->getQualityLevel();
	int newAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) newAudioTrack = mAlternateAudioDataSource->getQualityLevel();

	mStartTimeMS = (mDataSource->getStartTime() * 1000);

	LOGI("DataSource Start Time = %f", mDataSource->getStartTime());

	if (!InitSources())
	{
		LOGE("InitSources failed!");
		SetState(CUE_STOP);
		return;
	}

	status_t err;
	if(mVideoSource.get())
		err = mVideoSource->start();
	else
		err = mVideoSource23->start();

	if (err == OK)
	{
		if (!EnsureAudioPlayerCreatedAndSourcesSet())
		{
			LOGE("Setting Audio Tracks failed!");
		}
	}
	else
	{
		LOGI("Video Track failed to start: %s : %d", strerror(-err), __LINE__);
	}
	SetState(PLAYING);
	if (mJAudioTrack)
	{
		mJAudioTrack->Start();
	}

	NotifyFormatChange(curQuality, newQuality, curAudioTrack, newAudioTrack);
}

void HLSPlayer::PostError(int error, const char* msg)
{
	LOGE("Posting error %d : %s", error, msg);
	JNIEnv* env = NULL;
	if (!EnsureJNI(&env)) return;

	jstring jmsg = env->NewStringUTF(msg);
	env->CallStaticVoidMethod(mPlayerViewClass, mPostErrorID, error, jmsg);
	env->DeleteLocalRef(jmsg); // Cleaning up
}

void HLSPlayer::NotifyFormatChange(int curQuality, int newQuality, int curAudio, int newAudio)
{
	LOGTRACE("%s", __func__);
	if (curQuality == newQuality && curAudio == newAudio) return; // Nothing to notify

	AutoLock locker(&lock, __func__);

	JNIEnv* env = NULL;
	if (!EnsureJNI(&env)) return;

	if (curQuality != newQuality)
	{
		env->CallStaticVoidMethod(mPlayerViewClass, mNotifyFormatChangeComplete, newQuality);
		if (env->ExceptionCheck())
		{
			env->ExceptionDescribe();
			LOGI("Call to method com/kaltura/hlsplayersdk/PlayerView.notifyFormatChangeComplete() FAILED" );
		}
	}

	if (curAudio != newAudio)
	{
		env->CallStaticVoidMethod(mPlayerViewClass, mNotifyAudioTrackChangeComplete, newAudio);
		if (env->ExceptionCheck())
		{
			env->ExceptionDescribe();
			LOGI("Call to method com/kaltura/hlsplayersdk/PlayerView.notifyAudioTrackChangeComplete() FAILED" );
		}
	}

}

void HLSPlayer::Seek(double time)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Seeking To: %f", time);
	if (time < 0) time = 0;

	SetState(SEEKING);

	StopEverything();

	// Retrieve the current quality markers
	int curQuality = mDataSource->getQualityLevel();
	int curAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) curAudioTrack = mAlternateAudioDataSource->getQualityLevel();

	mDataSource.clear();
	mAlternateAudioDataSource.clear();
	mDataSourceCache.clear();

	// Need to request new segment because we killed all the data sources
	double segTime = RequestSegmentForTime(time);

	if (!mDataSource.get())
	{
		SetState(CUE_STOP);
		return;
	}
	mDataSource->logContinuityInfo();

	// Retrieve the new quality markers
	int newQuality = mDataSource->getQualityLevel();
	int newAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) newAudioTrack = mAlternateAudioDataSource->getQualityLevel();

	RequestNextSegment();

	mStartTimeMS = (mDataSource->getStartTime() * 1000);

	LOGI("Seeking To: %f | Segment Start Time = %f", time, segTime);

	int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
	if (!InitSources())
	{
		LOGE("InitSources failed!");
		return;
	}

	status_t err;
	if(mVideoSource.get())
		err = mVideoSource->start();
	else
		err = mVideoSource23->start();

	if (err == OK)
	{
		if (!EnsureAudioPlayerCreatedAndSourcesSet())
		{
			LOGE("Setting Audio Tracks failed!");
		}
	}
	else
	{
		LOGI("Video Track failed to start: %s : %d", strerror(-err), __LINE__);
	}

	bool doFormatChange = false;
	doFormatChange = !ReadUntilTime(time - segTime);
	if (!doFormatChange && mJAudioTrack)
	{
		doFormatChange = (!mJAudioTrack->ReadUntilTime(time - segTime));
	}


	if (doFormatChange)
	{
		ApplyFormatChange();
	}
	else
	{
		LOGI("Segment Count %d", segCount);
		SetState(PLAYING);
		if (mJAudioTrack)
		{
			// Call Start instead of Play, in order to ensure that the internal time values are correctly starting from zero.
			mJAudioTrack->Start();
		}

		NotifyFormatChange(curQuality, newQuality, curAudioTrack, newAudioTrack);
	}

	NoteHWRendererMode(mUseOMXRenderer, mWidth, mHeight, 4);
}

bool HLSPlayer::ReadUntilTime(double timeSecs)
{
	LOGTRACE("%s", __func__);
	status_t res = ERROR_END_OF_STREAM;
	MediaBuffer* mediaBuffer = NULL;

	int64_t targetTimeUs = (int64_t)(timeSecs * 1000000.0f);
	int64_t timeUs = 0;

	LOGI("Starting read to %f seconds: targetTimeUs = %lld", timeSecs, targetTimeUs);
	while (timeUs < targetTimeUs)
	{
		if(mVideoSource.get())
			res = mVideoSource->read(&mediaBuffer, NULL);

		if(mVideoSource23.get())
			res = mVideoSource23->read(&mediaBuffer, NULL);


		if (res == OK)
		{
			bool rval = mediaBuffer->meta_data()->findInt64(kKeyTime, &timeUs);
			if (!rval)
			{
				LOGI("Frame did not have time value: STOPPING");
				timeUs = 0;
			}

			//LOGI("Finished reading from the media buffer");
			RUNDEBUG(mediaBuffer->meta_data()->dumpToLog());
			LOGTIMING("key time = %lld | target time = %lld", timeUs, targetTimeUs);
		}
		else if (res == INFO_FORMAT_CHANGED)
		{
		}
		else if (res == ERROR_END_OF_STREAM)
		{
			LOGE("End of Video Stream");
			return false;
		}

		if (mediaBuffer != NULL)
		{
			mediaBuffer->release();
			mediaBuffer = NULL;
		}
		sched_yield();
	}

	mLastVideoTimeUs = timeUs;
	return true;
}


// I did not add this to a class or a header because I don't expect it to be used anywhere else
// All the other timing is based off the audio
uint32_t getTimeMS()
{

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint32_t)((now.tv_sec*1000000000LL + now.tv_nsec) / 100000);
}

void HLSPlayer::UpdateDroppedFrameInfo()
{
	// If we don't have a last second, reset everything to 0
	if (mDroppedFrameLastSecond == 0)
	{
		mDroppedFrameIndex = 0;
		memset(mDroppedFrameCounts, 0, sizeof(int) * MAX_DROPPED_FRAME_SECONDS);
		mDroppedFrameLastSecond = getTimeMS();
	}

	LOGI("GetTimeMS: %d" , getTimeMS());

	// If we've gone beyond a second, move our index up one, and set it to 0 to start
	// that seconds count over again.
	if (getTimeMS() - 1000 > mDroppedFrameLastSecond)
	{
		mDroppedFrameLastSecond = getTimeMS();
		mDroppedFrameIndex = (mDroppedFrameIndex + 1) % MAX_DROPPED_FRAME_SECONDS;
		LOGI("mDroppedFrameIndex = %d", mDroppedFrameIndex);
		mDroppedFrameCounts[mDroppedFrameIndex] = 0;
	}
}

void HLSPlayer::DroppedAFrame()
{
	AutoLock locker(&lock, __func__);

	UpdateDroppedFrameInfo();


	// Increment the count in the current second
	mDroppedFrameCounts[mDroppedFrameIndex]++;
}

int HLSPlayer::DroppedFramesPerSecond()
{
	AutoLock locker(&lock, __func__);

	UpdateDroppedFrameInfo();

	int sum = 0;
	for (int i = 0; i < MAX_DROPPED_FRAME_SECONDS; ++i)
	{
		sum += mDroppedFrameCounts[i];
		//LOGI("Dropped Frames Count [%d] = %d", i, mDroppedFrameCounts[i]);
	}
	//LOGI("Dropped Frames Sum = %d", sum);
	float dfs = ((float) sum / (float)MAX_DROPPED_FRAME_SECONDS);
	//LOGI("Dropped Frames Average = %f", dfs);
	return round(dfs);
}

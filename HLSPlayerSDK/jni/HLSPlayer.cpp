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
#include "androidVideoShim_ColorConverter444.h"
#include "HLSPlayerSDK.h"
#include "cmath"


#ifdef _FRAME_DUMP
#include <sys/system_properties.h>
#endif




extern HLSPlayerSDK* gHLSPlayerSDK;

using namespace android_video_shim;



int SEGMENTS_TO_BUFFER = 2; // The number of segments to buffer in addition to the currently playing segment

// I did not add this to a class or a header because I don't expect it to be used in any other file
// All the other timing is based off the audio
uint32_t getTimeMS()
{

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint32_t)((now.tv_sec*1000000000LL + now.tv_nsec) / NANOSEC_PER_MS);
}

void* aligned_malloc(size_t required_bytes, size_t alignment)
{
    void* p1; // original block
    void** p2; // aligned block
    int offset = alignment - 1 + sizeof(void*);
    if ((p1 = (void*)malloc(required_bytes + offset)) == NULL)
    {
       return NULL;
    }
    p2 = (void**)(((size_t)(p1) + offset) & ~(alignment - 1));
    p2[-1] = p1;
    return p2;
}

void aligned_free(void *p)
{
    free(((void**)p)[-1]);
}

//////////
//
// Thread stuff
//
/////////

void* audio_thread_func(void* arg)
{
	LOGTRACE("%s", __func__);
	LOGTHREAD("audio_thread_func STARTING");
	AudioPlayer* audioTrack = (AudioPlayer*)arg;
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
mSegmentTimeOffset(0), mVideoFrameDelta(0), mLastVideoTimeUs(-1), mVideoStartDelta(0),
mSegmentForTimeMethodID(NULL), mFrameCount(0), mDataSource(NULL), audioThread(0),
mScreenHeight(0), mScreenWidth(0), mAudioPlayer(NULL), mStartTimeMS(0), mUseOMXRenderer(true),
mNotifyFormatChangeComplete(NULL), mNotifyAudioTrackChangeComplete(NULL),
mDroppedFrameIndex(0), mDroppedFrameLastSecond(0), mPostErrorID(NULL), mPadWidth(0)
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

	if (mAudioPlayer)
	{
		int refCount = mAudioPlayer->release();
		LOGI("mJAudioTrack refCount = %d", refCount);
		mAudioPlayer = NULL;
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

	mLastVideoTimeUs = -1;
	mVideoStartDelta = 0;
	mSegmentTimeOffset = 0;
	mVideoFrameDelta = 0;
	mFrameCount = 0;
	mStartTimeMS = 0;

}


void HLSPlayer::SetSegmentCountToBuffer(int segmentCount)
{
	LOGI("Setting segment buffer count to %d", segmentCount);
	SEGMENTS_TO_BUFFER = segmentCount;
}

int HLSPlayer::GetSegmentCountToBuffer()
{
	return SEGMENTS_TO_BUFFER;
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
		mPostErrorID = (*env)->GetStaticMethodID(mPlayerViewClass, "postNativeError", "(IZLjava/lang/String;)V");
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

	int segCount = GetBufferedSegmentCount();


	sameEra = mDataSource->isSameEra(quality, continuityEra);

	if (sameEra && mAlternateAudioDataSource.get())
	{
		sameEra = mAlternateAudioDataSource->isSameEra(audioIndex, 0);
	}

	status_t err;

	bool noCurrentSegments = GetBufferedSegmentCount() == 0;

	if (sameEra)
	{
		if (GetState() == WAITING_ON_DATA && noCurrentSegments)
		{
			RestartPlayer(path, quality, continuityEra, altAudioPath, audioIndex, time, cryptoId, altAudioCryptoId);
			return OK;
		}
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
		PostError(MEDIA_ERROR_UNSUPPORTED, true, "Stream does not appear to have video." );
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
	mAudioPlayer = MakeAudioPlayer(mJvm, USE_OMX_AUDIO);
	if (!mAudioPlayer)
		return false;

	if (!mAudioPlayer->Init())
	{
		LOGE("JAudioTrack::Init() failed - quitting CreateAudioPlayer");
		mAudioPlayer->release();
		mAudioPlayer = NULL;
		return false;
	}

	if (pthread_create(&audioThread, NULL, audio_thread_func, (void*)mAudioPlayer  ) != 0)
		return false;


	if(mAudioSource.get())
		mAudioPlayer->Set(mAudioSource);
	else if (mAudioSource23.get())
		mAudioPlayer->Set23(mAudioSource23);
	else
		mAudioPlayer->ClearAudioSource();

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
			PostError(MEDIA_INCOMPATIBLE_PROFILE, true, "Tried to play video that exceeded baseline profile. Aborting.");
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
		const char* decoder;
		mVideoSource->getFormat()->findCString(kKeyDecoderComponent, &decoder);
		if (!strcasecmp(decoder, "OMX.qcom.video.decoder.avc"))
		{
			mPadWidth = 64;
			LOGI("Padding width to %d for decoder %s", mPadWidth, decoder);

		}

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
		if (USE_OMX_AUDIO)
		{
			if(AVSHIM_USE_NEWMEDIASOURCE)
				mAudioSource = OMXCodec::Create(iomx, audioFormat, false, mAudioTrack, NULL, 0);
			else
				mAudioSource23 = OMXCodec::Create23(iomx, audioFormat, false, mAudioTrack23, NULL, 0);

			LOGI("OMXCodec::Create() (audio) returned %p %p", mAudioSource.get(), mAudioSource23.get());
		}

		if (mOffloadAudio || !USE_OMX_AUDIO)
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
		if (USE_OMX_AUDIO)
		{
			clearOMX(mAudioSource);
			clearOMX(mAudioSource23);
		}
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
		ReadUntilTime(time);
		if (mAudioPlayer) mAudioPlayer->ReadUntilTime(time);
	}

	LOGI("Starting audio playback");

#ifdef USE_AUDIO
	if (!mAudioPlayer->Start())
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

int64_t HLSPlayer::GetLastTimeUS()
{
	return mLastVideoTimeUs;
}

int HLSPlayer::GetBufferedSegmentCount()
{
	int segCount = 0;
	if (mDataSource != NULL)
	{
		segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
	}

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
	return segCount;
}

long lastTouchTimeMS = 0;

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
			int segCount = GetBufferedSegmentCount();

			if (segCount < SEGMENTS_TO_BUFFER)
			{
				//LOGI("**** WAITING_ON_DATA: Requesting next segment... bufferedSegments: %d", segCount);
				RequestNextSegment();
			}
		}
		else
		{
			//LOGI("**** WAITING_ON_DATA: No DataSource : Requesting next segment...");
			RequestNextSegment();
		}
		return -1;
	}
	else if (GetState() != PLAYING)
	{
		LogState();
		return -1;
	}

	// Touch stuff!
	if (lastTouchTimeMS < GetCurrentTimeMS())
	{
		lastTouchTimeMS = GetCurrentTimeMS() + 1000;

		if (mDataSource != NULL)
		{
			((HLSDataSource*)mDataSource.get())->touch();
		}

		if (mDataSourceCache.size() > 0)
		{
			DATASRC_CACHE::iterator cur = mDataSourceCache.begin();
			DATASRC_CACHE::iterator end = mDataSourceCache.end();
			while (cur != end)
			{
				(*cur).dataSource->touch();
				++cur;
			}
		}
	}

	if (mDataSource != NULL)
	{
		int segCount = GetBufferedSegmentCount();
		LOGI("Segment Count %d, checking buffers...", segCount);
		if (segCount < SEGMENTS_TO_BUFFER)
		{
			RequestNextSegment();
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
			switch (err)
			{
			case INFO_DISCONTINUITY:
				LOGI("Discontinuity");
				mDataSource->logContinuityInfo();
				if (mVideoBuffer == NULL)
				{
					return 0;
				}
				break;
			case INFO_OUTPUT_BUFFERS_CHANGED:
				// If it doesn't have a valid buffer, maybe it's informational?
				LOGI("Output Buffers Changed");
				mDataSource->logContinuityInfo();
				if (mVideoBuffer == NULL) 
				{
					return 0;
				}
				break;
			case INFO_FORMAT_CHANGED:
				LOGI("Format Changed");
				mDataSource->logContinuityInfo();
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
				LOGI("End Of Stream: No additional sources detected");
				SetState(WAITING_ON_DATA);
				return -1;
				break;
			default:
				LOGI("Unhandled error err=%s,%x  Line: %d", strerror(-err), -err, __LINE__);
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
			int64_t audioTime = mAudioPlayer->GetTimeStamp();
#else
			// Set the audio time to the video time, which will keep the video running.
			// TODO: This should probably be set to system time with a delta, so that the video doesn't
			// run too fast.
			int64_t audioTime = timeUs;
#endif

			if (timeUs > mLastVideoTimeUs)
			{
				if (mLastVideoTimeUs == -1)
				{
					LOGTIMING("Setting mVideoStartDelta to %lld", timeUs);
					mLastVideoTimeUs = timeUs;

				}

				mVideoFrameDelta = timeUs - mLastVideoTimeUs;
			}
			else if (timeUs < mLastVideoTimeUs)
			{
				LogState();
				LOGE("timeUs = %lld | mLastVideoTimeUs = %lld :: Why did this happen? Were we seeking?", timeUs, mLastVideoTimeUs);
			}

			LOGTIMING("audioTime = %lld | videoTime = %lld | diff = %lld | mVideoFrameDelta = %lld", audioTime, timeUs, audioTime - timeUs, mVideoFrameDelta);

			int64_t delta = (audioTime + mVideoStartDelta) - timeUs;

			mLastVideoTimeUs = timeUs;
			if (delta < -10000) // video is running ahead
			{
				unsigned long sleepyTime = (-delta >  50000 ? 40000 : -10000 - delta);
				LOGTIMING("Video is running ahead - waiting til next time : delta = %lld : sleeping %lld", delta, -10000 - delta);
				//sched_yield();
				usleep(sleepyTime);
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
        LOGV("I420ColorConverter: cannot load getI420ColorConverter");
        return false;
    }

    // Fill the function pointers.
    gICFM = new I420ConverterFuncMap();
    getI420ColorConverter(gICFM);

    LOGV("I420ColorConverter: libI420colorconvert.so loaded");
    return true;
}

#ifdef _FRAME_DUMP
struct FrameHeader
{
	int32_t width;
	int32_t height;
	int32_t stride;
	int32_t format;
	int32_t cropleft;
	int32_t croptop;
	int32_t cropright;
	int32_t cropbottom;
	int32_t datasize;
	int32_t i420;
	char deviceString[1024];
};

void dumpFrame(FrameHeader& header, unsigned char* buffer)
{
	memset(header.deviceString, 0, 1024 ); // zero the device string - just to be sure
	char model_string[PROP_VALUE_MAX + 1];
	char name_string[PROP_VALUE_MAX + 1];
	char device_string[PROP_VALUE_MAX + 1];
	char build_string[PROP_VALUE_MAX + 1];
	char mfr_string[PROP_VALUE_MAX + 1];

	__system_property_get("ro.product.model", model_string);
	__system_property_get("ro.product.name", name_string);
	__system_property_get("ro.product.device", device_string);
	__system_property_get("ro.build.version.release", build_string);
	__system_property_get("ro.product.manufacturer", mfr_string);

	snprintf(header.deviceString, 1024, "mfr=%s model=%s name=%s device=%s build=%s", mfr_string, name_string, model_string, device_string, build_string);


	LOGI("Dumping Frame to /sdcard/vidbuffer.raw");
	FILE* frameFile = fopen("/sdcard/vidbuffer.raw", "wb+");
	if (frameFile != NULL)
	{
		LOGI("Writing Frame Header: { %d, %d, %d, 0x%0x, %d, %d, %d, %d, %d, %d, %s }", header.width, header.height, header.stride, header.format, header.cropleft, header.croptop, header.cropright, header.cropbottom, header.datasize, header.i420, header.deviceString);
		int res = fwrite(&header, sizeof(header), 1, frameFile);

		if (int err = ferror(frameFile) != 0)
		{
			LOGI("Error saving frame header: %d bytes written, err = %d", res, err);
		}
		LOGI("Wrote %d element", res);
		res = fwrite(buffer, sizeof(char), header.datasize, frameFile);
		if (int err = ferror(frameFile) != 0)
		{
			LOGI("Error saving frame data: %d bytes written, err = %d", res, err);
		}
		LOGI("Wrote %d bytes of %d", res, header.datasize);

		fclose(frameFile);

	}
	else
	{
		LOGI("Error opening Frame Dump File");
	}
}

#endif

bool HLSPlayer::RenderBuffer(MediaBuffer* buffer)
{
	LOGTRACE("%s", __func__);
	if(mOMXRenderer.get())
	{
        int fmt = -1;
        mVideoSource23->getFormat()->findInt32(kKeyColorFormat, &fmt);

		LOGRENDER("Cond1 for hw path colf=%d", fmt);

        void *id;
        if (buffer->meta_data()->findPointer(kKeyBufferID, &id)) 
        {
        	LOGRENDER("Cond2 for hw path");
            mOMXRenderer->render(id);
            LOGRENDER("Cond3 for hw path");
			//sched_yield();
            return true;
        }
	}

	//LOGI("Entered");
	if (!mWindow) { LOGI("mWindow is NULL"); return true; }
	if (!buffer) { LOGI("the MediaBuffer is NULL"); return true; }

	//RUNDEBUG(buffer->meta_data()->dumpToLog());
	LOGRENDER("Buffer size=%d | range_offset=%d | range_length=%d", buffer->size(), buffer->range_offset(), buffer->range_length());

	// Get the frame's width and height.
	int videoBufferWidth = 0, videoBufferHeight = 0, vbCropTop = 0, vbCropLeft = 0, vbCropBottom = 0, vbCropRight = 0;
	sp<MetaData> vidFormat;
	if(mVideoSource.get())
		vidFormat = mVideoSource->getFormat();
	if(mVideoSource23.get())
		vidFormat = mVideoSource23->getFormat();

	if(!buffer->meta_data()->findInt32(kKeyWidth, &videoBufferWidth) || !buffer->meta_data()->findInt32(kKeyHeight, &videoBufferHeight))
	{
		LOGRENDER("Falling back to source dimensions.");
		if(!vidFormat->findInt32(kKeyWidth, &videoBufferWidth) || !vidFormat->findInt32(kKeyHeight, &videoBufferHeight))
		{
			// I hope we're right!
			LOGRENDER("Setting best guess width/height %dx%d", mWidth, mHeight);
			videoBufferWidth = mWidth;
			videoBufferHeight = mHeight;
		}
	}

	int stride = -1;
	if(!buffer->meta_data()->findInt32(kKeyStride, &stride))
	{
		LOGRENDER("Trying source stride fallback");
		if(!vidFormat->findInt32(kKeyStride, &stride))
		{
			LOGRENDER("Got no source stride");
		}
	}

	if(stride != -1)
	{
		LOGRENDER("Got stride %d", stride);
	}

	int x = -1;
	buffer->meta_data()->findInt32(kKeyDisplayWidth, &x);
	if(x != -1)
	{
		LOGRENDER("got dwidth = %d", x);
	}

	int sliceHeight = -1;
	if(!buffer->meta_data()->findInt32(kKeySliceHeight, &sliceHeight))
	{
		if(!vidFormat->findInt32(kKeySliceHeight, &sliceHeight))	
		{
			LOGRENDER("Failed to get vidFormat slice height.");
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
	LOGRENDER("vbw=%d vbh=%d vbcl=%d vbct=%d vbcr=%d vbcb=%d", videoBufferWidth, videoBufferHeight, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom);

	if(sliceHeight != -1)
	{
		LOGRENDER("Setting buffer slice height %d", sliceHeight);
		videoBufferHeight = sliceHeight;
	}
	
	int colf = 0, internalColf = 0;
	bool res = vidFormat->findInt32(kKeyColorFormat, &colf);

	// We need to unswizzle certain formats for maximum proper behavior.
	if(checkI420Converter())
	{
		if (colf == 0x7fa30c04 || colf == QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka)
			internalColf = OMX_COLOR_FormatYUV420Planar;
		else
			internalColf = colf;

	}
	else if(colf == QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka)
		internalColf = OMX_COLOR_FormatYUV420SemiPlanar;
	else if(colf == OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka)
		internalColf = OMX_QCOM_COLOR_FormatYVU420SemiPlanar;
	else
		internalColf = colf;

	if (mPadWidth != 0 && (videoBufferWidth % mPadWidth != 0))
	{
		videoBufferWidth = ((videoBufferWidth + (mPadWidth - 1))&~(mPadWidth - 1));
	}

#ifdef _FRAME_DUMP
	static int frameCount = 0;
	++frameCount;
	LOGI("Frame Dump Frame Count = %d", frameCount);
	if (frameCount == _FRAME_DUMP && !checkI420Converter())
	{
		FrameHeader f = { videoBufferWidth, videoBufferHeight, stride, internalColf, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom, buffer->range_length() - buffer->range_offset(), 0 };
		dumpFrame(f, (unsigned char*)buffer->data() + buffer->range_offset());
	}
#endif

	LOGRENDER("Found Frame Color Format: %s colf=0x%x internalColf=0x%x", res ? "true" : "false", colf, internalColf);

	const char *omxCodecString = "";
	res = vidFormat->findCString(kKeyDecoderComponent, &omxCodecString);
	LOGRENDER("Found Frame decoder component: %s %s", res ? "true" : "false", omxCodecString);

	ColorConverter_Local lcc((OMX_COLOR_FORMATTYPE)internalColf, OMX_COLOR_Format16bitRGB565);
	LOGRENDER("Local ColorConversion from 0x%x is valid: %s", internalColf, lcc.isValid() ? "true" : "false" );

	ColorConverter cc((OMX_COLOR_FORMATTYPE)internalColf, OMX_COLOR_Format16bitRGB565); // Should be getting these from the formats, probably
	LOGRENDER("System ColorConversion from 0x%x is valid: %s", internalColf, cc.isValid() ? "true" : "false" );

	ColorConverter444 cc444((OMX_COLOR_FORMATTYPE)internalColf, OMX_COLOR_Format16bitRGB565); // Should be getting these from the formats, probably
	LOGRENDER("444 ColorConversion from 0x%x is valid: %s", internalColf, cc444.isValid() ? "true" : "false" );

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


			LOGRENDER("buffer locked (%d x %d stride=%d, format=%d)", windowBuffer.width, windowBuffer.height, windowBuffer.stride, windowBuffer.format);

			int32_t targetWidth = windowBuffer.stride;
			int32_t targetHeight = windowBuffer.height;

			unsigned short *pixels = (unsigned short *)windowBuffer.bits;

#ifdef _BLITTEST
			// Clear to black.
			memset(pixels, rand(), windowBuffer.stride * windowBuffer.height * 2);
#endif

			unsigned char *videoBits = (unsigned char*)buffer->data() + buffer->range_offset();
			LOGRENDER("Saw some source pixels: %x", *(int*)videoBits);

			LOGRENDER("mWidth=%d | mHeight=%d | mCropWidth=%d | mCropHeight=%d | buffer.width=%d | buffer.height=%d | buffer.stride=%d | videoBits=%p",
							mWidth, mHeight, mCropWidth, mCropHeight, windowBuffer.width, windowBuffer.height, windowBuffer.stride, videoBits);

			LOGRENDER("converting source coords, %d, %d, %d, %d, %d, %d", videoBufferWidth, videoBufferHeight, 0, 0, videoBufferWidth, videoBufferHeight);
			LOGRENDER("converting target coords, %d, %d, %d, %d, %d, %d", targetWidth, targetHeight, 0, 0, videoBufferWidth, videoBufferHeight);

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

			int tmpBuffSize = buffer->range_length() - buffer->range_offset();

#ifdef _FRAME_DUMP
			int isi420 = 1;
#endif
			// If it's a packed format round the size appropriately.
			if(checkI420Converter())
			{
				// Do we need to convert?
				unsigned char *tmpBuff = NULL;
				if(gICFM->getDecoderOutputFormat() != OMX_COLOR_FormatYUV420Planar)
				{
					LOGRENDER("Alloc'ing tmp buffer due to decoder format %x.", gICFM->getDecoderOutputFormat());
					tmpBuff = (unsigned char*)malloc(videoBufferWidth*videoBufferHeight*4);

#ifdef _FRAME_DUMP
					tmpBuffSize = videoBufferWidth*videoBufferHeight*4;
					isi420 = 2;
#endif

					LOGRENDER("Converting to tmp buffer due to decoder format %x with func=%p videoBits=%p tmpBuff=%p.",
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

#ifdef _FRAME_DUMP
	if (frameCount == _FRAME_DUMP)
	{
		LOGI("Dumping Frame");
		FrameHeader f = { videoBufferWidth, videoBufferHeight, stride, internalColf, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom, tmpBuffSize, isi420 };
		dumpFrame(f, tmpBuff);
	}
#endif


				if(cc444.isValid())
				{
					LOGRENDER("Doing 444 color conversion...");

					int a = vbCropLeft;
					int b = vbCropTop;
					int c = vbCropRight;
					int d = vbCropBottom;
					cc444.convert(tmpBuff, tmpBuffSize, videoBufferWidth,    videoBufferHeight,    a, b, c,   d,
						       pixels,  windowBuffer.stride, windowBuffer.height,  0, 0, c-a, d-b);
				}
				else if(cc.isValid())
				{
					LOGRENDER("Doing system color conversion...");

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
					LOGRENDER("Doing local conversion %dx%d %p %d %p %d",videoBufferWidth, videoBufferHeight,
						tmpBuff, 0,
						pixels, windowBuffer.stride * 2);

			//					lcc.convertYUV420Planar(videoBufferWidth, videoBufferHeight,
			//						tmpBuff, 0,
			//						pixels, windowBuffer.stride * 2);

					int a = vbCropLeft;
					int b = vbCropTop;
					int c = vbCropRight;
					int d = vbCropBottom;
					lcc.convert(videoBufferWidth, videoBufferHeight, tmpBuff, 0, pixels, windowBuffer.stride * 2);
				}


				if(gICFM->getDecoderOutputFormat() != OMX_COLOR_FormatYUV420Planar)
				{
					LOGRENDER("Freeing tmp buffer due to format %x.", gICFM->getDecoderOutputFormat());
					free(tmpBuff);
				}
			}
			else if(colf == QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka)
			{
				LOGRENDER("colf = QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka");
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
				LOGRENDER("colf = OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka");
#define ALIGN(x,multiple)    (((x)+(multiple-1))&~(multiple-1))
				int a = vbCropLeft;
				int b = vbCropTop;
				int c = vbCropRight;
				int d = vbCropBottom;

				// Bump size to multiple of 32.
				LOGRENDER("Rounding up to %dx%d", ALIGN(videoBufferWidth, 32), ALIGN(videoBufferHeight, 32));
				/*if(cc.isValid())
					cc.convert(videoBits, ALIGN(videoBufferWidth, 32), ALIGN(videoBufferHeight, 32), a, b, c, d,
					       pixels,  windowBuffer.stride, windowBuffer.height, a, b, c, d);
				else if(lcc.isValid()) */
					lcc.convertQCOMYUV420SemiPlanar(ALIGN(videoBufferWidth, 32), ALIGN(videoBufferHeight, 32), videoBits, 0, pixels, windowBuffer.stride * 2);
#undef ALIGN
			}
			else if(colf == OMX_COLOR_Format16bitRGB565)
			{
				LOGRENDER("colf = OMX_COLOR_Format16bitRGB565");
				// Directly copy 16 bit color.
				size_t bufSize = buffer->range_length() - buffer->range_offset();
				
				if(bufSize >= targetWidth * videoBufferWidth * sizeof(short))
				{
					LOGRENDER("A bufSize = %d", bufSize);
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
					LOGRENDER("B bufSize = %d targetWidth=%d videoBufferWidth=%d", bufSize, targetWidth, videoBufferWidth);
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
			else if (cc444.isValid())
			{
				LOGRENDER("Using 444 converter");
				// Use the system converter.
				cc444.convert(videoBits, buffer->range_length() - buffer->range_offset() , videoBufferWidth, videoBufferHeight, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom,
						pixels, windowBuffer.stride, windowBuffer.height, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom);

			}
			else if(cc.isValid())
			{
				LOGRENDER("Using system converter");
				// Use the system converter.
				cc.convert(videoBits, videoBufferWidth, videoBufferHeight, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom,
						pixels, windowBuffer.stride, windowBuffer.height, vbCropLeft, vbCropTop, vbCropRight, vbCropBottom);
			}
			else if(lcc.isValid())
			{
				LOGRENDER("Using own converter");
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

#define MIN_NEXT_SEGMENT_REQUEST_DELAY 500
uint32_t gLastRequestTime = 0;


void HLSPlayer::RequestNextSegment()
{

	LOGTRACE("%s", __func__);

	uint32_t curRequest = getTimeMS();
	if (curRequest - MIN_NEXT_SEGMENT_REQUEST_DELAY < gLastRequestTime)
		return;

	LOGI("Request Next Segment @ %d", curRequest);
	gLastRequestTime = curRequest;

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
		mAudioPlayer->Pause();
	}
	else if (!pause && GetState() == PAUSED)
	{
		SetState(PLAYING);
		mAudioPlayer->Play();
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
		mAudioPlayer->Stop();
	}
}

int32_t HLSPlayer::GetCurrentTimeMS()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	if (mAudioPlayer != NULL)
	{
		LOGTIMING("mSTartTimeMS=%d", mStartTimeMS);
		return (mAudioPlayer->GetTimeStamp() / 1000);
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
	if (mAudioPlayer) mAudioPlayer->Stop(true); // Passing true means we're seeking.

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

	mLastVideoTimeUs = -1;
	mVideoStartDelta = 0;
	mSegmentTimeOffset = 0;
	mVideoFrameDelta = 0;
	mFrameCount = 0;
}

bool HLSPlayer::EnsureAudioPlayerCreatedAndSourcesSet()
{
	LOGTRACE("%s", __func__);
	if (!mAudioPlayer)
	{
		return CreateAudioPlayer(); // CreateAudioPlayer sets the sources internally
	}
	else
	{
		if(mAudioSource.get())
			return mAudioPlayer->Set(mAudioSource);
		else if (mAudioSource23.get())
			return mAudioPlayer->Set23(mAudioSource23);
		else
		{
			mAudioPlayer->ClearAudioSource();
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
	if (mAudioPlayer)
	{
		mAudioPlayer->Start();
	}

	NotifyFormatChange(curQuality, newQuality, curAudioTrack, newAudioTrack);
}

void HLSPlayer::PostError(int error, bool fatal, const char* msg)
{
	LOGE("Posting error %d : %s", error, msg);
	JNIEnv* env = NULL;
	if (!EnsureJNI(&env)) return;

	jstring jmsg = env->NewStringUTF(msg);
	env->CallStaticVoidMethod(mPlayerViewClass, mPostErrorID, error, fatal, jmsg);
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

void HLSPlayer::RestartPlayer(const char* path, int32_t quality, int continuityEra, const char* altAudioPath, int audioIndex, double time, int cryptoId, int altAudioCryptoId)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Restarting");


	SetState(SEEKING);

	StopEverything();

	mDataSource.clear();
	mAlternateAudioDataSource.clear();
	mDataSourceCache.clear();
	LOGI("Data sources cleared");

	FeedSegment(path, quality, continuityEra, altAudioPath, audioIndex, time, cryptoId, altAudioCryptoId);

	if (!mDataSource.get())
	{
		SetState(CUE_STOP);
		LOGI("No data source - stopping");
		return;
	}
	mDataSource->logContinuityInfo();

	// Retrieve the new quality markers
	int newQuality = mDataSource->getQualityLevel();
	LOGI("newQuality=%d", newQuality);
	int newAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) newAudioTrack = mAlternateAudioDataSource->getQualityLevel();
	LOGI("newAudioTrack=%d", newAudioTrack);

	LOGI("Requesting Next Segment");
	RequestNextSegment();
	LOGI("RequestNextSegment completed");

	mStartTimeMS = (mDataSource->getStartTime() * 1000);
	LOGI("mStartTimeMS = %d", mStartTimeMS);

	int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
	LOGI("segCount=%d", segCount);
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
		LOGI("Video source started - Ensuring audio player is created and set up");
		if (!EnsureAudioPlayerCreatedAndSourcesSet())
		{
			LOGE("Setting Audio Tracks failed!");
		}
	}
	else
	{
		LOGI("Video Track failed to start: %s : %d", strerror(-err), __LINE__);
	}

	if (mAudioPlayer) mAudioPlayer->forceTimeStampUpdate();


	ApplyFormatChange();

	LOGI("Calling NoteHWRendererMode( %s, %d, %d, 4)", mUseOMXRenderer ? "True":"False", mWidth, mHeight );
	NoteHWRendererMode(mUseOMXRenderer, mWidth, mHeight, 4);
	SetState(PLAYING);
}

#define USE_DEFAULT_START -999

void HLSPlayer::Seek(double time)
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);
	if (GetState() == SEEKING)
	{
		LOGI("Already seeking. Ignoring additional seek.");
		return;
	}

	LOGI("Seeking To: %f", time);
	if (time < 0)
	{
		if ((int)time != USE_DEFAULT_START)
			time = 0;
	}

	SetState(SEEKING);

	StopEverything();

	// Retrieve the current quality markers
	int curQuality = mDataSource->getQualityLevel();
	LOGI("curQuality=%d", curQuality);
	int curAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) curAudioTrack = mAlternateAudioDataSource->getQualityLevel();
	LOGI("curAudioTrack=%d", curAudioTrack);

	mDataSource.clear();
	mAlternateAudioDataSource.clear();
	mDataSourceCache.clear();
	LOGI("Data sources cleared");

	// Need to request new segment because we killed all the data sources
	double segTime = RequestSegmentForTime(time);
	LOGI("segTime=%f", segTime);

	if (!mDataSource.get())
	{
		SetState(CUE_STOP);
		LOGI("No data source - stopping");
		return;
	}
	mDataSource->logContinuityInfo();

	// Retrieve the new quality markers
	int newQuality = mDataSource->getQualityLevel();
	LOGI("newQuality=%d", newQuality);
	int newAudioTrack = -1;
	if (mAlternateAudioDataSource.get()) newAudioTrack = mAlternateAudioDataSource->getQualityLevel();
	LOGI("newAudioTrack=%d", newAudioTrack);

	LOGI("Requesting Next Segment");
	RequestNextSegment();
	LOGI("RequestNextSegment completed");

	mStartTimeMS = (mDataSource->getStartTime() * 1000);
	LOGI("mStartTimeMS = %d", mStartTimeMS);

	if (time < 0)
	{
		LOGI("time < 0 - setting to segTime=%f", segTime);
		time = segTime;
	}
	LOGI("Seeking To: %f | Segment Start Time = %f", time, segTime);

	int segCount = ((HLSDataSource*) mDataSource.get())->getPreloadedSegmentCount();
	LOGI("segCount=%d", segCount);
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
		LOGI("Video source started - Ensuring audio player is created and set up");
		if (!EnsureAudioPlayerCreatedAndSourcesSet())
		{
			LOGE("Setting Audio Tracks failed!");
		}
	}
	else
	{
		LOGI("Video Track failed to start: %s : %d", strerror(-err), __LINE__);
	}

	if (mAudioPlayer) mAudioPlayer->forceTimeStampUpdate();

	bool doFormatChange = false;

	LOGI("Reading until time %f", time);
	doFormatChange = !ReadUntilTime(time);
	LOGI("doFormatChange = %s", doFormatChange ? "True":"False");

	if (!doFormatChange && mAudioPlayer)
	{
		LOGI("Reading Audio Track until time %f", time);
		doFormatChange = (!mAudioPlayer->ReadUntilTime(time));
		LOGI("doFormatChange = %s", doFormatChange ? "True":"False");
	}


	if (doFormatChange)
	{
		LOGI("Applying Format Change");
		ApplyFormatChange();
	}
	else
	{
		LOGI("Segment Count %d", segCount);
		if (mAudioPlayer)
		{
			LOGI("Starting audio track");
			// Call Start instead of Play, in order to ensure that the internal time values are correctly starting from zero.
			mAudioPlayer->Start();
		}
		LOGI("NotifyingFormatChange( %d, %d, %d, %d )", curQuality, newQuality, curAudioTrack, newAudioTrack);
		NotifyFormatChange(curQuality, newQuality, curAudioTrack, newAudioTrack);
	}

	LOGI("Calling NoteHWRendererMode( %s, %d, %d, 4)", mUseOMXRenderer ? "True":"False", mWidth, mHeight );
	NoteHWRendererMode(mUseOMXRenderer, mWidth, mHeight, 4);
	SetState(PLAYING);

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




void HLSPlayer::UpdateDroppedFrameInfo()
{
	// If we don't have a last second, reset everything to 0
	if (mDroppedFrameLastSecond == 0)
	{
		mDroppedFrameIndex = 0;
		memset(mDroppedFrameCounts, 0, sizeof(int) * MAX_DROPPED_FRAME_SECONDS);
		mDroppedFrameLastSecond = getTimeMS();
	}

	LOGV2("GetTimeMS: %d" , getTimeMS());

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

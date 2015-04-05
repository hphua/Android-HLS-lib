/*
 * HLSPlayer.h
 *
 *  Created on: May 5, 2014
 *      Author: Mark
 */

#ifndef HLSPLAYER_H_
#define HLSPLAYER_H_


#include <jni.h>

#include "androidVideoShim.h"

#include <android/native_window.h>
#include <android/window.h>
#include <unistd.h>

#include "AudioPlayer.h"

#include <pthread.h>
#include <list>

#define MAX_DROPPED_FRAME_SECONDS 5

namespace android
{
	class MPEG2TSExtractor;
}

class HLSSegment;

class HLSPlayer
{
public:
	HLSPlayer(JavaVM* jvm);
	~HLSPlayer();

	void Close(JNIEnv* env);
	void Reset();

	void SetSurface(JNIEnv* env, jobject surface);
	android_video_shim::status_t FeedSegment(const char* path, int32_t quality, int continuityEra, const char* altAudioPath, int audioIndex, double time, int cryptoId, int altCryptoId );
	void SetSegmentCountToBuffer(int segmentCount);
	int GetSegmentCountToBuffer();

	bool Play(double time);
	void Stop();
	int Update();

	int DroppedFramesPerSecond();

	void Seek(double time);

	void SetJavaVM(JavaVM* jvm);

	void Pause(bool pause);

	int GetState();
	int32_t GetCurrentTimeMS();

	void SetScreenSize(int w, int h);

	void ApplyFormatChange();
	void SetState(int status);

	bool ReadUntilTime(double timeSecs);

	void PostError(int error, bool fatal, const char* msg);

	int64_t GetLastTimeUS();

	int GetBufferedSegmentCount();

private:
	bool EnsureJNI(JNIEnv** env);
	void SetNativeWindow(ANativeWindow* window);
	bool InitAudio();
	bool InitSources();
	bool CreateAudioPlayer();
	bool EnsureAudioPlayerCreatedAndSourcesSet();
	bool CreateVideoPlayer();
	bool RenderBuffer(android_video_shim::MediaBuffer* buffer);
	void LogState();
	void RestartPlayer(const char* path, int32_t quality, int continuityEra, const char* altAudioPath, int audioIndex, double time, int cryptoId, int altAudioCryptoId);

	bool InitTracks();

	void RequestNextSegment();

	double RequestSegmentForTime(double time);
	void NoteVideoDimensions();
	void NoteHWRendererMode(bool enabled, int w, int h, int colf);
	void NotifyFormatChange(int curQuality, int newQuality, int curAudio, int newAudio);
	void ClearScreen();



	/// seeking methods
	void StopEverything();
	///

	struct DataSourceCacheObject
	{
		android_video_shim::sp<android_video_shim::HLSDataSource> dataSource;
		android_video_shim::sp<android_video_shim::HLSDataSource> altAudioDataSource;
		bool isSameEra(int32_t quality, int continuityEra, int audioIndex);
	};

	typedef std::list<DataSourceCacheObject> DATASRC_CACHE;
	DATASRC_CACHE mDataSourceCache;

	pthread_t audioThread;

	int mRenderedFrameCount;
	ANativeWindow* mWindow;

	JavaVM* mJvm;
	jmethodID mNextSegmentMethodID;
	jmethodID mSegmentForTimeMethodID;
	jmethodID mSetVideoResolutionID;
	jmethodID mEnableHWRendererModeID;
	jmethodID mNotifyFormatChangeComplete;
	jmethodID mNotifyAudioTrackChangeComplete;
	jmethodID mPostErrorID;
	jclass mPlayerViewClass;

	jobject mSurface;

	int mStatus;

	android_video_shim::OMXClient mClient;

	// These are our video and audo tracks that we shove data into
	android_video_shim::sp<android_video_shim::MediaSource> mVideoTrack;
	android_video_shim::sp<android_video_shim::MediaSource> mAudioTrack;
	android_video_shim::sp<android_video_shim::MediaSource23> mVideoTrack23;
	android_video_shim::sp<android_video_shim::MediaSource23> mAudioTrack23;

	android_video_shim::sp<android_video_shim::MetaData> mVideoTrack_md;
	android_video_shim::sp<android_video_shim::MetaData> mAudioTrack_md;

	// These are the codec converted sources that we get actual frames and audio from
	android_video_shim::sp<android_video_shim::MediaSource> mVideoSource;
	android_video_shim::sp<android_video_shim::MediaSource> mAudioSource;
	android_video_shim::sp<android_video_shim::MediaSource23> mVideoSource23;
	android_video_shim::sp<android_video_shim::MediaSource23> mAudioSource23;

	// Our datasource that handles loading segments.
	android_video_shim::sp<android_video_shim::HLSDataSource> mDataSource;
	android_video_shim::sp<android_video_shim::HLSDataSource> mAlternateAudioDataSource;

	// The object that pulls the initial data stream apart into separate audio and video sources
	android_video_shim::sp<android::MPEG2TSExtractor> mExtractor;
	android_video_shim::sp<android::MPEG2TSExtractor> mAlternateAudioExtractor;

	// Read Options
	android_video_shim::MediaSource::ReadOptions mOptions;
	android_video_shim::MediaSource23::ReadOptions mOptions23;

	AudioPlayer *mAudioPlayer;

	bool mUseOMXRenderer;
	bool mOffloadAudio;
	int64_t mDurationUs;

	android_video_shim::MediaBuffer* mVideoBuffer;

	android_video_shim::sp<android_video_shim::IOMXRenderer> mOMXRenderer;

	int64_t mBitrate;
	int32_t mWidth;
	int32_t mHeight;
	int32_t mCropWidth;
	int32_t mCropHeight;
	int32_t mActiveAudioTrackIndex;
	uint32_t mExtractorFlags;
	int mPadWidth;

	int64_t mLastVideoTimeUs;
	int64_t mSegmentTimeOffset;
	int64_t mVideoFrameDelta;
	int64_t mVideoStartDelta; 		// The starting time offset of the video (used in comparing audio time to video time)
	int64_t mFrameCount;

	int32_t mScreenWidth;
	int32_t mScreenHeight;

	int32_t mStartTimeMS;

	pthread_mutex_t lock;

	// DroppedFrameCounter
	int mDroppedFrameCounts[MAX_DROPPED_FRAME_SECONDS]; // each int holds the count for a single second
	int mDroppedFrameIndex;
	int32_t mDroppedFrameLastSecond;
	void DroppedAFrame();
	void UpdateDroppedFrameInfo();
};

//----------------------------
// template method to clear OMX
//
// Requires an sp<MediaSource> or sp<MediaSource23> object
//
template<typename T>
void clearOMX(T& t)
{
	if (t.get())
	{
		LOGI("Stopping && Clearing OMX %p", t.get());
		t->stop();

		android_video_shim::wp<android_video_shim::RefBase> tmp = NULL;

		tmp = t;

		t.clear();

		while (tmp.promote() != NULL)
		{
			usleep(1000);
		}
	}
}



#endif /* HLSPLAYER_H_ */

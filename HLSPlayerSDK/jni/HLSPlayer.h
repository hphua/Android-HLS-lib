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

#include <list>

class HLSSegment;

class HLSPlayer
{
public:
	enum
	{
		STOPPED,
		PAUSED,
		PLAYING,
		SEEKING
	};

	HLSPlayer(JavaVM* jvm);
	~HLSPlayer();

	void Close(JNIEnv* env);
	void Reset();

	void SetSurface(JNIEnv* env, jobject surface);
	android_video_shim::status_t FeedSegment(const char* path, int32_t quality, double time );

	bool Play();
	void Stop();
	int Update();

	void Seek(double time);

	void SetJavaVM(JavaVM* jvm);

	bool UpdateWindowBufferFormat();

	void TogglePause();

	int GetState();

private:
	void SetStatus(int status);
	void SetNativeWindow(ANativeWindow* window);
	bool InitAudio();
	bool InitSources();
	bool CreateAudioPlayer();
	bool CreateVideoPlayer();
	bool RenderBuffer(android_video_shim::MediaBuffer* buffer);
	void LogStatus();

	bool InitTracks();

	void RequestNextSegment();
	void RequestSegmentForTime(double time);

	std::list<HLSSegment* > mSegments;


	int mRenderedFrameCount;
	ANativeWindow* mWindow;

	JavaVM* mJvm;
	jmethodID mNextSegmentMethodID;
	jmethodID mSegmentForTimeMethodID;
	jclass mPlayerViewClass;

	jobject mSurface;

	int mStatus;

	android_video_shim::OMXClient mClient;

	// These are our video and audo tracks that we shove data into
	android_video_shim::sp<android_video_shim::MediaSource> mVideoTrack;
	android_video_shim::sp<android_video_shim::MediaSource> mAudioTrack;


	// These are the codec converted sources that we get actual frames and audio from
	android_video_shim::sp<android_video_shim::MediaSource> mVideoSource;
	android_video_shim::sp<android_video_shim::MediaSource> mAudioSource;

	android_video_shim::sp<android_video_shim::HLSDataSource> mDataSource;

	android_video_shim::sp<android_video_shim::MediaExtractor> mExtractor;    // The object that pulls the initial data source apart into separate audio and video sources

	android_video_shim::AudioPlayer* mAudioPlayer;
	android_video_shim::sp<android_video_shim::MediaPlayerBase::AudioSink> mAudioSink;

	android_video_shim::TimeSource* mTimeSource;
	bool mOffloadAudio;
	int64_t mDurationUs;

	android_video_shim::MediaBuffer* mVideoBuffer;

	int64_t mBitrate;
	int32_t mWidth;
	int32_t mHeight;
	int32_t mCropWidth;
	int32_t mCropHeight;
	int32_t mActiveAudioTrackIndex;
	uint32_t mExtractorFlags;

	int64_t mLastVideoTimeUs;
	int64_t mSegmentTimeOffset;
	int64_t mVideoFrameDelta;
	int64_t mFrameCount;


};



#endif /* HLSPLAYER_H_ */

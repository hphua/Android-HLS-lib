/*
 * HLSPlayer.h
 *
 *  Created on: May 5, 2014
 *      Author: Mark
 */

#ifndef HLSPLAYER_H_
#define HLSPLAYER_H_


#include <jni.h>

#include <../android-source/frameworks/av/include/media/stagefright/OMXClient.h>
#include <../android-source/frameworks/av/include/media/stagefright/MediaBuffer.h>
#include <../android-source/frameworks/av/include/media/stagefright/MediaSource.h>
#include <../android-source/frameworks/av/include/media/stagefright/MediaExtractor.h>
#include <../android-source/frameworks/av/include/media/stagefright/MetaData.h>
#include <../android-source/frameworks/av/include/media/stagefright/DataSource.h>
#include <../android-source/frameworks/av/include/media/stagefright/FileSource.h>
#include <../android-source/frameworks/av/include/media/stagefright/OMXCodec.h>
#include <../android-source/frameworks/av/include/media/stagefright/TimeSource.h>

#include <../android-source/frameworks/av/include/media/stagefright/AudioPlayer.h>
#include <../android-source/frameworks/av/include/media/MediaPlayerInterface.h>



#include <android/native_window.h>
#include <android/window.h>

#include <list>

class HLSSegment;
class HLSDataSource;

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

	void SetSurface(JNIEnv* env, jobject surface);
	android::status_t FeedSegment(const char* path, int32_t quality, double time );

	bool Play();
	int Update();

	void Seek(double time);

	void SetJavaVM(JavaVM* jvm);

	bool UpdateWindowBufferFormat();

private:
	void SetStatus(int status);
	void SetNativeWindow(ANativeWindow* window);
	bool InitAudio();
	bool InitSources();
	bool CreateAudioPlayer();
	bool CreateVideoPlayer();
	bool RenderBuffer(android::MediaBuffer* buffer);
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

	android::OMXClient mClient;

	// These are our video and audo tracks that we shove data into
	android::sp<android::MediaSource> mVideoTrack;
	android::sp<android::MediaSource> mAudioTrack;


	// These are the codec converted sources that we get actual frames and audio from
	android::sp<android::MediaSource> mVideoSource;
	android::sp<android::MediaSource> mAudioSource;

	android::sp<HLSDataSource> mDataSource;

	android::sp<android::MediaExtractor> mExtractor;    // The object that pulls the initial data source apart into separate audio and video sources

	android::AudioPlayer* mAudioPlayer;
	android::sp<android::MediaPlayerBase::AudioSink> mAudioSink;

	android::TimeSource* mTimeSource;
	bool mOffloadAudio;
	int64_t mDurationUs;

	android::MediaBuffer* mVideoBuffer;

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

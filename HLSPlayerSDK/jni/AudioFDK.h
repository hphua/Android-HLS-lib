/*
 * AudioFDK.h
 *
 *  Created on: Mar 24, 2015
 *      Author: Mark
 */

#ifndef AUDIOFDK_H_
#define AUDIOFDK_H_

#include <jni.h>
#include <androidVideoShim.h>
#include <semaphore.h>
#include <RefCounted.h>
#include <AudioPlayer.h>
#include <aacdecoder_lib.h>

class AudioFDK: public AudioPlayer {
public:
	AudioFDK(JavaVM* jvm);
	virtual ~AudioFDK();

	bool Init();
	void Close();

	virtual void unload(); // from RefCounted

	bool Start();
	void Play();
	void Pause();
	void Flush();
	bool Stop(bool seeking = false);

	bool Set(android_video_shim::sp<android_video_shim::MediaSource> audioSource, bool alreadyStarted = false);
	bool Set23(android_video_shim::sp<android_video_shim::MediaSource23> audioSource, bool alreadyStarted = false);
	void ClearAudioSource();

	int Update();

	int64_t GetTimeStamp();

	void forceTimeStampUpdate();

	int getBufferSize();

	bool UpdateFormatInfo();

	bool ReadUntilTime(double timeSecs);
private:
	void SetTimeStampOffset(double offsetSecs);

	bool InitJavaTrack();

	HANDLE_AACDECODER mAACDecoder;

	uint32_t mESDSType;
	const void* mESDSData;
	size_t mESDSSize;

	jclass mCAudioTrack;
	jmethodID mAudioTrack;
	jmethodID mGetMinBufferSize;
	jmethodID mPlay;
	jmethodID mPause;
	jmethodID mStop;
	jmethodID mRelease;
	jmethodID mGetTimestamp;
	jmethodID mWrite;
	jmethodID mFlush;
	jmethodID mSetPositionNotificationPeriod;
	jmethodID mGetPlaybackHeadPosition;

	jobject mTrack;
	jarray buffer;

	JavaVM* mJvm;

	android_video_shim::sp<android_video_shim::MediaSource> mAudioSource;
	android_video_shim::sp<android_video_shim::MediaSource23> mAudioSource23;

	int mSampleRate;
	int mNumChannels;
	int mChannelMask;
	int mBufferSizeInBytes;

	int mPlayState;
	bool mWaiting;
	bool mPlayingSilence;

	double mTimeStampOffset;
	bool mNeedsTimeStampOffset;

	long long samplesWritten;

	sem_t semPause;
	pthread_mutex_t updateMutex;
	pthread_mutex_t lock;

};

#endif /* AUDIOFDK_H_ */

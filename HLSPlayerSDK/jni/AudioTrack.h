/*
 * AudioTrack.h
 *
 *  Created on: Jul 17, 2014
 *      Author: Mark
 */

#ifndef AUDIOTRACK_H_
#define AUDIOTRACK_H_

#include <jni.h>
#include <androidVideoShim.h>
#include <semaphore.h>

class AudioTrack {
public:
	AudioTrack(JavaVM* jvm);
	~AudioTrack();

	bool Init();
	void Close();

	bool Start();
	void Play();
	void Pause();
	bool Stop();

	bool Set(android_video_shim::sp<android_video_shim::MediaSource> audioSource, bool alreadyStarted = false);
	bool Set23(android_video_shim::sp<android_video_shim::MediaSource23> audioSource, bool alreadyStarted = false);

	bool Update();

	int64_t GetTimeStamp();

private:
	jclass mCAudioTrack;
	jmethodID mAudioTrack;
	jmethodID mGetMinBufferSize;
	jmethodID mPlay;
	jmethodID mPause;
	jmethodID mStop;
	jmethodID mRelease;
	jmethodID mGetTimestamp;
	jmethodID mWrite;
	jmethodID mGetPlaybackHeadPosition;

	jobject mTrack;

	JavaVM* mJvm;

	android_video_shim::sp<android_video_shim::MediaSource> mAudioSource;
	android_video_shim::sp<android_video_shim::MediaSource23> mAudioSource23;

	int mSampleRate;
	int mNumChannels;
	int mChannelMask;
	int mBufferSizeInBytes;

	int mPlayState;

	sem_t semPause;

};

#endif /* AUDIOTRACK_H_ */

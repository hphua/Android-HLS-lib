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

class AudioTrack {
public:
	AudioTrack(JavaVM* jvm);
	~AudioTrack();

	bool Init();

	bool Start();
	void Play();
	void Pause();
	bool Stop();

	bool Set(android_video_shim::sp<android_video_shim::MediaSource> audioSource, bool alreadyStarted = false);

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

	int mSampleRate;
	int mNumChannels;
	int mChannelMask;
	int mBufferSizeInBytes;

	int mPlayState;

};

#endif /* AUDIOTRACK_H_ */

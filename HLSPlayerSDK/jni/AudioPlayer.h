/*
 * AudioPlayer.h
 *
 *  Created on: Jul 17, 2014
 *      Author: Mark
 */

#ifndef AUDIOPLAYER_H_
#define AUDIOPLAYER_H_

#include <jni.h>
#include <androidVideoShim.h>
#include <semaphore.h>
#include <RefCounted.h>

enum
{
	AUDIOTHREAD_WAIT,
	AUDIOTHREAD_CONTINUE,
	AUDIOTHREAD_FINISH
};

class AudioPlayer : public RefCounted {
public:
	virtual ~AudioPlayer() {};

	virtual bool Init() = 0;
	virtual void Close() = 0;

	virtual void unload() = 0; // from RefCounted

	virtual bool Start() = 0;
	virtual void Play() = 0;
	virtual void Pause() = 0;
	virtual void Flush() = 0;
	virtual bool Stop(bool seeking = false) = 0;

	virtual bool Set(android_video_shim::sp<android_video_shim::MediaSource> audioSource, bool alreadyStarted = false) = 0;
	virtual bool Set23(android_video_shim::sp<android_video_shim::MediaSource23> audioSource, bool alreadyStarted = false) = 0;
	virtual void ClearAudioSource() = 0;

	virtual int Update() = 0;

	virtual void shutdown() = 0;

	virtual int64_t GetTimeStamp() = 0;

	virtual void forceTimeStampUpdate() = 0;

	virtual int getBufferSize() = 0;

	virtual bool UpdateFormatInfo() = 0;

	virtual bool ReadUntilTime(double timeSecs) = 0;
};

AudioPlayer* MakeAudioPlayer(JavaVM* jvm, bool useOMX = false);

#endif /* AUDIOPLAYER_H_ */

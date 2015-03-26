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

/*
 * AudioPlayer
 *
 * An interface class for defining how the app works with the audio implementations
 * the HLSPlayer supports.
 *
 *	1) Call MakeAudioPlayer to create the type of player you want.
 *
 *	2) Call Init()
 *
 *	3) Set the sources
 *
 *	4) Call Start() to get ready to play
 *
 *	5) Call Play to begin playback
 *
 *	The current implementations of this interface expect to have a thread started that will call update repeatedly and
 *	watches for the various return values from Update that indicate it's state. The thread should call release once
 *	update returns AUDIOTHREAD_FINISH
 */

class AudioPlayer : public RefCounted {
public:
	virtual ~AudioPlayer() {};

	virtual bool Init() = 0;
	virtual void Close() = 0; // Stops the player and releases any memory and references to external objects

	virtual bool Start() = 0; // Should prepare an audio track for playing
	virtual void Play() = 0; // begins playback
	virtual void Pause() = 0; // pauses playback
	virtual void Flush() = 0; // Flushes any buffers
	virtual bool Stop(bool seeking = false) = 0; // Stops playback completely.

	virtual bool Set(android_video_shim::sp<android_video_shim::MediaSource> audioSource, bool alreadyStarted = false) = 0; // Set a DataSource from 4.0 android and later
	virtual bool Set23(android_video_shim::sp<android_video_shim::MediaSource23> audioSource, bool alreadyStarted = false) = 0; // Set a DataSource from 2.3 android
	virtual void ClearAudioSource() = 0; // Removes any references to the audio source

	virtual int Update() = 0; // Where the magic happens. This is called regularly, once playback is initiated, to handle reading from the data source and writing to our audio output

	virtual int64_t GetTimeStamp() = 0; // Returns our current timestamp in US format

	virtual void forceTimeStampUpdate() = 0; // Forces the timestamp offset to be updated the next frame

	virtual int getBufferSize() = 0;

	virtual bool UpdateFormatInfo() = 0; // Updates the audio format information ( sample rate, channels, etc...)

	virtual bool ReadUntilTime(double timeSecs) = 0; // Reads through the audio stream until the timestamp matches - sort of a fast forward

	virtual void unload() = 0; // from RefCounted - calls close internally, then deletes our object - do not call directly. Call AudioPlayer->release() instead.

};

/*
 * MakeAudioPlayer
 *
 * Creates an AudioTrack if useOMX is set to true, otherwise, creates an AudioFDK
 * AudioTrack is retained for testing/verification purposes.
 *
 */
AudioPlayer* MakeAudioPlayer(JavaVM* jvm, bool useOMX = false);

#endif /* AUDIOPLAYER_H_ */

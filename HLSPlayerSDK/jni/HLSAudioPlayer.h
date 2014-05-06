/*
 * HLSAudioPlayer.h
 *
 *  Created on: Apr 25, 2014
 *      Author: Mark
 */

#ifndef HLSAUDIOPLAYER_H_
#define HLSAUDIOPLAYER_H_

#include <../android-source/frameworks/av/include/media/stagefright/AudioPlayer.h>
#include <../android-source/frameworks/av/include/media/MediaPlayerInterface.h>

class HLSAudioPlayer
{
public:
	HLSAudioPlayer();
	~HLSAudioPlayer();

	android::sp<android::MediaPlayerBase::AudioSink> GetAudioSink();
	void SetAudioSink(android::sp<android::MediaPlayerBase::AudioSink> audioSink);

	android::AudioPlayer* GetAndroidAudioPlayer();
	void SetAndroidAudioPlayer(android::AudioPlayer* player);

private:
	android::sp<android::MediaPlayerBase::AudioSink> mAudioSink;
	android::AudioPlayer* mPlayer;

};



#endif /* HLSAUDIOPLAYER_H_ */

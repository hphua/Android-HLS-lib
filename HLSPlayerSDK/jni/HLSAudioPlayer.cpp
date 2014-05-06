/*
 * HLSAudioPlayer.cpp
 *
 *  Created on: Apr 25, 2014
 *      Author: Mark
 */

#include "HLSAudioPlayer.h"

HLSAudioPlayer::HLSAudioPlayer() : mPlayer(NULL), mAudioSink(NULL)
{

}

HLSAudioPlayer::~HLSAudioPlayer()
{

}

android::sp<android::MediaPlayerBase::AudioSink> HLSAudioPlayer::GetAudioSink()
{
	return mAudioSink;
}

void HLSAudioPlayer::SetAudioSink(android::sp<android::MediaPlayerBase::AudioSink> audioSink)
{
	mAudioSink = audioSink;
}

android::AudioPlayer* HLSAudioPlayer::GetAndroidAudioPlayer()
{
	return mPlayer;
}
void HLSAudioPlayer::SetAndroidAudioPlayer(android::AudioPlayer* player)
{
	mPlayer = player;
}

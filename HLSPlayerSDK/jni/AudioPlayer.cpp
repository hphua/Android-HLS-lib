/*
 * AudioPlayer.cpp
 *
 *  Created on: Mar 23, 2015
 *      Author: Mark
 */
#include "AudioPlayer.h"

#include "AudioTrack.h"
#include "AudioFDK.h"


AudioPlayer* MakeAudioPlayer(JavaVM* jvm, bool useOMX )
{
	if (!jvm) return NULL;
	if (useOMX)
	{
		return new AudioTrack(jvm);
	}
	else
	{
		return new AudioFDK(jvm);
	}
}

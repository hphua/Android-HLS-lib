/*
 * AudioTrack.cpp
 *
 *  Created on: Jul 17, 2014
 *      Author: Mark
 */

#include <jni.h>
#include "constants.h"
#include <AudioTrack.h>


using namespace android_video_shim;

AudioTrack::AudioTrack(JavaVM* jvm) : mJvm(jvm), mAudioTrack(NULL), mGetMinBufferSize(NULL), mPlay(NULL), mPause(NULL), mStop(NULL),
										mRelease(NULL), mGetTimestamp(NULL), mCAudioTrack(NULL), mWrite(NULL), mGetPlaybackHeadPosition(NULL),
										mSampleRate(0), mNumChannels(0), mBufferSizeInBytes(0), mChannelMask(0), mTrack(NULL), mPlayState(STOPPED)
{
	// TODO Auto-generated constructor stub
	if (!mJvm)
	{
		LOGE("Java VM is NULL");
	}
}

AudioTrack::~AudioTrack() {
	// TODO Auto-generated destructor stub
}

void AudioTrack::Close()
{
	Stop();
	if (mJvm)
	{
		JNIEnv* env = NULL;
		mJvm->AttachCurrentThread(&env, NULL);
		env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);
		env->DeleteGlobalRef(mTrack);
		mTrack = NULL;
		env->DeleteGlobalRef(mCAudioTrack);
		mCAudioTrack = NULL;

		if(mAudioSource.get())
			mAudioSource->stop();
		if(mAudioSource23.get())
			mAudioSource23->stop();
	}
}

bool AudioTrack::Init()
{
	if (!mJvm)
	{
		LOGE("Java VM is NULL - aborting init");
		return false;
	}
	JNIEnv* env = NULL;
	mJvm->GetEnv((void**)&env, JNI_VERSION_1_2);

    if (!mCAudioTrack)
    {
        /* Cache AudioTrack class and it's method id's
         * And do this only once!
         */

    	LOGE("Caching AudioTrack class and method ids");

        mCAudioTrack = env->FindClass("android/media/AudioTrack");
        if (!mCAudioTrack)
        {
            LOGE("android.media.AudioTrack class is not found. Are you running at least 1.5 version?");
            return false;
        }

        mCAudioTrack = (jclass)env->NewGlobalRef(mCAudioTrack);

        mAudioTrack = env->GetMethodID(mCAudioTrack, "<init>", "(IIIIII)V");
        mGetMinBufferSize = env->GetStaticMethodID(mCAudioTrack, "getMinBufferSize", "(III)I");
        mPlay = env->GetMethodID(mCAudioTrack, "play", "()V");
        mStop = env->GetMethodID(mCAudioTrack, "stop", "()V");
        mPause = env->GetMethodID(mCAudioTrack, "pause", "()V");
        mRelease = env->GetMethodID(mCAudioTrack, "release", "()V");
        mWrite = env->GetMethodID(mCAudioTrack, "write", "([BII)I");
        mGetPlaybackHeadPosition = env->GetMethodID(mCAudioTrack, "getPlaybackHeadPosition", "()I");
    }
    return true;
}


bool AudioTrack::Set(sp<MediaSource> audioSource, bool alreadyStarted)
{
	LOGI("Set with %p", audioSource.get());
	mAudioSource = audioSource;
	if (!alreadyStarted) mAudioSource->start(NULL);

	sp<MetaData> format = mAudioSource->getFormat();
	RUNDEBUG(format->dumpToLog());
	const char* mime;
	bool success = format->findCString(kKeyMIMEType, &mime);
	if (!success)
	{
		LOGE("Could not find mime type");
		return false;
	}
	if (strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_RAW))
	{
		LOGE("Mime Type was not audio/raw. Was: %s", mime);
		return false;
	}
	success = format->findInt32(kKeySampleRate, &mSampleRate);
	if (!success)
	{
		LOGE("Could not find audio sample rate");
		return false;
	}

	success = format->findInt32(kKeyChannelCount, &mNumChannels);
	if (!success)
	{
		LOGE("Could not find channel count");
		return false;
	}
	if (!format->findInt32(kKeyChannelMask, &mChannelMask))
	{
		if (mNumChannels > 2)
		{
			LOGI("Source format didn't specify channel mask. Using (%d) channel order", mNumChannels);
		}
		mChannelMask = 0; // CHANNEL_MASK_USE_CHANNEL_ORDER
	}
}


bool AudioTrack::Set23(sp<MediaSource23> audioSource, bool alreadyStarted)
{
	LOGI("Set23 with %p", audioSource.get());
	mAudioSource23 = audioSource;
	if (!alreadyStarted) mAudioSource23->start(NULL);

	sp<MetaData> format = mAudioSource23->getFormat();
	RUNDEBUG(format->dumpToLog());
	const char* mime;
	bool success = format->findCString(kKeyMIMEType, &mime);
	if (!success)
	{
		LOGE("Could not find mime type");
		return false;
	}
	if (strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_RAW))
	{
		LOGE("Mime Type was not audio/raw. Was: %s", mime);
		return false;
	}
	success = format->findInt32(kKeySampleRate, &mSampleRate);
	if (!success)
	{
		LOGE("Could not find audio sample rate");
		return false;
	}

	success = format->findInt32(kKeyChannelCount, &mNumChannels);
	if (!success)
	{
		LOGE("Could not find channel count");
		return false;
	}
	if (!format->findInt32(kKeyChannelMask, &mChannelMask))
	{
		if (mNumChannels > 2)
		{
			LOGI("Source format didn't specify channel mask. Using (%d) channel order", mNumChannels);
		}
		mChannelMask = 0; // CHANNEL_MASK_USE_CHANNEL_ORDER
	}
}

#define STREAM_MUSIC 3
#define CHANNEL_CONFIGURATION_MONO 4
#define CHANNEL_CONFIGURATION_STEREO 12
#define CHANNEL_CONFIGURATION_5_1 1052
#define ENCODING_PCM_8BIT 3
#define ENCODING_PCM_16BIT 2
#define MODE_STREAM 1

bool AudioTrack::Start()
{

//	audio_format_t audioFormat = AUDIO_FORMAT_PCM_16_BIT;
//
//	int avgBitRate = -1;
//	format->findInt32(kKeyBitRate, &avgBitRate);

	JNIEnv* env;
	mJvm->AttachCurrentThread(&env, NULL);

	int channelConfig = CHANNEL_CONFIGURATION_STEREO;
	switch (mNumChannels)
	{
	case 1:
//		channelConfig = CHANNEL_CONFIGURATION_MONO;
//		break;
	case 2:
		channelConfig = CHANNEL_CONFIGURATION_STEREO;
		break;
	case 6:
		channelConfig = CHANNEL_CONFIGURATION_5_1;
		break;
	default:
		LOGI("Failed to identify channelConfig, defaulting to stereo.");
		break;
	}

	LOGI("mNumChannels=%d | channelConfig=%d | mSampleRate=%d", mNumChannels, channelConfig, mSampleRate);

	mBufferSizeInBytes = env->CallStaticIntMethod(mCAudioTrack, mGetMinBufferSize, mSampleRate, channelConfig,ENCODING_PCM_16BIT) * 4; // HACK ALERT!! Note that this value was originally 2... this is a quick hack to test the audio sending since the media buffer I am seeing is exactly the same size as this value * 4

	LOGI("mBufferSizeInBytes=%d", mBufferSizeInBytes);


	mTrack = env->NewGlobalRef(env->NewObject(mCAudioTrack, mAudioTrack, STREAM_MUSIC, mSampleRate, channelConfig, ENCODING_PCM_16BIT, mBufferSizeInBytes * 2, MODE_STREAM ));
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPlay);
	mPlayState = PLAYING;
	return true;

}

void AudioTrack::Play()
{
	if (mPlayState == PLAYING) return;
	mPlayState = PLAYING;
	JNIEnv* env;
	mJvm->AttachCurrentThread(&env, NULL);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPlay);

}

bool AudioTrack::Stop()
{
	if (mPlayState == STOPPED) return true;
	mPlayState = STOPPED;
	JNIEnv* env;
	mJvm->AttachCurrentThread(&env, NULL);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);
	return true;
}

void AudioTrack::Pause()
{
	if (mPlayState == PAUSED) return;
	mPlayState = PAUSED;
	JNIEnv* env;
	mJvm->AttachCurrentThread(&env, NULL);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPause);
}

int64_t AudioTrack::GetTimeStamp()
{
	JNIEnv* env;
	mJvm->AttachCurrentThread(&env, NULL);
	double frames = env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mGetPlaybackHeadPosition);
	double secs = frames / (double)mSampleRate;
	return (secs * 1000000);
}


bool AudioTrack::Update()
{
	if (mPlayState != PLAYING) return false; // We don't really want to add more stuff to the buffer
											// and potentially run past the end of buffered source data
											// if we're not actively playing
	JNIEnv* env;
	mJvm->AttachCurrentThread(&env, NULL);

	MediaBuffer* mediaBuffer = NULL;

	//LOGI("Reading to the media buffer");
	status_t res;
	
	if(mAudioSource.get())
		res = mAudioSource->read(&mediaBuffer, NULL);
	
	if(mAudioSource23.get())
		res = mAudioSource23->read(&mediaBuffer, NULL);

	//LOGI("Finished reading from the media buffer");
	if (res == OK)
	{
		RUNDEBUG(mediaBuffer->meta_data()->dumpToLog());
		env->PushLocalFrame(2);

		jarray buffer = env->NewByteArray(mBufferSizeInBytes);

		void* pBuffer = env->GetPrimitiveArrayCritical(buffer, NULL);

		if (pBuffer)
		{
			size_t mbufSize = mediaBuffer->range_length();
			//LOGI("MediaBufferSize = %d, mBufferSizeInBytes = %d", mbufSize, mBufferSizeInBytes );
			if (mbufSize <= mBufferSizeInBytes)
			{
				//LOGI("Writing data to jAudioTrack %d", mbufSize);
				memcpy(pBuffer, mediaBuffer->data(), mbufSize);
				unsigned short* pBShorts = (unsigned short*)pBuffer;
				LOGV("%hd %hd %hd %hd", pBShorts[0], pBShorts[1], pBShorts[2], pBShorts[3]);
				int len = mbufSize / 2;
				LOGV("%hd %hd %hd %hd", pBShorts[len - 4], pBShorts[len - 3], pBShorts[len - 2], pBShorts[len - 1]);

				env->ReleasePrimitiveArrayCritical(buffer, pBuffer, 0);
				//LOGI("Finished copying audio data to buffer");
				env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mWrite, buffer, 0, mbufSize  );
				//LOGI("Finished Writing Data to jAudioTrack");
			}
			else
			{
				LOGI("MediaBufferSize > mBufferSizeInBytes");
			}
		}

		env->PopLocalFrame(NULL);

	}
	else if (res == INFO_FORMAT_CHANGED)
	{
		Update();
	}
	else if (res == ERROR_END_OF_STREAM)
	{
		LOGE("End of Audio Stream");
		mJvm->DetachCurrentThread();
		return false;
	}

	mJvm->DetachCurrentThread();

	if (mediaBuffer != NULL)
	{
		mediaBuffer->release();
	}

	return true;


}



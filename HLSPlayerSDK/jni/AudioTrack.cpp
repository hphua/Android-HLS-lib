/*
 * AudioTrack.cpp
 *
 *  Created on: Jul 17, 2014
 *      Author: Mark
 */

#include <jni.h>
#include "constants.h"
#include <AudioTrack.h>
#include "HLSPlayerSDK.h"
#include "HLSPlayer.h"

extern HLSPlayerSDK* gHLSPlayerSDK;


using namespace android_video_shim;

AudioTrack::AudioTrack(JavaVM* jvm) : mJvm(jvm), mAudioTrack(NULL), mGetMinBufferSize(NULL), mPlay(NULL), mPause(NULL), mStop(NULL), mFlush(NULL), buffer(NULL),
										mRelease(NULL), mGetTimestamp(NULL), mCAudioTrack(NULL), mWrite(NULL), mGetPlaybackHeadPosition(NULL), mSetPositionNotificationPeriod(NULL),
										mSampleRate(0), mNumChannels(0), mBufferSizeInBytes(0), mChannelMask(0), mTrack(NULL), mPlayState(STOPPED),
										mTimeStampOffset(0), samplesWritten(0), mWaiting(true)
{
	if (!mJvm)
	{
		LOGE("Java VM is NULL");
	}

	int err = pthread_mutex_init(&updateMutex, NULL);
	LOGI(" AudioTrack mutex err = %d", err);
}

AudioTrack::~AudioTrack() {
}

void AudioTrack::unload()
{
	LOGI("Unloading");
	if (mTrack)
	{
		// we're not closed!!!
		LOGI("Closing");
		Close();
	}
	delete this;
}

void AudioTrack::Close()
{
	Stop();
	if (mJvm)
	{
		JNIEnv* env = NULL;
		gHLSPlayerSDK->GetEnv(&env);
		env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);
		env->DeleteGlobalRef(buffer);
		buffer = NULL;
		env->DeleteGlobalRef(mTrack);
		mTrack = NULL;
		env->DeleteGlobalRef(mCAudioTrack);
		mCAudioTrack = NULL;

		if(mAudioSource.get())
			mAudioSource->stop();
		if(mAudioSource23.get())
			mAudioSource23->stop();

		sem_destroy(&semPause);
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
	gHLSPlayerSDK->GetEnv(&env);


	int err = sem_init(&semPause, 0, 0);
	if (err != 0)
	{
		LOGE("Failed to init audio pause semaphore : %d", err);
		return false;
	}

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
        mFlush = env->GetMethodID(mCAudioTrack, "flush", "()V");
        mRelease = env->GetMethodID(mCAudioTrack, "release", "()V");
        mWrite = env->GetMethodID(mCAudioTrack, "write", "([BII)I");
        mSetPositionNotificationPeriod = env->GetMethodID(mCAudioTrack, "setPositionNotificationPeriod", "(I)I");
        mGetPlaybackHeadPosition = env->GetMethodID(mCAudioTrack, "getPlaybackHeadPosition", "()I");
    }
    return true;
}


bool AudioTrack::Set(sp<MediaSource> audioSource, bool alreadyStarted)
{
	if (mAudioSource.get())
	{
		mAudioSource->stop();
		mAudioSource.clear();
	}

	LOGI("Set with %p", audioSource.get());
	mAudioSource = audioSource;
	if (!alreadyStarted) mAudioSource->start(NULL);

	mWaiting = false;
	return UpdateFormatInfo();
}


bool AudioTrack::Set23(sp<MediaSource23> audioSource, bool alreadyStarted)
{
	if (mAudioSource23.get())
		mAudioSource23->stop();

	LOGI("Set23 with %p", audioSource.get());
	mAudioSource23 = audioSource;
	if (!alreadyStarted) mAudioSource23->start(NULL);
	mWaiting = false;
	return UpdateFormatInfo();
}

bool AudioTrack::UpdateFormatInfo()
{
	sp<MetaData> format;
	if(mAudioSource.get())
		format = mAudioSource->getFormat();
	else if(mAudioSource23.get())
		format = mAudioSource23->getFormat();
	else
	{
		LOGE("Couldn't find format!");
		return false;
	}

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

	return true;
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
	LOGI("Setting buffer = NULL");
	buffer = NULL;

	LOGI("Attaching to current java thread");
	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);

	LOGI("Updating Format Info");
	// Refresh our format information.
	if(!UpdateFormatInfo())
	{
		LOGE("Failed to update format info!");
		return false;
	}

	LOGI("Setting Channel Config");
	int channelConfig = CHANNEL_CONFIGURATION_STEREO;
	switch (mNumChannels)
	{
	case 1:
		channelConfig = CHANNEL_CONFIGURATION_MONO;
		break;
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

	LOGI("Creating AudioTrack mNumChannels=%d | channelConfig=%d | mSampleRate=%d", mNumChannels, channelConfig, mSampleRate);

	// HACK ALERT!! Note that this value was originally 2... this is a quick hack to test the audio sending since 
	// the media buffer I am seeing is exactly the same size as this value * 4
	mBufferSizeInBytes = env->CallStaticIntMethod(mCAudioTrack, mGetMinBufferSize, mSampleRate, channelConfig,ENCODING_PCM_16BIT) * 4; 

	LOGI("mBufferSizeInBytes=%d", mBufferSizeInBytes);




	// Release our old track.
	if(mTrack)
	{
		LOGI("Releasing old java AudioTrack");
		env->DeleteGlobalRef(mTrack);
		mTrack = NULL;
	}

	LOGI("Generating java AudioTrack reference");
	mTrack = env->NewGlobalRef(env->NewObject(mCAudioTrack, mAudioTrack, STREAM_MUSIC, mSampleRate, channelConfig, ENCODING_PCM_16BIT, mBufferSizeInBytes * 2, MODE_STREAM ));

	LOGI("Calling java AudioTrack Play");
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPlay);
	int lastPlayState = mPlayState;

	mPlayState = PLAYING;

	if (lastPlayState == PAUSED || lastPlayState == SEEKING)
	{
		LOGI("Playing Audio Thread: state = %s | semPause.count = %d", lastPlayState==PAUSED?"PAUSED":(lastPlayState==SEEKING?"SEEKING":"Not Possible!"), semPause.count );
		sem_post(&semPause);
	}
	mWaiting = false;
	samplesWritten = 0;
	return true;

}

void AudioTrack::Play()
{
	LOGI("Trying to play: state = %d", mPlayState);
	mWaiting = false;
	if (mPlayState == PLAYING) return;
	int lastPlayState = mPlayState;

	mPlayState = PLAYING;

	if (lastPlayState == PAUSED || lastPlayState == SEEKING)
	{
		LOGI("Playing Audio Thread: state = %s | semPause.count = %d", lastPlayState==PAUSED?"PAUSED":(lastPlayState==SEEKING?"SEEKING":"Not Possible!"), semPause.count );
		sem_post(&semPause);
	}

	LOGI("Audio State = PLAYING: semPause.count = %d", semPause.count);

	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPlay);
	samplesWritten = 0;

}

bool AudioTrack::Stop(bool seeking)
{
	if (mPlayState == STOPPED) return true;

	int lastPlayState = mPlayState;

	if (seeking)
		mPlayState = SEEKING;
	else
		mPlayState = STOPPED;

	if (lastPlayState == PAUSED)
	{
		LOGI("Stopping Audio Thread: state = PAUSED | semPause.count = %d", semPause.count );
		sem_post(&semPause);
	}
	else if (lastPlayState == SEEKING)
	{
		LOGI("Stopping Audio Thread: state = SEEKING | semPause.count = %d", semPause.count );
		sem_post(&semPause);
	}

	pthread_mutex_lock(&updateMutex);

	if(seeking)
	{

		if (mAudioSource.get())
		{
			mAudioSource->stop();
			mAudioSource.clear();
		}

		if (mAudioSource23.get())
		{
			mAudioSource23->stop();
			mAudioSource23.clear();
		}

	}

	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);

	pthread_mutex_unlock(&updateMutex);

	return true;
}

void AudioTrack::Pause()
{
	if (mPlayState == PAUSED) return;
	mPlayState = PAUSED;
	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPause);
}

void AudioTrack::Flush()
{
	if (mPlayState == PLAYING) return;
	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mFlush);
	
	pthread_mutex_lock(&updateMutex);
	samplesWritten = 0;
	pthread_mutex_unlock(&updateMutex);

}

void AudioTrack::SetTimeStampOffset(double offsetSecs)
{
	mTimeStampOffset = offsetSecs;
}


int64_t AudioTrack::GetTimeStamp()
{
	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	double frames = env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mGetPlaybackHeadPosition);
	double secs = frames / (double)mSampleRate;
	LOGV2("secs = %f | mTimeStampOffset = %f", secs, mTimeStampOffset);
	return ((secs + mTimeStampOffset) * 1000000);
}

void AudioTrack::ReadUntilTime(double timeSecs)
{
	status_t res;
	MediaBuffer* mediaBuffer = NULL;

	int64_t targetTimeUs = (int64_t)(timeSecs * 1000000.0f);
	int64_t timeUs = 0;

	LOGI("Starting read to %f seconds: targetTimeUs = %lld", timeSecs, targetTimeUs);
	while (timeUs < targetTimeUs)
	{
		if(mAudioSource.get())
			res = mAudioSource->read(&mediaBuffer, NULL);

		if(mAudioSource23.get())
			res = mAudioSource23->read(&mediaBuffer, NULL);


		if (res == OK)
		{
			bool rval = mediaBuffer->meta_data()->findInt64(kKeyTime, &timeUs);
			if (!rval)
			{
				LOGI("Frame did not have time value: STOPPING");
				timeUs = 0;
			}

			//LOGI("Finished reading from the media buffer");
			RUNDEBUG(mediaBuffer->meta_data()->dumpToLog());
			LOGI("key time = %lld | target time = %lld", timeUs, targetTimeUs);
		}
		else if (res == INFO_FORMAT_CHANGED)
		{
		}
		else if (res == ERROR_END_OF_STREAM)
		{
			LOGE("End of Audio Stream");
			return;
		}

		if (mediaBuffer != NULL)
		{
			mediaBuffer->release();
			mediaBuffer = NULL;
		}
	}

	mTimeStampOffset = ((double)timeUs / 1000000.0f);
}

int AudioTrack::Update()
{
	LOGV("Audio Update Thread Running");
	if (mWaiting) return AUDIOTHREAD_WAIT;
	if (mPlayState != PLAYING)
	{
		while (mPlayState == PAUSED)
		{
			LOGI("Pausing Audio Thread: state = PAUSED | semPause.count = %d", semPause.count );
			sem_wait(&semPause);
		}

		while (mPlayState == SEEKING)
		{
			LOGI("Pausing Audio Thread: state = SEEKING | semPause.count = %d", semPause.count );
			sem_wait(&semPause);
			LOGI("Resuming Audio Thread: state = %d | semPause.count = %d", mPlayState, semPause.count );
		}

		if (mPlayState == STOPPED)
		{
			LOGI("mPlayState == STOPPED. Ending audio update thread!");
			return AUDIOTHREAD_FINISH; // We don't really want to add more stuff to the buffer
							// and potentially run past the end of buffered source data
							// if we're not actively playing
		}
	}

	pthread_mutex_lock(&updateMutex);

	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);


	MediaBuffer* mediaBuffer = NULL;

	//LOGI("Reading to the media buffer");
	status_t res;

	if(mAudioSource.get())
		res = mAudioSource->read(&mediaBuffer, NULL);

	if(mAudioSource23.get())
		res = mAudioSource23->read(&mediaBuffer, NULL);

	if (res == OK)
	{
		//LOGI("Finished reading from the media buffer");
		RUNDEBUG(mediaBuffer->meta_data()->dumpToLog());
		env->PushLocalFrame(2);

		if(!buffer)
		{
			buffer = env->NewByteArray(mBufferSizeInBytes);
			buffer = (jarray)env->NewGlobalRef(buffer);
		}

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
				//LOGV("%hd %hd %hd %hd", pBShorts[0], pBShorts[1], pBShorts[2], pBShorts[3]);
				int len = mbufSize / 2;
				//LOGV("%hd %hd %hd %hd", pBShorts[len - 4], pBShorts[len - 3], pBShorts[len - 2], pBShorts[len - 1]);

				env->ReleasePrimitiveArrayCritical(buffer, pBuffer, 0);
				//LOGI("Finished copying audio data to buffer");
				samplesWritten += env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mWrite, buffer, 0, mbufSize  );
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
		LOGI("Format Changed");

		// Flush our existing track.
		Flush();

		// Create new one.
		Start();

		pthread_mutex_unlock(&updateMutex);
		Update();
	}
	else if (res == ERROR_END_OF_STREAM)
	{
		LOGE("End of Audio Stream");
		mWaiting = true;
		if (gHLSPlayerSDK)
		{
			if (gHLSPlayerSDK->GetPlayer())
			{
				gHLSPlayerSDK->GetPlayer()->SetState(FOUND_DISCONTINUITY);
			}
		}
		pthread_mutex_unlock(&updateMutex);
		return AUDIOTHREAD_WAIT;
	}

	if (mediaBuffer != NULL)
		mediaBuffer->release();

	pthread_mutex_unlock(&updateMutex);
	return AUDIOTHREAD_CONTINUE;
}

void AudioTrack::shutdown()
{
	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	env->DeleteGlobalRef(buffer);
}

int AudioTrack::getBufferSize()
{
	JNIEnv* env;
	gHLSPlayerSDK->GetEnv(&env);
	long long frames = env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mGetPlaybackHeadPosition);

	return (samplesWritten / 2) - frames;
}

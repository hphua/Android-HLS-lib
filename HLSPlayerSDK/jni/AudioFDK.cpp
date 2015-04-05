/*
 * AudioFDK.cpp
 *
 *  Created on: Mar 24, 2015
 *      Author: Mark
 */

#include <jni.h>
#include "constants.h"
#include "HLSPlayerSDK.h"
#include "HLSPlayer.h"
#include <unistd.h>
#include <AudioFDK.h>
#include <ESDS.h>

extern HLSPlayerSDK* gHLSPlayerSDK;


#define STREAM_MUSIC 3
#define CHANNEL_CONFIGURATION_MONO 4
#define CHANNEL_CONFIGURATION_STEREO 12
#define CHANNEL_CONFIGURATION_5_1 1052
#define ENCODING_PCM_8BIT 3
#define ENCODING_PCM_16BIT 2
#define MODE_STREAM 1


using namespace android_video_shim;

AudioFDK::AudioFDK(JavaVM* jvm) : mJvm(jvm), mAudioTrack(NULL), mGetMinBufferSize(NULL), mPlay(NULL), mPause(NULL), mStop(NULL), mFlush(NULL), buffer(NULL),
		mRelease(NULL), mGetTimestamp(NULL), mCAudioTrack(NULL), mWrite(NULL), mGetPlaybackHeadPosition(NULL), mSetPositionNotificationPeriod(NULL),
		mSampleRate(0), mNumChannels(0), mBufferSizeInBytes(0), mChannelMask(0), mTrack(NULL), mPlayState(INITIALIZED),
		mTimeStampOffset(0), samplesWritten(0), mWaiting(true), mNeedsTimeStampOffset(true), mAACDecoder(NULL), mESDSType(TT_UNKNOWN), mESDSData(NULL), mESDSSize(0),
		mPlayingSilence(false)
{
	if (!mJvm)
	{
		LOGE("Java VM is NULL");
	}

	int err = pthread_mutex_init(&updateMutex, NULL);
	LOGI(" AudioTrack mutex err = %d", err);
	err = initRecursivePthreadMutex(&lock);
}

AudioFDK::~AudioFDK()
{
	// TODO Auto-generated destructor stub
}

void AudioFDK::unload()
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

void AudioFDK::Close()
{
	Stop();

	if (mJvm)
	{
		AutoLock locker(&lock, __func__);
		JNIEnv* env = NULL;
		gHLSPlayerSDK->GetEnv(&env);
		if (env)
		{
			env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);
			env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mRelease);
			env->DeleteGlobalRef(buffer);
			env->DeleteGlobalRef(mTrack);
			env->DeleteGlobalRef(mCAudioTrack);
		}
		buffer = NULL;
		mTrack = NULL;
		mCAudioTrack = NULL;

		if (mAACDecoder) aacDecoder_Close(mAACDecoder);

		sem_destroy(&semPause);
	}
}

// TODO: This could likely be moved in it's entirety to the base class
bool AudioFDK::Init()
{
	if (!mJvm)
	{
		LOGE("Java VM is NULL - aborting init");
		return false;
	}
	JNIEnv* env = NULL;
	if (!gHLSPlayerSDK->GetEnv(&env)) return false;


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

void AudioFDK::ClearAudioSource()
{
	Set(NULL, true);
	Set23(NULL, true);
}

bool AudioFDK::Set(sp<MediaSource> audioSource, bool alreadyStarted)
{
	if (mAudioSource.get())
	{
		mAudioSource->stop();
		mAudioSource.clear();
	}

	LOGI("Set with %p", audioSource.get());
	mAudioSource = audioSource;
	if (!alreadyStarted && mAudioSource.get()) mAudioSource->start(NULL);

	mWaiting = false;
	return UpdateFormatInfo();
}

bool AudioFDK::Set23(sp<MediaSource23> audioSource, bool alreadyStarted)
{
	if (mAudioSource23.get())
		mAudioSource23->stop();

	LOGI("Set23 with %p", audioSource.get());
	mAudioSource23 = audioSource;
	if (!alreadyStarted && mAudioSource23.get()) mAudioSource23->start(NULL);
	mWaiting = false;
	return UpdateFormatInfo();
}

bool AudioFDK::UpdateFormatInfo()
{
	sp<MetaData> format;

	mPlayingSilence = false;
	if(mAudioSource.get())
		format = mAudioSource->getFormat();
	else if(mAudioSource23.get())
		format = mAudioSource23->getFormat();
	else
	{
		LOGE("We do not have an audio source. Setting a base format for feeding silence.");
		mSampleRate=44100;
		mNumChannels = 2;
		mChannelMask = 0;
		mPlayingSilence = true;
		return true;
	}

	format->dumpToLog();

	const char* mime;
	bool success = format->findCString(kKeyMIMEType, &mime);
	if (!success)
	{
		LOGE("Could not find mime type");
		return false;
	}
	if (strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC))
	{
		LOGE("Mime Type was not audio/mp4a-latm. Was: %s", mime);
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


	if (!format->findData(kKeyESDS, &mESDSType, &mESDSData, &mESDSSize))
	{
		// Uh - what do we do now?
		LOGE("Couldn't find ESDS data");
	}

	return true;
}

// TODO: Figure out the difference between start and play and document that!!!
bool AudioFDK::Start()
{
	LOGTRACE("%s", __func__);
	AutoLock locker(&lock, __func__);

	LOGI("Updating Format Info");
	// Refresh our format information.
	if(!UpdateFormatInfo())
	{
		LOGE("Failed to update format info!");
		return false;
	}

	ESDS esds((const char*)mESDSData, mESDSSize);
	if (status_t ec = esds.InitCheck() != OK)
	{
		LOGE("ESDS is not okay: 0x%4.4x", ec);
		mPlayingSilence = true;
	}


	if (!mPlayingSilence)
	{
		mAACDecoder = aacDecoder_Open(TT_MP4_ADIF, 1); // This is what SoftAAC2 does in initDecoder()
		if (mAACDecoder != NULL)
		{
			const void* codec_specific_data;
			size_t codec_specific_data_size;
			esds.getCodecSpecificInfo(&codec_specific_data, &codec_specific_data_size);


			UCHAR* inBuffer[1] = { (UCHAR*)codec_specific_data };
			UINT inBufferLength[1] = { codec_specific_data_size };
			AAC_DECODER_ERROR decoderErr = aacDecoder_ConfigRaw(mAACDecoder, inBuffer, inBufferLength);
			if (decoderErr != AAC_DEC_OK)
			{
				LOGE("aac ESDS length = %d, ptr=%p", mESDSSize, mESDSData);
				UCHAR* d = (UCHAR*)mESDSData;
				LogBytes("Begin ESDSData", "End ESDSData", (char*)d, mESDSSize);

				LOGE("aacDecoder_ConfigRaw decoderErr = 0x%4.4x", decoderErr );
				return false;
			}
		}
		else
		{
			LOGE("Could not open aac decoder");
		}
	}

	if (!InitJavaTrack())
		return false;


	int lastPlayState = mPlayState;

	mPlayState = PLAYING;

	if (lastPlayState == PAUSED || lastPlayState == SEEKING || lastPlayState == INITIALIZED)
	{
		LOGI("Playing Audio Thread: state = %s | semPause.count = %d", lastPlayState==PAUSED?"PAUSED":(lastPlayState==SEEKING?"SEEKING":(lastPlayState==INITIALIZED?"INITIALIZED":"Not Possible!")), semPause.count );
		sem_post(&semPause);
	}
	mWaiting = false;
	samplesWritten = 0;
	return true;
}

bool AudioFDK::InitJavaTrack()
{

	LOGI("Attaching to current java thread");
	JNIEnv* env;
	if (!gHLSPlayerSDK->GetEnv(&env)) return false;

	AutoLock locker(&lock, __func__);

	LOGI("Setting buffer = NULL");
	if (buffer)
	{
		env->DeleteGlobalRef(buffer);
		buffer = NULL;
	}

	if(mSampleRate == 0)
	{
		LOGE("Zero sample rate");
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

	LOGV("mBufferSizeInBytes=%d", mBufferSizeInBytes);

	// Release our old track.
	if(mTrack)
	{
		LOGI("Releasing old java AudioTrack");
		env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);
		env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mRelease);
		env->DeleteGlobalRef(mTrack);
		mTrack = NULL;
	}

	LOGI("Generating java AudioTrack reference");
	mTrack = env->NewGlobalRef(env->NewObject(mCAudioTrack, mAudioTrack, STREAM_MUSIC, mSampleRate, channelConfig, ENCODING_PCM_16BIT, mBufferSizeInBytes * 2, MODE_STREAM ));

	LOGI("Calling java AudioTrack Play");
	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPlay);

	if(!buffer)
	{
		buffer = env->NewByteArray(mBufferSizeInBytes);
		buffer = (jarray)env->NewGlobalRef(buffer);
	}


	return mTrack != NULL;
}


void AudioFDK::Play()
{
	LOGTRACE("%s", __func__);
	LOGI("Trying to play: state = %d", mPlayState);
	mWaiting = false;
	if (mPlayState == PLAYING) return;
	int lastPlayState = mPlayState;

	mPlayState = PLAYING;

	if (lastPlayState == PAUSED || lastPlayState == SEEKING || lastPlayState == INITIALIZED)
	{
		LOGI("Playing Audio Thread: state = %s | semPause.count = %d", lastPlayState==PAUSED?"PAUSED":(lastPlayState==SEEKING?"SEEKING":(lastPlayState==INITIALIZED?"INITIALIZED":"Not Possible!")), semPause.count );
		sem_post(&semPause);
	}

	LOGI("Audio State = PLAYING: semPause.count = %d", semPause.count);

	AutoLock locker(&lock, __func__);
	if (mTrack)
	{
		JNIEnv* env;
		if (gHLSPlayerSDK->GetEnv(&env))
			env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPlay);
	}

	samplesWritten = 0;
}

bool AudioFDK::Stop(bool seeking)
{
	LOGTRACE("%s", __func__);
	if (mPlayState == STOPPED && seeking == false) return true;

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


	JNIEnv* env;
	if (!gHLSPlayerSDK->GetEnv(&env))
	{
		LOGE("Could not get Java Environment in order to stop the track");
		return false;
	}

	AutoLock locker(&lock, __func__);
	pthread_mutex_lock(&updateMutex);

	if (mTrack == NULL)
	{
			pthread_mutex_unlock(&updateMutex);
			return true; // We're already stopped since we don't have a track
	}

	env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mStop);

	if(seeking)
	{
		if (mAACDecoder) aacDecoder_Close(mAACDecoder);
		mAACDecoder = NULL;
		env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mRelease);
		env->DeleteGlobalRef(mTrack);
		mTrack = NULL;
	}


	pthread_mutex_unlock(&updateMutex);

	return true;
}

void AudioFDK::Pause()
{
	LOGTRACE("%s", __func__);
	if (mPlayState == PAUSED) return;
	mPlayState = PAUSED;

}

void AudioFDK::Flush()
{
	LOGTRACE("%s", __func__);
	if (mPlayState == PLAYING) return;

	JNIEnv* env;
	if (gHLSPlayerSDK->GetEnv(&env))
		env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mFlush);

	pthread_mutex_lock(&updateMutex);
	samplesWritten = 0;
	pthread_mutex_unlock(&updateMutex);

}

void AudioFDK::forceTimeStampUpdate()
{
	LOGTRACE("%s", __func__);
	mNeedsTimeStampOffset = true;
}

void AudioFDK::SetTimeStampOffset(double offsetSecs)
{
	LOGTRACE("%s", __func__);
	LOGTIMING("Setting mTimeStampOffset to: %f", offsetSecs);
	mTimeStampOffset = offsetSecs;
	mNeedsTimeStampOffset = false;
}

int64_t AudioFDK::GetTimeStamp()
{
	LOGTRACE("%s", __func__);
	JNIEnv* env;
	if (!gHLSPlayerSDK->GetEnv(&env)) return 0;

	AutoLock locker(&lock, __func__);

	if(!mTrack || !mCAudioTrack)
	{
		LOGI("No track! aborting...");
		return mTimeStampOffset * NANOSEC_PER_MS;
	}

	double frames = env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mGetPlaybackHeadPosition);
	double secs = frames / (double)mSampleRate;
	LOGTIMING("TIMESTAMP: secs = %f | mTimeStampOffset = %f | timeStampUS = %lld", secs, mTimeStampOffset, (int64_t)((secs + mTimeStampOffset) * 1000000));
	return ((secs + mTimeStampOffset) * NANOSEC_PER_MS);
}

bool AudioFDK::ReadUntilTime(double timeSecs)
{
	LOGTRACE("%s", __func__);
	status_t res = ERROR_END_OF_STREAM;
	MediaBuffer* mediaBuffer = NULL;

	int64_t targetTimeUs = (int64_t)(timeSecs * 1000000.0f);
	int64_t timeUs = 0;

	LOGI("Starting read to %f seconds: targetTimeUs = %lld", timeSecs, targetTimeUs);
	while (timeUs < targetTimeUs)
	{
		if(mAudioSource.get())
			res = mAudioSource->read(&mediaBuffer, NULL);
		else if(mAudioSource23.get())
			res = mAudioSource23->read(&mediaBuffer, NULL);
		else
		{
			// Set timeUs to our target, and let the loop fall out so that we can get the timestamp
			// set properly.
			timeUs = targetTimeUs;
			continue;
		}


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
			LOGTIMING("key time = %lld | target time = %lld", timeUs, targetTimeUs);
		}
		else if (res == INFO_FORMAT_CHANGED)
		{
			LOGI("Audio Stream Format Changed");
		}
		else if (res == ERROR_END_OF_STREAM)
		{
			LOGE("End of Audio Stream");
			return false;
		}

		if (mediaBuffer != NULL)
		{
			mediaBuffer->release();
			mediaBuffer = NULL;
		}

		sched_yield();
	}

	mTimeStampOffset = ((double)timeUs / 1000000.0f);
	return true;
}

int AudioFDK::Update()
{
	LOGTRACE("%s", __func__);
	LOGTHREAD("Audio Update Thread Running");
	if (mWaiting) return AUDIOTHREAD_WAIT;
	if (mPlayState != PLAYING)
	{
		while (mPlayState == INITIALIZED)
		{
			LOGI("Audio Thread initialized. Waiting to start | semPause.count = %d", semPause.count);
			sem_wait(&semPause);
		}

		while (mPlayState == PAUSED)
		{
			LOGI("Pausing Audio Thread: state = PAUSED | semPause.count = %d", semPause.count );

			// Pause the java track here so that we aren't in the middle of feeding it data when we pause.
			JNIEnv* env;
			if (gHLSPlayerSDK->GetEnv(&env))
				env->CallNonvirtualVoidMethod(mTrack, mCAudioTrack, mPause);
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


	JNIEnv* env;
	if (!gHLSPlayerSDK->GetEnv(&env))
		return AUDIOTHREAD_FINISH; // If we don't have a java environment at this point, something has killed it,
	// so we better kill the thread.

	pthread_mutex_lock(&updateMutex);

	MediaBuffer* mediaBuffer = NULL;

	//LOGI("Reading to the media buffer");
	status_t res = OK;

	if(mAudioSource.get())
		res = mAudioSource->read(&mediaBuffer, NULL);
	else if(mAudioSource23.get())
		res = mAudioSource23->read(&mediaBuffer, NULL);
	else
	{
		res = OK;
	}

	if (res == OK)
	{
		//LOGI("Finished reading from the media buffer");
		RUNDEBUG( {if (mediaBuffer) mediaBuffer->meta_data()->dumpToLog();} );
		env->PushLocalFrame(2);

		if (mediaBuffer && mAACDecoder)
		{
			int64_t timeUs;
			bool rval = mediaBuffer->meta_data()->findInt64(kKeyTime, &timeUs);
			if (!rval)
			{
				timeUs = 0;

			}
			LOGTIMING("Audiotrack timeUs=%lld | mNeedsTimeStampOffset=%s", timeUs, mNeedsTimeStampOffset ? "True":"False");

			// If we need the timestamp offset (our audio starts at 0, which is not quite accurate and won't match
			// the video time), set it. This should only be the case when we first start a stream.
			if (mNeedsTimeStampOffset)
			{
				LOGTIMING("Need to set mTimeStampOffset = %lld", timeUs);
				SetTimeStampOffset(((double)timeUs / 1000000.0f));
			}

			size_t mbufSize = mediaBuffer->range_length();

			AAC_DECODER_ERROR err;
			UINT valid = mbufSize;
			UINT bufSize = mbufSize;
			unsigned char* data = (unsigned char*)mediaBuffer->data();

			int dataOffset = 0;

			while (valid > 0)
			{
				bufSize = bufSize - dataOffset;
				unsigned char* dataBuffer = data + dataOffset;
#ifdef _AUDIO_FDK
				LogBytes("aac buffer", "aac buffer", (char*)dataBuffer, 30);
#endif

				err = aacDecoder_Fill(mAACDecoder, &dataBuffer, &bufSize , &valid);
				if (err != AAC_DEC_OK)
				{
					LOGE("aacDecoder_Fill() failed: %x", err);
					mediaBuffer->release();
					return AUDIOTHREAD_FINISH;
				}

				LOGV("Valid = %d", valid);
				dataOffset = bufSize - valid;

#define BUFFER_SIZE (8192 * 2)

				INT_PCM* tmpBuffer = (INT_PCM*)malloc(BUFFER_SIZE);


				LOGAUDIO("MediaBufferSize = %d, mBufferSizeInBytes = %d", mbufSize, mBufferSizeInBytes );
				if (mbufSize <= mBufferSizeInBytes)
				{
					err = AAC_DEC_OK;

					void* pBuffer = env->GetPrimitiveArrayCritical(buffer, NULL);
					int offset = 0;
					while (err == AAC_DEC_OK)
					{
						LOGAUDIO("tmpBuffer size = %d", BUFFER_SIZE);

						memset(tmpBuffer, 0xCD, BUFFER_SIZE);

						err = aacDecoder_DecodeFrame(mAACDecoder, (INT_PCM*)tmpBuffer, BUFFER_SIZE, 0 );
						if (err != AAC_DEC_OK)
						{
							if (err == AAC_DEC_NOT_ENOUGH_BITS)
								LOGAUDIO("aacDecoder_DecodeFrame() NOT ENOUGH BITS");
							else
								LOGE("aacDecoder_DecodeFrame() failed: %x", err);
						}
						else
						{
							LOGAUDIO("Decoded Frame");
							int frameSize = 0;
							CStreamInfo* streamInfo = aacDecoder_GetStreamInfo(mAACDecoder);
							bool reinitJava = false;
							if (streamInfo->sampleRate != mSampleRate)
							{
								if (streamInfo->sampleRate > mSampleRate)
								{
									LOGAUDIO("Sample Rate changed from %d to %d", mSampleRate, streamInfo->sampleRate);
									mSampleRate = streamInfo->sampleRate;
									reinitJava = true;
								}
							}
							if (streamInfo->numChannels != mNumChannels)
							{
								LOGAUDIO("Num Channels changed from %d to %d", mNumChannels, streamInfo->numChannels);
								mNumChannels = streamInfo->numChannels;
								reinitJava = true;
							}

							if (reinitJava)
							{
								env->ReleasePrimitiveArrayCritical(buffer, pBuffer, 0);
								InitJavaTrack();
								pBuffer = env->GetPrimitiveArrayCritical(buffer, NULL);
								mNeedsTimeStampOffset = true;
							}

							frameSize = streamInfo->frameSize;
							int channels = streamInfo->numChannels;
							LOGAUDIO("offset = %d, frameSize = %d, tmpBufferSize=%d, channels=%d, sampleRate=%d", offset, frameSize, BUFFER_SIZE, channels, streamInfo->sampleRate);

							//LogBytes("Begin Frame", "End Frame", (char*)tmpBuffer, sizeof(tmpBuffer));

							int copySize = frameSize * sizeof(INT_PCM) * channels;

							if (offset + copySize > mBufferSizeInBytes)
							{
								LOGAUDIO("offset (%d) + sizeof(tmpBuffer) (%d) > mBufferSizeinBytes (%d) -- Writing to java audio track and getting new java buffer.", offset, BUFFER_SIZE, mBufferSizeInBytes);
								// We need to empty our buffer and make a new one!!!
								env->ReleasePrimitiveArrayCritical(buffer, pBuffer, 0);
								samplesWritten += env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mWrite, buffer, 0, offset  );

								pBuffer = env->GetPrimitiveArrayCritical(buffer, NULL);
								offset = 0;
							}
							LOGAUDIO("Copying %d bytes to buffer at offset %d", copySize, offset);
							memcpy(((char*)pBuffer) + offset, tmpBuffer, copySize);
							offset += copySize;
						}

					}

					env->ReleasePrimitiveArrayCritical(buffer, pBuffer, 0);
					samplesWritten += env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mWrite, buffer, 0, offset  );

				}
				else
				{
					LOGV("MediaBufferSize > mBufferSizeInBytes");
				}

				free(tmpBuffer);
			}
		}
		else
		{
			if (mNeedsTimeStampOffset)
			{
				LOGTIMING("Need to set mTimeStampOffset");
				int64_t videoTimeUs = gHLSPlayerSDK->GetPlayer()->GetLastTimeUS();
				if (videoTimeUs >= 0)
					SetTimeStampOffset(((double) videoTimeUs / (double)NANOSEC_PER_MS));
			}
			void* pBuffer = env->GetPrimitiveArrayCritical(buffer, NULL);
			if (pBuffer)
			{
				LOGAUDIO("Writing zeros to the audio buffer");
				memset(pBuffer, 0, mBufferSizeInBytes);
				int len = mBufferSizeInBytes / 2;
				env->ReleasePrimitiveArrayCritical(buffer, pBuffer, 0);
				samplesWritten += env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mWrite, buffer, 0, mBufferSizeInBytes  );
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


int AudioFDK::getBufferSize()
{
	LOGTRACE("%s", __func__);
	JNIEnv* env;
	if (!gHLSPlayerSDK->GetEnv(&env)) return 0;

	AutoLock locker(&lock, __func__);

	if(!mTrack)
	{
		LOGE("No track! aborting...");
		return 0;
	}

	long long frames = env->CallNonvirtualIntMethod(mTrack, mCAudioTrack, mGetPlaybackHeadPosition);

	return (samplesWritten / 2) - frames;
}


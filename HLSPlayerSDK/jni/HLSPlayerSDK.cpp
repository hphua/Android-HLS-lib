 /*
 * HLSPlayerSDK.cpp
 *
 *  Created on: Apr 11, 2014
 *      Author: Mark
 */

#include <jni.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "HLSPlayer.h"
#include "HLSPlayerSDK.h"
#include "debug.h"
#include "constants.h"
#include "androidVideoShim.h"
#include "HLSSegmentCache.h"

HLSPlayerSDK* gHLSPlayerSDK = NULL;

extern "C"
{
	void Java_com_kaltura_hlsplayersdk_PlayerViewController_InitNativeDecoder(JNIEnv * env, jobject jcaller)
	{
		android_video_shim::initLibraries();
		
		JavaVM* jvm = NULL;
		env->GetJavaVM(&jvm);

		HLSSegmentCache::initialize(jvm);

		// Handy test for the C++ segment cache API.
		/*
		unsigned char buff[200];
		int64_t readBytes = HLSSegmentCache::read("https://google.com/", 100, 199, buff);
		buff[199] = 0;
		LOGI("Attempting goog read: %lld bytes and got %s", readBytes, buff);
		*/

		if (gHLSPlayerSDK == NULL)
		{
			LOGI("Initializing player SDK.");
			gHLSPlayerSDK = new HLSPlayerSDK(jvm);
		}
		if (gHLSPlayerSDK && gHLSPlayerSDK->GetPlayer() == NULL) 
		{
			LOGI("Initializing decoder.");
			gHLSPlayerSDK->CreateDecoder();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_CloseNativeDecoder(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			LOGE("Closing decoder");
			gHLSPlayerSDK->Close(env);
			delete gHLSPlayerSDK;
			gHLSPlayerSDK = NULL;
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_ResetPlayer(JNIEnv* env, jobject jcaller)
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Reset();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_StopPlayer(JNIEnv* env, jobject jcaller)
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Stop();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_PlayFile(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			gHLSPlayerSDK->PlayFile();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_TogglePause(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->TogglePause();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_SetSurface(JNIEnv* env, jobject jcaller, jobject surface)
	{
		//LOGI("Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer() == NULL) 
				gHLSPlayerSDK->CreateDecoder();

			LOGI("HLSPlayerSDK is not null");

			gHLSPlayerSDK->GetPlayer()->SetSurface(env, surface);
		}
	}

	jint Java_com_kaltura_hlsplayersdk_PlayerViewController_NextFrame(JNIEnv* env, jobject jcaller)
	{
		//LOGI("Entered");
		if (gHLSPlayerSDK == NULL)
			return 0;
		if (!gHLSPlayerSDK->GetPlayer())
			return 0;
		int rval = gHLSPlayerSDK->GetPlayer()->Update();
		if (rval >= 0)
			return gHLSPlayerSDK->GetPlayer()->GetCurrentTimeMS();
		return rval;
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_FeedSegment(JNIEnv* env, jobject jcaller, jstring jurl, jint quality, jint continuityEra, jdouble startTime )
	{
		LOGI("Entered");
		
		if (gHLSPlayerSDK == NULL)
		{
			LOGE("No player SDK!");
			return;
		}
		
		if (gHLSPlayerSDK->GetPlayer() == NULL)
		{
			LOGE("No Player instance on the SDK!");
			return;
		}

		const char* url = env->GetStringUTFChars(jurl, 0);
		gHLSPlayerSDK->GetPlayer()->FeedSegment(url, quality, continuityEra, startTime);
		env->ReleaseStringUTFChars(jurl, url);
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_ClearAlternateAudio(JNIEnv* env, jobject jcaller )
	{
		LOGI("Entered");

		if (gHLSPlayerSDK == NULL)
		{
			LOGE("No player SDK!");
			return;
		}

		if (gHLSPlayerSDK->GetPlayer() == NULL)
		{
			LOGE("No Player instance on the SDK!");
			return;
		}

		gHLSPlayerSDK->GetPlayer()->ClearAlternateAudio();
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_FeedAlternateAudioSegment(JNIEnv* env, jobject jcaller, jstring jurl, jdouble startTime )
	{
		LOGI("Entered");

		if (gHLSPlayerSDK == NULL)
		{
			LOGE("No player SDK!");
			return;
		}

		if (gHLSPlayerSDK->GetPlayer() == NULL)
		{
			LOGE("No Player instance on the SDK!");
			return;
		}

		const char* url = env->GetStringUTFChars(jurl, 0);
		gHLSPlayerSDK->GetPlayer()->FeedAlternateAudioSegment(url, startTime);
		env->ReleaseStringUTFChars(jurl, url);
	}


	void Java_com_kaltura_hlsplayersdk_PlayerViewController_SeekTo(JNIEnv* env, jobject jcaller, jdouble time )
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer())
			{
				gHLSPlayerSDK->GetPlayer()->Seek(time);
			}
		}
	}

	jint Java_com_kaltura_hlsplayersdk_PlayerViewController_GetState(JNIEnv* env, jobject jcaller, jdouble time )
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			return gHLSPlayerSDK->GetPlayer()->GetState();
		}
		return STOPPED;
	}

	void Java_com_kaltura_hlsplayersdk_PlayerViewController_ApplyFormatChange(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->ApplyFormatChange();
		}
	}

}

HLSPlayerSDK::HLSPlayerSDK(JavaVM* jvm) : mPlayer(NULL), mJvm(jvm)
{

}

HLSPlayerSDK::~HLSPlayerSDK()
{

}

void HLSPlayerSDK::Close(JNIEnv* env)
{
	if (mPlayer != NULL)
	{
		mPlayer->Close(env);
		delete mPlayer;
		mPlayer = NULL;
	}
}

bool HLSPlayerSDK::CreateDecoder()
{
	if (!mPlayer)
	{
		mPlayer = new HLSPlayer(mJvm);
	}

	return mPlayer != NULL;
}

//#define METHOD CLASS_NAME"::GetDecoder()"
//HLSDecoder* HLSPlayerSDK::GetDecoder()
//{
//	return mDecoder;
//}

HLSPlayer* HLSPlayerSDK::GetPlayer()
{
	return mPlayer;
}

void HLSPlayerSDK::PlayFile()
{
	LOGI("::PlayFile()");
	if (!mPlayer && !CreateDecoder()) 
	{
		LOGE("Failed to initialize player.");
		return;
	}

	if (mPlayer->Play())
	{
		while (mPlayer->Update() == 0)
		{
			LOGI("Decoded a frame!");
		}
	}
}



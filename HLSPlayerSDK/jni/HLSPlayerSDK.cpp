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

HLSPlayerSDK* gHLSPlayerSDK = NULL;


extern "C"
{
	void Java_com_kaltura_hlsplayersdk_PlayerView_InitNativeDecoder(JNIEnv * env, jobject jcaller)
	{
		android_video_shim::initLibraries();
		JavaVM* jvm = NULL;
		env->GetJavaVM(&jvm);
		if (gHLSPlayerSDK == NULL)
			gHLSPlayerSDK = new HLSPlayerSDK(jvm);
		if (gHLSPlayerSDK && gHLSPlayerSDK->GetPlayer() == NULL) gHLSPlayerSDK->CreateDecoder();
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_CloseNativeDecoder(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			gHLSPlayerSDK->Close(env);
			delete gHLSPlayerSDK;
			gHLSPlayerSDK = NULL;
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_ResetPlayer(JNIEnv* env, jobject jcaller)
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Reset();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_StopPlayer(JNIEnv* env, jobject jcaller)
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Stop();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_PlayFile(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			gHLSPlayerSDK->PlayFile();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_TogglePause(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->TogglePause();
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface(JNIEnv* env, jobject jcaller, jobject surface)
	{
		//LOGI("Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer() == NULL) gHLSPlayerSDK->CreateDecoder();
			LOGI("HLSPlayerSDK is not null");

			gHLSPlayerSDK->GetPlayer()->SetSurface(env, surface);

//			ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
//			LOGI(CLASS_NAME, "Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface() - window = %0x", window);
//			if (window)
//			{
//				HLSDecoder* decoder = gHLSPlayerSDK->GetDecoder();
//				if (decoder) decoder->SetNativeWindow(window);
//			}
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_SetScreenSize(JNIEnv* env, jobject jcaller, jint width, jint height)
	{
		//LOGI("Entered");
		if (!gHLSPlayerSDK || !gHLSPlayerSDK->GetPlayer())
			return;

		gHLSPlayerSDK->GetPlayer()->SetScreenSize(width, height);
	}

	int Java_com_kaltura_hlsplayersdk_PlayerView_NextFrame(JNIEnv* env, jobject jcaller)
	{
		//LOGI("Entered");
		if (gHLSPlayerSDK == NULL)
			return 0;
		if (!gHLSPlayerSDK->GetPlayer())
			return 0;
		if (gHLSPlayerSDK->GetPlayer()->Update() >= 0)
			return 0;
		return gHLSPlayerSDK->GetPlayer()->GetCurrentTimeMS();
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_FeedSegment(JNIEnv* env, jobject jcaller, jstring jurl, jint quality, jdouble startTime )
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer())
			{
				const char* url = env->GetStringUTFChars(jurl, 0);
				gHLSPlayerSDK->GetPlayer()->FeedSegment(url, quality, startTime);
				env->ReleaseStringUTFChars(jurl, url);
			}
		}
	}

	void Java_com_kaltura_hlsplayersdk_PlayerView_SeekTo(JNIEnv* env, jobject jcaller, jdouble time )
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

	jint Java_com_kaltura_hlsplayersdk_PlayerView_GetState(JNIEnv* env, jobject jcaller, jdouble time )
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			return gHLSPlayerSDK->GetPlayer()->GetState();
		}
		return STOPPED;
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
	if (!mPlayer && !CreateDecoder()) return;

	//	mPlayer->FeedSegment("/storage/emulated/0/Movies/segment1_0_av.ts");
	//	mPlayer->FeedSegment("/storage/emulated/0/Movies/segment2_0_av.ts");
	//mPlayer->FeedSegment("/storage/emulated/0/Movies/segment3_0_av.ts");
	//mPlayer->FeedSegment("/storage/emulated/0/Movies/segment4_0_av.ts");
	//mPlayer->FeedSegment("/storage/emulated/0/Movies/segment5_0_av.ts");
	//mPlayer->FeedSegment("/storage/emulated/0/Movies/segment6_0_av.ts");
	if (mPlayer->Play())
	{
		while (mPlayer->Update() == 0)
		{
			LOGI("Decoded a frame!");
		}
	}
}



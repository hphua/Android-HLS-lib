/*
 * HLSPlayerSDK.cpp
 *
 *  Created on: Apr 11, 2014
 *      Author: Mark
 */

#include <jni.h>

#include "HLSPlayer.h"
#include "HLSPlayerSDK.h"


#include <android/native_window.h>
#include <android/native_window_jni.h>



#include "debug.h"
#include "constants.h"

#define CLASS_NAME APP_NAME"::HLSPlayerSDK"


HLSPlayerSDK* gHLSPlayerSDK = NULL;


extern "C"
{
	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_InitNativeDecoder()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_InitNativeDecoder(JNIEnv * env, jobject jcaller)
	{
		JavaVM* jvm = NULL;
		env->GetJavaVM(&jvm);
		if (gHLSPlayerSDK == NULL)
			gHLSPlayerSDK = new HLSPlayerSDK(jvm);
		if (gHLSPlayerSDK && gHLSPlayerSDK->GetPlayer() == NULL) gHLSPlayerSDK->CreateDecoder();
	}

	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_CloseNativeDecoder()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_CloseNativeDecoder(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			gHLSPlayerSDK->Close(env);
			delete gHLSPlayerSDK;
			gHLSPlayerSDK = NULL;
		}
	}

	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_PlayFile()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_PlayFile(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			gHLSPlayerSDK->PlayFile();
		}
	}

	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface(JNIEnv* env, jobject jcaller, jobject surface)
	{
		//LOGINFO(METHOD, "Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer() == NULL) gHLSPlayerSDK->CreateDecoder();
			LOGINFO(METHOD, "HLSPlayerSDK is not null");

			gHLSPlayerSDK->GetPlayer()->SetSurface(env, surface);

//			ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
//			LOGINFO(CLASS_NAME, "Java_com_kaltura_hlsplayersdk_PlayerView_SetSurface() - window = %0x", window);
//			if (window)
//			{
//				HLSDecoder* decoder = gHLSPlayerSDK->GetDecoder();
//				if (decoder) decoder->SetNativeWindow(window);
//			}
		}
	}

	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_NextFrame()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_NextFrame(JNIEnv* env, jobject jcaller)
	{
		bool rval = false;
		//LOGINFO(METHOD, "Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer())
			{
				if (gHLSPlayerSDK->GetPlayer()->Update() >= 0) rval = true;
			}

		}
	}

	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_FeedSegment()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_FeedSegment(JNIEnv* env, jobject jcaller, jstring jurl, jint quality, jdouble startTime )
	{
		LOGINFO(METHOD, "Entered");
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

	#define METHOD CLASS_NAME"::Java_com_kaltura_hlsplayersdk_PlayerView_SeekTo()"
	void Java_com_kaltura_hlsplayersdk_PlayerView_SeekTo(JNIEnv* env, jobject jcaller, jdouble time )
	{
		LOGINFO(METHOD, "Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer())
			{
				gHLSPlayerSDK->GetPlayer()->Seek(time);
			}
		}
	}

}

#define METHOD CLASS_NAME"::HLSPlayerSDK()"
HLSPlayerSDK::HLSPlayerSDK(JavaVM* jvm) : mPlayer(NULL), mJvm(jvm)
{

}

#define METHOD CLASS_NAME"::HLSPlayerSDK()"
HLSPlayerSDK::~HLSPlayerSDK()
{

}

#define METHOD CLASS_NAME"::Close()"
void HLSPlayerSDK::Close(JNIEnv* env)
{
	if (mPlayer != NULL)
	{
		mPlayer->Close(env);
		delete mPlayer;
		mPlayer = NULL;
	}
}

#define METHOD CLASS_NAME"::CreateDecoder()"
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

#define METHOD CLASS_NAME"::GetPlayer()"
HLSPlayer* HLSPlayerSDK::GetPlayer()
{
	return mPlayer;
}

#define METHOD CLASS_NAME"::PlayFile()"
void HLSPlayerSDK::PlayFile()
{
	LOGINFO(METHOD, "::PlayFile()");
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
			LOGINFO(CLASS_NAME, "Decoded a frame!");
		}
	}
}



 /*
 * HLSPlayerSDK.cpp
 *
 *  Created on: Apr 11, 2014
 *      Author: Mark
 */

#include <jni.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "aes.h"
#include "HLSPlayer.h"
#include "HLSPlayerSDK.h"
#include "debug.h"
#include "constants.h"
#include "androidVideoShim.h"
#include "HLSSegmentCache.h"

#include <unordered_map>

HLSPlayerSDK* gHLSPlayerSDK = NULL;


// AES crypto engine state.
bool gCryptoStateMapInitialized = false;
pthread_mutex_t gCryptoStateMapLock;
int gCryptoStateMapCounter = 1000;
std::tr1::unordered_map<int, AesCtx *> gCryptoStateMap;

extern "C"
{

	jint Java_com_kaltura_hlsplayersdk_cache_SegmentCacheItem_allocAESCryptoState(JNIEnv *env, jobject caller, jbyteArray key, jbyteArray iv)
	{
		if(gCryptoStateMapInitialized == false)
		{
			initRecursivePthreadMutex(&gCryptoStateMapLock);
			gCryptoStateMapInitialized = true;
		}

		AutoLock locker(&gCryptoStateMapLock);

		jbyte *keyPtr = env->GetByteArrayElements(key, NULL);
		jbyte *ivPtr = env->GetByteArrayElements(iv, NULL);

		// Initialize the AES context.
		AesCtx *ctx = new AesCtx();
		AesCtxIni(ctx, (unsigned char*)ivPtr, (unsigned char*)keyPtr, KEY128, CBC);

		LOGI("AES KEY = %8x%8x%8x%8x", *(int*)&keyPtr[0], *(int*)&keyPtr[4], *(int*)&keyPtr[8], *(int*)&keyPtr[12]);
		LOGI("AES IV  = %8x%8x%8x%8x", *(int*)&ivPtr[0], *(int*)&ivPtr[4], *(int*)&ivPtr[8], *(int*)&ivPtr[12]);

		env->ReleaseByteArrayElements(key, keyPtr, 0);
		env->ReleaseByteArrayElements(iv, ivPtr, 0);

		// Insert and assign a handle/id.
		int idx = gCryptoStateMapCounter++;
		gCryptoStateMap.insert(std::make_pair<int, AesCtx*>(idx, ctx));
		return idx;
	}

	void Java_com_kaltura_hlsplayersdk_cache_SegmentCacheItem_freeCryptoState(JNIEnv, jobject caller, jint handle)
	{
		if(gCryptoStateMapInitialized == false)
		{
			initRecursivePthreadMutex(&gCryptoStateMapLock);
			gCryptoStateMapInitialized = true;
		}

		AutoLock locker(&gCryptoStateMapLock);

		std::tr1::unordered_map<int, AesCtx*>::const_iterator got = gCryptoStateMap.find(handle);
		if(got == gCryptoStateMap.end())
		{
			LOGE("Failed to locate cryptostate %d! Ignoring free request...", handle);
			return;
		}

		// Free and remove from the map.
		gCryptoStateMap.erase(handle);
		delete got->second;
	}

	jlong Java_com_kaltura_hlsplayersdk_cache_SegmentCacheItem_decrypt(JNIEnv *env, jobject caller, jint handle, jbyteArray bytes, jlong offset, jlong length)
	{
		// Can we modify bytes in place?
		jboolean isCopy = false;
		jbyte *bytesPtr = env->GetByteArrayElements(bytes, &isCopy);
		if(isCopy)
		{
			LOGE("Got a copy; this could cause a lot of overhead!");
		}

		// Dummy debug to compare with AES; just writes zeroes over the "decrypted" region.
		// memset((unsigned char*)bytesPtr + offset, 0, length);

		// Get AES crypto state.
		std::tr1::unordered_map<int, AesCtx*>::const_iterator got = gCryptoStateMap.find(handle);
		if(got == gCryptoStateMap.end())
		{
			LOGE("Failed to locate cryptostate %d! Ignoring decrypt request...", handle);
			env->ReleaseByteArrayElements(bytes, bytesPtr, 0);
			return -1;
		}

		// Deal with buffering. 
		//
		// Key points:
		//    - AES requires 16 byte chunks.
		//    - Therefore we want to align all our start and end points
		//      to 16 bytes - easily done since we know we always decrypt
		//		the buffer from the start. We just have to make sure we
		//      always round up to a multiple of 16.
		//    - We have to pad with nulls at the end of the file to hit 
		//      a 16 byte boundary, and strip the decrypted nulls after.
		//
		// Secondary points:
		//	  - Don't worry about start, assume it's aligned (since we control
		//      evolution of the high water mark).
		//    - Push end out to 16 byte multiple.
		//    - For last chunk, decode using a small 16 byte temp buffer to avoid
		//      extraneous copies.

		jsize totalBufferLength = env->GetArrayLength(bytes);
		jsize targetEnd = offset + length;
		jsize neededGrowth = 16 - (targetEnd % 16);
		neededGrowth = (neededGrowth == 16) ? 0 : neededGrowth;

		bool convertFinalWithPadding = false;

		// Do we have to adjust anything?
		if(neededGrowth != 0)
		{
			if(targetEnd + neededGrowth <= totalBufferLength)
			{
				// Easy case - bump end out if we have room.
				length += neededGrowth;
			}
			else
			{
				// Slightly trickier case - round DOWN to nearest 16,
				// and set the "convert last bit" flag.
				length = (length + neededGrowth) - 16;
				convertFinalWithPadding = true;
			}
		}

		// Decrypt into a temporary buffer for now. We can rewrite the AES routine
		// to do this internally with much smaller buffers.
		void *outputBits = malloc(length);
		AesDecrypt(got->second, (unsigned char*)bytesPtr + offset, (unsigned char*)outputBits, length);

		// Copy back into place.
		memcpy(bytesPtr + offset, outputBits, length);

		// Do the final bit if needed.
		if(convertFinalWithPadding)
		{
			assert(totalBufferLength - (offset + length) < 16);

			LOGE("FINAL CASE %d %d", (int)(offset+length), (int)totalBufferLength);
			char tmpIn[16], tmpOut[16];
			int bufOffset = 0;
			
			// Null pad and copy last few bytes.
			memset(tmpIn, 0, 16);
			bufOffset=0;
			for(int i=offset+length; i<totalBufferLength; i++)
				tmpIn[bufOffset++] = bytesPtr[i];

			// Decrypt.
			AesDecrypt(got->second, (unsigned char*)tmpIn, (unsigned char*)tmpOut, 16);

			// Copy back out...
			bufOffset=0;
			for(int i=offset+length; i<totalBufferLength; i++)
				bytesPtr[i] = tmpIn[bufOffset++];

			// ... and update length.
			length += bufOffset;
			LOGE("FINAL CASE bufOffset = %d final = %d", (int)(bufOffset), (int)(length + offset));
		}

		// Clean up.
		free(outputBits);
		env->ReleaseByteArrayElements(bytes, bytesPtr, 0);

		// Is it meaningful to adjust the requested end point?
		return offset + length;
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_InitNativeDecoder(JNIEnv * env, jobject jcaller)
	{
		android_video_shim::initLibraries();
		
		JavaVM* jvm = NULL;
		env->GetJavaVM(&jvm);
		int jniVersion = env->GetVersion();

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
			gHLSPlayerSDK = new HLSPlayerSDK(jvm, jniVersion);
		}
		if (gHLSPlayerSDK && gHLSPlayerSDK->GetPlayer() == NULL) 
		{
			LOGI("Initializing decoder.");
			gHLSPlayerSDK->CreateDecoder();
		}
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_CloseNativeDecoder(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL)
		{
			LOGE("Closing decoder");
			gHLSPlayerSDK->Close(env);
			delete gHLSPlayerSDK;
			gHLSPlayerSDK = NULL;
		}
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_ResetPlayer(JNIEnv* env, jobject jcaller)
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Reset();
		}
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_StopPlayer(JNIEnv* env, jobject jcaller)
	{
		LOGI("Entered");
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Stop();
		}
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_PlayFile(JNIEnv* env, jobject jcaller, jdouble time)
	{
		if (gHLSPlayerSDK != NULL)
		{
			gHLSPlayerSDK->PlayFile(time);
		}
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_Pause(JNIEnv* env, jobject jcaller, jboolean pause)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->Pause(pause);
		}
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_SetSurface(JNIEnv* env, jobject jcaller, jobject surface)
	{
		//LOGI("Entered");
		if (gHLSPlayerSDK != NULL)
		{
			if (gHLSPlayerSDK->GetPlayer() == NULL) 
				gHLSPlayerSDK->CreateDecoder();

			LOGI("HLSPlayerSDK is not null");

			gHLSPlayerSDK->GetPlayer()->SetSurface(env, surface);

			LOGI("Done");
		}
	}

	jint Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_NextFrame(JNIEnv* env, jobject jcaller)
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

	jint Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_DroppedFramesPerSecond(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK == NULL)
			return 0;
		if (!gHLSPlayerSDK->GetPlayer())
			return 0;
		return gHLSPlayerSDK->GetPlayer()->DroppedFramesPerSecond();
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_FeedSegment(JNIEnv* env, jobject jcaller, jstring jurl, jint quality, jint continuityEra, jstring jaltAudioUrl, jint altAudioIndex, jdouble startTime, int cryptoId, int altCryptoId )
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
		if (jaltAudioUrl) // this is because GetStringUTFChars returns const char*, but dies if you pass it a NULL pointer. Why can't it just return NULL if you pass it NULL, huh?
		{
			const char* altAudioUrl = env->GetStringUTFChars(jaltAudioUrl, 0);
			gHLSPlayerSDK->GetPlayer()->FeedSegment(url, quality, continuityEra, altAudioUrl, altAudioIndex, startTime, cryptoId, altCryptoId);
			env->ReleaseStringUTFChars(jaltAudioUrl, altAudioUrl);
		}
		else
		{
			gHLSPlayerSDK->GetPlayer()->FeedSegment(url, quality, continuityEra, NULL, altAudioIndex, startTime, cryptoId, altCryptoId);
		}
		env->ReleaseStringUTFChars(jurl, url);
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_SeekTo(JNIEnv* env, jobject jcaller, jdouble time )
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

	jint Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_GetState(JNIEnv* env, jobject jcaller, jdouble time )
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			return gHLSPlayerSDK->GetPlayer()->GetState();
		}
		return STOPPED;
	}

	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_ApplyFormatChange(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->ApplyFormatChange();
		}
	}


	void Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_SetSegmentCountToBuffer(JNIEnv* env, jobject jcaller, jint segCount)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			gHLSPlayerSDK->GetPlayer()->SetSegmentCountToBuffer(segCount);
		}
	}

	jint Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_GetSegmentCountToBuffer(JNIEnv* env, jobject jcaller)
	{
		if (gHLSPlayerSDK != NULL && gHLSPlayerSDK->GetPlayer())
		{
			return gHLSPlayerSDK->GetPlayer()->GetSegmentCountToBuffer();
		}
		return 0;
	}

	jboolean Java_com_kaltura_hlsplayersdk_HLSPlayerViewController_AllowAllProfiles(JNIEnv* env, jobject jcaller )
	{
#ifdef ALLOW_ALL_PROFILES
		return true;
#else
		return false;
#endif
	}


}

HLSPlayerSDK::HLSPlayerSDK(JavaVM* jvm, int jniVersion) : mPlayer(NULL), mJvm(jvm), mJniVersion(jniVersion)
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
	mJvm = NULL;

}

bool HLSPlayerSDK::CreateDecoder()
{
	if (!mPlayer)
	{
		mPlayer = new HLSPlayer(mJvm);
	}

	return mPlayer != NULL;
}

JavaVM* HLSPlayerSDK::getJVM()
{
	return mJvm;
}

int HLSPlayerSDK::getJVMVersion()
{
	return mJniVersion;
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

void HLSPlayerSDK::PlayFile(double time)
{
	LOGI("::PlayFile()");
	if (!mPlayer && !CreateDecoder()) 
	{
		LOGE("Failed to initialize player.");
		return;
	}

	if (mPlayer->Play(time))
	{
//	Not sure what the point of this was... commenting out for testing purposes
//		while (mPlayer->Update() == 0)
//		{
//			LOGI("Decoded a frame!");
//		}
	}
}

bool HLSPlayerSDK::GetEnv(JNIEnv** env)
{
	if (!mJvm) return false;
	int  rval = mJvm->GetEnv((void**)env, mJniVersion);
	if (rval == JNI_EDETACHED)
	{
		rval = mJvm->AttachCurrentThread(env, NULL);
		if (rval != 0)
		{
			LOGE("Could not get the java environment (env)");
			(*env) = NULL;
			return false;
		}
	}
	return true;
}



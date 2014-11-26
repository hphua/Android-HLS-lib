/*
 * HLSPlayerSDK.h
 *
 *  Created on: Apr 2, 2014
 *      Author: Mark
 */

#ifndef HLSPLAYERSDK_H_
#define HLSPLAYERSDK_H_

//class HLSDecoder;
class HLSPlayer;

#define CLASS_NAME "HLSPlayerSDK"

class HLSPlayerSDK
{
public:
	HLSPlayerSDK(JavaVM* jvm, int jniVersion);
	~HLSPlayerSDK();

	void Close(JNIEnv* env);

	bool CreateDecoder();
	void PlayFile(double time);

	//HLSDecoder* GetDecoder();
	HLSPlayer* GetPlayer();

	JavaVM* getJVM();
	int getJVMVersion();

	bool GetEnv(JNIEnv** env);

private:

	JavaVM* mJvm;
	int mJniVersion;

	HLSPlayer* mPlayer;

};



#endif /* HLSPLAYERSDK_H_ */

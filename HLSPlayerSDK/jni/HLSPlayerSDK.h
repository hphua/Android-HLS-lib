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
	HLSPlayerSDK(JavaVM* jvm);
	~HLSPlayerSDK();

	void Close(JNIEnv* env);

	bool CreateDecoder();
	void PlayFile();

	//HLSDecoder* GetDecoder();
	HLSPlayer* GetPlayer();

private:

	JavaVM* mJvm;

	HLSPlayer* mPlayer;

};



#endif /* HLSPLAYERSDK_H_ */

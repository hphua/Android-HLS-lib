/*
 * constants.h
 *
 *  Created on: Apr 14, 2014
 *      Author: Mark
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#define APP_NAME "HLSPlayerSDK"

enum Playstate
{
	INITIALIZED = 0,
	STOPPED,
	PAUSED,
	PLAYING,
	SEEKING,
	FORMAT_CHANGING,
	FOUND_DISCONTINUITY
};


#endif /* CONSTANTS_H_ */

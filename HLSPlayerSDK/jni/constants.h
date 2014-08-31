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
	STOPPED = 1,
	PAUSED,
	PLAYING,
	SEEKING,
	FORMAT_CHANGING,
	FOUND_DISCONTINUITY
};


#endif /* CONSTANTS_H_ */

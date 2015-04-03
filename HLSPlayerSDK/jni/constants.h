/*
 * constants.h
 *
 *  Created on: Apr 14, 2014
 *      Author: Mark
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#define APP_NAME "HLSPlayerSDK"
#define USE_OMX_AUDIO false
#define NANOSEC_PER_MS 1000000

enum ErrorCode
{
	MEDIA_ERROR_UNKNOWN = -100,

    /**
     * Invalid media was given
     */
	MEDIA_ERROR_NOT_VALID = -101,

	/** File or network related operation errors. */
	MEDIA_ERROR_IO = -102,

	/** Bitstream is not conforming to the related coding standard or file spec. */
	MEDIA_ERROR_MALFORMED = -103,

	/** Bitstream is conforming to the related coding standard or file spec, but
	 * the media framework does not support the feature. */
	MEDIA_ERROR_UNSUPPORTED = -104,

	/** Some operation takes too long to complete, usually more than 3-5 seconds. */
	MEDIA_ERROR_TIMED_OUT = -105,

	/** profile is incompatible to hardware */
	MEDIA_INCOMPATIBLE_PROFILE = -106

};

enum Playstate
{
	INITIALIZED = 0,
	STOPPED,
	PAUSED,
	PLAYING,
	SEEKING,
	FORMAT_CHANGING,
	FOUND_DISCONTINUITY,
	WAITING_ON_DATA,
	CUE_STOP
};


#endif /* CONSTANTS_H_ */

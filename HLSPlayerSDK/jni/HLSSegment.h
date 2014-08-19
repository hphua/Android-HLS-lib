/*
 * HLSSegment.h
 *
 *  Created on: Apr 29, 2014
 *      Author: Mark
 */

#ifndef HLSSEGMENT_H_
#define HLSSEGMENT_H_

class HLSSegment
{
public:
	HLSSegment(int32_t quality, double time);
	~HLSSegment();

	int32_t GetWidth();
	int32_t GetHeight();
	int32_t GetQuality();
	double GetStartTime();


private:

	int32_t mQuality;
	double mStartTime;

};



#endif /* HLSSEGMENT_H_ */

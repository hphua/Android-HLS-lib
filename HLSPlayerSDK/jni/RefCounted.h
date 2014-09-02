/*
 * RefCounted.h
 *
 *  Created on: Sep 2, 2014
 *      Author: Mark
 */

#ifndef REFCOUNTED_H_
#define REFCOUNTED_H_

#include <pthread.h>

class RefCounted {
public:
	RefCounted();
	virtual ~RefCounted();

	int addRef();
	int release();
	int refCount();

	virtual void unload() = 0;

private:
	int mRefCount;

	pthread_mutex_t lock;
};

#endif /* REFCOUNTED_H_ */

/*
 * RefCounted.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: Mark
 */

#include <androidVideoShim.h>
#include <RefCounted.h>

RefCounted::RefCounted() : mRefCount(0) {

	int err = initRecursivePthreadMutex(&lock);
	LOGI(" RefCounted mutex err = %d", err);

	++mRefCount;

}

RefCounted::~RefCounted() {

}

int RefCounted::addRef()
{
	AutoLock locker(&lock);
	return (++mRefCount);
}

int RefCounted::release()
{
	AutoLock locker(&lock);
	int refCount = (--mRefCount);
	if (mRefCount == 0)
	{
		unload();
	}
	return refCount;

}

int RefCounted::refCount()
{
	return mRefCount;
}

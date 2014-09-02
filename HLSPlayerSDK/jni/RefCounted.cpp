/*
 * RefCounted.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: Mark
 */

#include <RefCounted.h>

RefCounted::RefCounted() : mRefCount(0) {
	++mRefCount;

}

RefCounted::~RefCounted() {

}

int RefCounted::addRef()
{
	return (++mRefCount);
}

int RefCounted::release()
{
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

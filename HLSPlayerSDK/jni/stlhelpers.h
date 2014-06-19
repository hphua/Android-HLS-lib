/*
 * stlhelpers.h
 *
 *  Created on: Jun 18, 2014
 *      Author: Mark
 */

#ifndef STLHELPERS_H_
#define STLHELPERS_H_
#include <stddef.h>

    template<class T>
    void stlwipe(T &t)
    {
        //get the first iterator
        typename T::iterator i=t.begin();
        typename T::iterator e=t.end();

        //iterate to the end and delete items (should be pointers)
        for(; i!=e;++i)
        {
            if (*i) delete(*i);
            (*i) = NULL;
        }

        //clear the collection (now full with dead pointers)
        t.clear();
    }



#endif /* STLHELPERS_H_ */

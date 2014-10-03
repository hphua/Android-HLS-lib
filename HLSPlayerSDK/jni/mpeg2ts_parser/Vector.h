/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_VECTOR_H
#define ANDROID_VECTOR_H

#include <new>
#include <stdint.h>
#include <sys/types.h>

#include <android/log.h>
//#include <utils/VectorImpl.h>
//#include <utils/TypeHelpers.h>

// ---------------------------------------------------------------------------

namespace android {

/*
 * Types traits
 */
    
template <typename T> struct trait_trivial_ctor  { enum { value = false }; };
template <typename T> struct trait_trivial_dtor  { enum { value = false }; };
template <typename T> struct trait_trivial_copy  { enum { value = false }; };
template <typename T> struct trait_trivial_assign{ enum { value = false }; };

template <typename T> struct trait_pointer     { enum { value = false }; };    
template <typename T> struct trait_pointer<T*> { enum { value = true }; };

#define ANDROID_BASIC_TYPES_TRAITS( T )                                       \
    template<> struct trait_trivial_ctor< T >  { enum { value = true }; };    \
    template<> struct trait_trivial_dtor< T >  { enum { value = true }; };    \
    template<> struct trait_trivial_copy< T >  { enum { value = true }; };    \
    template<> struct trait_trivial_assign< T >{ enum { value = true }; }; 

#define ANDROID_TYPE_TRAITS( T, ctor, dtor, copy, assign )                    \
    template<> struct trait_trivial_ctor< T >  { enum { value = ctor }; };    \
    template<> struct trait_trivial_dtor< T >  { enum { value = dtor }; };    \
    template<> struct trait_trivial_copy< T >  { enum { value = copy }; };    \
    template<> struct trait_trivial_assign< T >{ enum { value = assign }; }; 

template <typename TYPE>
struct traits {
    enum {
        is_pointer          = trait_pointer<TYPE>::value,
        has_trivial_ctor    = is_pointer || trait_trivial_ctor<TYPE>::value,
        has_trivial_dtor    = is_pointer || trait_trivial_dtor<TYPE>::value,
        has_trivial_copy    = is_pointer || trait_trivial_copy<TYPE>::value,
        has_trivial_assign  = is_pointer || trait_trivial_assign<TYPE>::value   
    };
};

template <typename T, typename U>
struct aggregate_traits {
    enum {
        is_pointer          = false,
        has_trivial_ctor    = traits<T>::has_trivial_ctor && traits<U>::has_trivial_ctor,
        has_trivial_dtor    = traits<T>::has_trivial_dtor && traits<U>::has_trivial_dtor,
        has_trivial_copy    = traits<T>::has_trivial_copy && traits<U>::has_trivial_copy,
        has_trivial_assign  = traits<T>::has_trivial_assign && traits<U>::has_trivial_assign
    };
};

// ---------------------------------------------------------------------------

/*
 * basic types traits
 */
 
ANDROID_BASIC_TYPES_TRAITS( void );
ANDROID_BASIC_TYPES_TRAITS( bool );
ANDROID_BASIC_TYPES_TRAITS( char );
ANDROID_BASIC_TYPES_TRAITS( unsigned char );
ANDROID_BASIC_TYPES_TRAITS( short );
ANDROID_BASIC_TYPES_TRAITS( unsigned short );
ANDROID_BASIC_TYPES_TRAITS( int );
ANDROID_BASIC_TYPES_TRAITS( unsigned int );
ANDROID_BASIC_TYPES_TRAITS( long );
ANDROID_BASIC_TYPES_TRAITS( unsigned long );
ANDROID_BASIC_TYPES_TRAITS( long long );
ANDROID_BASIC_TYPES_TRAITS( unsigned long long );
ANDROID_BASIC_TYPES_TRAITS( float );
ANDROID_BASIC_TYPES_TRAITS( double );

// ---------------------------------------------------------------------------

    
/*
 * compare and order types
 */

template<typename TYPE> inline
int strictly_order_type(const TYPE& lhs, const TYPE& rhs) {
    return (lhs < rhs) ? 1 : 0;
}

template<typename TYPE> inline
int compare_type(const TYPE& lhs, const TYPE& rhs) {
    return strictly_order_type(rhs, lhs) - strictly_order_type(lhs, rhs);
}

/*
 * create, destroy, copy and assign types...
 */
 
template<typename TYPE> inline
void construct_type(TYPE* p, size_t n) {
    if (!traits<TYPE>::has_trivial_ctor) {
        while (n--) {
            new(p++) TYPE;
        }
    }
}

template<typename TYPE> inline
void destroy_type(TYPE* p, size_t n) {
    if (!traits<TYPE>::has_trivial_dtor) {
        while (n--) {
            p->~TYPE();
            p++;
        }
    }
}

template<typename TYPE> inline
void copy_type(TYPE* d, const TYPE* s, size_t n) {
    if (!traits<TYPE>::has_trivial_copy) {
        while (n--) {
            new(d) TYPE(*s);
            d++, s++;
        }
    } else {
        memcpy(d,s,n*sizeof(TYPE));
    }
}

template<typename TYPE> inline
void assign_type(TYPE* d, const TYPE* s, size_t n) {
    if (!traits<TYPE>::has_trivial_assign) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        memcpy(d,s,n*sizeof(TYPE));
    }
}

template<typename TYPE> inline
void splat_type(TYPE* where, const TYPE* what, size_t n) {
    if (!traits<TYPE>::has_trivial_copy) {
        while (n--) {
            new(where) TYPE(*what);
            where++;
        }
    } else {
         while (n--) {
             *where++ = *what;
        }
    }
}

template<typename TYPE> inline
void move_forward_type(TYPE* d, const TYPE* s, size_t n = 1) {
    if (!traits<TYPE>::has_trivial_copy || !traits<TYPE>::has_trivial_dtor) {
        d += n;
        s += n;
        while (n--) {
            --d, --s;
            if (!traits<TYPE>::has_trivial_copy) {
                new(d) TYPE(*s);
            } else {
                *d = *s;
            }
            if (!traits<TYPE>::has_trivial_dtor) {
                s->~TYPE();
            }
        }
    } else {
        memmove(d,s,n*sizeof(TYPE));
    }
}

template<typename TYPE> inline
void move_backward_type(TYPE* d, const TYPE* s, size_t n = 1) {
    if (!traits<TYPE>::has_trivial_copy || !traits<TYPE>::has_trivial_dtor) {
        while (n--) {
            if (!traits<TYPE>::has_trivial_copy) {
                new(d) TYPE(*s);
            } else {
                *d = *s;
            }
            if (!traits<TYPE>::has_trivial_dtor) {
                s->~TYPE();
            }
            d++, s++;
        }
    } else {
        memmove(d,s,n*sizeof(TYPE));
    }
}
// ---------------------------------------------------------------------------

/*
 * a key/value pair
 */

template <typename KEY, typename VALUE>
struct key_value_pair_t {
    KEY     key;
    VALUE   value;
    key_value_pair_t() { }
    key_value_pair_t(const key_value_pair_t& o) : key(o.key), value(o.value) { }
    key_value_pair_t(const KEY& k, const VALUE& v) : key(k), value(v)  { }
    key_value_pair_t(const KEY& k) : key(k) { }
    inline bool operator < (const key_value_pair_t& o) const {
        return strictly_order_type(key, o.key);
    }
};

template<>
template <typename K, typename V>
struct trait_trivial_ctor< key_value_pair_t<K, V> >
{ enum { value = aggregate_traits<K,V>::has_trivial_ctor }; };
template<> 
template <typename K, typename V>
struct trait_trivial_dtor< key_value_pair_t<K, V> >
{ enum { value = aggregate_traits<K,V>::has_trivial_dtor }; };
template<> 
template <typename K, typename V>
struct trait_trivial_copy< key_value_pair_t<K, V> >
{ enum { value = aggregate_traits<K,V>::has_trivial_copy }; };
template<> 
template <typename K, typename V>
struct trait_trivial_assign< key_value_pair_t<K, V> >
{ enum { value = aggregate_traits<K,V>::has_trivial_assign};};


/*!
 * Implementation of the guts of the vector<> class
 * this ensures backward binary compatibility and
 * reduces code size.
 * For performance reasons, we expose mStorage and mCount
 * so these fields are set in stone.
 *
 */

class VectorImpl
{
public:
    enum { // flags passed to the ctor
        HAS_TRIVIAL_CTOR    = 0x00000001,
        HAS_TRIVIAL_DTOR    = 0x00000002,
        HAS_TRIVIAL_COPY    = 0x00000004,
        HAS_TRIVIAL_ASSIGN  = 0x00000008
    };

                            VectorImpl(size_t itemSize, uint32_t flags);
                            VectorImpl(const VectorImpl& rhs);
    virtual                 ~VectorImpl();

    /*! must be called from subclasses destructor */
            void            finish_vector();

            VectorImpl&     operator = (const VectorImpl& rhs);    
            
    /*! C-style array access */
    inline  const void*     arrayImpl() const       { return mStorage; }
            void*           editArrayImpl();
            
    /*! vector stats */
    inline  size_t          size() const        { return mCount; }
    inline  bool            isEmpty() const     { return mCount == 0; }
            size_t          capacity() const;
            ssize_t         setCapacity(size_t size);

            /*! append/insert another vector */
            ssize_t         insertVectorAt(const VectorImpl& vector, size_t index);
            ssize_t         appendVector(const VectorImpl& vector);
            ssize_t         insertArrayAt(const void* array, size_t index, size_t length);
            ssize_t         appendArray(const void* array, size_t length);

            /*! add/insert/replace items */
            ssize_t         insertAt(size_t where, size_t numItems = 1);
            ssize_t         insertAt(const void* item, size_t where, size_t numItems = 1);
            void            pop();
            void            push();
            void            push(const void* item);
            ssize_t         add();
            ssize_t         add(const void* item);
            ssize_t         replaceAt(size_t index);
            ssize_t         replaceAt(const void* item, size_t index);

            /*! remove items */
            ssize_t         removeItemsAt(size_t index, size_t count = 1);
            void            clear();

            const void*     itemLocation(size_t index) const;
            void*           editItemLocation(size_t index);

            typedef int (*compar_t)(const void* lhs, const void* rhs);
            typedef int (*compar_r_t)(const void* lhs, const void* rhs, void* state);
            status_t        sort(compar_t cmp);
            status_t        sort(compar_r_t cmp, void* state);
            ssize_t resize(size_t size);
            
protected:
            size_t          itemSize() const;
            void            release_storage();

    virtual void            do_construct(void* storage, size_t num) const = 0;
    virtual void            do_destroy(void* storage, size_t num) const = 0;
    virtual void            do_copy(void* dest, const void* from, size_t num) const = 0;
    virtual void            do_splat(void* dest, const void* item, size_t num) const = 0;
    virtual void            do_move_forward(void* dest, const void* from, size_t num) const = 0;
    virtual void            do_move_backward(void* dest, const void* from, size_t num) const = 0;

    // take care of FBC...
    virtual void            reservedVectorImpl1();
    virtual void            reservedVectorImpl2();
    virtual void            reservedVectorImpl3();
    virtual void            reservedVectorImpl4();
    virtual void            reservedVectorImpl5();
    virtual void            reservedVectorImpl6();
    virtual void            reservedVectorImpl7();
    virtual void            reservedVectorImpl8();
    
private:
        void* _grow(size_t where, size_t amount);
        void  _shrink(size_t where, size_t amount);

        inline void _do_construct(void* storage, size_t num) const;
        inline void _do_destroy(void* storage, size_t num) const;
        inline void _do_copy(void* dest, const void* from, size_t num) const;
        inline void _do_splat(void* dest, const void* item, size_t num) const;
        inline void _do_move_forward(void* dest, const void* from, size_t num) const;
        inline void _do_move_backward(void* dest, const void* from, size_t num) const;

            // These 2 fields are exposed in the inlines below,
            // so they're set in stone.
            void *      mStorage;   // base address of the vector
            size_t      mCount;     // number of items

    const   uint32_t    mFlags;
    const   size_t      mItemSize;
};



class SortedVectorImpl : public VectorImpl
{
public:
                            SortedVectorImpl(size_t itemSize, uint32_t flags);
                            SortedVectorImpl(const VectorImpl& rhs);
    virtual                 ~SortedVectorImpl();
    
    SortedVectorImpl&     operator = (const SortedVectorImpl& rhs);    

    //! finds the index of an item
            ssize_t         indexOf(const void* item) const;

    //! finds where this item should be inserted
            size_t          orderOf(const void* item) const;

    //! add an item in the right place (or replaces it if there is one)
            ssize_t         add(const void* item);

    //! merges a vector into this one
            ssize_t         merge(const VectorImpl& vector);
            ssize_t         merge(const SortedVectorImpl& vector);
             
    //! removes an item
            ssize_t         remove(const void* item);
        
protected:
    virtual int             do_compare(const void* lhs, const void* rhs) const = 0;

    // take care of FBC...
    virtual void            reservedSortedVectorImpl1();
    virtual void            reservedSortedVectorImpl2();
    virtual void            reservedSortedVectorImpl3();
    virtual void            reservedSortedVectorImpl4();
    virtual void            reservedSortedVectorImpl5();
    virtual void            reservedSortedVectorImpl6();
    virtual void            reservedSortedVectorImpl7();
    virtual void            reservedSortedVectorImpl8();

private:
            ssize_t         _indexOrderOf(const void* item, size_t* order = 0) const;

            // these are made private, because they can't be used on a SortedVector
            // (they don't have an implementation either)
            ssize_t         add();
            void            pop();
            void            push();
            void            push(const void* item);
            ssize_t         insertVectorAt(const VectorImpl& vector, size_t index);
            ssize_t         appendVector(const VectorImpl& vector);
            ssize_t         insertAt(size_t where, size_t numItems = 1);
            ssize_t         insertAt(const void* item, size_t where, size_t numItems = 1);
            ssize_t         replaceAt(size_t index);
            ssize_t         replaceAt(const void* item, size_t index);
};

/*!
 * The main templated vector class ensuring type safety
 * while making use of VectorImpl.
 * This is the class users want to use.
 */

template <class TYPE>
class Vector : private VectorImpl
{
public:
            typedef TYPE    value_type;
    
    /*! 
     * Constructors and destructors
     */
    
                            Vector();
                            Vector(const Vector<TYPE>& rhs);
    virtual                 ~Vector();

    /*! copy operator */
            const Vector<TYPE>&     operator = (const Vector<TYPE>& rhs) const;
            Vector<TYPE>&           operator = (const Vector<TYPE>& rhs);    

    /*
     * empty the vector
     */

    inline  void            clear()             { VectorImpl::clear(); }

    /*! 
     * vector stats
     */

    //! returns number of items in the vector
    inline  size_t          size() const                { return VectorImpl::size(); }
    //! returns wether or not the vector is empty
    inline  bool            isEmpty() const             { return VectorImpl::isEmpty(); }
    //! returns how many items can be stored without reallocating the backing store
    inline  size_t          capacity() const            { return VectorImpl::capacity(); }
    //! setst the capacity. capacity can never be reduced less than size()
    inline  ssize_t         setCapacity(size_t size)    { return VectorImpl::setCapacity(size); }

    /*! 
     * C-style array access
     */
     
    //! read-only C-style access 
    inline  const TYPE*     array() const;
    //! read-write C-style access
            TYPE*           editArray();
    
    /*! 
     * accessors
     */

    //! read-only access to an item at a given index
    inline  const TYPE&     operator [] (size_t index) const;
    //! alternate name for operator []
    inline  const TYPE&     itemAt(size_t index) const;
    //! stack-usage of the vector. returns the top of the stack (last element)
            const TYPE&     top() const;
    //! same as operator [], but allows to access the vector backward (from the end) with a negative index
            const TYPE&     mirrorItemAt(ssize_t index) const;

    /*!
     * modifing the array
     */

    //! copy-on write support, grants write access to an item
            TYPE&           editItemAt(size_t index);
    //! grants right acces to the top of the stack (last element)
            TYPE&           editTop();

            /*! 
             * append/insert another vector
             */
            
    //! insert another vector at a given index
            ssize_t         insertVectorAt(const Vector<TYPE>& vector, size_t index);

    //! append another vector at the end of this one
            ssize_t         appendVector(const Vector<TYPE>& vector);


            /*! 
             * add/insert/replace items
             */
             
    //! insert one or several items initialized with their default constructor
    inline  ssize_t         insertAt(size_t index, size_t numItems = 1);
    //! insert on onr several items initialized from a prototype item
            ssize_t         insertAt(const TYPE& prototype_item, size_t index, size_t numItems = 1);
    //! pop the top of the stack (removes the last element). No-op if the stack's empty
    inline  void            pop();
    //! pushes an item initialized with its default constructor
    inline  void            push();
    //! pushes an item on the top of the stack
            void            push(const TYPE& item);
    //! same as push() but returns the index the item was added at (or an error)
    inline  ssize_t         add();
    //! same as push() but returns the index the item was added at (or an error)
            ssize_t         add(const TYPE& item);            
    //! replace an item with a new one initialized with its default constructor
    inline  ssize_t         replaceAt(size_t index);
    //! replace an item with a new one
            ssize_t         replaceAt(const TYPE& item, size_t index);

    /*!
     * remove items
     */

    //! remove several items
    inline  ssize_t         removeItemsAt(size_t index, size_t count = 1);
    //! remove one item
    inline  ssize_t         removeAt(size_t index)  { return removeItemsAt(index); }

    /*!
     * sort (stable) the array
     */
     
     typedef int (*compar_t)(const TYPE* lhs, const TYPE* rhs);
     typedef int (*compar_r_t)(const TYPE* lhs, const TYPE* rhs, void* state);
     
     inline status_t        sort(compar_t cmp);
     inline status_t        sort(compar_r_t cmp, void* state);

inline bool empty() const{ return isEmpty(); }

protected:
    virtual void    do_construct(void* storage, size_t num) const;
    virtual void    do_destroy(void* storage, size_t num) const;
    virtual void    do_copy(void* dest, const void* from, size_t num) const;
    virtual void    do_splat(void* dest, const void* item, size_t num) const;
    virtual void    do_move_forward(void* dest, const void* from, size_t num) const;
    virtual void    do_move_backward(void* dest, const void* from, size_t num) const;
};


// ---------------------------------------------------------------------------
// No user serviceable parts from here...
// ---------------------------------------------------------------------------

template<class TYPE> inline
Vector<TYPE>::Vector()
    : VectorImpl(sizeof(TYPE),
                ((traits<TYPE>::has_trivial_ctor   ? HAS_TRIVIAL_CTOR   : 0)
                |(traits<TYPE>::has_trivial_dtor   ? HAS_TRIVIAL_DTOR   : 0)
                |(traits<TYPE>::has_trivial_copy   ? HAS_TRIVIAL_COPY   : 0)
                |(traits<TYPE>::has_trivial_assign ? HAS_TRIVIAL_ASSIGN : 0))
                )
{
}

template<class TYPE> inline
Vector<TYPE>::Vector(const Vector<TYPE>& rhs)
    : VectorImpl(rhs) {
}

template<class TYPE> inline
Vector<TYPE>::~Vector() {
    finish_vector();
}

template<class TYPE> inline
Vector<TYPE>& Vector<TYPE>::operator = (const Vector<TYPE>& rhs) {
    VectorImpl::operator = (rhs);
    return *this; 
}

template<class TYPE> inline
const Vector<TYPE>& Vector<TYPE>::operator = (const Vector<TYPE>& rhs) const {
    VectorImpl::operator = (rhs);
    return *this; 
}

template<class TYPE> inline
const TYPE* Vector<TYPE>::array() const {
    return static_cast<const TYPE *>(arrayImpl());
}

template<class TYPE> inline
TYPE* Vector<TYPE>::editArray() {
    return static_cast<TYPE *>(editArrayImpl());
}


template<class TYPE> inline
const TYPE& Vector<TYPE>::operator[](size_t index) const {
    LOG_FATAL_IF( index>=size(),
                  "itemAt: index %d is past size %d", (int)index, (int)size() );
    return *(array() + index);
}

template<class TYPE> inline
const TYPE& Vector<TYPE>::itemAt(size_t index) const {
    return operator[](index);
}

template<class TYPE> inline
const TYPE& Vector<TYPE>::mirrorItemAt(ssize_t index) const {
    LOG_FATAL_IF( (index>0 ? index : -index)>=size(),
                  "mirrorItemAt: index %d is past size %d",
                  (int)index, (int)size() );
    return *(array() + ((index<0) ? (size()-index) : index));
}

template<class TYPE> inline
const TYPE& Vector<TYPE>::top() const {
    return *(array() + size() - 1);
}

template<class TYPE> inline
TYPE& Vector<TYPE>::editItemAt(size_t index) {
    return *( static_cast<TYPE *>(editItemLocation(index)) );
}

template<class TYPE> inline
TYPE& Vector<TYPE>::editTop() {
    return *( static_cast<TYPE *>(editItemLocation(size()-1)) );
}

template<class TYPE> inline
ssize_t Vector<TYPE>::insertVectorAt(const Vector<TYPE>& vector, size_t index) {
    return VectorImpl::insertVectorAt(reinterpret_cast<const VectorImpl&>(vector), index);
}

template<class TYPE> inline
ssize_t Vector<TYPE>::appendVector(const Vector<TYPE>& vector) {
    return VectorImpl::appendVector(reinterpret_cast<const VectorImpl&>(vector));
}

template<class TYPE> inline
ssize_t Vector<TYPE>::insertAt(const TYPE& item, size_t index, size_t numItems) {
    return VectorImpl::insertAt(&item, index, numItems);
}

template<class TYPE> inline
void Vector<TYPE>::push(const TYPE& item) {
    return VectorImpl::push(&item);
}

template<class TYPE> inline
ssize_t Vector<TYPE>::add(const TYPE& item) {
    return VectorImpl::add(&item);
}

template<class TYPE> inline
ssize_t Vector<TYPE>::replaceAt(const TYPE& item, size_t index) {
    return VectorImpl::replaceAt(&item, index);
}

template<class TYPE> inline
ssize_t Vector<TYPE>::insertAt(size_t index, size_t numItems) {
    return VectorImpl::insertAt(index, numItems);
}

template<class TYPE> inline
void Vector<TYPE>::pop() {
    VectorImpl::pop();
}

template<class TYPE> inline
void Vector<TYPE>::push() {
    VectorImpl::push();
}

template<class TYPE> inline
ssize_t Vector<TYPE>::add() {
    return VectorImpl::add();
}

template<class TYPE> inline
ssize_t Vector<TYPE>::replaceAt(size_t index) {
    return VectorImpl::replaceAt(index);
}

template<class TYPE> inline
ssize_t Vector<TYPE>::removeItemsAt(size_t index, size_t count) {
    return VectorImpl::removeItemsAt(index, count);
}

template<class TYPE> inline
status_t Vector<TYPE>::sort(Vector<TYPE>::compar_t cmp) {
    return VectorImpl::sort((VectorImpl::compar_t)cmp);
}

template<class TYPE> inline
status_t Vector<TYPE>::sort(Vector<TYPE>::compar_r_t cmp, void* state) {
    return VectorImpl::sort((VectorImpl::compar_r_t)cmp, state);
}

// ---------------------------------------------------------------------------

template<class TYPE>
void Vector<TYPE>::do_construct(void* storage, size_t num) const {
    construct_type( reinterpret_cast<TYPE*>(storage), num );
}

template<class TYPE>
void Vector<TYPE>::do_destroy(void* storage, size_t num) const {
    destroy_type( reinterpret_cast<TYPE*>(storage), num );
}

template<class TYPE>
void Vector<TYPE>::do_copy(void* dest, const void* from, size_t num) const {
    copy_type( reinterpret_cast<TYPE*>(dest), reinterpret_cast<const TYPE*>(from), num );
}

template<class TYPE>
void Vector<TYPE>::do_splat(void* dest, const void* item, size_t num) const {
    splat_type( reinterpret_cast<TYPE*>(dest), reinterpret_cast<const TYPE*>(item), num );
}

template<class TYPE>
void Vector<TYPE>::do_move_forward(void* dest, const void* from, size_t num) const {
    move_forward_type( reinterpret_cast<TYPE*>(dest), reinterpret_cast<const TYPE*>(from), num );
}

template<class TYPE>
void Vector<TYPE>::do_move_backward(void* dest, const void* from, size_t num) const {
    move_backward_type( reinterpret_cast<TYPE*>(dest), reinterpret_cast<const TYPE*>(from), num );
}

}; // namespace android


// ---------------------------------------------------------------------------

#endif // ANDROID_VECTOR_H
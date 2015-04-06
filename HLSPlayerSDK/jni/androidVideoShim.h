#ifndef _ANDROIDVIDEOSHIM_H_
#define _ANDROIDVIDEOSHIM_H_

#include <jni.h>
#include <errno.h>

// This is to get a load of type defines, there's probably a better include for
// this purpose.
#include <sys/types.h>
#include <stdlib.h>

#include <assert.h>
#include <typeinfo>
#include <dlfcn.h>

#include <android/log.h>

#include <vector>

#include <pthread.h>

#include "debug.h"

#include "HLSSegmentCache.h"

// Handy pthreads autolocker.
class AutoLock
{
public:
    AutoLock(pthread_mutex_t * lock, const char* path="")
    : lock(lock), mPath(path)
    {
        LOGTHREAD("Locking mutex %p, %s", lock, path);
        pthread_mutex_lock(lock);
    }

    ~AutoLock()
    {
        LOGTHREAD("Unlocking mutex %p, %s", lock, mPath);
        pthread_mutex_unlock(lock);
    }

private:
    pthread_mutex_t * lock;
    const char* mPath;
};

inline int initRecursivePthreadMutex(pthread_mutex_t *lock)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    return pthread_mutex_init(lock, &attr);
}


/******************************************************************************

    Android Video Compatibility Shim

    This module allows use of private Android libstagefright APIs across
    multiple versions of Android. It provides the following key features:

       1. No private Android libs or headers are required to build your app.

       2. Compatibility with multiple versions of the library with different
          symbol names. VTables can be respected and API variations handled.

       3. Subclassing is supported.

    Of course, the downside is manually performing a lot of work that GCC does
    for you. This means understanding the Itanium C++ ABI (which has been
    adopted by ARM). Further reading: http://mentorembedded.github.io/cxx-abi/abi.html
    especially section 2.5.

    That said, here are the highlights for the benefit of those who need to
    maintain or extend this compatibility shim:

    First off, class/function NAMES, ORDERS, and ATTRIBUTES don't matter. But
    the class HIERARCHY does. A key element to properly passing pointers to
    C++ objects is the structure and order of base classes and especially any
    virtual bases. Basically this means that if Android has

        class A : public virtual B, public C

    you need to match the structure but not the names, ie, this is fine:

        class Q : public virtual D, public F1

    If you are trying to call methods in vtables and the data in the
    vtable is screwy, or you are looking up members and can't find them,
    this is the first thing to check.

    Second, ORDER OF MEMBER VARIABLES matters. Methods/funcs/attributes don't
    affect the layout, but members in base classes do. So if you need to access
    a member variable you have to have all the member variables in all the base
    classes right in order and size up to the definition of the member you want
    to read.

    Third, the VTABLE is an array of function pointers pointed to by the first
    4 bytes of a C++ object with virtual methods. There can sometimes be gaps/
    non-function-pointer data in the VTABLE but it's always accessed with this
    idiom which you will find throughout the following code:

            const int vtableOffset = 6;
            typedef status_t (*localFuncCast)(void *thiz);
            localFuncCast **fakeObj = (localFuncCast**)this;
            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];

    Note that so far we have been writing non-virtual shim functions to do the
    vtable dispatch. This allows best safety/compatibility and logging. Often,
    the expected symbol is looked up and dumped to help confirm that we're
    doing the vtable lookup properly. It is in theory possible to match the
    vtable structure, but it means we can't tolerate it if the vtable structure
    changes between Android versions, so it's probably better to stick with the
    shim approach. *If you see virtual funcs, it's best not to call them.*

    Fourth, note that C++ member functions have an invisible "this" pointer
    passed as the first argument. So if you want to construct the function
    pointer, you will need to add a "void *thiz" to the start and pass the
    pointer to the object yourself. (Note that "void *this" is ambiguous/illegal
    as this is a keyword. Also note that if you have your class hierarchy set
    up properly esp. w.r.t. virtual bases then multiple inheritance will Just
    Work.)

    Note that ARM calling conventions have a break when the arg count goes over
    4, so if you encounter issues there you may have to do something special. So
    far so good, though, so don't assume that's the problem. See section 5 of
    http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042e/IHI0042E_aapcs.pdf
    for more information.

    Fifth, the Android crash report log output is your friend. It helpfully shows
    you register r0-r3 which contain the args to the broken function call (if
    there are 4 or less args), and also dumps any memory near any registers. So
    you can tell what was being called (pc has this) and with what args if you
    are experiencing crashes.

    Sixth, you can use nm or strings to dump all the symbol names from an
    Android library file. So you can grab libstagefright.so from /system/lib
    and dump all its symbol names to see why you can't find something. Often
    it's a small change in an API which changes the symbol name resulting in
    failure to find it; then you just have to run c++filt (also in the compiler
    bin folder) on the symbol to find out what its type signature is. Note that
    return types aren't included so you have to figure that out yourself, but
    the Android source code usually makes it easy to find it.

******************************************************************************/
namespace android_video_shim
{
    // API to initialize dynamic libraries and search them.
    void initLibraries();
    void *searchSymbol(const char *symName);

    // API to switch logic/vtables by OS version.
    extern int gAPILevel;
    #define AVSHIM_USE_NEWMEDIASOURCE (android_video_shim::gAPILevel >= 14)
    #define AVSHIM_USE_NEWMEDIASOURCEVTABLE (android_video_shim::gAPILevel > 14)
    #define AVSHIM_USE_NEWDATASOURCEVTABLE (android_video_shim::gAPILevel > 14)
    #define AVSHIM_HAS_OMXRENDERERPATH (android_video_shim::gAPILevel < 11)

   // Duplicates of many Android libstagefright classes with their innard rewritten
    // to load and call symbols dynamically.
    //
    // Note that their inheritance hierarchy MUST match in terms of base/virtual base
    // classes or your pointers will be all off and you won't be able to call anything.
    typedef int32_t     status_t;

    enum {
        OK                = 0,    // Everything's swell.
        NO_ERROR          = 0,    // No errors.

        UNKNOWN_ERROR       = 0x80000000,

        NO_MEMORY           = -ENOMEM,
        INVALID_OPERATION   = -ENOSYS,
        BAD_VALUE           = -EINVAL,
        BAD_TYPE            = 0x80000001,
        NAME_NOT_FOUND      = -ENOENT,
        PERMISSION_DENIED   = -EPERM,
        NO_INIT             = -ENODEV,
        ALREADY_EXISTS      = -EEXIST,
        DEAD_OBJECT         = -EPIPE,
        FAILED_TRANSACTION  = 0x80000002,
        JPARKS_BROKE_IT     = -EPIPE,
    #if !defined(HAVE_MS_C_RUNTIME)
        BAD_INDEX           = -EOVERFLOW,
        NOT_ENOUGH_DATA     = -ENODATA,
        WOULD_BLOCK         = -EWOULDBLOCK,
        TIMED_OUT           = -ETIMEDOUT,
        UNKNOWN_TRANSACTION = -EBADMSG,
    #else
        BAD_INDEX           = -E2BIG,
        NOT_ENOUGH_DATA     = 0x80000003,
        WOULD_BLOCK         = 0x80000004,
        TIMED_OUT           = 0x80000005,
        UNKNOWN_TRANSACTION = 0x80000006,
    #endif
        FDS_NOT_ALLOWED     = 0x80000007,
    };

    enum {
        MEDIA_ERROR_BASE        = -1000,

        ERROR_ALREADY_CONNECTED = MEDIA_ERROR_BASE,
        ERROR_NOT_CONNECTED     = MEDIA_ERROR_BASE - 1,
        ERROR_UNKNOWN_HOST      = MEDIA_ERROR_BASE - 2,
        ERROR_CANNOT_CONNECT    = MEDIA_ERROR_BASE - 3,
        ERROR_IO                = MEDIA_ERROR_BASE - 4,
        ERROR_CONNECTION_LOST   = MEDIA_ERROR_BASE - 5,
        ERROR_MALFORMED         = MEDIA_ERROR_BASE - 7,
        ERROR_OUT_OF_RANGE      = MEDIA_ERROR_BASE - 8,
        ERROR_BUFFER_TOO_SMALL  = MEDIA_ERROR_BASE - 9,
        ERROR_UNSUPPORTED       = MEDIA_ERROR_BASE - 10,
        ERROR_END_OF_STREAM     = MEDIA_ERROR_BASE - 11,

        // Not technically an error.
        INFO_FORMAT_CHANGED    = MEDIA_ERROR_BASE - 12,
        INFO_DISCONTINUITY     = MEDIA_ERROR_BASE - 13,
        INFO_OUTPUT_BUFFERS_CHANGED = MEDIA_ERROR_BASE - 14,

        // The following constant values should be in sync with
        // drm/drm_framework_common.h
        DRM_ERROR_BASE = -2000,

        ERROR_DRM_UNKNOWN                       = DRM_ERROR_BASE,
        ERROR_DRM_NO_LICENSE                    = DRM_ERROR_BASE - 1,
        ERROR_DRM_LICENSE_EXPIRED               = DRM_ERROR_BASE - 2,
        ERROR_DRM_SESSION_NOT_OPENED            = DRM_ERROR_BASE - 3,
        ERROR_DRM_DECRYPT_UNIT_NOT_INITIALIZED  = DRM_ERROR_BASE - 4,
        ERROR_DRM_DECRYPT                       = DRM_ERROR_BASE - 5,
        ERROR_DRM_CANNOT_HANDLE                 = DRM_ERROR_BASE - 6,
        ERROR_DRM_TAMPER_DETECTED               = DRM_ERROR_BASE - 7,
        ERROR_DRM_NOT_PROVISIONED               = DRM_ERROR_BASE - 8,
        ERROR_DRM_DEVICE_REVOKED                = DRM_ERROR_BASE - 9,
        ERROR_DRM_RESOURCE_BUSY                 = DRM_ERROR_BASE - 10,

        ERROR_DRM_VENDOR_MAX                    = DRM_ERROR_BASE - 500,
        ERROR_DRM_VENDOR_MIN                    = DRM_ERROR_BASE - 999,

        // Heartbeat Error Codes
        HEARTBEAT_ERROR_BASE = -3000,
        ERROR_HEARTBEAT_TERMINATE_REQUESTED                     = HEARTBEAT_ERROR_BASE,
    };


    typedef enum OMX_COLOR_FORMATTYPE {
        OMX_COLOR_FormatUnused,
        OMX_COLOR_FormatMonochrome,
        OMX_COLOR_Format8bitRGB332,
        OMX_COLOR_Format12bitRGB444,
        OMX_COLOR_Format16bitARGB4444,
        OMX_COLOR_Format16bitARGB1555,
        OMX_COLOR_Format16bitRGB565,
        OMX_COLOR_Format16bitBGR565,
        OMX_COLOR_Format18bitRGB666,
        OMX_COLOR_Format18bitARGB1665,
        OMX_COLOR_Format19bitARGB1666,
        OMX_COLOR_Format24bitRGB888,
        OMX_COLOR_Format24bitBGR888,
        OMX_COLOR_Format24bitARGB1887,
        OMX_COLOR_Format25bitARGB1888,
        OMX_COLOR_Format32bitBGRA8888,
        OMX_COLOR_Format32bitARGB8888,
        OMX_COLOR_FormatYUV411Planar,
        OMX_COLOR_FormatYUV411PackedPlanar,
        OMX_COLOR_FormatYUV420Planar,
        OMX_COLOR_FormatYUV420PackedPlanar,
        OMX_COLOR_FormatYUV420SemiPlanar,
        OMX_COLOR_FormatYUV422Planar,
        OMX_COLOR_FormatYUV422PackedPlanar,
        OMX_COLOR_FormatYUV422SemiPlanar,
        OMX_COLOR_FormatYCbYCr,
        OMX_COLOR_FormatYCrYCb,
        OMX_COLOR_FormatCbYCrY,
        OMX_COLOR_FormatCrYCbY,
        OMX_COLOR_FormatYUV444Interleaved,
        OMX_COLOR_FormatRawBayer8bit,
        OMX_COLOR_FormatRawBayer10bit,
        OMX_COLOR_FormatRawBayer8bitcompressed,
        OMX_COLOR_FormatL2,
        OMX_COLOR_FormatL4,
        OMX_COLOR_FormatL8,
        OMX_COLOR_FormatL16,
        OMX_COLOR_FormatL24,
        OMX_COLOR_FormatL32,
        OMX_COLOR_FormatYUV420PackedSemiPlanar,
        OMX_COLOR_FormatYUV422PackedSemiPlanar,
        OMX_COLOR_Format18BitBGR666,
        OMX_COLOR_Format24BitARGB6666,
        OMX_COLOR_Format24BitABGR6666,
        OMX_COLOR_FormatKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
        OMX_COLOR_FormatVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
        /**<Reserved android opaque colorformat. Tells the encoder that
         * the actual colorformat will be  relayed by the
         * Gralloc Buffers.
         * FIXME: In the process of reserving some enum values for
         * Android-specific OMX IL colorformats. Change this enum to
         * an acceptable range once that is done.
         * */
        OMX_COLOR_FormatAndroidOpaque = 0x7F000789,
        OMX_TI_COLOR_FormatYUV420PackedSemiPlanar = 0x7F000100,
        OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00,
        QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7fa30c03,
        OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka = 0x7fa30c04,

        OMX_COLOR_FormatMax = 0x7FFFFFFF
    } OMX_COLOR_FORMATTYPE;

    extern const char *MEDIA_MIMETYPE_IMAGE_JPEG;

    extern const char *MEDIA_MIMETYPE_VIDEO_VP8;
    extern const char *MEDIA_MIMETYPE_VIDEO_VP9;
    extern const char *MEDIA_MIMETYPE_VIDEO_AVC;
    extern const char *MEDIA_MIMETYPE_VIDEO_MPEG4;
    extern const char *MEDIA_MIMETYPE_VIDEO_H263;
    extern const char *MEDIA_MIMETYPE_VIDEO_MPEG2;
    extern const char *MEDIA_MIMETYPE_VIDEO_RAW;

    extern const char *MEDIA_MIMETYPE_AUDIO_AMR_NB;
    extern const char *MEDIA_MIMETYPE_AUDIO_AMR_WB;
    extern const char *MEDIA_MIMETYPE_AUDIO_MPEG;           // layer III
    extern const char *MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_I;
    extern const char *MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II;
    extern const char *MEDIA_MIMETYPE_AUDIO_AAC;
    extern const char *MEDIA_MIMETYPE_AUDIO_QCELP;
    extern const char *MEDIA_MIMETYPE_AUDIO_VORBIS;
    extern const char *MEDIA_MIMETYPE_AUDIO_G711_ALAW;
    extern const char *MEDIA_MIMETYPE_AUDIO_G711_MLAW;
    extern const char *MEDIA_MIMETYPE_AUDIO_RAW;
    extern const char *MEDIA_MIMETYPE_AUDIO_FLAC;
    extern const char *MEDIA_MIMETYPE_AUDIO_AAC_ADTS;
    extern const char *MEDIA_MIMETYPE_AUDIO_MSGSM;

    extern const char *MEDIA_MIMETYPE_CONTAINER_MPEG4;
    extern const char *MEDIA_MIMETYPE_CONTAINER_WAV;
    extern const char *MEDIA_MIMETYPE_CONTAINER_OGG;
    extern const char *MEDIA_MIMETYPE_CONTAINER_MATROSKA;
    extern const char *MEDIA_MIMETYPE_CONTAINER_MPEG2TS;
    extern const char *MEDIA_MIMETYPE_CONTAINER_AVI;
    extern const char *MEDIA_MIMETYPE_CONTAINER_MPEG2PS;

    extern const char *MEDIA_MIMETYPE_CONTAINER_WVM;

    extern const char *MEDIA_MIMETYPE_TEXT_3GPP;
    extern const char *MEDIA_MIMETYPE_TEXT_SUBRIP;

    class MetaData;

    class RefBase
    {
    public:
        void incStrong(void *id)
        {
            typedef void (*localFuncCast)(void *thiz, void *id);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android7RefBase9incStrongEPKv");
            LOGREFBASE("RefBase - Inc'ing this=%p id=%p func=%p", (void*)this, id, lfc);
            assert(lfc);
            lfc(this, id);
        }

        void decStrong(void *id)
        {
            typedef void (*localFuncCast)(void *thiz, void *id);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android7RefBase9decStrongEPKv");
            LOGREFBASE("RefBase - Dec'ing this=%p id=%p func=%p", (void*)this, id, lfc);
            assert(lfc);
            lfc(this, id);
        }

        class weakref_type
        {
        public:
            RefBase*            refBase() const;

            void                incWeak(void* id)
            {
                typedef void (*localFuncCast)(void *thiz, void *id);
                localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android7RefBase12weakref_type7incWeakEPKv");
                LOGREFBASE("RefBase::weakref_type - incWeak this=%p id=%p func=%p", (void*)this, id, lfc);
                assert(lfc);
                lfc(this, id);

            }
            void                decWeak(void* id)
            {
                typedef void (*localFuncCast)(void *thiz, void *id);
                localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android7RefBase12weakref_type7decWeakEPKv");
                LOGREFBASE("RefBase::weakref_type - decWeak this=%p id=%p func=%p", (void*)this, id, lfc);
                assert(lfc);
                lfc(this, id);
            }

            // acquires a strong reference if there is already one.
            bool                attemptIncStrong(void* id)
            {
                typedef bool (*localFuncCast)(void *thiz, void *id);
                localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android7RefBase12weakref_type16attemptIncStrongEPKv");
                LOGREFBASE("RefBase::weakref_type - attemptIncStrong this=%p id=%p func=%p", (void*)this, id, lfc);
                assert(lfc);
                return lfc(this, id);
            }

        };

        weakref_type*   createWeak(void* id)
        {
            typedef weakref_type* (*localFuncCast)(void *thiz, void *id);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android7RefBase10createWeakEPKv");
            LOGREFBASE("RefBase::weakref_type - createWeak this=%p id=%p func=%p", (void*)this, id, lfc);
            assert(lfc);
            return lfc(this, id);
        }

        weakref_type*   getWeakRefs() const;

        RefBase() : mRefs(0)
        {
            // Call our c'tor.
            LOGREFBASE("RefBase - ctor %p", this);
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android7RefBaseC2Ev");
            assert(lfc);
            lfc(this);
        }

        virtual ~RefBase()
        {
            LOGREFBASE("RefBase - dtor %p mRefs=%p", this, mRefs);
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android7RefBaseD2Ev");
            assert(lfc);
            lfc(this);
        }

        virtual void            onFirstRef() {};
        virtual void            onLastStrongRef(const void* id) {};
        virtual bool            onIncStrongAttempted(uint32_t flags, const void* id) {};
        virtual void            onLastWeakRef(const void* id) {};

        void *mRefs;
    };

    // ---------------------------------------------------------------------------

    #define COMPARE_WEAK(_op_)                                      \
    inline bool operator _op_ (const sp<T>& o) const {              \
        return m_ptr _op_ o.m_ptr;                                  \
    }                                                               \
    inline bool operator _op_ (const T* o) const {                  \
        return m_ptr _op_ o;                                        \
    }                                                               \
    template<typename U>                                            \
    inline bool operator _op_ (const sp<U>& o) const {              \
        return m_ptr _op_ o.m_ptr;                                  \
    }                                                               \
    template<typename U>                                            \
    inline bool operator _op_ (const U* o) const {                  \
        return m_ptr _op_ o;                                        \
    }

    // ---------------------------------------------------------------------------

    template<typename T> class sp;

    template <typename T>
    class wp
    {
    public:
        typedef typename RefBase::weakref_type weakref_type;

        inline wp() : m_ptr(0), m_refs(0) { }

        wp(T* other);
        wp(const wp<T>& other);
        wp(const sp<T>& other);
        template<typename U> wp(U* other);
        template<typename U> wp(const sp<U>& other);
        template<typename U> wp(const wp<U>& other);

        ~wp();

        // Assignment

        wp& operator = (T* other);
        wp& operator = (const wp<T>& other);
        wp& operator = (const sp<T>& other);

        template<typename U> wp& operator = (U* other);
        template<typename U> wp& operator = (const wp<U>& other);
        template<typename U> wp& operator = (const sp<U>& other);

        void set_object_and_refs(T* other, weakref_type* refs);

        // promotion to sp

        sp<T> promote() const;

        // Reset

        void clear();

        // Accessors

        inline  weakref_type* get_refs() const { return m_refs; }

        inline  T* unsafe_get() const { return m_ptr; }

        // Operators

        COMPARE_WEAK(==)
        COMPARE_WEAK(!=)
        COMPARE_WEAK(>)
        COMPARE_WEAK(<)
        COMPARE_WEAK(<=)
        COMPARE_WEAK(>=)

        inline bool operator == (const wp<T>& o) const {
            return (m_ptr == o.m_ptr) && (m_refs == o.m_refs);
        }
        template<typename U>
        inline bool operator == (const wp<U>& o) const {
            return m_ptr == o.m_ptr;
        }

        inline bool operator > (const wp<T>& o) const {
            return (m_ptr == o.m_ptr) ? (m_refs > o.m_refs) : (m_ptr > o.m_ptr);
        }
        template<typename U>
        inline bool operator > (const wp<U>& o) const {
            return (m_ptr == o.m_ptr) ? (m_refs > o.m_refs) : (m_ptr > o.m_ptr);
        }

        inline bool operator < (const wp<T>& o) const {
            return (m_ptr == o.m_ptr) ? (m_refs < o.m_refs) : (m_ptr < o.m_ptr);
        }
        template<typename U>
        inline bool operator < (const wp<U>& o) const {
            return (m_ptr == o.m_ptr) ? (m_refs < o.m_refs) : (m_ptr < o.m_ptr);
        }
                             inline bool operator != (const wp<T>& o) const { return m_refs != o.m_refs; }
        template<typename U> inline bool operator != (const wp<U>& o) const { return !operator == (o); }
                             inline bool operator <= (const wp<T>& o) const { return !operator > (o); }
        template<typename U> inline bool operator <= (const wp<U>& o) const { return !operator > (o); }
                             inline bool operator >= (const wp<T>& o) const { return !operator < (o); }
        template<typename U> inline bool operator >= (const wp<U>& o) const { return !operator < (o); }

    private:
        template<typename Y> friend class sp;
        template<typename Y> friend class wp;

        T*              m_ptr;
        weakref_type*   m_refs;
    };

	#undef COMPARE_WEAK

    template<typename T>
    wp<T>::wp(T* other)
        : m_ptr(other)
    {
        if (other) m_refs = other->createWeak(this);
    }

    template<typename T>
    wp<T>::wp(const wp<T>& other)
        : m_ptr(other.m_ptr), m_refs(other.m_refs)
    {
        if (m_ptr) m_refs->incWeak(this);
    }

    template<typename T>
    wp<T>::wp(const sp<T>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_refs = m_ptr->createWeak(this);
        }
    }

    template<typename T> template<typename U>
    wp<T>::wp(U* other)
        : m_ptr(other)
    {
        if (other) m_refs = other->createWeak(this);
    }

    template<typename T> template<typename U>
    wp<T>::wp(const wp<U>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_refs = other.m_refs;
            m_refs->incWeak(this);
        }
    }

    template<typename T> template<typename U>
    wp<T>::wp(const sp<U>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_refs = m_ptr->createWeak(this);
        }
    }

    template<typename T>
    wp<T>::~wp()
    {
        if (m_ptr) m_refs->decWeak(this);
    }

    template<typename T>
    wp<T>& wp<T>::operator = (T* other)
    {
        weakref_type* newRefs =
            other ? other->createWeak(this) : 0;
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = other;
        m_refs = newRefs;
        return *this;
    }

    template<typename T>
    wp<T>& wp<T>::operator = (const wp<T>& other)
    {
        weakref_type* otherRefs(other.m_refs);
        T* otherPtr(other.m_ptr);
        if (otherPtr) otherRefs->incWeak(this);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = otherRefs;
        return *this;
    }

    template<typename T>
    wp<T>& wp<T>::operator = (const sp<T>& other)
    {
        weakref_type* newRefs =
            other != NULL ? other->createWeak(this) : 0;
        T* otherPtr(other.m_ptr);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = newRefs;
        return *this;
    }

    template<typename T> template<typename U>
    wp<T>& wp<T>::operator = (U* other)
    {
        weakref_type* newRefs =
            other ? other->createWeak(this) : 0;
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = other;
        m_refs = newRefs;
        return *this;
    }

    template<typename T> template<typename U>
    wp<T>& wp<T>::operator = (const wp<U>& other)
    {
        weakref_type* otherRefs(other.m_refs);
        U* otherPtr(other.m_ptr);
        if (otherPtr) otherRefs->incWeak(this);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = otherRefs;
        return *this;
    }

    template<typename T> template<typename U>
    wp<T>& wp<T>::operator = (const sp<U>& other)
    {
        weakref_type* newRefs =
            other != NULL ? other->createWeak(this) : 0;
        U* otherPtr(other.m_ptr);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = newRefs;
        return *this;
    }

    template<typename T>
    void wp<T>::set_object_and_refs(T* other, weakref_type* refs)
    {
        if (other) refs->incWeak(this);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = other;
        m_refs = refs;
    }

    template<typename T>
    sp<T> wp<T>::promote() const
    {
        sp<T> result;
        if (m_ptr && m_refs->attemptIncStrong(&result)) {
            result.set_pointer(m_ptr);
        }
        return result;
    }

    template<typename T>
    void wp<T>::clear()
    {
        if (m_ptr) {
            m_refs->decWeak(this);
            m_ptr = 0;
        }
    }


    // ---------------------------------------------------------------------------
    #define COMPARE(_op_)                                           \
    inline bool operator _op_ (const sp<T>& o) const {              \
        return m_ptr _op_ o.m_ptr;                                  \
    }                                                               \
    inline bool operator _op_ (const T* o) const {                  \
        return m_ptr _op_ o;                                        \
    }                                                               \
    template<typename U>                                            \
    inline bool operator _op_ (const sp<U>& o) const {              \
        return m_ptr _op_ o.m_ptr;                                  \
    }                                                               \
    template<typename U>                                            \
    inline bool operator _op_ (const U* o) const {                  \
        return m_ptr _op_ o;                                        \
    }                                                               \
    inline bool operator _op_ (const wp<T>& o) const {              \
        return m_ptr _op_ o.m_ptr;                                  \
    }                                                               \
    template<typename U>                                            \
    inline bool operator _op_ (const wp<U>& o) const {              \
        return m_ptr _op_ o.m_ptr;                                  \
    }
    // ---------------------------------------------------------------------------
    template<typename T>
    class sp {
    public:
        inline sp() : m_ptr(0) { }
        sp(T* other);
        sp(const sp<T>& other);
        template<typename U> sp(U* other);
        template<typename U> sp(const sp<U>& other);
        ~sp();
        // Assignment
        sp& operator = (T* other);
        sp& operator = (const sp<T>& other);
        template<typename U> sp& operator = (const sp<U>& other);
        template<typename U> sp& operator = (U* other);
        //! Special optimization for use by ProcessState (and nobody else).
        void force_set(T* other);
        // Reset
        void clear();
        // Accessors
        inline  T&      operator* () const  { return *m_ptr; }
        inline  T*      operator-> () const { return m_ptr;  }
        inline  T*      get() const         { return m_ptr; }
        // Operators
        COMPARE(==)
        COMPARE(!=)
        COMPARE(>)
        COMPARE(<)
        COMPARE(<=)
        COMPARE(>=)
    private:    
        template<typename Y> friend class sp;
        template<typename Y> friend class wp;
        void set_pointer(T* ptr);
        T* m_ptr;
    };
    #undef COMPARE
    // ---------------------------------------------------------------------------
    // No user serviceable parts below here.
    template<typename T>
    sp<T>::sp(T* other)
            : m_ptr(other) {
        if (other)
            other->incStrong(this);
    }
    template<typename T>
    sp<T>::sp(const sp<T>& other)
            : m_ptr(other.m_ptr) {
        if (m_ptr)
            m_ptr->incStrong(this);
    }
    template<typename T> template<typename U>
    sp<T>::sp(U* other)
            : m_ptr(other) {
        if (other)
            ((T*) other)->incStrong(this);
    }
    template<typename T> template<typename U>
    sp<T>::sp(const sp<U>& other)
            : m_ptr(other.m_ptr) {
        if (m_ptr)
            m_ptr->incStrong(this);
    }
    template<typename T>
    sp<T>::~sp() {
        if (m_ptr)
            m_ptr->decStrong(this);
    }
    template<typename T>
    sp<T>& sp<T>::operator =(const sp<T>& other) {
        T* otherPtr(other.m_ptr);
        if (otherPtr)
            otherPtr->incStrong(this);
        if (m_ptr)
            m_ptr->decStrong(this);
        m_ptr = otherPtr;
        return *this;
    }
    template<typename T>
    sp<T>& sp<T>::operator =(T* other) {
        if (other)
            other->incStrong(this);
        if (m_ptr)
            m_ptr->decStrong(this);
        m_ptr = other;
        return *this;
    }
    template<typename T> template<typename U>
    sp<T>& sp<T>::operator =(const sp<U>& other) {
        T* otherPtr(other.m_ptr);
        if (otherPtr)
            otherPtr->incStrong(this);
        if (m_ptr)
            m_ptr->decStrong(this);
        m_ptr = otherPtr;
        return *this;
    }
    template<typename T> template<typename U>
    sp<T>& sp<T>::operator =(U* other) {
        if (other)
            ((T*) other)->incStrong(this);
        if (m_ptr)
            m_ptr->decStrong(this);
        m_ptr = other;
        return *this;
    }
    template<typename T>
    void sp<T>::force_set(T* other) {
        other->forceIncStrong(this);
        m_ptr = other;
    }
    template<typename T>
    void sp<T>::clear() {
        if (m_ptr) {
            m_ptr->decStrong(this);
            m_ptr = 0;
        }
    }
    template<typename T>
    void sp<T>::set_pointer(T* ptr) {
        m_ptr = ptr;
    }

    struct ANativeWindow
    {

    };

    template <typename NATIVE_TYPE, typename TYPE, typename REF>
    class EGLNativeBase : public NATIVE_TYPE, public REF
    {
    };

    class ISurface : public virtual RefBase
    {

    };

    class Surface
        : public EGLNativeBase<ANativeWindow, Surface, RefBase>
    {
    public:
        sp<ISurface> getISurface()
        {
            typedef sp<ISurface> (*localFuncCast)(void *);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android7Surface11getISurfaceEv");
            assert(lfc);
            LOGI("getISurface=%p %p", lfc, this);

            return lfc(this);
        }
    };
    
    /* Can't use VideoRenderer, as it assumes local access to DSP RAM.
       IOMXRenderer proxies it in correct memory space.

    class VideoRenderer
    {
    public:
        virtual ~VideoRenderer() {}
        virtual void render(
                const void *data, size_t size, void *platformPrivate) = 0;

    protected:
        VideoRenderer() {}
        VideoRenderer(const VideoRenderer &);
        VideoRenderer &operator=(const VideoRenderer &);
    };

    inline VideoRenderer *instantiateSoftwareRenderer(OMX_COLOR_FORMATTYPE colorFormat,
            const sp<ISurface> &surface,
            size_t displayWidth, size_t displayHeight,
            size_t decodedWidth, size_t decodedHeight,
            int32_t rotationDegrees = 0)
    {
        typedef void *(*localFuncCast)(void *thiz, OMX_COLOR_FORMATTYPE colorFormat,
            const sp<ISurface> &surface,
            size_t displayWidth, size_t displayHeight,
            size_t decodedWidth, size_t decodedHeight,
            int32_t rotationDegrees);
        localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android16SoftwareRendererC2E20OMX_COLOR_FORMATTYPERKNS_2spINS_8ISurfaceEEEjjjji");

        if(!lfc)
        {
            LOGE("Could not resolve software renderer ctor.");
            return NULL;
        }

        void *mem = malloc(8192); // Over allocate.
        return (VideoRenderer*)lfc(mem, colorFormat, surface, displayWidth, displayHeight, decodedWidth, decodedHeight, rotationDegrees);
    } */

    class IInterface : public virtual RefBase
    {

    };

    class IOMXRenderer : public IInterface
    {
    public:
        void render(void *buffer)
        {
            typedef void (*localFuncCast)(void *thiz, void *buffer);

            // Do a vtable lookup.
            const int vtableOffset = 4;

            localFuncCast **fakeObj = (localFuncCast**)this;

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];

            for(int i=0; i<8; i++)
            {
                LOGV2("vtable[%d] = %p", i, fakeObj[0][i]);
            }

            LOGV2("expected OMXRenderer::render=%p", searchSymbol("_ZN7android11OMXRenderer6renderEPv"));

            LOGI("virtual IOMXRenderer::render=%p", lfc);
            
            lfc((void*)this, buffer);
        }
    };

    class IOMX : public IInterface
    {
    public:

        sp<IOMXRenderer> createRenderer(
                const sp<ISurface> &surface,
                const char *componentName,
                OMX_COLOR_FORMATTYPE colorFormat,
                size_t encodedWidth, size_t encodedHeight,
                size_t displayWidth, size_t displayHeight,
                int32_t rotationDegrees)
        {
            typedef sp<IOMXRenderer> (*localFuncCast)(
                    void *thiz,
                    const sp<ISurface> &surface,
                    const char *componentName,
                    OMX_COLOR_FORMATTYPE colorFormat,
                    size_t encodedWidth, size_t encodedHeight,
                    size_t displayWidth, size_t displayHeight,
                    int32_t rotationDegrees);

            // Do a vtable lookup.
            const int vtableOffset = 20;

            localFuncCast **fakeObj = (localFuncCast**)this;

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];

            for(int i=0; i<32; i++)
            {
                LOGV("vtable[%d] = %p", i, fakeObj[0][i]);
            }

            LOGV("expected OMX::createRenderer=%p", searchSymbol("_ZN7android3OMX14createRendererERKNS_2spINS_8ISurfaceEEEPKc20OMX_COLOR_FORMATTYPEjjjji"));

            LOGI("virtual IOMX::createRenderer=%p", lfc);
            sp<IOMXRenderer> r = lfc((void*)this, surface, componentName, colorFormat, encodedWidth, encodedHeight, displayWidth, displayHeight, rotationDegrees);
            LOGI("    o got %p", r.get());
            return r;
        }

        #define ANDROID_VIEW_SURFACE_JNI_ID    "mNativeSurface"

        sp<IOMXRenderer> createRendererFromJavaSurface(
            JNIEnv *env, jobject javaSurface,
            const char *componentName,
            OMX_COLOR_FORMATTYPE colorFormat,
            size_t encodedWidth, size_t encodedHeight,
            size_t displayWidth, size_t displayHeight,
            int32_t rotationDegrees)
        {

            LOGV2("Resolving android.view.Surface class.");
            jclass surfaceClass = env->FindClass("android/view/Surface");
            if (surfaceClass == NULL) 
            {
                LOGE("Can't find android/view/Surface");
                return NULL;
            }

            LOGV2("   o Got %p", surfaceClass);

            LOGV2("Resolving android.view.Surface field ID");
            jfieldID surfaceID = env->GetFieldID(surfaceClass, ANDROID_VIEW_SURFACE_JNI_ID, "I");
            if (surfaceID == NULL) 
            {
                LOGE("Can't find Surface.mSurface");
                return NULL;
            }
            LOGV2("   o Got %p", surfaceID);

            LOGV2("Getting Surface off of the Java Surface");
            sp<Surface> surface = (Surface *)env->GetIntField(javaSurface, surfaceID);
            LOGV2("   o Got %p", surface.get());

            LOGV2("Getting ISurface off of the Surface");
            sp<ISurface> surfInterface = surface->getISurface();
            LOGV2("   o Got %p", surfInterface.get());

            LOGV2("Calling createRenderer %p %s...", surfInterface.get(), componentName);
            return createRenderer(
                    surfInterface, componentName, colorFormat, encodedWidth,
                    encodedHeight, displayWidth, displayHeight,
                    rotationDegrees);
        }
    };

    struct AMessage;
    class DrmManagerClient;
    class HLSDataSource;

    class String8
    {
    public:

        const char *data;
         String8(const char *d = NULL) : data(d) {}
    };

    class DataSource : public RefBase {
    public:
        enum Flags {
            kWantsPrefetching      = 1,
            kStreamedFromLocalHost = 2,
            kIsCachingDataSource   = 4,
            kIsHTTPBasedSource     = 8,
        };

        static sp<DataSource> CreateFromURI(
                const char *uri,
                //const KeyedVector<String8, String8> *headers = NULL);
                void *headers = NULL)
        {
            typedef sp<DataSource> (*localFuncCast)(const char *, void *);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android10DataSource13CreateFromURIEPKcPKNS_11KeyedVectorINS_7String8ES4_EE");
            assert(lfc);
            LOGI("Calling %p with %s and %p", lfc, uri, headers);

            return lfc(uri, headers);
        }

        DataSource()
        {
            // Locate and call the constructor.
            /*typedef sp<DataSource> (*localFuncCast)(void *);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android10DataSource13CreateFromURIEPKcPKNS_11KeyedVectorINS_7String8ES4_EE");
            assert(lfc);
            LOGI("Calling DataSource ctor %p", lfc);
            lfc(this);*/
        }

        status_t initCheck()
        {
            const int vtableOffset = 6;
            typedef status_t (*localFuncCast)(void *thiz);
            localFuncCast **fakeObj = (localFuncCast**)this;

            for(int i=0; i<24; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[0][i]);
            }

            LOGI("FileSource::initCheck should be %p", searchSymbol("_ZNK7android10FileSource9initCheckEv"));
            LOGI("NuCachedSource2::initCheck should be %p", searchSymbol("_ZNK7android15NuCachedSource29initCheckEv"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            LOGI("virtual initCheck=%p", lfc);
            status_t r = lfc((void*)this);
            LOGI("    o got %d", r);
            return r;
        }

        ssize_t readAt(off64_t offset, void *data, size_t size)
        {
            const int vtableOffset = 7;
            typedef ssize_t (*localFuncCast)(void *thiz, off64_t offset, void *data, size_t size);
            localFuncCast **fakeObj = (localFuncCast**)this;

            for(int i=0; i<17; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[0][i]); 
            }

            LOGV2("FileSource::readAt should be %p", searchSymbol("_ZN7android10FileSource9readAtDRMExPvj"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            LOGV2("virtual readAt=%p", lfc);
            ssize_t r = lfc((void*)this, offset, data, size);
            //LOGI("    o got %ld", r);
            return r;
        }

        ssize_t readAt_23(int64_t offset, void *data, size_t size)
        {
            const int vtableOffset = 7;
            typedef ssize_t (*localFuncCast)(void *thiz, int64_t offset, void *data, size_t size);
            localFuncCast **fakeObj = (localFuncCast**)this;

            for(int i=0; i<17; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[0][i]);
            }

            //LOGI("FileSource::readAt should be %p", searchSymbol("_ZN7android10FileSource9readAtDRMExPvj"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            LOGV2("virtual readAt=%p", lfc);
            ssize_t r = lfc((void*)this, offset, data, size);
            LOGV2("    o got %ld", r);
            return r;
        }

        // Convenience methods:
        bool getUInt16(off64_t offset, uint16_t *x) { return false; };
        bool getUInt24(off64_t offset, uint32_t *x) { return false; }; // 3 byte int, returned as a 32-bit int
        bool getUInt32(off64_t offset, uint32_t *x) { return false; };
        bool getUInt64(off64_t offset, uint64_t *x) { return false; };

        // May return ERROR_UNSUPPORTED.
        status_t getSize(off64_t *size)
        {
            const int vtableOffset = 8;
            typedef status_t (*localFuncCast)(void *thiz, off64_t *size);
            localFuncCast **fakeObj = (localFuncCast**)this;

            for(int i=0; i<17; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[0][i]);
            }

            LOGV2("FileSource::getSize should be %p", searchSymbol("_ZN7android10FileSource7getSizeEPx"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            LOGV2("virtual getSize=%p", lfc);
            status_t r = lfc((void*)this, size);
            LOGV2("    o got %d", r);
            return r;
        }

        status_t getSize_23(off64_t *size)
        {
            const int vtableOffset = 8;
            typedef status_t (*localFuncCast)(void *thiz, off64_t *size);
            localFuncCast **fakeObj = (localFuncCast**)this;

            for(int i=0; i<17; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[0][i]);
            }

            LOGV2("FileSource::getSize should be %p", searchSymbol("_ZN7android10FileSource7getSizeEPx"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            LOGV2("virtual getSize=%p", lfc);
            status_t r = lfc((void*)this, size);
            LOGV2("    o got %d", r);
            return r;
        }

         uint32_t flags() {
            return 0;
        }

         status_t reconnectAtOffset(off64_t offset) {
            return ERROR_UNSUPPORTED;
        }

        // for DRM
        //virtual sp<DecryptHandle> DrmInitialization(const char *mime = NULL) {
         sp<RefBase> DrmInitialization(const char *mime = NULL) {
            return NULL;
        }
        //virtual void getDrmInfo(sp<DecryptHandle> &handle, DrmManagerClient **client) {};
         void getDrmInfo(void *&handle, DrmManagerClient **client) {};

         String8 getUri() {
            return String8();
        }

         String8 getMIMEType() const { return NULL; }

    protected:
        virtual ~DataSource() {}
    };

    class MediaBuffer
    {
    public:

        MediaBuffer()
        {
            assert(0); // We don't want to make our own with this path.
        }

        MediaBuffer(size_t size)
        {
            typedef void (*localFuncCast)(void *thiz, unsigned int size);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11MediaBufferC1Ej");
            assert(lfc);
            LOGV2("MediaBuffer::ctor with size = %p", lfc);
            lfc(this, size);
        }

        // Decrements the reference count and returns the buffer to its
        // associated MediaBufferGroup if the reference count drops to 0.
        void release()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11MediaBuffer7releaseEv");
            assert(lfc);
            LOGV2("MediaBuffer::release = %p", lfc);

            lfc(this);
        }

        // Increments the reference count.
        void add_ref()
        {
            assert(0);
        }

        void *data()
        {
            typedef void *(*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android11MediaBuffer4dataEv");
            assert(lfc);
            LOGV2("MediaBuffer::data = %p this=%p", lfc, this);
            return lfc(this);
        }

        size_t size()
        {
            typedef size_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android11MediaBuffer4sizeEv");
            assert(lfc);
            LOGV2("MediaBuffer::size = %p this=%p", lfc, this);
            return lfc(this);
        }

        size_t range_offset()
        {
            typedef size_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android11MediaBuffer12range_offsetEv");
            assert(lfc);
            LOGV2("MediaBuffer::range_offset = %p this=%p", lfc, this);
            return lfc(this);
        }

        size_t range_length()
        {
            typedef size_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android11MediaBuffer12range_lengthEv");
            assert(lfc);
            LOGV2("MediaBuffer::range_length = %p this=%p", lfc, this);
            return lfc(this);
        }

        void set_range(size_t offset, size_t length)
        {
            assert(0);
        }

        sp<RefBase> graphicBuffer() const
        {
            assert(0);
            return NULL;
        }

        sp<MetaData> meta_data()
        {
            typedef sp<MetaData> (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11MediaBuffer9meta_dataEv");
            assert(lfc);
            LOGV2("MediaBuffer::meta_data = %p this=%p", lfc, this);

            return lfc(this);
        }

        // Clears meta data and resets the range to the full extent.
        void reset()
        {
            assert(0);
        }

        // Returns a clone of this MediaBuffer increasing its reference count.
        // The clone references the same data but has its own range and
        // MetaData.
        MediaBuffer *clone()
        {
            assert(0);
        }

        int refcount() const
        {
            assert(0);
        }

        // Dummy memory to make sure we can hold everything.
        char dummy[256];

    };

    class MediaSource : public virtual RefBase
    {
    public:
        // To be called before any other methods on this object, except
        // getFormat().
        status_t start(MetaData *params = NULL)
        {
            const int vtableOffset = 0;
            typedef status_t (*localFuncCast)(void *thiz, MetaData *params);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];
            LOGI("virtual start=%p", lfc);
            return lfc(this, params);
        }

        // Any blocking read call returns immediately with a result of NO_INIT.
        // It is an error to call any methods other than start after this call
        // returns. Any buffers the object may be holding onto at the time of
        // the stop() call are released.
        // Also, it is imperative that any buffers output by this object and
        // held onto by callers be released before a call to stop() !!!
        status_t stop()
        {
            const int vtableOffset = 1;
            typedef status_t (*localFuncCast)(void *thiz);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];
            LOGI("virtual stop=%p", lfc);
            return lfc(this);
        }

        // Returns the format of the data output by this media source.
        sp<MetaData> getFormat()
        {
            const int vtableOffset = AVSHIM_USE_NEWMEDIASOURCEVTABLE ? 2 : 8;
            typedef sp<MetaData> (*localFuncCast)(void *thiz);
            localFuncCast **fakeObj = (localFuncCast **)this;

            //test_dlsym();
            LOGV2("_getFormat");
            LOGV2("this = %p", this);
            LOGV2("*this = %p", *(void**)this);
            LOGV2("**this = %p", **(void***)this);
            LOGV2("MPEG4Source::getFormat should be %p", searchSymbol("_ZN7android11MPEG4Source9getFormatEv"));
            LOGV2("AnotherPacketSource::getFormat should be %p", searchSymbol("_ZN7android19AnotherPacketSource9getFormatEv"));
            LOGV2("AACSource::getFormat should be %p", searchSymbol("_ZN7android9AACSource9getFormatEv"));
            LOGV2("MPEG2TSSource::getFormat should be %p", searchSymbol("_ZN7android13MPEG2TSSource9getFormatEv"));
            LOGV2("OMXCodec::getFormat might be %p", searchSymbol("_ZN7android8OMXCodec9getFormatEv"));


            LOGV2("Listing layout:");
            for(int i=-2; i<20; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[0][i]);  
            } 

            // Attempt log type name.
            /* type_info *whatIsIt = (type_info *)fakeObj[0][-1];
            LOGI("Saw type_info @ %p", whatIsIt);
            LOGI("   - name sez '%s'", whatIsIt->name()); */

            localFuncCast lfc = fakeObj[0][vtableOffset];
            return lfc(this);
        }

        struct ReadOptions;

        // Returns a new buffer of data. Call blocks until a
        // buffer is available, an error is encountered of the end of the stream
        // is reached.
        // End of stream is signalled by a result of ERROR_END_OF_STREAM.
        // A result of INFO_FORMAT_CHANGED indicates that the format of this
        // MediaSource has changed mid-stream, the client can continue reading
        // but should be prepared for buffers of the new configuration.
        status_t read(
                MediaBuffer **buffer, const ReadOptions *options = NULL)
        {
            const int vtableOffset = 3;
            typedef status_t (*localFuncCast)(void *thiz, MediaBuffer **buffer, const ReadOptions *options);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];
            LOGV2("virtual read=%p", lfc);
            return lfc(this, buffer, options);
        }

        // Options that modify read() behaviour. The default is to
        // a) not request a seek
        // b) not be late, i.e. lateness_us = 0
        struct ReadOptions {
            enum SeekMode {
                SEEK_PREVIOUS_SYNC,
                SEEK_NEXT_SYNC,
                SEEK_CLOSEST_SYNC,
                SEEK_CLOSEST,
            };

            ReadOptions()
            {
                memset(this, 0, sizeof(ReadOptions));
            }

            // Reset everything back to defaults.
            void reset();

            void setSeekTo(int64_t time_us, SeekMode mode = SEEK_CLOSEST_SYNC);
            void clearSeekTo();
            bool getSeekTo(int64_t *time_us, SeekMode *mode) const
            {
                return false;
            }

            void setLateBy(int64_t lateness_us);
            int64_t getLateBy() const;

        public:
            enum Options {
                kSeekTo_Option      = 1,
            };

            uint32_t mOptions;
            int64_t mSeekTimeUs;
            SeekMode mSeekMode;
            int64_t mLatenessUs;
        };

        // Causes this source to suspend pulling data from its upstream source
        // until a subsequent read-with-seek. Currently only supported by
        // OMXCodec.
        status_t pause() {
            return ERROR_UNSUPPORTED;
        }

        // The consumer of this media source requests that the given buffers
        // are to be returned exclusively in response to read calls.
        // This will be called after a successful start() and before the
        // first read() call.
        // Callee assumes ownership of the buffers if no error is returned.
        /*virtual status_t setBuffers(const Vector<MediaBuffer *> &buffers) {
            return ERROR_UNSUPPORTED;
        }*/

    protected:
        virtual ~MediaSource()
        {

        }

    };

    // Note 2.3 eschews the virtual base
    class MediaSource23 : public RefBase
    {
    public:
        // To be called before any other methods on this object, except
        // getFormat().
        status_t start(MetaData *params = NULL)
        {
            const int vtableOffset = 6;
            typedef status_t (*localFuncCast)(void *thiz, MetaData *params);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];

            // Dump the vtable.
            for(int i=0; i<16; i++)
            {
                LOGV2("   vtable[%d] = %p", i, fakeObj[0][i]);
            }
            LOGV("MPEG4Source::start should be %p", searchSymbol("_ZN7android13MPEG2TSSource5startEPNS_8MetaDataE"));
            LOGV("AnotherPacketSource::start should be %p", searchSymbol("_ZN7android19AnotherPacketSource5startEPNS_8MetaDataE"));
            LOGV("OMXCodec::start might be %p", searchSymbol("_ZN7android8OMXCodec5startEPNS_8MetaDataE"));
            LOGV("AACDecoder::start might be %p", searchSymbol("_ZN7android10AACDecoder5startEPNS_8MetaDataE"));

            LOGV("virtual start=%p", lfc);
            return lfc(this, params);
        }

        // Any blocking read call returns immediately with a result of NO_INIT.
        // It is an error to call any methods other than start after this call
        // returns. Any buffers the object may be holding onto at the time of
        // the stop() call are released.
        // Also, it is imperative that any buffers output by this object and
        // held onto by callers be released before a call to stop() !!!
        status_t stop()
        {
            const int vtableOffset = 7;
            typedef status_t (*localFuncCast)(void *thiz);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];
            LOGI("virtual stop=%p", lfc);
            return lfc(this);
        }

        // Returns the format of the data output by this media source.
        sp<MetaData> getFormat()
        {
            const int vtableOffset = AVSHIM_USE_NEWMEDIASOURCEVTABLE ? 2 : 8;
            typedef sp<MetaData> (*localFuncCast)(void *thiz);
            localFuncCast **fakeObj = (localFuncCast **)this;

            //test_dlsym();
            #if 0
            LOGI("_getFormat");
            LOGI("this = %p", this);
            LOGI("*this = %p", *(void**)this);
            LOGI("**this = %p", **(void***)this);
            LOGI("MPEG4Source::getFormat should be %p", searchSymbol("_ZN7android11MPEG4Source9getFormatEv"));
            LOGI("AnotherPacketSource::getFormat should be %p", searchSymbol("_ZN7android19AnotherPacketSource9getFormatEv"));
            LOGI("AACSource::getFormat should be %p", searchSymbol("_ZN7android9AACSource9getFormatEv"));
            LOGI("MPEG2TSSource::getFormat should be %p", searchSymbol("_ZN7android13MPEG2TSSource9getFormatEv"));
            LOGI("OMXCodec::getFormat might be %p", searchSymbol("_ZN7android8OMXCodec9getFormatEv"));


            LOGI("Listing layout:");
            for(int i=-2; i<20; i++) LOGI("virtual layout[%d]=%p", i, fakeObj[0][i]);

            #endif

            // Attempt log type name.
            /* type_info *whatIsIt = (type_info *)fakeObj[0][-1];
            LOGI("Saw type_info @ %p", whatIsIt);
            LOGI("   - name sez '%s'", whatIsIt->name()); */

            localFuncCast lfc = fakeObj[0][vtableOffset];
            return lfc(this);
        }

        struct ReadOptions;

        // Returns a new buffer of data. Call blocks until a
        // buffer is available, an error is encountered of the end of the stream
        // is reached.
        // End of stream is signalled by a result of ERROR_END_OF_STREAM.
        // A result of INFO_FORMAT_CHANGED indicates that the format of this
        // MediaSource has changed mid-stream, the client can continue reading
        // but should be prepared for buffers of the new configuration.
        status_t read(
                MediaBuffer **buffer, const ReadOptions *options = NULL)
        {
            const int vtableOffset = 9;
            typedef status_t (*localFuncCast)(void *thiz, MediaBuffer **buffer, const ReadOptions *options);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];
            LOGV2("virtual read=%p this=%p", lfc, this);
            return lfc(this, buffer, options);
        }

        // Options that modify read() behaviour. The default is to
        // a) not request a seek
        // b) not be late, i.e. lateness_us = 0
        struct ReadOptions {
            enum SeekMode {
                SEEK_PREVIOUS_SYNC,
                SEEK_NEXT_SYNC,
                SEEK_CLOSEST_SYNC,
                SEEK_CLOSEST,
            };

            ReadOptions()
            {
                memset(this, 0, sizeof(ReadOptions));
            }

            // Reset everything back to defaults.
            void reset();

            void setSeekTo(int64_t time_us, SeekMode mode = SEEK_CLOSEST_SYNC);
            void clearSeekTo();
            bool getSeekTo(int64_t *time_us, SeekMode *mode) const;

            void setLateBy(int64_t lateness_us);
            int64_t getLateBy() const;

        public:
            enum Options {
                kSeekTo_Option      = 1,
            };

            uint32_t mOptions;
            int64_t mSeekTimeUs;
            SeekMode mSeekMode;
            int64_t mLatenessUs;

            int64_t mSkipFrameUntilTimeUs;

        };

        // Causes this source to suspend pulling data from its upstream source
        // until a subsequent read-with-seek. Currently only supported by
        // OMXCodec.
        status_t pause() {
            LOGI("Unsupported: pause");
            assert(0);
            return ERROR_UNSUPPORTED;
        }

        // The consumer of this media source requests that the given buffers
        // are to be returned exclusively in response to read calls.
        // This will be called after a successful start() and before the
        // first read() call.
        // Callee assumes ownership of the buffers if no error is returned.
        /*virtual status_t setBuffers(const Vector<MediaBuffer *> &buffers) {
            return ERROR_UNSUPPORTED;
        }*/

    protected:
        virtual ~MediaSource23()
        {
        }
    };

    // Do nothing stub for OMXClient.
    class OMXClient
    {
    public:
        sp<IOMX> omx;

        OMXClient()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android9OMXClientC1Ev");
            assert(lfc);
            lfc(this);
        }

        int connect()
        {
            typedef int (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android9OMXClient7connectEv");
            assert(lfc);
            return lfc(this);
        }

        sp<IOMX> interface()
        {
            return omx;
        }
    };

    class MediaExtractor : public RefBase
    {
    public:
        static sp<MediaExtractor> Create(const sp<DataSource> &source, const char *mime = NULL)
        {
            typedef sp<MediaExtractor> (*localFuncCast)(const sp<DataSource> &source, const char *mime);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android14MediaExtractor6CreateERKNS_2spINS_10DataSourceEEEPKc");
            assert(lfc);
            return lfc(source, mime);
        }

        size_t countTracks()
        {
            const int vtableOffset = 6;
            typedef status_t (*localFuncCast)(void *thiz);

            LOGV2("this = %p", this);
            LOGV2("*this = %p", *(void**)this);
            LOGV2("Mpeg2TSExtractor::countTracks should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor11countTracksEv"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            for(int i=0; i<10; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[i]);
            }

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGV2("virtual countTracks=%p", lfc);
            return lfc(this);
        }

        sp<MediaSource> getTrack(size_t index)
        {
            const int vtableOffset = 7;
            typedef sp<MediaSource> (*localFuncCast)(void *thiz, size_t idx);

            LOGV2("this = %p", this);
            LOGV2("*this = %p", *(void**)this);
            LOGV2("Mpeg2TSExtractor::getTrack should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor8getTrackEj"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            for(int i=0; i<10; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[i]);
            }

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGV2("virtual getTrack=%p", lfc);
            return lfc(this, index);
        }

        sp<MediaSource23> getTrack23(size_t index)
        {
            const int vtableOffset = 7;
            typedef sp<MediaSource23> (*localFuncCast)(void *thiz, size_t idx);

            LOGV2("this = %p", this);
            LOGV2("*this = %p", *(void**)this);
            LOGV2("Mpeg2TSExtractor::getTrack should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor8getTrackEj"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            for(int i=0; i<10; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[i]);
            }

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGV2("virtual getTrack=%p", lfc);
            return lfc(this, index);
        }

        sp<MetaData> getTrackMetaData(
                size_t index, uint32_t flags = 0)
		{
            const int vtableOffset = 8;
            typedef sp<MetaData> (*localFuncCast)(void *thiz, size_t idx, uint32_t flags);

            LOGV2("this = %p", this);
            LOGV2("*this = %p", *(void**)this);
            LOGV2("Mpeg2TSExtractor::getTrackMetaData should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor16getTrackMetaDataEjj"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            for(int i=0; i<10; i++)
            {
                LOGV2("virtual layout[%d]=%p", i, fakeObj[i]);
            }

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGV2("virtual getTrackMetaData=%p", lfc);
            return lfc(this, index, flags);
		}

        // for DRM
        void setDrmFlag(bool flag) {
            mIsDrm = flag;
        };
        bool getDrmFlag() {
            return mIsDrm;
        }

    private:
        bool mIsDrm;
    };

    // The following keys map to int32_t data unless indicated otherwise.
    enum {
        kKeyMIMEType          = 'mime',  // cstring
        kKeyWidth             = 'widt',  // int32_t, image pixel
        kKeyHeight            = 'heig',  // int32_t, image pixel
        kKeyDisplayWidth      = 'dWid',  // int32_t, display/presentation
        kKeyDisplayHeight     = 'dHgt',  // int32_t, display/presentation
        // a rectangle, if absent assumed to be (0, 0, width - 1, height - 1)
        kKeyCropRect          = 'crop',
        kKeyRotation          = 'rotA',  // int32_t (angle in degrees)
        kKeyIFramesInterval   = 'ifiv',  // int32_t
        kKeyStride            = 'strd',  // int32_t
        kKeySliceHeight       = 'slht',  // int32_t
        kKeyChannelCount      = '#chn',  // int32_t
        kKeyChannelMask       = 'chnm',  // int32_t
        kKeySampleRate        = 'srte',  // int32_t (audio sampling rate Hz)
        kKeyFrameRate         = 'frmR',  // int32_t (video frame rate fps)
        kKeyBitRate           = 'brte',  // int32_t (bps)
        kKeyESDS              = 'esds',  // raw data
        kKeyAVCC              = 'avcc',  // raw data
        kTypeAVCC             = 'avcc',
        kKeyD263              = 'd263',  // raw data
        kKeyVorbisInfo        = 'vinf',  // raw data
        kKeyVorbisBooks       = 'vboo',  // raw data
        kKeyWantsNALFragments = 'NALf',
        kKeyIsSyncFrame       = 'sync',  // int32_t (bool)
        kKeyIsCodecConfig     = 'conf',  // int32_t (bool)
        kKeyTime              = 'time',  // int64_t (usecs)
        kKeyDecodingTime      = 'decT',  // int64_t (decoding timestamp in usecs)
        kKeyNTPTime           = 'ntpT',  // uint64_t (ntp-timestamp)
        kKeyTargetTime        = 'tarT',  // int64_t (usecs)
        kKeyDriftTime         = 'dftT',  // int64_t (usecs)
        kKeyAnchorTime        = 'ancT',  // int64_t (usecs)
        kKeyDuration          = 'dura',  // int64_t (usecs)
        kKeyColorFormat       = 'colf',
        kKeyPlatformPrivate   = 'priv',  // pointer
        kKeyDecoderComponent  = 'decC',  // cstring
        kKeyBufferID          = 'bfID',
        kKeyMaxInputSize      = 'inpS',
        kKeyThumbnailTime     = 'thbT',  // int64_t (usecs)
        kKeyTrackID           = 'trID',
        kKeyIsDRM             = 'idrm',  // int32_t (bool)
        kKeyAlbum             = 'albu',  // cstring
        kKeyArtist            = 'arti',  // cstring
        kKeyAlbumArtist       = 'aart',  // cstring
        kKeyComposer          = 'comp',  // cstring
        kKeyGenre             = 'genr',  // cstring
        kKeyTitle             = 'titl',  // cstring
        kKeyYear              = 'year',  // cstring
        kKeyAlbumArt          = 'albA',  // compressed image data
        kKeyAlbumArtMIME      = 'alAM',  // cstring
        kKeyAuthor            = 'auth',  // cstring
        kKeyCDTrackNumber     = 'cdtr',  // cstring
        kKeyDiscNumber        = 'dnum',  // cstring
        kKeyDate              = 'date',  // cstring
        kKeyWriter            = 'writ',  // cstring
        kKeyCompilation       = 'cpil',  // cstring
        kKeyLocation          = 'loc ',  // cstring
        kKeyTimeScale         = 'tmsl',  // int32_t
        // video profile and level
        kKeyVideoProfile      = 'vprf',  // int32_t
        kKeyVideoLevel        = 'vlev',  // int32_t
        // Set this key to enable authoring files in 64-bit offset
        kKey64BitFileOffset   = 'fobt',  // int32_t (bool)
        kKey2ByteNalLength    = '2NAL',  // int32_t (bool)
        // Identify the file output format for authoring
        // Please see <media/mediarecorder.h> for the supported
        // file output formats.
        kKeyFileType          = 'ftyp',  // int32_t
        // Track authoring progress status
        // kKeyTrackTimeStatus is used to track progress in elapsed time
        kKeyTrackTimeStatus   = 'tktm',  // int64_t
        kKeyNotRealTime       = 'ntrt',  // bool (int32_t)
        // Ogg files can be tagged to be automatically looping...
        kKeyAutoLoop          = 'autL',  // bool (int32_t)
        kKeyValidSamples      = 'valD',  // int32_t
        kKeyIsUnreadable      = 'unre',  // bool (int32_t)
        // An indication that a video buffer has been rendered.
        kKeyRendered          = 'rend',  // bool (int32_t)
        // The language code for this media
        kKeyMediaLanguage     = 'lang',  // cstring
        // To store the timed text format data
        kKeyTextFormatData    = 'text',  // raw data
        kKeyRequiresSecureBuffers = 'secu',  // bool (int32_t)
        kKeySARWidth = 'sarW',
        kKeySARHeight = 'sarH',
        kKeyIsADTS            = 'adts',  // bool (int32_t)
        kTypeESDS        = 'esds',
        kTypeD263        = 'd263',
    };


    // Some audio types - these are from <system/audio.h>
    // I'm only including types necessary for PCM as, by the time we need this information
    // we should only be dealing with PCM
    /* PCM sub formats */
    typedef enum {
        AUDIO_FORMAT_PCM_SUB_16_BIT          = 0x1, /* DO NOT CHANGE - PCM signed 16 bits */
        AUDIO_FORMAT_PCM_SUB_8_BIT           = 0x2, /* DO NOT CHANGE - PCM unsigned 8 bits */
        AUDIO_FORMAT_PCM_SUB_32_BIT          = 0x3, /* PCM signed .31 fixed point */
        AUDIO_FORMAT_PCM_SUB_8_24_BIT        = 0x4, /* PCM signed 7.24 fixed point */
    } audio_format_pcm_sub_fmt_t;

    /* Audio format consists in a main format field (upper 8 bits) and a sub format
     * field (lower 24 bits).
     *
     * The main format indicates the main codec type. The sub format field
     * indicates options and parameters for each format. The sub format is mainly
     * used for record to indicate for instance the requested bitrate or profile.
     * It can also be used for certain formats to give informations not present in
     * the encoded audio stream (e.g. octet alignement for AMR).
     */
    typedef enum {
        AUDIO_FORMAT_INVALID             = 0xFFFFFFFFUL,
        AUDIO_FORMAT_DEFAULT             = 0,
        AUDIO_FORMAT_PCM                 = 0x00000000UL, /* DO NOT CHANGE */
        AUDIO_FORMAT_MP3                 = 0x01000000UL,
        AUDIO_FORMAT_AMR_NB              = 0x02000000UL,
        AUDIO_FORMAT_AMR_WB              = 0x03000000UL,
        AUDIO_FORMAT_AAC                 = 0x04000000UL,
        AUDIO_FORMAT_HE_AAC_V1           = 0x05000000UL,
        AUDIO_FORMAT_HE_AAC_V2           = 0x06000000UL,
        AUDIO_FORMAT_VORBIS              = 0x07000000UL,
        AUDIO_FORMAT_MAIN_MASK           = 0xFF000000UL,
        AUDIO_FORMAT_SUB_MASK            = 0x00FFFFFFUL,

        /* Aliases */
        AUDIO_FORMAT_PCM_16_BIT          = (AUDIO_FORMAT_PCM |
                                            AUDIO_FORMAT_PCM_SUB_16_BIT),
        AUDIO_FORMAT_PCM_8_BIT           = (AUDIO_FORMAT_PCM |
                                            AUDIO_FORMAT_PCM_SUB_8_BIT),
        AUDIO_FORMAT_PCM_32_BIT          = (AUDIO_FORMAT_PCM |
                                            AUDIO_FORMAT_PCM_SUB_32_BIT),
        AUDIO_FORMAT_PCM_8_24_BIT        = (AUDIO_FORMAT_PCM |
                                            AUDIO_FORMAT_PCM_SUB_8_24_BIT),
    } audio_format_t;

    class MetaData : public RefBase
    {
    public:
        char data[1024]; // Padding to make sure we have enough RAM.

        MetaData()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaDataC1Ev");
            assert(lfc);
            lfc(this);

            LOGV2("ctor=%p", this);
        }

        ~MetaData()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaDataD1Ev");
            assert(lfc);
            lfc(this);
        }

        bool findData(uint32_t key, uint32_t *type, const void **data, size_t *size)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, uint32_t *type, const void **data, size_t *size);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android8MetaData8findDataEjPjPPKvS1_");
            assert(lfc);
            return lfc(this, key, type, data, size);
        }

        bool findPointer(uint32_t key, void **value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, void **value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData11findPointerEjPPv");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool findInt32(uint32_t key, int32_t *value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, int32_t *value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData9findInt32EjPi");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool findInt64(uint32_t key, int64_t *value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, int64_t *value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData9findInt64EjPx");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool findCString(uint32_t key, const char **value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, const char **value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData11findCStringEjPPKc");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool findRect(uint32_t key, int32_t *left, int32_t *top, int32_t *right, int32_t *bottom)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, int32_t *left, int32_t *top, int32_t *right, int32_t *bottom);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData8findRectEjPiS1_S1_S1_");
            if(!lfc)
                return false;
            assert(lfc);
            return lfc(this, key, left, top, right, bottom);
        }

        bool setCString(uint32_t key, const char *value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, const char *value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData10setCStringEjPKc");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool setInt32(uint32_t key, int32_t value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, int32_t value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData8setInt32Eji");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool setInt64(uint32_t key, int64_t value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, int64_t value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData8setInt64Ejx");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool setFloat(uint32_t key, float value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, float value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData8setFloatEjf");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool setPointer(uint32_t key, void *value)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, void *value);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData10setPointerEjPv");
            assert(lfc);
            return lfc(this, key, value);
        }

        bool setData(uint32_t key, uint32_t type, const void *data, size_t size)
        {
            typedef bool (*localFuncCast)(void *thiz, uint32_t key, uint32_t type, const void *data, size_t size);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaData7setDataEjjPKvj");
            assert(lfc);
            return lfc(this, key, type, data, size);
        }

        void dumpToLog()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android8MetaData9dumpToLogEv");
            if(!lfc)
            {
                LOGV("Failing to dumpToLog, symbol not found.");
                return;
            }
            assert(lfc);
            LOGV("dumpToLog %p this=%p", lfc, this);
            lfc(this);
        }
    };

    class MediaBufferObserver {
    public:
        MediaBufferObserver() {}
        virtual ~MediaBufferObserver() {}

        virtual void signalBufferReturned(MediaBuffer *buffer) = 0;

    private:
        MediaBufferObserver(const MediaBufferObserver &);
        MediaBufferObserver &operator=(const MediaBufferObserver &);
    };

    class OMXCodec : public MediaSource, public MediaBufferObserver
    {
    public:

        //  android::OMXCodec::Create(android::sp<android::IOMX> const&, android::sp<android::MetaData> const&, bool,
        //                            android::sp<android::MediaSource> const&, char const*, unsigned int, android::sp<ANativeWindow> const&)
        static sp<MediaSource> Create(sp<IOMX> &iomx,
                            sp<MetaData> &metadata,
                            bool flag,
                            sp<MediaSource> &mediaSource,
                            char const *string,
                            unsigned int value /*,
                            sp<ANativeWindow> &nativeWindow */)
        {
            typedef sp<MediaSource> (*localFuncCast_4series)(sp<IOMX> &iomx,
                            sp<MetaData> &metadata,
                            bool createEncoder,
                            sp<MediaSource> &mediaSource,
                            char const *matchComponentName,
                            unsigned int flags,
                            sp<RefBase> &);

            typedef sp<MediaSource> (*localFuncCast_23)(
                sp<IOMX> const&,
                sp<MetaData> const&,
                bool,
                sp<MediaSource> const&,
                char const*,
                unsigned int);

            localFuncCast_4series lfc_4series = (localFuncCast_4series)searchSymbol("_ZN7android8OMXCodec6CreateERKNS_2spINS_4IOMXEEERKNS1_INS_8MetaDataEEEbRKNS1_INS_11MediaSourceEEEPKcjRKNS1_I13ANativeWindowEE");
            localFuncCast_23 lfc_23 = (localFuncCast_23)searchSymbol("_ZN7android8OMXCodec6CreateERKNS_2spINS_4IOMXEEERKNS1_INS_8MetaDataEEEbRKNS1_INS_11MediaSourceEEEPKcj");
            sp<RefBase> nullWindow(NULL);

            if(lfc_4series)
                return lfc_4series(iomx, metadata, flag, mediaSource, string, value, nullWindow);
            else if(lfc_23)
                return lfc_23(iomx, metadata, flag, mediaSource, string, value);

            LOGE("Unable to resolve OMXCodec::Create");
            assert(0);
            return NULL;
        }

        //  android::OMXCodec::Create(android::sp<android::IOMX> const&, android::sp<android::MetaData> const&, bool,
        //                            android::sp<android::MediaSource> const&, char const*, unsigned int, android::sp<ANativeWindow> const&)
        static sp<MediaSource23> Create23(sp<IOMX> &iomx,
                            sp<MetaData> &metadata,
                            bool flag,
                            sp<MediaSource23> &mediaSource,
                            char const *string,
                            unsigned int value /*,
                            sp<ANativeWindow> &nativeWindow */)
        {
            typedef sp<MediaSource23> (*localFuncCast_4series)(sp<IOMX> &iomx,
                            sp<MetaData> &metadata,
                            bool createEncoder,
                            sp<MediaSource23> &mediaSource,
                            char const *matchComponentName,
                            unsigned int flags,
                            sp<RefBase> &);

            typedef sp<MediaSource23> (*localFuncCast_23)(
                sp<IOMX> const&,
                sp<MetaData> const&,
                bool,
                sp<MediaSource23> const&,
                char const*,
                unsigned int);

            localFuncCast_4series lfc_4series = (localFuncCast_4series)searchSymbol("_ZN7android8OMXCodec6CreateERKNS_2spINS_4IOMXEEERKNS1_INS_8MetaDataEEEbRKNS1_INS_11MediaSourceEEEPKcjRKNS1_I13ANativeWindowEE");
            localFuncCast_23 lfc_23 = (localFuncCast_23)searchSymbol("_ZN7android8OMXCodec6CreateERKNS_2spINS_4IOMXEEERKNS1_INS_8MetaDataEEEbRKNS1_INS_11MediaSourceEEEPKcj");
            sp<RefBase> nullWindow(NULL);

            if(lfc_4series)
                return lfc_4series(iomx, metadata, flag, mediaSource, string, value, nullWindow);
            else if(lfc_23)
                return lfc_23(iomx, metadata, flag, mediaSource, string, value);

            LOGE("Unable to resolve OMXCodec::Create");
            assert(0);
            return NULL;
        }
    };

    // Provide our own data source with cross-device compatibility.
    class HLSDataSource : public DataSource
    {
    public:
        HLSDataSource(): mSourceIdx(0), mSegmentStartOffset(0), mOffsetAdjustment(0),
        				 mContinuityEra(0), mQuality(0), mStartTime(0)
        {
            // Initialize our mutex.
            int err = initRecursivePthreadMutex(&lock);
            LOGI(" HLSDataSource mutex err = %d", err);
        }

        void dummyDtor()
        {

        }

        void patchTable()
        {
            // Fake up the right vtable.

            // First look up and make a copy of the official vtable.
            // This leaks a bit of RAM per source but we can deal with that later.
            // Update - we can't resolve this symbol on some x86 devices, and it turns
            // out we don't need it - we can just set stuff to 0s and it works OK.
            // This is obviously a bit finicky but adequate for now.
            //void *officialVtable = searchSymbol("_ZTVN7android10DataSourceE");
            //assert(officialVtable); // Gotta have a vtable!
            void *newVtable = malloc(1024); // Arbitrary size... As base class we
                                            // we always get ptr to start of vtable.
            //memcpy(newVtable, officialVtable, 1024);
            memset(newVtable, 0, 1024);

            // Now we can patch the vtable...
            void ***fakeObj = (void***)this;

            // Take into account mandatory vtable offsets.
            fakeObj[0] = (void**)(((int*)newVtable) + 2);

            // Dump some useful known symbols.
            #if 0
            #define DLSYM_MACRO(s) LOGI("   o %s=%p", #s, searchSymbol(#s));

            DLSYM_MACRO(_ZN7android10DataSource7getSizeEPx); // Only on higher
            DLSYM_MACRO(_ZN7android10DataSource7getSizeEPl); // Only on 2.3
            DLSYM_MACRO(_ZN7android10DataSource9getUInt16ExPt);

            #undef DLSYM_MACRO
            #endif

            // Dump the vtable.
            for(int i=0; i<16; i++)
            {
              LOGV2("vtable[%d] = %p", i, fakeObj[0][i]);
            }

            // The compiler may complain about these as we are getting into
            // pointer-to-member-function (pmf) territory. However, we aren't
            // actually treating them as such here because there's no instance.
            // So we should be OK! But if the values here report as not code
            // segment values then you might need to revisit.
            LOGSYMBOLERROR(" _initCheck=%p", (void*)&HLSDataSource::_initCheck);
            LOGSYMBOLERROR(" _readAt=%p", (void*)&HLSDataSource::_readAt);
            LOGSYMBOLERROR(" _getSize=%p", (void*)&HLSDataSource::_getSize);

            LOGSYMBOLERROR(" _readAt_23=%p", (void*)&HLSDataSource::_readAt_23);
            LOGSYMBOLERROR(" _getSize_23=%p", (void*)&HLSDataSource::_getSize_23);

            // And override the pointers as appropriate.
            if(AVSHIM_USE_NEWDATASOURCEVTABLE)
            {
                // Stub in a dummy function for the other entries so that if
                // e.g. someone tries to call a destructor it won't segfault.
                for(int i=0; i<18; i++)
                    fakeObj[0][i] = (void*)&HLSDataSource::dummyDtor;

                // 4.x entry points
                fakeObj[0][6] = (void*)&HLSDataSource::_initCheck;
                fakeObj[0][7] = (void*)&HLSDataSource::_readAt;
                fakeObj[0][8] = (void*)&HLSDataSource::_getSize;
            }
            else
            {
                // Confirm what we can that we're doing this right...
                void *oldGetSize = searchSymbol("_ZN7android10DataSource7getSizeEPl");
                void *oldGetSize2 = searchSymbol("_ZN7android10DataSource7getSizeEPx");
                LOGI("  oldGetSize_l=%p oldGetSize_x=%p fakeObj[0][8]=%p", oldGetSize, oldGetSize2, fakeObj[0][8]);

                // Stub in a dummy function for the other entries so that if
                // e.g. someone tries to call a destructor it won't segfault.
                for(int i=0; i<18; i++)
                    fakeObj[0][i] = (void*)&HLSDataSource::dummyDtor;

                // 2.3 entry points
                fakeObj[0][6] = (void*)&HLSDataSource::_initCheck;
                fakeObj[0][7] = (void*)&HLSDataSource::_readAt_23;
                fakeObj[0][8] = (void*)&HLSDataSource::_getSize_23;
            }
        }

        virtual ~HLSDataSource()
        {

        }

        void clearSources()
        {
            AutoLock locker(&lock, __func__);
        	mSources.clear();
        	mSourceIdx = 0;
        	mOffsetAdjustment = 0;
        }

        bool isSameEra(int quality, int continuityEra)
        {
        	return mSources.size() == 0 || (mSources.size() > 0 && quality == mQuality && continuityEra == mContinuityEra);
        }

        status_t append(const char* uri, int quality, int continuityEra, double startTime, int cryptoId)
        {
            AutoLock locker(&lock, __func__);

            if (mSources.size() > 0 && (quality != mQuality || continuityEra != mContinuityEra))
            	return INFO_DISCONTINUITY;

            if (mSources.size() == 0) // storing the start time of the first segment in the source list
            	mStartTime = startTime;

            mQuality = quality;
            mContinuityEra = continuityEra;

            // Small memory leak, look out.
            uri = strdup(uri);

            // Stick it in our sources.
            mSources.push_back(uri);

            return OK;
        }

        void logContinuityInfo()
        {
        	LOGI("Quality = %d | Continuity Era = %d | Time = %f | First URI = %s ", mQuality, mContinuityEra, mStartTime, *mSources.begin()  );
        }

        int getQualityLevel()
        {
        	return mQuality;
        }

        int getContinuityEra()
        {
        	return mContinuityEra;
        }

        double getStartTime()
        {
        	return mStartTime;
        }

        int getPreloadedSegmentCount()
        {
            AutoLock locker(&lock, __func__);
            int res = (mSources.size() - mSourceIdx) - 1;
            return res;
        }

        void touch()
        {
            AutoLock locker(&lock, __func__);
            for (int i = mSourceIdx; i < mSources.size(); ++i)
            {
            	HLSSegmentCache::touch(mSources[i]);
            }
        }

        status_t _initCheck()
        {
            AutoLock locker(&lock, __func__);
            LOGI("_initCheck!");

            // With the new segment cache, we cannot fail!

            return OK;
        }

        ssize_t _readAt(off64_t offset, void* data, size_t size)
        {
            AutoLock locker(&lock, __func__);

            // Sanity check.
            if(mSources.size() == 0)
            {
                LOGE("No sources in HLSDataSource! Aborting read...");
                return 0;
            }

            LOGDATAMINING("Attempting _readAt mSources[mSourceIdx]=%s %lld %p %d mOffsetAdjustment=%lld", mSources[mSourceIdx], offset, data, size, mOffsetAdjustment);

            // Calculate adjusted offset based on reads so far. The TSExtractor
            // always reads in order.
            ssize_t adjOffset = offset - mOffsetAdjustment;

            // Read chunks from the segment cache until we've fulfilled the request.
            ssize_t sizeLeft = size;
            ssize_t readSize = 0;
            int safety = 10;
            while(sizeLeft > 0 && safety--)
            {
                // If we have a negative adjOffset it means we moved into a new segment - but readSize should compensate.
                while(adjOffset + readSize < 0)
                {
                	LOGDATAMINING("Got negative offset, adjOffset=%ld readSize=%ld", adjOffset, readSize);

                    assert(mSourceIdx > 0);

                    // Walk back to preceding source!
                    int64_t sourceSize = HLSSegmentCache::getSize(mSources[mSourceIdx-1]);
                    LOGDATAMINING("Retreating by %lld bytes!", sourceSize);

                    mOffsetAdjustment -= sourceSize;
                    adjOffset += sourceSize;

                    mSourceIdx--;
                }

                // Attempt a read. Blocking and tries VERY hard not to fail.
                ssize_t lastReadSize = HLSSegmentCache::read(mSources[mSourceIdx], adjOffset + readSize, sizeLeft, ((unsigned char*)data) + readSize);

                if (sizeLeft - lastReadSize < 0)
                {
                	LOGW("NEGATIVE SIZE LEFT: sizeLeft=%d lastReadSize=%d source=%s", sizeLeft, lastReadSize, mSources[mSourceIdx]); // Something happened to the segment - maybe it's 404
                	assert (sizeLeft - lastReadSize >= 0);
                }


                // Account for read.
                sizeLeft -= lastReadSize;
                readSize += lastReadSize;

                // If done reading, then we can break out.
                if(sizeLeft == 0)
                    break;

                // Otherwise, we need to move to the next source if we have one.
                if(mSourceIdx + 1 < mSources.size())
                {
                    // Advance by the current source size.
                    int64_t sourceSize = HLSSegmentCache::getSize(mSources[mSourceIdx]);
                    LOGDATAMINING("Advancing by %lld bytes, size of current source", sourceSize);

                    mOffsetAdjustment += sourceSize;
                    adjOffset -= sourceSize;

                    mSourceIdx++;
                }
                else
                {
                    // No more sources?
                    LOGI("Reached end of segment list.");
                    break;
                }
            }

            // Make sure we didn't do anything weird.
            if(safety == 0)
            {
                LOGE("****** HIT SAFETY ON READ LOOP");
                LOGE("****** HIT SAFETY ON READ LOOP");
                LOGE("****** HIT SAFETY ON READ LOOP");
            }

            // Return what we read.
            return readSize;
        }

        ssize_t _readAt_23(int64_t offset, void* data, unsigned int size)
        {
            // Bump control to the main path.
            return _readAt(offset, data, size);
        }

        status_t _getSize(off64_t* size)
        {
            AutoLock locker(&lock, __func__);
            LOGV("Attempting _getSize");
            //status_t rval = mSources[mSourceIdx]->getSize(size);
            *size = 0;
            LOGV("getSize - %p | size = %lld",this, *size);
            return 0;
        }

        status_t _getSize_23(off64_t* size)
        {
            return _getSize(size);
        }

    private:

        pthread_mutex_t lock;
        std::vector< const char * > mSources;
        uint32_t mSourceIdx;
        off64_t mSegmentStartOffset;
        off64_t mOffsetAdjustment;
        int mQuality;
        int mContinuityEra;
        double mStartTime;

    };

    struct ColorConverter
    {
        char buffer[8192];

        ColorConverter(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to)
        {
            typedef void (*localFuncCast)(void *thiz, OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android14ColorConverterC1E20OMX_COLOR_FORMATTYPES1_");
            assert(lfc);
            lfc(this, from, to);
        }

        ~ColorConverter()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android14ColorConverterD1Ev");
            assert(lfc);
            lfc(this);
        }


        bool isValid()
        {
            typedef bool (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android14ColorConverter7isValidEv");
            assert(lfc);
            return lfc(this);
        }

        void convert(
                const void *srcBits,
                size_t srcWidth, size_t srcHeight,
                size_t srcCropLeft, size_t srcCropTop,
                size_t srcCropRight, size_t srcCropBottom,
                void *dstBits,
                size_t dstWidth, size_t dstHeight,
                size_t dstCropLeft, size_t dstCropTop,
                size_t dstCropRight, size_t dstCropBottom)
        {
            typedef void (*localFuncCast)(void *thiz, const void *srcBits,
                size_t srcWidth, size_t srcHeight,
                size_t srcCropLeft, size_t srcCropTop,
                size_t srcCropRight, size_t srcCropBottom,
                void *dstBits,
                size_t dstWidth, size_t dstHeight,
                size_t dstCropLeft, size_t dstCropTop,
                size_t dstCropRight, size_t dstCropBottom);

            typedef void (*localFuncCast2)(void *thiz, size_t width, size_t height, const void *srcBits, size_t srcSkip, void *dstBits, size_t dstSkip);

            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android14ColorConverter7convertEPKvjjjjjjPvjjjjjj");
            localFuncCast2 lfc2 = (localFuncCast2)searchSymbol("_ZN7android14ColorConverter7convertEjjPKvjPvj");

            LOGV("color convert = %p or %p srcBits=%p dstBits=%p", lfc, lfc2, srcBits, dstBits);

            if(lfc)
                lfc(this, srcBits,
                     srcWidth,  srcHeight,
                     srcCropLeft,  srcCropTop,
                     srcCropRight,  srcCropBottom,
                    dstBits,
                     dstWidth,  dstHeight,
                     dstCropLeft,  dstCropTop,
                     dstCropRight,  dstCropBottom);
            else if(lfc2)
            {
                lfc2(this, srcWidth, srcHeight, srcBits, 0, dstBits, dstWidth * 2);
            }
            else
            {
                LOGE("Failed to find conversion function.");
            }

            // Debug line.
            //for(int i=0; i<dstWidth*dstHeight; i++) ((unsigned short*)dstBits)[i] = rand();            
        }

    };


    /* Audio stream types */
    typedef enum {
        AUDIO_STREAM_DEFAULT          = -1,
        AUDIO_STREAM_VOICE_CALL       = 0,
        AUDIO_STREAM_SYSTEM           = 1,
        AUDIO_STREAM_RING             = 2,
        AUDIO_STREAM_MUSIC            = 3,
        AUDIO_STREAM_ALARM            = 4,
        AUDIO_STREAM_NOTIFICATION     = 5,
        AUDIO_STREAM_BLUETOOTH_SCO    = 6,
        AUDIO_STREAM_ENFORCED_AUDIBLE = 7, /* Sounds that cannot be muted by user and must be routed to speaker */
        AUDIO_STREAM_DTMF             = 8,
        AUDIO_STREAM_TTS              = 9,

        AUDIO_STREAM_CNT,
        AUDIO_STREAM_MAX              = AUDIO_STREAM_CNT - 1,
    } audio_stream_type_t;

    // Check whether the stream defined by meta can be offloaded to hardware
    inline bool canOffloadStream(const sp<MetaData>& meta, bool hasVideo,
                          bool isStreaming, audio_stream_type_t streamType)
    {
        typedef bool (*localFuncCast)(const sp<MetaData>& meta, bool hasVideo,
                					  bool isStreaming, audio_stream_type_t streamType);
        localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android16canOffloadStreamERKNS_2spINS_8MetaDataEEEbb19audio_stream_type_t");
        if(!lfc)
        {
            LOGI("Cannot find canOffloadStream! Returning false...");
            return false;
        }
        assert(lfc);
        return lfc(meta, hasVideo, isStreaming, streamType);
    }

}

#endif

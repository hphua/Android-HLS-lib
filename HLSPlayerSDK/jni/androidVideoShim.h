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

#include "debug.h"

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
    #define ANDROID_VIDEO_SHIM_CHECK_IS_4x (android_video_shim::gAPILevel > 10)

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
            LOGV("RefBase - Inc'ing %p %p %p", (void*)this, id, lfc);
            assert(lfc);
            lfc(this, id);
        }

        void decStrong(void *id)
        {
            typedef void (*localFuncCast)(void *thiz, void *id);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android7RefBase9decStrongEPKv");
            LOGV("RefBase - Dec'ing %p %p %p", (void*)this, id, lfc);
            assert(lfc);
            lfc(this, id);
        }

        RefBase()
        {
            // Call our c'tor.
            LOGV("RefBase - ctor %p", this);
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android7RefBaseC2Ev");
            assert(lfc);
            lfc(this);
        }

        virtual ~RefBase()
        {
        }

        virtual void            onFirstRef() {};
        virtual void            onLastStrongRef(const void* id) {};
        virtual bool            onIncStrongAttempted(uint32_t flags, const void* id) {};
        virtual void            onLastWeakRef(const void* id) {};

        void *mRefs;
    };

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
    // ---------------------------------------------------------------------------
    template <typename T>
    class sp
    {
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
        void set_pointer(T* ptr);
        T* m_ptr;
    };
    #undef COMPARE

    // ---------------------------------------------------------------------------
    // No user serviceable parts below here.
    template<typename T>
    sp<T>::sp(T* other)
    : m_ptr(other)
      {
        if (other) other->incStrong(this);
      }
    template<typename T>
    sp<T>::sp(const sp<T>& other)
    : m_ptr(other.m_ptr)
      {
        if (m_ptr) m_ptr->incStrong(this);
      }
    template<typename T> template<typename U>
    sp<T>::sp(U* other) : m_ptr(other)
    {
        if (other) ((T*)other)->incStrong(this);
    }
    template<typename T> template<typename U>
    sp<T>::sp(const sp<U>& other)
    : m_ptr(other.m_ptr)
      {
        if (m_ptr) m_ptr->incStrong(this);
      }
    template<typename T>
    sp<T>::~sp()
    {
        if (m_ptr) m_ptr->decStrong(this);
    }
    template<typename T>
    sp<T>& sp<T>::operator = (const sp<T>& other) {
        T* otherPtr(other.m_ptr);
        if (otherPtr) otherPtr->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = otherPtr;
        return *this;
    }
    template<typename T>
    sp<T>& sp<T>::operator = (T* other)
    {
        if (other) other->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = other;
        return *this;
    }
    template<typename T> template<typename U>
    sp<T>& sp<T>::operator = (const sp<U>& other)
    {
        T* otherPtr(other.m_ptr);
        if (otherPtr) otherPtr->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = otherPtr;
        return *this;
    }
    template<typename T> template<typename U>
    sp<T>& sp<T>::operator = (U* other)
    {
        if (other) ((T*)other)->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = other;
        return *this;
    }
    template<typename T>
    void sp<T>::force_set(T* other)
    {
        other->forceIncStrong(this);
        m_ptr = other;
    }
    template<typename T>
    void sp<T>::clear()
    {
        if (m_ptr) {
            m_ptr->decStrong(this);
            m_ptr = 0;
        }
    }
    template<typename T>
    void sp<T>::set_pointer(T* ptr) {
        m_ptr = ptr;
    }

    class IOMX : public virtual RefBase
    {
    public:

    };

    struct AMessage;
    class String8
    {
    public:

        const char *data;
         String8(const char *d = NULL) : data(d) {}
    };
    class DrmManagerClient;
    class HLSDataSource;

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

            //for(int i=0; i<24; i++)
            //    LOGI("virtual layout[%d]=%p", i, fakeObj[0][i]);

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

            /*for(int i=0; i<17; i++)
                LOGI("virtual layout[%d]=%p", i, fakeObj[0][i]); */

            LOGV("FileSource::readAt should be %p", searchSymbol("_ZN7android10FileSource9readAtDRMExPvj"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            LOGV("virtual readAt=%p", lfc);
            ssize_t r = lfc((void*)this, offset, data, size);
            //LOGI("    o got %ld", r);
            return r;
        }

        ssize_t readAt_23(off_t offset, void *data, size_t size)
        {
            const int vtableOffset = 7;
            typedef ssize_t (*localFuncCast)(void *thiz, off_t offset, void *data, size_t size);
            localFuncCast **fakeObj = (localFuncCast**)this;

            /*for(int i=0; i<17; i++)
                LOGI("virtual layout[%d]=%p", i, fakeObj[0][i]); */

            //LOGI("FileSource::readAt should be %p", searchSymbol("_ZN7android10FileSource9readAtDRMExPvj"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            //LOGI("virtual readAt=%p", lfc);
            ssize_t r = lfc((void*)this, offset, data, size);
            //LOGI("    o got %ld", r);
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

            /*for(int i=0; i<17; i++)
                LOGI("virtual layout[%d]=%p", i, fakeObj[0][i]); */

            //LOGI("FileSource::getSize should be %p", searchSymbol("_ZN7android10FileSource7getSizeEPx"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            //LOGI("virtual getSize=%p", lfc);
            status_t r = lfc((void*)this, size);
            //LOGI("    o got %d", r);
            return r;
        }

        status_t getSize_23(off_t *size)
        {
            const int vtableOffset = 8;
            typedef status_t (*localFuncCast)(void *thiz, off_t *size);
            localFuncCast **fakeObj = (localFuncCast**)this;

            /*for(int i=0; i<17; i++)
                LOGI("virtual layout[%d]=%p", i, fakeObj[0][i]); */

            //LOGI("FileSource::getSize should be %p", searchSymbol("_ZN7android10FileSource7getSizeEPx"));

            localFuncCast lfc = (localFuncCast)fakeObj[0][vtableOffset];
            //LOGI("virtual getSize=%p", lfc);
            status_t r = lfc((void*)this, size);
            //LOGI("    o got %d", r);
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
        // The underlying data remains the responsibility of the caller!
        /*MediaBuffer(void *data, size_t size)
        {
            assert(0);
        }

        MediaBuffer(size_t size)
        {
            assert(0);
        }

        MediaBuffer(const sp<GraphicBuffer>& graphicBuffer)
        {
            assert(0);
        }

        MediaBuffer(const sp<ABuffer> &buffer)
        {
            assert(0);
        }*/

        // Decrements the reference count and returns the buffer to its
        // associated MediaBufferGroup if the reference count drops to 0.
        void release()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11MediaBuffer7releaseEv");
            assert(lfc);
            LOGI("MediaBuffer::release = %p", lfc);

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
            LOGI("MediaBuffer::data = %p this=%p", lfc, this);
            return lfc(this);
        }

        size_t size()
        {
            typedef size_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android11MediaBuffer4sizeEv");
            assert(lfc);
            LOGI("MediaBuffer::size = %p this=%p", lfc, this);
            return lfc(this);
        }
        size_t range_offset() const
        {
            assert(0);
        }

        size_t range_length()
        {
            typedef size_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android11MediaBuffer12range_lengthEv");
            assert(lfc);
            LOGI("MediaBuffer::range_length = %p this=%p", lfc, this);
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
            LOGV("MediaBuffer::meta_data = %p this=%p", lfc, this);

            return lfc(this);
        }

        // Clears meta data and resets the range to the full extent.
        void reset()
        {
            assert(0);
        }

        /*void setObserver(MediaBufferObserver *group)
        {
            assert(0);
        }*/

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
            const int vtableOffset = ANDROID_VIDEO_SHIM_CHECK_IS_4x ? 2 : 8;
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
            const int vtableOffset = 3;
            typedef status_t (*localFuncCast)(void *thiz, MediaBuffer **buffer, const ReadOptions *options);
            localFuncCast **fakeObj = (localFuncCast **)this;
            localFuncCast lfc = fakeObj[0][vtableOffset];
            LOGI("virtual read=%p", lfc);
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

        private:
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
        virtual ~MediaSource();


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
                LOGI("   vtable[%d] = %p", i, fakeObj[0][i]);
            LOGI("MPEG4Source::getFormat should be %p", searchSymbol("_ZN7android13MPEG2TSSource5startEPNS_8MetaDataE"));
            LOGI("AnotherPacketSource::getFormat should be %p", searchSymbol("_ZN7android19AnotherPacketSource5startEPNS_8MetaDataE"));
            LOGI("AACSource::getFormat should be %p", searchSymbol("_ZN7android9AACSource9getFormatEv"));
            LOGI("MPEG2TSSource::getFormat should be %p", searchSymbol("_ZN7android13MPEG2TSSource9getFormatEv"));
            LOGI("OMXCodec::getFormat might be %p", searchSymbol("_ZN7android8OMXCodec5startEPNS_8MetaDataE"));

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
            const int vtableOffset = ANDROID_VIDEO_SHIM_CHECK_IS_4x ? 2 : 8;
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
            LOGI("virtual read=%p", lfc);
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

        private:
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
        virtual ~MediaSource23();


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

            LOGI("this = %p", this);
            LOGI("*this = %p", *(void**)this);
            LOGI("Mpeg2TSExtractor::countTracks should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor11countTracksEv"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            //for(int i=0; i<10; i++)
            //    LOGI("virtual layout[%d]=%p", i, fakeObj[i]);


            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGI("virtual countTracks=%p", lfc);
            return lfc(this);
        }

        sp<MediaSource> getTrack(size_t index)
        {
            const int vtableOffset = 7;
            typedef sp<MediaSource> (*localFuncCast)(void *thiz, size_t idx);

            LOGI("this = %p", this);
            LOGI("*this = %p", *(void**)this);
            LOGI("Mpeg2TSExtractor::getTrack should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor8getTrackEj"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            //for(int i=0; i<10; i++)
            //    LOGI("virtual layout[%d]=%p", i, fakeObj[i]);

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGI("virtual getTrack=%p", lfc);
            return lfc(this, index);
        }

        sp<MediaSource23> getTrack23(size_t index)
        {
            const int vtableOffset = 7;
            typedef sp<MediaSource23> (*localFuncCast)(void *thiz, size_t idx);

            LOGI("this = %p", this);
            LOGI("*this = %p", *(void**)this);
            LOGI("Mpeg2TSExtractor::getTrack should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor8getTrackEj"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            //for(int i=0; i<10; i++)
            //    LOGI("virtual layout[%d]=%p", i, fakeObj[i]);

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGI("virtual getTrack=%p", lfc);
            return lfc(this, index);
        }

        sp<MetaData> getTrackMetaData(
                size_t index, uint32_t flags = 0)
		{
            const int vtableOffset = 8;
            typedef sp<MetaData> (*localFuncCast)(void *thiz, size_t idx, uint32_t flags);

            LOGI("this = %p", this);
            LOGI("*this = %p", *(void**)this);
            LOGI("Mpeg2TSExtractor::getTrackMetaData should be %p", searchSymbol("_ZN7android16MPEG2TSExtractor16getTrackMetaDataEjj"));

            localFuncCast **fakeObj = *((localFuncCast***)this);

            //for(int i=0; i<10; i++)
            //    LOGI("virtual layout[%d]=%p", i, fakeObj[i]);

            localFuncCast lfc = (localFuncCast)fakeObj[vtableOffset];
            LOGI("virtual getTrackMetaData=%p", lfc);
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
        char data[8192];

        MetaData()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaDataC2Ev");
            assert(lfc);
            lfc(this);
        }

        ~MetaData()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android8MetaDataD1Ev");
            assert(lfc);
            lfc(this);
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

        void dumpToLog()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZNK7android8MetaData9dumpToLogEv");
            if(!lfc)
            {
                LOGI("Failing to dumpToLog, symbol not found.");
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

            assert(0);
            return NULL;
        }
    };

    class TimeSource {
    public:
        TimeSource() {}
        virtual ~TimeSource() {}

         int64_t getRealTimeUs();

    private:
        TimeSource(const TimeSource &);
        TimeSource &operator=(const TimeSource &);
    };

    // How do we override a DataSource?
    class HLSDataSource : public DataSource
    {
    public:
        HLSDataSource(): mSourceIdx(0), mSegmentStartOffset(0), mOffsetAdjustment(0)
        {
        }

        void patchTable()
        {
            // Fake up the right vtable.

            // First look up and make a copy of the official vtable.
            // This leaks a bit of RAM per source but we can deal with that later.
            void *officialVtable = searchSymbol("_ZTVN7android10DataSourceE");
            assert(officialVtable); // Gotta have a vtable!
            void *newVtable = malloc(1024); // Arbitrary size... As base class we
                                            // we always get ptr to start of vtable.
            memcpy(newVtable, officialVtable, 1024);

            // Now we can patch the vtable...
            void ***fakeObj = (void***)this;

            // Take into account mandatory vtable offsets.
            fakeObj[0] = (void**)(((int*)newVtable) + 2);

/*
No add
07-08 06:28:04.539: I/native-activity(22824): vtable[0] = 0x0
07-08 06:28:04.539: I/native-activity(22824): vtable[1] = 0x0
07-08 06:28:04.539: I/native-activity(22824): vtable[2] = 0xa2f4930d
07-08 06:28:04.539: I/native-activity(22824): vtable[3] = 0xa2f49331
*/

            /*
Add +2
07-08 06:31:38.914: I/native-activity(23530): vtable[0] = 0x0
07-08 06:31:38.925: I/native-activity(23530): vtable[1] = 0x930d0000
07-08 06:31:38.925: I/native-activity(23530): vtable[2] = 0x9331a2f4
07-08 06:31:38.925: I/native-activity(23530): vtable[3] = 0x49e9a2f4

Add 2 with int*
07-08 06:32:38.838: I/native-activity(23750): vtable[0] = 0xa2f4930d
07-08 06:32:38.838: I/native-activity(23750): vtable[1] = 0xa2f49331
07-08 06:32:38.838: I/native-activity(23750): vtable[2] = 0xa81149e9
            */

            // Dump some useful known symbols.
            #if 1
            #define DLSYM_MACRO(s) LOGI("   o %s=%p", #s, searchSymbol(#s));

            DLSYM_MACRO(_ZN7android10DataSource7getSizeEPx); // Only on higher
            DLSYM_MACRO(_ZN7android10DataSource7getSizeEPl); // Only on 2.3
            DLSYM_MACRO(_ZN7android10DataSource9getUInt16ExPt);

            #undef DLSYM_MACRO
            #endif

            // Dump the vtable.
            for(int i=0; i<16; i++)
              LOGV("vtable[%d] = %p", i, fakeObj[0][i]);

            // The compiler may complain about these as we are getting into
            // pointer-to-member-function (pmf) territory. However, we aren't
            // actually treating them as such here because there's no instance.
            // So we should be OK! But if the values here report as not code
            // segment values then you might need to revisit.
            LOGI(" _initCheck=%p", (void*)&HLSDataSource::_initCheck);
            LOGI(" _readAt=%p", (void*)&HLSDataSource::_readAt);
            LOGI(" _getSize=%p", (void*)&HLSDataSource::_getSize);

            // And override the pointers as appropriate.
            if(ANDROID_VIDEO_SHIM_CHECK_IS_4x)
            {
                // 4.x
                fakeObj[0][6] = (void*)&HLSDataSource::_initCheck;
                fakeObj[0][7] = (void*)&HLSDataSource::_readAt;
                fakeObj[0][8] = (void*)&HLSDataSource::_getSize;
            }
            else
            {
                // Confirm what we can that we're doing this right...
                void *oldGetSize = searchSymbol("_ZN7android10DataSource7getSizeEPl");
                LOGI("  oldGetSize=%p fakeObj[0][8]=%p", oldGetSize, fakeObj[0][8]);

                // 2.3
                fakeObj[0][6] = (void*)&HLSDataSource::_initCheck;
                fakeObj[0][7] = (void*)&HLSDataSource::_readAt_23;
                fakeObj[0][8] = (void*)&HLSDataSource::_getSize_23;
            }
        }

        virtual ~HLSDataSource()
        {

        }

        virtual void foo() { assert(0); }

        status_t append(const char* uri)
        {
            sp<DataSource> dataSource = DataSource::CreateFromURI(uri);
            if(!dataSource.get())
            {
                LOGI("Failed to create DataSource for %s", uri);
                return -1;
            }

            status_t rval = dataSource->initCheck();
            LOGE("DataSource initCheck() result: %s", strerror(-rval));
            mSources.push_back(dataSource);
            return rval;
        }

        int getPreloadedSegmentCount()
        {
            return mSources.size() - mSourceIdx;
        }

        status_t _initCheck() const
        {
            LOGI("_initCheck - Source Count = %d", mSources.size());
            if (mSources.size() > 0)
                return mSources[mSourceIdx]->initCheck();

            LOGI("   o Returning NO_INIT");
            return NO_INIT;
        }

        ssize_t _readAt(off64_t offset, void* data, size_t size)
        {
            //LOGV("Attempting _readAt");

            off64_t sourceSize = 0;
            mSources[mSourceIdx]->getSize(&sourceSize);

            off64_t adjoffset = offset - mOffsetAdjustment;  // get our adjusted offset. It should always be >= 0

            if (adjoffset >= sourceSize && (mSourceIdx + 1 < mSources.size())) // The thinking here is that if we run out of sources, we should just let it pass through to read the last source at the invalid buffer, generating the proper return code
                                                                               // However, this doesn't solve the problem of delayed fragment downloads... not sure what to do about that, yet
                                                                               // This should at least prevent us from crashing
            {
            	LOGI("Changing Segments: curIdx=%d, nextIdx=%d", mSourceIdx, mSourceIdx + 1);
                adjoffset -= sourceSize; // subtract the size of the current source from the offset
                mOffsetAdjustment += sourceSize; // Add the size of the current source to our offset adjustment for the future
                ++mSourceIdx;
            }

            ssize_t rsize = mSources[mSourceIdx]->readAt(adjoffset, data, size);

            if (rsize < size && mSourceIdx + 1 < mSources.size())
            {
            	LOGI("Incomplete Read - Changing Segments : curIdx=%d, nextIdx=%d", mSourceIdx, mSourceIdx + 1);
                adjoffset -= sourceSize; // subtract the size of the current source from the offset
                mOffsetAdjustment += sourceSize; // Add the size of the current source to our offset adjustment for the future
                ++mSourceIdx;

                LOGI("Reading At %lld | New ", adjoffset + rsize);
                rsize += mSources[mSourceIdx]->readAt(adjoffset + rsize, (unsigned char*)data + rsize, size - rsize);
            }


            //LOGI("%p | getSize = %lld | offset=%lld | offsetAdjustment = %lld | adjustedOffset = %lld | requested size = %d | rsize = %ld",
            //                this, sourceSize, offset, mOffsetAdjustment, adjoffset, size, rsize);
            return rsize;
        }

        ssize_t _readAt_23(off_t offset, void* data, size_t size)
        {
            LOGV("Attempting _readAt");

            off_t sourceSize = 0;
            mSources[mSourceIdx]->getSize_23(&sourceSize);

            off_t adjoffset = offset - mOffsetAdjustment;  // get our adjusted offset. It should always be >= 0

            if (adjoffset >= sourceSize && (mSourceIdx + 1 < mSources.size())) // The thinking here is that if we run out of sources, we should just let it pass through to read the last source at the invalid buffer, generating the proper return code
                                                                               // However, this doesn't solve the problem of delayed fragment downloads... not sure what to do about that, yet
                                                                               // This should at least prevent us from crashing
            {
                adjoffset -= sourceSize; // subtract the size of the current source from the offset
                mOffsetAdjustment += sourceSize; // Add the size of the current source to our offset adjustment for the future
                ++mSourceIdx;
            }

            ssize_t rsize = mSources[mSourceIdx]->readAt_23(adjoffset, data, size);

            LOGV("%p | getSize = %ld | offset=%ld | offsetAdjustment = %lld | adjustedOffset = %ld | requested size = %d | rsize = %ld",
                            this, sourceSize, offset, mOffsetAdjustment, adjoffset, size, rsize);
            return rsize;
        }

        status_t _getSize(off64_t* size)
        {
            LOGI("Attempting _getSize");
            //status_t rval = mSources[mSourceIdx]->getSize(size);
            *size = 0;
            LOGI("getSize - %p | size = %lld",this, *size);
            return 0;
        }

        status_t _getSize_23(off_t* size)
        {
            LOGI("Attempting _getSize");
            status_t rval = mSources[mSourceIdx]->getSize_23(size);
            LOGI("getSize - %p | size = %ld",this, *size);
            return rval;
        }

    private:

        std::vector< sp<DataSource> > mSources;
        uint32_t mSourceIdx;
        off64_t mSegmentStartOffset;
        off64_t mOffsetAdjustment;
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
        OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m = 0x7fa30c04,

        OMX_COLOR_FormatMax = 0x7FFFFFFF
    } OMX_COLOR_FORMATTYPE;

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

        status_t convert(
                const void *srcBits,
                size_t srcWidth, size_t srcHeight,
                size_t srcCropLeft, size_t srcCropTop,
                size_t srcCropRight, size_t srcCropBottom,
                void *dstBits,
                size_t dstWidth, size_t dstHeight,
                size_t dstCropLeft, size_t dstCropTop,
                size_t dstCropRight, size_t dstCropBottom)
        {
            typedef status_t (*localFuncCast)(void *thiz, const void *srcBits,
                size_t srcWidth, size_t srcHeight,
                size_t srcCropLeft, size_t srcCropTop,
                size_t srcCropRight, size_t srcCropBottom,
                void *dstBits,
                size_t dstWidth, size_t dstHeight,
                size_t dstCropLeft, size_t dstCropTop,
                size_t dstCropRight, size_t dstCropBottom);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android14ColorConverter7convertEPKvjjjjjjPvjjjjjj");
            //localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android14ColorConverter7convertEPKvjjjjjjPvjjjjjj");

            assert(lfc);
            return lfc(this, srcBits,
                 srcWidth,  srcHeight,
                 srcCropLeft,  srcCropTop,
                 srcCropRight,  srcCropBottom,
                dstBits,
                 dstWidth,  dstHeight,
                 dstCropLeft,  dstCropTop,
                 dstCropRight,  dstCropBottom);
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

    class MediaPlayerBase : public RefBase
    {
    public:
        // AudioSink: abstraction layer for audio output
        class AudioSink : public RefBase {
//        public:
//            enum cb_event_t {
//                CB_EVENT_FILL_BUFFER,   // Request to write more data to buffer.
//                CB_EVENT_STREAM_END,    // Sent after all the buffers queued in AF and HW are played
//                                        // back (after stop is called)
//                CB_EVENT_TEAR_DOWN      // The AudioTrack was invalidated due to use case change:
//                                        // Need to re-evaluate offloading options
//            };
//
//            // Callback returns the number of bytes actually written to the buffer.
//            typedef size_t (*AudioCallback)(
//                    AudioSink *audioSink, void *buffer, size_t size, void *cookie,
//                            cb_event_t event);
//
//            virtual             ~AudioSink() {}
//            virtual bool        ready() const = 0; // audio output is open and ready
//            virtual bool        realtime() const = 0; // audio output is real-time output
//            virtual ssize_t     bufferSize() const = 0;
//            virtual ssize_t     frameCount() const = 0;
//            virtual ssize_t     channelCount() const = 0;
//            virtual ssize_t     frameSize() const = 0;
//            virtual uint32_t    latency() const = 0;
//            virtual float       msecsPerFrame() const = 0;
//            virtual status_t    getPosition(uint32_t *position) const = 0;
//            virtual status_t    getFramesWritten(uint32_t *frameswritten) const = 0;
//            virtual int         getSessionId() const = 0;
//            virtual audio_stream_type_t getAudioStreamType() const = 0;
//
//            // If no callback is specified, use the "write" API below to submit
//            // audio data.
//            virtual status_t    open(
//                    uint32_t sampleRate, int channelCount, audio_channel_mask_t channelMask,
//                    audio_format_t format=AUDIO_FORMAT_PCM_16_BIT,
//                    int bufferCount=DEFAULT_AUDIOSINK_BUFFERCOUNT,
//                    AudioCallback cb = NULL,
//                    void *cookie = NULL,
//                    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE,
//                    const audio_offload_info_t *offloadInfo = NULL) = 0;
//
//            virtual status_t    start() = 0;
//            virtual ssize_t     write(const void* buffer, size_t size) = 0;
//            virtual void        stop() = 0;
//            virtual void        flush() = 0;
//            virtual void        pause() = 0;
//            virtual void        close() = 0;
//
//            virtual status_t    setPlaybackRatePermille(int32_t rate) { return INVALID_OPERATION; }
//            virtual bool        needsTrailingPadding() { return true; }
//
//            virtual status_t    setParameters(const String8& keyValuePairs) { return NO_ERROR; };
//            virtual String8     getParameters(const String8& keys) { return String8::empty(); };
        };

//                            MediaPlayerBase() : mCookie(0), mNotify(0) {}
//        virtual             ~MediaPlayerBase() {}
//        virtual status_t    initCheck() = 0;
//        virtual bool        hardwareOutput() = 0;
//
//        virtual status_t    setUID(uid_t uid) {
//            return INVALID_OPERATION;
//        }
//
//        virtual status_t    setDataSource(
//                const char *url,
//                const KeyedVector<String8, String8> *headers = NULL) = 0;
//
//        virtual status_t    setDataSource(int fd, int64_t offset, int64_t length) = 0;
//
//        virtual status_t    setDataSource(const sp<IStreamSource> &source) {
//            return INVALID_OPERATION;
//        }
//
//        // pass the buffered IGraphicBufferProducer to the media player service
//        virtual status_t    setVideoSurfaceTexture(
//                                    const sp<IGraphicBufferProducer>& bufferProducer) = 0;
//
//        virtual status_t    prepare() = 0;
//        virtual status_t    prepareAsync() = 0;
//        virtual status_t    start() = 0;
//        virtual status_t    stop() = 0;
//        virtual status_t    pause() = 0;
//        virtual bool        isPlaying() = 0;
//        virtual status_t    seekTo(int msec) = 0;
//        virtual status_t    getCurrentPosition(int *msec) = 0;
//        virtual status_t    getDuration(int *msec) = 0;
//        virtual status_t    reset() = 0;
//        virtual status_t    setLooping(int loop) = 0;
//        virtual player_type playerType() = 0;
//        virtual status_t    setParameter(int key, const Parcel &request) = 0;
//        virtual status_t    getParameter(int key, Parcel *reply) = 0;
//
//        // default no-op implementation of optional extensions
//        virtual status_t setRetransmitEndpoint(const struct sockaddr_in* endpoint) {
//            return INVALID_OPERATION;
//        }
//        virtual status_t getRetransmitEndpoint(struct sockaddr_in* endpoint) {
//            return INVALID_OPERATION;
//        }
//        virtual status_t setNextPlayer(const sp<MediaPlayerBase>& next) {
//            return OK;
//        }
//
//        // Invoke a generic method on the player by using opaque parcels
//        // for the request and reply.
//        //
//        // @param request Parcel that is positioned at the start of the
//        //                data sent by the java layer.
//        // @param[out] reply Parcel to hold the reply data. Cannot be null.
//        // @return OK if the call was successful.
//        virtual status_t    invoke(const Parcel& request, Parcel *reply) = 0;
//
//        // The Client in the MetadataPlayerService calls this method on
//        // the native player to retrieve all or a subset of metadata.
//        //
//        // @param ids SortedList of metadata ID to be fetch. If empty, all
//        //            the known metadata should be returned.
//        // @param[inout] records Parcel where the player appends its metadata.
//        // @return OK if the call was successful.
//        virtual status_t    getMetadata(const media::Metadata::Filter& ids,
//                                        Parcel *records) {
//            return INVALID_OPERATION;
//        };
//
//        void        setNotifyCallback(
//                void* cookie, notify_callback_f notifyFunc) {
//            Mutex::Autolock autoLock(mNotifyLock);
//            mCookie = cookie; mNotify = notifyFunc;
//        }
//
//        void        sendEvent(int msg, int ext1=0, int ext2=0,
//                              const Parcel *obj=NULL) {
//            Mutex::Autolock autoLock(mNotifyLock);
//            if (mNotify) mNotify(mCookie, msg, ext1, ext2, obj);
//        }
//
//        virtual status_t dump(int fd, const Vector<String16> &args) const {
//            return INVALID_OPERATION;
//        }
//
//        virtual status_t updateProxyConfig(
//                const char *host, int32_t port, const char *exclusionList) {
//            return INVALID_OPERATION;
//        }
    };

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

    class AudioPlayer : public TimeSource {
    public:

    	char buffer[8192]; // Space for members we don't directly access.
        enum {
            REACHED_EOS,
            SEEK_COMPLETE
        };

        enum {
            ALLOW_DEEP_BUFFERING = 0x01,
            USE_OFFLOAD = 0x02,
            HAS_VIDEO   = 0x1000,
            IS_STREAMING = 0x2000

        };

        AudioPlayer(const sp<MediaPlayerBase::AudioSink> &audioSink,
                    uint32_t flags = 0,
                    void *audioObserver = NULL)
        {
            typedef void (*localFuncCast)(void *thiz, const sp<MediaPlayerBase::AudioSink> &audioSink,
                    uint32_t flags,
                    void *audioObserver);
            typedef void (*localFuncCast2)(void *thiz, const sp<MediaPlayerBase::AudioSink> &audioSink,
                    bool flags,
                    void *audioObserver);

            typedef void (*localFuncCast3)(void *thiz, const sp<MediaPlayerBase::AudioSink> &audioSink, 
                    void *audioObserver);

            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayerC1ERKNS_2spINS_15MediaPlayerBase9AudioSinkEEEjPNS_13AwesomePlayerE");
            localFuncCast2 lfc2 = (localFuncCast2)searchSymbol("_ZN7android11AudioPlayerC1ERKNS_2spINS_15MediaPlayerBase9AudioSinkEEEbPNS_13AwesomePlayerE");
            localFuncCast3 lfc3 = (localFuncCast3)searchSymbol("_ZN7android11AudioPlayerC1ERKNS_2spINS_15MediaPlayerBase9AudioSinkEEEPNS_13AwesomePlayerE");

            LOGI("Using C1 Not C2");
            if(lfc)
            {
                LOGI("AudioPlayer ctor 1");
                lfc(this, audioSink, flags, audioObserver);
            }
            else if(lfc2)
            {
                LOGI("AudioPlayer ctor 2, ignoring flags=%x", flags);
                lfc2(this, audioSink, false, audioObserver);
            }
            else if(lfc3)
            {
                LOGI("AudioPlayer ctor 3 variant 2, ignoring flags=%x", flags);
                lfc3(this, audioSink, audioObserver);                
            }
            else
            {
                LOGI("No AudioPlayer ctor found");
                assert(0);
            }
        }

        ~AudioPlayer()
        {

        }

        // Caller retains ownership of "source".
        void setSource(const sp<MediaSource> &source)
        {
            typedef void (*localFuncCast)(void *thiz, const sp<MediaSource> &source);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer9setSourceERKNS_2spINS_11MediaSourceEEE");
            assert(lfc);
            lfc(this, source);

        }

        // Return time in us.
        //virtual
        int64_t getRealTimeUs()
        {
            typedef int64_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer13getRealTimeUsEv");
            assert(lfc);
            return lfc(this);
        }

        status_t start(bool sourceAlreadyStarted = false)
        {
            typedef status_t (*localFuncCast)(void *thiz, bool sourceAlreadyStarted);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer5startEb");
            assert(lfc);
            return lfc(this, sourceAlreadyStarted);

        }

        void pause(bool playPendingSamples = false)
        {
            typedef void (*localFuncCast)(void *thiz, bool playPendingSamples);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer5pauseEb");
            assert(lfc);
            lfc(this, playPendingSamples);
        }
        status_t resume()
        {
            typedef status_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer6resumeEv");
            assert(lfc);
            return lfc(this);
        }

        // Returns the timestamp of the last buffer played (in us).
        int64_t getMediaTimeUs()
        {
            typedef int64_t (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer14getMediaTimeUsEv");
            assert(lfc);
            return lfc(this);
        }

        // Returns true iff a mapping is established, i.e. the AudioPlayer
        // has played at least one frame of audio.
        bool getMediaTimeMapping(int64_t *realtime_us, int64_t *mediatime_us)
        {
            typedef bool (*localFuncCast)(void *thiz, int64_t *realtime_us, int64_t *mediatime_us);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer19getMediaTimeMappingEPxS1_");
            assert(lfc);
            return lfc(this, realtime_us, mediatime_us);
        }

        status_t seekTo(int64_t time_us)
        {
            typedef status_t (*localFuncCast)(void *thiz, int64_t time_us);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer6seekToEx");
            assert(lfc);
            return lfc(this, time_us);
        }

        bool isSeeking()
        {
            typedef bool (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer9isSeekingEv");
            assert(lfc);
            return lfc(this);
        }
        bool reachedEOS(status_t *finalStatus)
        {
            typedef bool (*localFuncCast)(void *thiz, status_t *finalStatus);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer10reachedEOSEPi");
            assert(lfc);
            return lfc(this, finalStatus);
        }

        status_t setPlaybackRatePermille(int32_t ratePermille)
        {
            typedef status_t (*localFuncCast)(void *thiz, int32_t ratePermille);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer23setPlaybackRatePermilleEi");
            assert(lfc);
            return lfc(this, ratePermille);
        }

        void notifyAudioEOS()
        {
            typedef void (*localFuncCast)(void *thiz);
            localFuncCast lfc = (localFuncCast)searchSymbol("_ZN7android11AudioPlayer14notifyAudioEOSEv");
            assert(lfc);
            lfc(this);
        }
    };
}

#endif

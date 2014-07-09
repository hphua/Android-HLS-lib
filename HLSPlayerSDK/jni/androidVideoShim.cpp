#include "androidVideoShim.h"

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

static int property_get(const char *key, char *value, const char *default_value)
{
    int len;
    len = __system_property_get(key, value);
    if(len > 0) {
        return len;
    }

    if(default_value) {
        len = strlen(default_value);
        memcpy(value, default_value, len + 1);
    }
    return len;
}

namespace android_video_shim
{
    int gAPILevel = -1;

    // Libraries to search, in priority order.
    static int gLibCount = 8;
    static const char *gLibName[] = {
        "libandroid.so",
        "libandroid_runtime.so",
        "libstagefright.so",
        "libstagefright_avc_common.so",
        "libstagefright_color_conversion.so",
        "libutils.so",
        "libmedia.so",
        "libstlport.so"
    };
    static void *gLib[128];
    static int gLibsInitialized = 0;

    void *searchSymbol(const char *symName)
    {
        if(gLibsInitialized == 0)
        {
            LOGI("Forgot to load dynamic libs! Call initLibraries!");
            assert(0);
        }

        const char *strErr;
        for(int i=0; i<gLibCount; i++)
        {
            // Skip unloaded libraries.
            if(gLib[i] == NULL)
                continue;

            // Look for the symbol...
            void *r = dlsym(gLib[i], symName);

            // Catch error for later display.
            strErr = dlerror();

            // Return result if we got one!
            if(r)
                return r;
        }

        if(strErr) LOGI("   - dlerror %s for %s", strErr, symName);
        return NULL;
    }

    void initLibraries()
    {
        // Skip if already init'ed.
        if(gLibsInitialized)
        {
            LOGI("Already initialized android video shim libraries.");
            return;
        }

        // Check the SDK version.
        char sdk_ver_str[512] = "0";
        property_get("ro.build.version.sdk", sdk_ver_str, "0");
        gAPILevel = atoi(sdk_ver_str);
        LOGI("Video Shim Library on API %d", gAPILevel);

        // Load our libraries.
        for(int i=0; i<gLibCount; i++)
        {
            gLib[i] = dlopen(gLibName[i], RTLD_LAZY);
            LOGI("Video Shim Library[%d] %s = %p", i, gLibName[i], gLib[i]);
        }

        gLibsInitialized = 1;
    }
}

// Left over code for dumping a ton of useful/relevant symbol addrs.
using namespace android_video_shim;

void test_dlsym()
{
#define DLSYM_MACRO(s) LOGI("   o %s=%p", #s, searchSymbol(#s));

    DLSYM_MACRO(_ZdlPv);
    DLSYM_MACRO(_ZN7android10DataSource13CreateFromURIEPKcPKNS_11KeyedVectorINS_7String8ES4_EE);
    DLSYM_MACRO(_ZN7android11AudioPlayer5pauseEb);
    DLSYM_MACRO(_ZN7android11AudioPlayer5startEb);
    DLSYM_MACRO(_ZN7android11AudioPlayer9setSourceERKNS_2spINS_11MediaSourceEEE);
    DLSYM_MACRO(_ZN7android11AudioPlayerC1ERKNS_2spINS_15MediaPlayerBase9AudioSinkEEEjPNS_13AwesomePlayerE);
    DLSYM_MACRO(_ZN7android11AudioPlayerC1ERKNS_2spINS_15MediaPlayerBase9AudioSinkEEEPNS_13AwesomePlayerE);
    DLSYM_MACRO(_ZN7android11MediaBuffer7releaseEv);
    DLSYM_MACRO(_ZN7android11MediaBuffer9meta_dataEv);
    DLSYM_MACRO(_ZN7android11MediaSource11ReadOptionsC1Ev);
    DLSYM_MACRO(_ZN7android11MediaSourceC2Ev);
    DLSYM_MACRO(_ZN7android11MediaSourceD0Ev);
    DLSYM_MACRO(_ZN7android11MediaSourceD1Ev);
    DLSYM_MACRO(_ZN7android11MediaSourceD2Ev);
    DLSYM_MACRO(_ZN7android14ColorConverter7convertEPKvjjjjjjPvjjjjjj);
    DLSYM_MACRO(_ZN7android14ColorConverterC1E20OMX_COLOR_FORMATTYPES1_);
    DLSYM_MACRO(_ZN7android14ColorConverterD1Ev);
    DLSYM_MACRO(_ZN7android14MediaExtractor6CreateERKNS_2spINS_10DataSourceEEEPKc);
    DLSYM_MACRO(_ZN7android16canOffloadStreamERKNS_2spINS_8MetaDataEEEbb19audio_stream_type_t);
    DLSYM_MACRO(_ZN7android7RefBase10onFirstRefEv);
    DLSYM_MACRO(_ZN7android7RefBase13onLastWeakRefEPKv);
    DLSYM_MACRO(_ZN7android7RefBase15onLastStrongRefEPKv);
    DLSYM_MACRO(_ZN7android7RefBase20onIncStrongAttemptedEjPKv);
    DLSYM_MACRO(_ZN7android7RefBaseC2Ev);
    DLSYM_MACRO(_ZN7android7RefBaseD2Ev);
    DLSYM_MACRO(_ZN7android7String8C1EPKc);
    DLSYM_MACRO(_ZN7android7String8D1Ev);
    DLSYM_MACRO(_ZN7android8MetaData11findCStringEjPPKc);
    DLSYM_MACRO(_ZN7android8MetaData8findRectEjPiS1_S1_S1_);
    DLSYM_MACRO(_ZN7android8MetaData8setInt32Eji);
    DLSYM_MACRO(_ZN7android8MetaData9findInt32EjPi);
    DLSYM_MACRO(_ZN7android8MetaData9findInt64EjPx);
    DLSYM_MACRO(_ZN7android8MetaDataC1ERKS0_);
    DLSYM_MACRO(_ZN7android8OMXCodec6CreateERKNS_2spINS_4IOMXEEERKNS1_INS_8MetaDataEEEbRKNS1_INS_11MediaSourceEEEPKcj);
    DLSYM_MACRO(_ZN7android8OMXCodec6CreateERKNS_2spINS_4IOMXEEERKNS1_INS_8MetaDataEEEbRKNS1_INS_11MediaSourceEEEPKcjRKNS1);
    DLSYM_MACRO(_ZN7android9OMXClient7connectEv);
    DLSYM_MACRO(_ZN7android9OMXClientC1Ev);
    DLSYM_MACRO(_ZNK7android11MediaBuffer12range_lengthEv);
    DLSYM_MACRO(_ZNK7android11MediaBuffer4dataEv);
    DLSYM_MACRO(_ZNK7android11MediaBuffer4sizeEv);
    DLSYM_MACRO(_ZNK7android11MediaSource11ReadOptions9getSeekToEPxPNS1_8SeekModeE);
    DLSYM_MACRO(_ZN7android14ColorConverter7convertEjjPKvjPvj);
    DLSYM_MACRO(_ZNK7android14ColorConverter7isValidEv);
    DLSYM_MACRO(_ZNK7android7RefBase9decStrongEPKv);
    DLSYM_MACRO(_ZNK7android7RefBase9incStrongEPKv);
    DLSYM_MACRO(_ZNK7android8MetaData9dumpToLogEv);
    DLSYM_MACRO(_ZNSt8__detail15_List_node_base7_M_hookEPS0_);
    DLSYM_MACRO(_ZNSt8__detail15_List_node_base9_M_unhookEv);
    DLSYM_MACRO(_Znwj);
    DLSYM_MACRO(_ZTv0_n12_N7android11MediaSourceD0Ev);
    DLSYM_MACRO(_ZTv0_n12_N7android11MediaSourceD1Ev);
        
}

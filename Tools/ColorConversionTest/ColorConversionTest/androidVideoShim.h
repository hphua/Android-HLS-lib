#ifndef __ANDROID_VIDEO_SHIM_H_
#define __ANDROID_VIDEO_SHIM_H_

#include <errno.h>

namespace android_video_shim {


	// Duplicates of many Android libstagefright classes with their innard rewritten
	// to load and call symbols dynamically.
	//
	// Note that their inheritance hierarchy MUST match in terms of base/virtual base
	// classes or your pointers will be all off and you won't be able to call anything.
	typedef int32_t     status_t;

	enum {
		OK = 0,    // Everything's swell.
		NO_ERROR = 0,    // No errors.

		UNKNOWN_ERROR = 0x80000000,

		NO_MEMORY = -ENOMEM,
		INVALID_OPERATION = -ENOSYS,
		BAD_VALUE = -EINVAL,
		BAD_TYPE = 0x80000001,
		NAME_NOT_FOUND = -ENOENT,
		PERMISSION_DENIED = -EPERM,
		NO_INIT = -ENODEV,
		ALREADY_EXISTS = -EEXIST,
		DEAD_OBJECT = -EPIPE,
		FAILED_TRANSACTION = 0x80000002,
		JPARKS_BROKE_IT = -EPIPE,
#if !defined(HAVE_MS_C_RUNTIME)
		BAD_INDEX = -EOVERFLOW,
		NOT_ENOUGH_DATA = -ENODATA,
		WOULD_BLOCK = -EWOULDBLOCK,
		TIMED_OUT = -ETIMEDOUT,
		UNKNOWN_TRANSACTION = -EBADMSG,
#else
		BAD_INDEX = -E2BIG,
		NOT_ENOUGH_DATA = 0x80000003,
		WOULD_BLOCK = 0x80000004,
		TIMED_OUT = 0x80000005,
		UNKNOWN_TRANSACTION = 0x80000006,
#endif
		FDS_NOT_ALLOWED = 0x80000007,
	};

	enum {
		MEDIA_ERROR_BASE = -1000,

		ERROR_ALREADY_CONNECTED = MEDIA_ERROR_BASE,
		ERROR_NOT_CONNECTED = MEDIA_ERROR_BASE - 1,
		ERROR_UNKNOWN_HOST = MEDIA_ERROR_BASE - 2,
		ERROR_CANNOT_CONNECT = MEDIA_ERROR_BASE - 3,
		ERROR_IO = MEDIA_ERROR_BASE - 4,
		ERROR_CONNECTION_LOST = MEDIA_ERROR_BASE - 5,
		ERROR_MALFORMED = MEDIA_ERROR_BASE - 7,
		ERROR_OUT_OF_RANGE = MEDIA_ERROR_BASE - 8,
		ERROR_BUFFER_TOO_SMALL = MEDIA_ERROR_BASE - 9,
		ERROR_UNSUPPORTED = MEDIA_ERROR_BASE - 10,
		ERROR_END_OF_STREAM = MEDIA_ERROR_BASE - 11,

		// Not technically an error.
		INFO_FORMAT_CHANGED = MEDIA_ERROR_BASE - 12,
		INFO_DISCONTINUITY = MEDIA_ERROR_BASE - 13,
		INFO_OUTPUT_BUFFERS_CHANGED = MEDIA_ERROR_BASE - 14,

		// The following constant values should be in sync with
		// drm/drm_framework_common.h
		DRM_ERROR_BASE = -2000,

		ERROR_DRM_UNKNOWN = DRM_ERROR_BASE,
		ERROR_DRM_NO_LICENSE = DRM_ERROR_BASE - 1,
		ERROR_DRM_LICENSE_EXPIRED = DRM_ERROR_BASE - 2,
		ERROR_DRM_SESSION_NOT_OPENED = DRM_ERROR_BASE - 3,
		ERROR_DRM_DECRYPT_UNIT_NOT_INITIALIZED = DRM_ERROR_BASE - 4,
		ERROR_DRM_DECRYPT = DRM_ERROR_BASE - 5,
		ERROR_DRM_CANNOT_HANDLE = DRM_ERROR_BASE - 6,
		ERROR_DRM_TAMPER_DETECTED = DRM_ERROR_BASE - 7,
		ERROR_DRM_NOT_PROVISIONED = DRM_ERROR_BASE - 8,
		ERROR_DRM_DEVICE_REVOKED = DRM_ERROR_BASE - 9,
		ERROR_DRM_RESOURCE_BUSY = DRM_ERROR_BASE - 10,

		ERROR_DRM_VENDOR_MAX = DRM_ERROR_BASE - 500,
		ERROR_DRM_VENDOR_MIN = DRM_ERROR_BASE - 999,

		// Heartbeat Error Codes
		HEARTBEAT_ERROR_BASE = -3000,
		ERROR_HEARTBEAT_TERMINATE_REQUESTED = HEARTBEAT_ERROR_BASE,
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
}
#endif // __ANDROID_VIDEO_SHIM_H_
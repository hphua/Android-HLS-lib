/*
 * Copyright (C) 2009 The Android Open Source Project
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
#ifndef COLOR_CONVERTER_H_
#define COLOR_CONVERTER_H_
#include <sys/types.h>
#include <stdint.h>

namespace android_video_shim {

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

struct ColorConverter_Local {
    ColorConverter_Local(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to);
    ~ColorConverter_Local();

    bool isValid() const;

    void convert(
            size_t width, size_t height,
            const void *srcBits, size_t srcSkip,
            void *dstBits, size_t dstSkip);

//private:
    OMX_COLOR_FORMATTYPE mSrcFormat, mDstFormat;
    uint8_t *mClip;

    uint8_t *initClip();

    void convertCbYCrY(
            size_t width, size_t height,
            const void *srcBits, size_t srcSkip,
            void *dstBits, size_t dstSkip);

    void convertYUV420Planar(
            size_t width, size_t height,
            const void *srcBits, size_t srcSkip,
            void *dstBits, size_t dstSkip);

    void convertQCOMYUV420SemiPlanar(
            size_t width, size_t height,
            const void *srcBits, size_t srcSkip,
            void *dstBits, size_t dstSkip);

    void convertYUV420SemiPlanar(
            size_t width, size_t height,
            const void *srcBits, size_t srcSkip,
            void *dstBits, size_t dstSkip);

    void convertNV12Tile(
        size_t width, size_t height,
        const void *srcBits, size_t srcSkip,
        void *dstBits, size_t dstSkip);

    size_t nv12TileGetTiledMemBlockNum(
        size_t bx, size_t by,
        size_t nbx, size_t nby);

    void nv12TileComputeRGB(
        uint8_t **dstPtr,const uint8_t *blockUV,
        const uint8_t *blockY, size_t blockWidth,
        size_t dstSkip);

    void nv12TileTraverseBlock(
        uint8_t **dstPtr, const uint8_t *blockY,
        const uint8_t *blockUV, size_t blockWidth,
        size_t blockHeight, size_t dstSkip);

    void convertYUV420SemiPlanar32Aligned(
            size_t width, size_t height,
            const void *srcBits, size_t srcSkip,
            void *dstBits, size_t dstSkip,
            size_t alignedWidth);

    //ColorConverter(const ColorConverter &);
    //ColorConverter &operator=(const ColorConverter &);
};
}  // namespace android
#endif  // COLOR_CONVERTER_H_
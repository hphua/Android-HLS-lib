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

#include "androidVideoShim.h"

namespace android_video_shim {
struct ColorConverter_Local {
    ColorConverter_Local(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to);
    ~ColorConverter_Local();
    bool isValid() const;
    status_t convert(
            const void *srcBits,
            size_t srcWidth, size_t srcHeight,
            size_t srcCropLeft, size_t srcCropTop,
            size_t srcCropRight, size_t srcCropBottom,
            void *dstBits,
            size_t dstWidth, size_t dstHeight,
            size_t dstCropLeft, size_t dstCropTop,
            size_t dstCropRight, size_t dstCropBottom);
private:
    struct BitmapParams {
        BitmapParams(
                void *bits,
                size_t width, size_t height,
                size_t cropLeft, size_t cropTop,
                size_t cropRight, size_t cropBottom);
        size_t cropWidth() const;
        size_t cropHeight() const;
        void *mBits;
        size_t mWidth, mHeight;
        size_t mCropLeft, mCropTop, mCropRight, mCropBottom;
    };
    OMX_COLOR_FORMATTYPE mSrcFormat, mDstFormat;
    uint8_t *mClip;
    uint8_t *initClip();
    status_t convertCbYCrY(
            const BitmapParams &src, const BitmapParams &dst);
    status_t convertYUV420Planar(
            const BitmapParams &src, const BitmapParams &dst);
    status_t convertQCOMYUV420SemiPlanar(
            const BitmapParams &src, const BitmapParams &dst);
    status_t convertYUV420SemiPlanar(
            const BitmapParams &src, const BitmapParams &dst);
    status_t convertTIYUV420PackedSemiPlanar(
            const BitmapParams &src, const BitmapParams &dst);

	size_t nv12TileGetTiledMemBlockNum(
	        size_t bx, size_t by,
	        size_t nbx, size_t nby);

	void nv12TileComputeRGB(
	        uint8_t **dstPtr, const uint8_t *blockUV,
	        const uint8_t *blockY, size_t blockWidth,
	        size_t dstSkip);

	void nv12TileTraverseBlock(
	        uint8_t **dstPtr, const uint8_t *blockY,
	        const uint8_t *blockUV, size_t blockWidth,
	        size_t blockHeight, size_t dstSkip);
	
	void convertNV12Tile(
	        size_t width, size_t height,
	        const void *srcBits, size_t srcSkip,
	        void *dstBits, size_t dstSkip);

	status_t convertQOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka(const BitmapParams &src, const BitmapParams &dst);

    ColorConverter_Local(const ColorConverter &);
    ColorConverter_Local &operator=(const ColorConverter &);
};
}  // namespace android
#endif  // COLOR_CONVERTER_H_
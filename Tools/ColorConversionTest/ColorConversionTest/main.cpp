#include "../sdl2-2.0.3/include/sdl.h"
#include <stdio.h>
#include "androidVideoShim_ColorConverter.h"
#include "ColorConverter444.h"
#include <string>

struct FrameHeader
{
	int32_t width;
	int32_t height;
	int32_t stride;
	int32_t format;
	int32_t cropleft;
	int32_t croptop;
	int32_t cropright;
	int32_t cropbottom;
	int32_t datasize;
	int32_t decoderPath; // 0 == standard, 1 == uses i420 w/o modded data, 2 == uses i420 w/ modded data
	char deviceString[1024];
};

int colorFormat = 0;
float windowSizeMultiplier = 1.0f;

using namespace android_video_shim;

const char* getColorFormatName(int fmt)
{
	switch (fmt)
	{
	case OMX_COLOR_FormatMonochrome:
		return "OMX_COLOR_FormatMonochrome";
		
	case OMX_COLOR_Format8bitRGB332:
		return "OMX_COLOR_Format8bitRGB332";
		
	case OMX_COLOR_Format12bitRGB444:
		return "OMX_COLOR_Format12bitRGB444";
		
	case OMX_COLOR_Format16bitARGB4444:
		return "OMX_COLOR_Format16bitARGB4444";
		
	case OMX_COLOR_Format16bitARGB1555:
		return "OMX_COLOR_Format16bitARGB1555";
		
	case OMX_COLOR_Format16bitRGB565:
		return "OMX_COLOR_Format16bitRGB565";
		
	case OMX_COLOR_Format16bitBGR565:
		return "OMX_COLOR_Format16bitBGR565";
		
	case OMX_COLOR_Format18bitRGB666:
		return "OMX_COLOR_Format18bitRGB666";
		
	case OMX_COLOR_Format18bitARGB1665:
		return "OMX_COLOR_Format18bitARGB1665";
		
	case OMX_COLOR_Format19bitARGB1666:
		return "OMX_COLOR_Format19bitARGB1666";
		
	case OMX_COLOR_Format24bitRGB888:
		return "OMX_COLOR_Format24bitRGB888";
		
	case OMX_COLOR_Format24bitBGR888:
		return "OMX_COLOR_Format24bitBGR888";
		
	case OMX_COLOR_Format24bitARGB1887:
		return "OMX_COLOR_Format24bitARGB1887";
		
	case OMX_COLOR_Format25bitARGB1888:
		return "OMX_COLOR_Format25bitARGB1888";
		
	case OMX_COLOR_Format32bitBGRA8888:
		return "OMX_COLOR_Format32bitBGRA8888";
		
	case OMX_COLOR_Format32bitARGB8888:
		return "OMX_COLOR_Format32bitARGB8888";
		
	case OMX_COLOR_FormatYUV411Planar:
		return "OMX_COLOR_FormatYUV411Planar";
		
	case OMX_COLOR_FormatYUV411PackedPlanar:
		return "OMX_COLOR_FormatYUV411PackedPlanar";
		
	case OMX_COLOR_FormatYUV420Planar:
		return "OMX_COLOR_FormatYUV420Planar";
		
	case OMX_COLOR_FormatYUV420PackedPlanar:
		return "OMX_COLOR_FormatYUV420PackedPlanar";
		
	case OMX_COLOR_FormatYUV420SemiPlanar:
		return "OMX_COLOR_FormatYUV420SemiPlanar";
		
	case OMX_COLOR_FormatYUV422Planar:
		return "OMX_COLOR_FormatYUV422Planar";
		
	case OMX_COLOR_FormatYUV422PackedPlanar:
		return "OMX_COLOR_FormatYUV422PackedPlanar";
		
	case OMX_COLOR_FormatYUV422SemiPlanar:
		return "OMX_COLOR_FormatYUV422SemiPlanar";
		
	case OMX_COLOR_FormatYCbYCr:
		return "OMX_COLOR_FormatYCbYCr";
		
	case OMX_COLOR_FormatYCrYCb:
		return "OMX_COLOR_FormatYCrYCb";
		
	case OMX_COLOR_FormatCbYCrY:
		return "OMX_COLOR_FormatCbYCrY";
		
	case OMX_COLOR_FormatCrYCbY:
		return "OMX_COLOR_FormatCrYCbY";
		
	case OMX_COLOR_FormatYUV444Interleaved:
		return "OMX_COLOR_FormatYUV444Interleaved";
		
	case OMX_COLOR_FormatRawBayer8bit:
		return "OMX_COLOR_FormatRawBayer8bit";
		
	case OMX_COLOR_FormatRawBayer10bit:
		return "OMX_COLOR_FormatRawBayer10bit";
		
	case OMX_COLOR_FormatRawBayer8bitcompressed:
		return "OMX_COLOR_FormatRawBayer8bitcompressed";
		
	case OMX_COLOR_FormatL2:
		return "OMX_COLOR_FormatL2";
		
	case OMX_COLOR_FormatL4:
		return "OMX_COLOR_FormatL4";
		
	case OMX_COLOR_FormatL8:
		return "OMX_COLOR_FormatL8";
		
	case OMX_COLOR_FormatL16:
		return "OMX_COLOR_FormatL16";
		
	case OMX_COLOR_FormatL24:
		return "OMX_COLOR_FormatL24";
		
	case OMX_COLOR_FormatL32:
		return "OMX_COLOR_FormatL32";
		
	case OMX_COLOR_FormatYUV420PackedSemiPlanar:
		return "OMX_COLOR_FormatYUV420PackedSemiPlanar";
		
	case OMX_COLOR_FormatYUV422PackedSemiPlanar:
		return "OMX_COLOR_FormatYUV422PackedSemiPlanar";
		
	case OMX_COLOR_Format18BitBGR666:
		return "OMX_COLOR_Format18BitBGR666";
		
	case OMX_COLOR_Format24BitARGB6666:
		return "OMX_COLOR_Format24BitARGB6666";
		
	case OMX_COLOR_Format24BitABGR6666:
		return "OMX_COLOR_Format24BitABGR6666";
		
	case OMX_COLOR_FormatAndroidOpaque:
		return "OMX_COLOR_FormatAndroidOpaque";
		
	case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
		return "OMX_TI_COLOR_FormatYUV420PackedSemiPlanar";
		
	case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
		return "OMX_QCOM_COLOR_FormatYVU420SemiPlanar";
		
	case QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka:
		return "QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka";
		
	case OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka:
		return "OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka";
	}
	return "Unknown";
}

void printColorFormatName(int fmt)
{
	printf("FMT = %s\n", getColorFormatName(fmt));
}

bool colorFormatExists(int fmt)
{
	switch (fmt)
	{
	case OMX_COLOR_FormatMonochrome:
	case OMX_COLOR_Format8bitRGB332:
	case OMX_COLOR_Format12bitRGB444:
	case OMX_COLOR_Format16bitARGB4444:
	case OMX_COLOR_Format16bitARGB1555:
	case OMX_COLOR_Format16bitRGB565:
	case OMX_COLOR_Format16bitBGR565:
	case OMX_COLOR_Format18bitRGB666:
	case OMX_COLOR_Format18bitARGB1665:
	case OMX_COLOR_Format19bitARGB1666:
	case OMX_COLOR_Format24bitRGB888:
	case OMX_COLOR_Format24bitBGR888:
	case OMX_COLOR_Format24bitARGB1887:
	case OMX_COLOR_Format25bitARGB1888:
	case OMX_COLOR_Format32bitBGRA8888:
	case OMX_COLOR_Format32bitARGB8888:
	case OMX_COLOR_FormatYUV411Planar:
	case OMX_COLOR_FormatYUV411PackedPlanar:
	case OMX_COLOR_FormatYUV420Planar:
	case OMX_COLOR_FormatYUV420PackedPlanar:
	case OMX_COLOR_FormatYUV420SemiPlanar:
	case OMX_COLOR_FormatYUV422Planar:
	case OMX_COLOR_FormatYUV422PackedPlanar:
	case OMX_COLOR_FormatYUV422SemiPlanar:
	case OMX_COLOR_FormatYCbYCr:
	case OMX_COLOR_FormatYCrYCb:
	case OMX_COLOR_FormatCbYCrY:
	case OMX_COLOR_FormatCrYCbY:
	case OMX_COLOR_FormatYUV444Interleaved:
	case OMX_COLOR_FormatRawBayer8bit:
	case OMX_COLOR_FormatRawBayer10bit:
	case OMX_COLOR_FormatRawBayer8bitcompressed:
	case OMX_COLOR_FormatL2:
	case OMX_COLOR_FormatL4:
	case OMX_COLOR_FormatL8:
	case OMX_COLOR_FormatL16:
	case OMX_COLOR_FormatL24:
	case OMX_COLOR_FormatL32:
	case OMX_COLOR_FormatYUV420PackedSemiPlanar:
	case OMX_COLOR_FormatYUV422PackedSemiPlanar:
	case OMX_COLOR_Format18BitBGR666:
	case OMX_COLOR_Format24BitARGB6666:
	case OMX_COLOR_Format24BitABGR6666:
	case OMX_COLOR_FormatAndroidOpaque:
	case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
	case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
	case QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka:
	case OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka:
		return true;
	}
	return false;
}

void incrementColorFormat()
{
	++colorFormat;
	if (colorFormatExists(colorFormat))
	{
		printColorFormatName(colorFormat);
		return;
	}
	if (colorFormat > OMX_COLOR_Format24BitABGR6666 && colorFormat < OMX_COLOR_FormatAndroidOpaque)
	{
		colorFormat = OMX_COLOR_FormatAndroidOpaque;
		printColorFormatName(colorFormat);
		return;
	}
	if (colorFormat > OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka)
	{
		colorFormat = OMX_COLOR_FormatMonochrome;
		printColorFormatName(colorFormat);
		return;
	}
	if (colorFormat > OMX_COLOR_FormatAndroidOpaque && colorFormat < OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka)
	{
		++colorFormat;
		while (!colorFormatExists(colorFormat))
		{
			++colorFormat;
		}
		printColorFormatName(colorFormat);
	}

}

const char* fileName = "vidbuffer.raw";

int main(int argc, char* args[])
{
	std::string fileName = args[0];

	int colorConverter = 0;

	if (argc > 1)
	{
		std::string argFile = args[1];
		size_t pos = argFile.find_last_not_of(':');
		if (pos == std::string::npos)
		{
			pos = fileName.find_last_of('\\');

			fileName = fileName.substr(0, pos + 1) + argFile;
		}
		else
		{
			pos = fileName.find('\\');
			if (pos == 0)
			{
				fileName = argFile;
			}
			else
			{
				pos = fileName.find_last_of('\\');
				fileName = fileName.substr(0, pos + 1) + argFile;
			}
		}
	}
	else
	{
		size_t pos = fileName.find_last_of('\\');

		fileName = fileName.substr(0, pos + 1) + "vidbuffer.raw";
	}

	printf("Loading %s\n", fileName.c_str());
	

	char* imgbytes = NULL;
	FrameHeader f;

	FILE* file = fopen(fileName.c_str(), "rb");
	if (file)
	{

		int res = fread(&f, sizeof(FrameHeader), 1, file);
		printf("FrameHeader = {\n width=%d,\n height=%d,\n stride=%d,\n colorFormat=0x%0x (%s),\n cropLeft=%d,\n cropTop=%d,\n cropRight=%d,\n cropBottom=%d,\n dataSize=%d,\n decoderPath=%d,\n deviceInfo=%s\n}\n", 
			f.width, f.height, f.stride, f.format, getColorFormatName(f.format), f.cropleft, f.croptop, f.cropright, f.cropbottom, f.datasize, f.decoderPath, f.deviceString);

		imgbytes = new char[f.datasize];
		res = fread(imgbytes, sizeof(char), f.datasize, file);
		printf("Read %d bytes\n", res);
		fclose(file);
		colorFormat = f.format;
	}
	else
	{
		int err = *_errno();
		printf("Could not open file %s\n", fileName.c_str());
	}


	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;

	SDL_Surface* screenSurface = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		return 1;
	}
	else
	{
		//Create window
		window = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (f.width * windowSizeMultiplier) + 8, (f.height * windowSizeMultiplier) + 8, SDL_WINDOW_SHOWN);
		if (window == NULL)
		{
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		}
		else
		{
			renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
			//Get window surface
			screenSurface = SDL_GetWindowSurface(window);

			SDL_Event e;

			bool quit = false;

			SDL_Texture* texture = NULL;

			texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, f.width, f.height);

			SDL_Rect r = { 0, 0, f.width, f.height };

			if (f.format == 0x7fa30c04)
				f.format = android_video_shim::OMX_COLOR_FormatYUV420Planar;

			printColorFormatName(f.format);
			void* dstpixels;
			int pitch;
			




			
			int pitchCur = 0;

			while (!quit)
			{
				while (SDL_PollEvent(&e) != 0)
				{
					if (e.type == SDL_QUIT)
						quit = true;
					if (e.type == SDL_MOUSEWHEEL)
					{
						pitchCur += e.wheel.y;
						printf("pitchOffset = %d\n", pitchCur);
					}
					if (e.type == SDL_KEYDOWN)
					{
						if (e.key.keysym.sym == SDLK_UP)
						{
							incrementColorFormat();
						}
						else if (e.key.keysym.sym == SDLK_DOWN)
						{

						}
						else if (e.key.keysym.sym == SDLK_RIGHT)
						{
							colorConverter = (colorConverter + 1) % 2;
							if (colorConverter == 1)
								printf("Switched to 2.3 color converter\n");
							else
								printf("Switched to 4.4.4 color converter\n");
						}
						else if (e.key.keysym.sym == SDLK_z)
						{
							windowSizeMultiplier += 0.5f;
							if (windowSizeMultiplier > 3.0f) windowSizeMultiplier = 1.0f;
							SDL_SetWindowSize(window, (f.width * windowSizeMultiplier) + 8, (f.height * windowSizeMultiplier) + 8);
							printf("Zoom to %.2fx\n", windowSizeMultiplier);
						}
					}
				}

				SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
				SDL_RenderClear(renderer);

				SDL_LockTexture(texture, &r, &dstpixels, &pitch);
				memset((unsigned char*)dstpixels, 0xff, pitch * r.h);

				if (colorConverter == 1)
				{
					android_video_shim::ColorConverter_Local lcc((android_video_shim::OMX_COLOR_FORMATTYPE)colorFormat, android_video_shim::OMX_COLOR_Format16bitRGB565);
					if (lcc.isValid())
					{
						lcc.convert(f.width, f.height, imgbytes, 0, dstpixels, pitch + pitchCur);
					}
					else
					{
						printf("Color Format conversion Not Valid - skipping\n");
						incrementColorFormat();
					}
				}
				else if (colorConverter == 0)
				{
					
					android_video_shim::ColorConverter cc((android_video_shim::OMX_COLOR_FORMATTYPE)colorFormat, android_video_shim::OMX_COLOR_Format16bitRGB565);
					if (cc.isValid())
					{
						cc.convert(imgbytes, f.datasize, f.width, f.height, f.cropleft, f.croptop, f.cropright, f.cropbottom,
							dstpixels, (pitch / 2) + pitchCur, f.height, 0, 0, f.cropright - f.cropleft, f.cropbottom - f.croptop);
					}
					else
					{
						printf("Color Format conversion Not Valid - skipping\n");
						incrementColorFormat();
					}
				}

				SDL_UnlockTexture(texture);
				dstpixels = NULL;

				SDL_Rect dr = { 4, 4, r.w * windowSizeMultiplier, r.h * windowSizeMultiplier };


				SDL_RenderCopy(renderer, texture, &r, &dr);

				//Update the surface
				SDL_RenderPresent(renderer);
			}

			if (imgbytes != NULL)
				delete[] imgbytes;

		}
	}

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
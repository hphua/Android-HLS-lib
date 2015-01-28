#include "../sdl2-2.0.3/include/sdl.h"
#include <stdio.h>
#include "androidVideoShim_ColorConverter.h"
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

using namespace android_video_shim;

void printColorFormatName(int fmt)
{
	switch (fmt)
	{
	case OMX_COLOR_FormatMonochrome:
		printf("FMT = OMX_COLOR_FormatMonochrome\n");
		break;
	case OMX_COLOR_Format8bitRGB332:
		printf("FMT = OMX_COLOR_Format8bitRGB332\n");
		break;
	case OMX_COLOR_Format12bitRGB444:
		printf("FMT = OMX_COLOR_Format12bitRGB444\n");
		break;
	case OMX_COLOR_Format16bitARGB4444:
		printf("FMT = OMX_COLOR_Format16bitARGB4444\n");
		break;
	case OMX_COLOR_Format16bitARGB1555:
		printf("FMT = OMX_COLOR_Format16bitARGB1555\n");
		break;
	case OMX_COLOR_Format16bitRGB565:
		printf("FMT = OMX_COLOR_Format16bitRGB565\n");
		break;
	case OMX_COLOR_Format16bitBGR565:
		printf("FMT = OMX_COLOR_Format16bitBGR565\n");
		break;
	case OMX_COLOR_Format18bitRGB666:
		printf("FMT = OMX_COLOR_Format18bitRGB666\n");
		break;
	case OMX_COLOR_Format18bitARGB1665:
		printf("FMT = OMX_COLOR_Format18bitARGB1665\n");
		break;
	case OMX_COLOR_Format19bitARGB1666:
		printf("FMT = OMX_COLOR_Format19bitARGB1666\n");
		break;
	case OMX_COLOR_Format24bitRGB888:
		printf("FMT = OMX_COLOR_Format24bitRGB888\n");
		break;
	case OMX_COLOR_Format24bitBGR888:
		printf("FMT = OMX_COLOR_Format24bitBGR888\n");
		break;
	case OMX_COLOR_Format24bitARGB1887:
		printf("FMT = OMX_COLOR_Format24bitARGB1887\n");
		break;
	case OMX_COLOR_Format25bitARGB1888:
		printf("FMT = OMX_COLOR_Format25bitARGB1888\n");
		break;
	case OMX_COLOR_Format32bitBGRA8888:
		printf("FMT = OMX_COLOR_Format32bitBGRA8888\n");
		break;
	case OMX_COLOR_Format32bitARGB8888:
		printf("FMT = OMX_COLOR_Format32bitARGB8888\n");
		break;
	case OMX_COLOR_FormatYUV411Planar:
		printf("FMT = OMX_COLOR_FormatYUV411Planar\n");
		break;
	case OMX_COLOR_FormatYUV411PackedPlanar:
		printf("FMT = OMX_COLOR_FormatYUV411PackedPlanar\n");
		break;
	case OMX_COLOR_FormatYUV420Planar:
		printf("FMT = OMX_COLOR_FormatYUV420Planar\n");
		break;
	case OMX_COLOR_FormatYUV420PackedPlanar:
		printf("FMT = OMX_COLOR_FormatYUV420PackedPlanar\n");
		break;
	case OMX_COLOR_FormatYUV420SemiPlanar:
		printf("FMT = OMX_COLOR_FormatYUV420SemiPlanar\n");
		break;
	case OMX_COLOR_FormatYUV422Planar:
		printf("FMT = OMX_COLOR_FormatYUV422Planar\n");
		break;
	case OMX_COLOR_FormatYUV422PackedPlanar:
		printf("FMT = OMX_COLOR_FormatYUV422PackedPlanar\n");
		break;
	case OMX_COLOR_FormatYUV422SemiPlanar:
		printf("FMT = OMX_COLOR_FormatYUV422SemiPlanar\n");
		break;
	case OMX_COLOR_FormatYCbYCr:
		printf("FMT = OMX_COLOR_FormatYCbYCr\n");
		break;
	case OMX_COLOR_FormatYCrYCb:
		printf("FMT = OMX_COLOR_FormatYCrYCb\n");
		break;
	case OMX_COLOR_FormatCbYCrY:
		printf("FMT = OMX_COLOR_FormatCbYCrY\n");
		break;
	case OMX_COLOR_FormatCrYCbY:
		printf("FMT = OMX_COLOR_FormatCrYCbY\n");
		break;
	case OMX_COLOR_FormatYUV444Interleaved:
		printf("FMT = OMX_COLOR_FormatYUV444Interleaved\n");
		break;
	case OMX_COLOR_FormatRawBayer8bit:
		printf("FMT = OMX_COLOR_FormatRawBayer8bit\n");
		break;
	case OMX_COLOR_FormatRawBayer10bit:
		printf("FMT = OMX_COLOR_FormatRawBayer10bit\n");
		break;
	case OMX_COLOR_FormatRawBayer8bitcompressed:
		printf("FMT = OMX_COLOR_FormatRawBayer8bitcompressed\n");
		break;
	case OMX_COLOR_FormatL2:
		printf("FMT = OMX_COLOR_FormatL2\n");
		break;
	case OMX_COLOR_FormatL4:
		printf("FMT = OMX_COLOR_FormatL4\n");
		break;
	case OMX_COLOR_FormatL8:
		printf("FMT = OMX_COLOR_FormatL8\n");
		break;
	case OMX_COLOR_FormatL16:
		printf("FMT = OMX_COLOR_FormatL16\n");
		break;
	case OMX_COLOR_FormatL24:
		printf("FMT = OMX_COLOR_FormatL24\n");
		break;
	case OMX_COLOR_FormatL32:
		printf("FMT = OMX_COLOR_FormatL32\n");
		break;
	case OMX_COLOR_FormatYUV420PackedSemiPlanar:
		printf("FMT = OMX_COLOR_FormatYUV420PackedSemiPlanar\n");
		break;
	case OMX_COLOR_FormatYUV422PackedSemiPlanar:
		printf("FMT = OMX_COLOR_FormatYUV422PackedSemiPlanar\n");
		break;
	case OMX_COLOR_Format18BitBGR666:
		printf("FMT = OMX_COLOR_Format18BitBGR666\n");
		break;
	case OMX_COLOR_Format24BitARGB6666:
		printf("FMT = OMX_COLOR_Format24BitARGB6666\n");
		break;
	case OMX_COLOR_Format24BitABGR6666:
		printf("FMT = OMX_COLOR_Format24BitABGR6666\n");
		break;
	case OMX_COLOR_FormatAndroidOpaque:
		printf("FMT = OMX_COLOR_FormatAndroidOpaque\n");
		break;
	case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
		printf("FMT = OMX_TI_COLOR_FormatYUV420PackedSemiPlanar\n");
		break;
	case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
		printf("FMT = OMX_QCOM_COLOR_FormatYVU420SemiPlanar\n");
		break;
	case QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka:
		printf("FMT = QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka\n");
		break;
	case OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka:
		printf("FMT = OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m4ka\n");
		break;
	default:
		printf("FMT = Unknown\n");
		break;
	}
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
		printf("FrameHeader = {\n width=%d,\n height=%d,\n stride=%d,\n colorFormat=0x%0x,\n cropLeft=%d,\n cropTop=%d,\n cropRight=%d,\n cropBottom=%d,\n dataSize=%d,\n decoderPath=%d,\n deviceInfo=%s\n}\n", 
			f.width, f.height, f.stride, f.format, f.cropleft, f.croptop, f.cropright, f.cropbottom, f.datasize, f.decoderPath, f.deviceString);

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
		window = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (f.width * 2) + 8, (f.height * 2) + 8, SDL_WINDOW_SHOWN);
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
			void* pixels;
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
					}
				}

				SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
				SDL_RenderClear(renderer);

				SDL_LockTexture(texture, &r, &pixels, &pitch);
				android_video_shim::ColorConverter_Local lcc((android_video_shim::OMX_COLOR_FORMATTYPE)colorFormat, android_video_shim::OMX_COLOR_Format16bitRGB565);
				if (lcc.isValid())
				{
					lcc.convert(f.width, f.height, imgbytes, 0, pixels, pitch + pitchCur);
				}
				else
				{
					printf("Color Format conversion Not Valid - skipping\n");
					incrementColorFormat();
				}

				SDL_UnlockTexture(texture);
				pixels = NULL;

				SDL_Rect dr = { 4, 4, r.w * 2, r.h * 2 };


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
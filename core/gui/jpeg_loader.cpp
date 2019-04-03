/*
    Created on: Mar 31, 2019

	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <turbojpeg.h>
#include "jpeg_loader.h"

const char *subsampName[] = {
  "4:4:4", "4:2:2", "4:2:0", "Grayscale", "4:4:0", "4:1:1"
};

const char *colorspaceName[] = {
  "RGB", "YCbCr", "GRAY", "CMYK", "YCCK"
};

u8 *load_jpeg(const std::string& path, int& width, int& height)
{
	// Load file into memory
	FILE *jpegfile = fopen(path.c_str(), "rb");
	if (jpegfile == NULL)
		return NULL;
	fseek(jpegfile, 0, SEEK_END);
	size_t size = ftell(jpegfile);
	fseek(jpegfile, 0, SEEK_SET);
	if (size == 0)
	{
		fclose(jpegfile);
		return NULL;
	}
	u8 *jpegBuf = (u8 *)tjAlloc(size);
	if (jpegBuf == NULL)
	{
		fclose(jpegfile);
		return NULL;
	}
	if (fread(jpegBuf, size, 1, jpegfile) < 1)
	{
		tjFree(jpegBuf);
		fclose(jpegfile);
		return NULL;
	}
	fclose(jpegfile);

	// Decompress
	tjhandle turboj = tjInitDecompress();
	if (turboj == NULL)
	{
		tjFree(jpegBuf);
		return NULL;
	}
	int inSubsamp, inColorspace;
	if (tjDecompressHeader3(turboj, jpegBuf, size, &width, &height, &inSubsamp, &inColorspace) < 0)
	{
		tjDestroy(turboj);
		tjFree(jpegBuf);
		return NULL;
	}
	//printf("Image:  %d x %d pixels, %s subsampling, %s colorspace\n", width, height, subsampName[inSubsamp], colorspaceName[inColorspace]);

	u8 *imgBuf = (u8 *)tjAlloc(width * height * 4);
	if (imgBuf == NULL)
	{
		tjDestroy(turboj);
		tjFree(jpegBuf);
		return NULL;
	}
	int flags = 0;	// TJFLAG_FASTUPSAMPLE ? TJFLAG_FASTDCT ?
	if (tjDecompress2(turboj, jpegBuf, size, imgBuf, width, 0, height, TJPF_RGBA, flags) < 0)
	{
		tjFree(imgBuf);
		tjDestroy(turboj);
		tjFree(jpegBuf);
		return NULL;
	}

	tjDestroy(turboj);
	tjFree(jpegBuf);
	return imgBuf;
}

void free_jpeg(u8 *data)
{
	tjFree(data);
}

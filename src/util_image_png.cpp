/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image.h"
#include "util_image_png.hpp"
#include <fsys/filesystem.h>
#include <sharedutils/scope_guard.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <cstring>

#include <png.h>        /* libpng header */
#include <pngstruct.h>

// Header data
#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) > (b)? (a) : (b))
#  define MIN(a,b)  ((a) < (b)? (a) : (b))
#endif

#ifdef DEBUG
#  define Trace(x)  {fprintf x ; fflush(stderr); fflush(stdout);}
#else
#  define Trace(x)  ;
#endif

typedef unsigned char   uch;
typedef unsigned short  ush;
typedef unsigned long   ulg;


/* prototypes for public functions in readpng.c */

struct PNGImage
{
	void readpng_version_info(void);
	int readpng_init(VFilePtr &f, ulg *pWidth, ulg *pHeight);
	int readpng_get_bgcolor(uch *bg_red, uch *bg_green, uch *bg_blue);
	uch *readpng_get_image(double display_exponent, int *pChannels,
						   ulg *pRowbytes);
	void readpng_cleanup(int free_image_data);

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	png_uint_32  width, height;
	int  bit_depth, color_type;
	uch  *image_data = NULL;
};


//

/* future versions of libpng will provide this macro: */
#ifndef png_jmpbuf
#  define png_jmpbuf(png_ptr)   ((png_ptr)->jmpbuf)
#endif


void PNGImage::readpng_version_info(void)
{
    fprintf(stderr, "   Compiled with libpng %s; using libpng %s.\n",
      PNG_LIBPNG_VER_STRING, png_libpng_ver);
    fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n",
      ZLIB_VERSION, zlib_version);
}


/* return value = 0 for success, 1 for bad sig, 2 for bad IHDR, 4 for no mem */

int PNGImage::readpng_init(VFilePtr &f, ulg *pWidth, ulg *pHeight)
{
    uch sig[8];


    /* first do a quick check that the file really is a PNG image; could
     * have used slightly more general png_sig_cmp() function instead */

	f->Read(sig,8);
    if (png_sig_cmp(sig, 0, 8))
        return 1;   /* bad signature */


    /* could pass pointers to user-defined error handlers instead of NULLs: */

    png_ptr = png_create_read_struct(png_get_libpng_ver(NULL), NULL, NULL,
        NULL);
    if (!png_ptr)
        return 4;   /* out of memory */

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return 4;   /* out of memory */
    }


    /* we could create a second info struct here (end_info), but it's only
     * useful if we want to keep pre- and post-IDAT chunk info separated
     * (mainly for PNG-aware image editors and converters) */


    /* setjmp() must be called in every function that calls a PNG-reading
     * libpng function */

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 2;
    }

	png_set_read_fn(png_ptr,f.get(),[](png_structp png_ptr,png_bytep png_data,png_size_t png_size) {
		static_cast<VFilePtrInternal*>(png_ptr->io_ptr)->Read(png_data,png_size);;
	});
    png_set_sig_bytes(png_ptr, 8);  /* we already read the 8 signature bytes */

    png_read_info(png_ptr, info_ptr);  /* read all PNG info up to image data */


    /* alternatively, could make separate calls to png_get_image_width(),
     * etc., but want bit_depth and color_type for later [don't care about
     * compression_type and filter_type => NULLs] */

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
      NULL, NULL, NULL);
    *pWidth = width;
    *pHeight = height;


    /* OK, that's all we need for now; return happy */

    return 0;
}




/* returns 0 if succeeds, 1 if fails due to no bKGD chunk, 2 if libpng error;
 * scales values to 8-bit if necessary */

int PNGImage::readpng_get_bgcolor(uch *red, uch *green, uch *blue)
{
    png_color_16p pBackground;


    /* setjmp() must be called in every function that calls a PNG-reading
     * libpng function */

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 2;
    }


    if (!png_get_valid(png_ptr, info_ptr, PNG_INFO_bKGD))
        return 1;

    /* it is not obvious from the libpng documentation, but this function
     * takes a pointer to a pointer, and it always returns valid red, green
     * and blue values, regardless of color_type: */

    png_get_bKGD(png_ptr, info_ptr, &pBackground);


    /* however, it always returns the raw bKGD data, regardless of any
     * bit-depth transformations, so check depth and adjust if necessary */

    if (bit_depth == 16) {
        *red   = pBackground->red   >> 8;
        *green = pBackground->green >> 8;
        *blue  = pBackground->blue  >> 8;
    } else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        if (bit_depth == 1)
            *red = *green = *blue = pBackground->gray? 255 : 0;
        else if (bit_depth == 2)
            *red = *green = *blue = (255/3) * pBackground->gray;
        else /* bit_depth == 4 */
            *red = *green = *blue = (255/15) * pBackground->gray;
    } else {
        *red   = (uch)pBackground->red;
        *green = (uch)pBackground->green;
        *blue  = (uch)pBackground->blue;
    }

    return 0;
}

/* display_exponent == LUT_exponent * CRT_exponent */

uch *PNGImage::readpng_get_image(double display_exponent, int *pChannels, ulg *pRowbytes)
{
    double  gamma;
    png_uint_32  i, rowbytes;
    png_bytepp  row_pointers = NULL;


    /* setjmp() must be called in every function that calls a PNG-reading
     * libpng function */

    if (setjmp(png_jmpbuf(png_ptr))) {
        free(image_data);
        image_data = NULL;
        free(row_pointers);
        row_pointers = NULL;
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }


    /* expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
     * transparency chunks to full alpha channel; strip 16-bit-per-sample
     * images to 8 bits per sample; and convert grayscale to RGB[A] */

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_expand(png_ptr);
#ifdef PNG_READ_16_TO_8_SUPPORTED
    if (bit_depth == 16)
#  ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
        png_set_scale_16(png_ptr);
#  else
        png_set_strip_16(png_ptr);
#  endif
#endif
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);


    /* unlike the example in the libpng documentation, we have *no* idea where
     * this file may have come from--so if it doesn't have a file gamma, don't
     * do any correction ("do no harm") */

    if (png_get_gAMA(png_ptr, info_ptr, &gamma))
        png_set_gamma(png_ptr, display_exponent, gamma);


    /* all transformations have been registered; now update info_ptr data,
     * get rowbytes and channels, and allocate image memory */

    png_read_update_info(png_ptr, info_ptr);

    *pRowbytes = rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    *pChannels = (int)png_get_channels(png_ptr, info_ptr);

    /* Guard against integer overflow */
    if (height > ((size_t)(-1))/rowbytes) {
        fprintf(stderr, "readpng:  image_data buffer would be too large\n");
        return NULL;
    }

    if ((image_data = (uch *)malloc(rowbytes*height)) == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }
    if ((row_pointers = (png_bytepp)malloc(height*sizeof(png_bytep))) == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(image_data);
        image_data = NULL;
        return NULL;
    }

    Trace((stderr, "readpng_get_image:  channels = %d, rowbytes = %ld, height = %ld\n",
        *pChannels, rowbytes, height));


    /* set the individual row_pointers to point at the correct offsets */

    for (i = 0;  i < height;  ++i)
        row_pointers[i] = image_data + i*rowbytes;


    /* now we can go ahead and just read the whole image */

    png_read_image(png_ptr, row_pointers);


    /* and we're done!  (png_read_end() can be omitted if no processing of
     * post-IDAT text/time/etc. is desired) */

    free(row_pointers);
    row_pointers = NULL;

    png_read_end(png_ptr, NULL);

    return image_data;
}


void PNGImage::readpng_cleanup(int free_image_data)
{
    if (free_image_data && image_data) {
        free(image_data);
        image_data = NULL;
    }

    if (png_ptr && info_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        png_ptr = NULL;
        info_ptr = NULL;
    }
}

std::shared_ptr<uimg::Image> uimg::impl::load_png_image(std::shared_ptr<VFilePtrInternal> &file)
{
	auto image = std::make_shared<uimg::Image>();
	auto bSuccess = image->Initialize([&file](std::vector<uint8_t> &outData,uimg::ImageType &outType,uint32_t &outWidth,uint32_t &outHeight) -> bool {
		PNGImage pngImg {};
		unsigned long width,height;
		if(pngImg.readpng_init(file,&width,&height) != 0)
			return false;
		ScopeGuard sg([&pngImg]() {
			pngImg.readpng_cleanup(true);
		});
		switch(pngImg.color_type)
		{
			case PNG_COLOR_TYPE_PALETTE:
				png_set_palette_to_rgb(pngImg.png_ptr);
				break;
			case PNG_COLOR_TYPE_GRAY:
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				png_set_gray_to_rgb(pngImg.png_ptr);
				break;
			case PNG_COLOR_TYPE_RGB:
				png_set_filler(pngImg.png_ptr,0xffff,PNG_FILLER_AFTER);
				break;
		}
		if(pngImg.bit_depth < 8)
			png_set_packing(pngImg.png_ptr);
		if(pngImg.bit_depth == 16)
			png_set_strip_16(pngImg.png_ptr);
		if(pngImg.bit_depth != 8)
			return false;

		/*if (pngImg.color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(pngImg.png_ptr);

		if (pngImg.color_type == PNG_COLOR_TYPE_GRAY) png_set_gray_to_rgb(pngImg.png_ptr);

		if (png_get_valid(pngImg.png_ptr, pngImg.info_ptr,
			PNG_INFO_tRNS)) png_set_tRNS_to_alpha(pngImg.png_ptr);*/

		if(pngImg.color_type == PNG_COLOR_TYPE_RGB || pngImg.color_type == PNG_COLOR_TYPE_RGB_ALPHA)
			png_set_bgr(pngImg.png_ptr);

		outWidth = width;
		outHeight = height;
		
		auto numChannels = -1;
		unsigned long rowBytes = 0u;
		auto *imgData = pngImg.readpng_get_image(1.0,&numChannels,&rowBytes);
		if(pngImg.color_type &PNG_COLOR_MASK_COLOR)
		{
			if(numChannels == 4)
			{
				outType = uimg::ImageType::RGBA;
				outData.resize(width *height *4);
				memcpy(outData.data(),imgData,outData.size());
				if((pngImg.color_type &PNG_COLOR_MASK_ALPHA) == 0)
				{
					for(auto i=decltype(outData.size()){0};i<outData.size();i+=4)
						outData.at(i +3u) = std::numeric_limits<uint8_t>::max();
				}
			}
			else
			{
				if(numChannels != 3)
					return false;
				outType = uimg::ImageType::RGB;
				outData.resize(width *height *3);
				memcpy(outData.data(),imgData,outData.size());
			}
		}
		else if(pngImg.color_type &PNG_COLOR_MASK_ALPHA)
		{
			if(numChannels != 1)
				return false;
			outType = uimg::ImageType::Alpha;
			outData.resize(width *height);
			memcpy(outData.data(),imgData,outData.size());
		}
		else
			return false;
		return true;
	});
	return bSuccess ? image : nullptr;
}

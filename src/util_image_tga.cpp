/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image.h"
#include "util_image_tga.hpp"
#include <fsys/filesystem.h>
#include <cstring>

struct TGAInfo
{
	uint8_t header[6];
	uint32_t bytesPerPixel;
	uint32_t imageSize;
	uint32_t temp;
	uint32_t type;
	uint32_t Height;
	uint32_t Width;
	uint32_t Bpp;
};

struct TGAHeader
{
	uint8_t Header[12];
};

const uint8_t uTGAcompare[12] = {0,0,2,0,0,0,0,0,0,0,0,0};
const uint8_t cTGAcompare[12] = {0,0,10,0,0,0,0,0,0,0,0,0};

static bool load_uncompressed_tga(uimg::Image &texture,VFilePtr &fTGA)
{
	TGAInfo tga;
	if(fTGA->Read(&tga.header,sizeof(tga.header)) == 0)
		return false;
	texture.Initialize([&tga,&fTGA](std::vector<uint8_t> &outData,uimg::ImageType &outType,uint32_t &outWidth,uint32_t &outHeight) -> bool {
		outWidth = tga.header[1] *256 +tga.header[0];
		outHeight = tga.header[3] *256 +tga.header[2];
		auto bpp = tga.header[4];
		tga.Width = outWidth;
		tga.Height = outHeight;
		tga.Bpp = bpp;

		if((outWidth <= 0) || (outHeight <= 0) || ((bpp != 24) && (bpp != 32)))
			return false;
		if(bpp == 24)
			outType = uimg::ImageType::RGB;
		else
			outType = uimg::ImageType::RGBA;

		tga.bytesPerPixel = (tga.Bpp /8);
		tga.imageSize = (tga.bytesPerPixel *tga.Width *tga.Height);
		outData.resize(tga.imageSize);
		if(fTGA->Read(outData.data(),tga.imageSize) != tga.imageSize)
			return false;
		for(uint32_t cswap=0;cswap<(int32_t)tga.imageSize;cswap+=tga.bytesPerPixel)
		{
			outData[cswap] ^= outData[cswap+2] ^=
			outData[cswap] ^= outData[cswap+2];
		}
		return true;
	});
	return true;
}

static bool load_compressed_tga(uimg::Image &texture,VFilePtr &fTGA)
{
	TGAInfo tga {};
	if(fTGA->Read(&tga.header[0],sizeof(tga.header)) == 0)
		return false;
	texture.Initialize([&tga,&fTGA](std::vector<uint8_t> &outData,uimg::ImageType &outType,uint32_t &outWidth,uint32_t &outHeight) -> bool {
		outWidth = tga.header[1] *256 +tga.header[0];
		outHeight = tga.header[3] *256 +tga.header[2];
		auto bpp = tga.header[4];
		tga.Width = outWidth;
		tga.Height = outHeight;
		tga.Bpp = bpp;
		if((outWidth <= 0) || (outHeight <= 0) || ((bpp != 24) && (bpp != 32)))
			return false;
		if(bpp == 24)
			outType = uimg::ImageType::RGB;
		else
			outType = uimg::ImageType::RGBA;

		tga.bytesPerPixel = (tga.Bpp /8);
		tga.imageSize = (tga.bytesPerPixel *tga.Width *tga.Height);
		outData.resize(tga.imageSize);
		auto pixelcount = tga.Height *tga.Width;
		uint32_t currentpixel = 0;
		uint32_t currentbyte = 0;
		std::vector<uint8_t> colorbuffer(tga.bytesPerPixel);
		do
		{
			uint8_t chunkheader = 0;
			if(fTGA->Read(&chunkheader,sizeof(uint8_t)) == 0)
				return false;
			if(chunkheader < 128)
			{
				chunkheader++;
				for(int16_t counter = 0;counter < chunkheader;counter++)
				{
					if(fTGA->Read(colorbuffer.data(),tga.bytesPerPixel) != tga.bytesPerPixel)
						return false;
					outData[currentbyte] = colorbuffer[2];
					outData[currentbyte +1] = colorbuffer[1];
					outData[currentbyte +2] = colorbuffer[0];
					if(tga.bytesPerPixel == 4)
						outData[currentbyte +3] = colorbuffer[3];
					currentbyte += tga.bytesPerPixel;
					currentpixel++;
					if(currentpixel > pixelcount)
						return false;
				}
			}
			else
			{
				chunkheader -= 127;
				if(fTGA->Read(colorbuffer.data(),tga.bytesPerPixel) != tga.bytesPerPixel)
					return false;

				for(int16_t counter=0;counter < chunkheader;counter++)
				{
					outData[currentbyte] = colorbuffer[2];
					outData[currentbyte +1] = colorbuffer[1];
					outData[currentbyte +2] = colorbuffer[0];
					if(tga.bytesPerPixel == 4)
						outData[currentbyte +3] = colorbuffer[3];
					currentbyte += tga.bytesPerPixel;
					currentpixel++;
					if(currentpixel > pixelcount)
						return false;
				}
			}
		}
		while(currentpixel < pixelcount);
		return true;
	});
	return true;
}

static bool load(VFilePtr &fTGA,std::shared_ptr<uimg::Image> &texture)
{
	texture = nullptr;
	TGAHeader tgaheader;
	if(fTGA->Read(&tgaheader,sizeof(TGAHeader)) == 0)
		return false;
	texture = std::make_shared<uimg::Image>();
	if(memcmp(uTGAcompare,&tgaheader,sizeof(tgaheader)) == 0)
		return load_uncompressed_tga(*texture.get(),fTGA);
	else if(memcmp(cTGAcompare,&tgaheader,sizeof(tgaheader)) == 0)
		return load_compressed_tga(*texture.get(),fTGA);
	return false;
}

static bool load(const std::string &path,std::shared_ptr<uimg::Image> &texture)
{
	texture = nullptr;
	auto fTGA = FileManager::OpenFile(path.c_str(),"rb");
	if(fTGA == nullptr)
		return false;
	return load(fTGA,texture);
}

std::shared_ptr<uimg::Image> uimg::impl::load_tga_image(std::shared_ptr<VFilePtrInternal> &file)
{
	std::shared_ptr<uimg::Image> image;
	auto bSuccess = load(file,image);
	return bSuccess ? image : nullptr;
}

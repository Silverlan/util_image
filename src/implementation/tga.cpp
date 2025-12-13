// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <cassert>

module pragma.image;

import :tga;

struct TGAInfo {
	uint8_t header[6];
	uint32_t bytesPerPixel;
	uint32_t imageSize;
	uint32_t temp;
	uint32_t type;
	uint32_t Height;
	uint32_t Width;
	uint32_t Bpp;
};

struct TGAHeader {
	uint8_t Header[12];
};

const uint8_t uTGAcompare[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const uint8_t cTGAcompare[12] = {0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static bool load_uncompressed_tga(std::shared_ptr<pragma::image::ImageBuffer> &texture, pragma::fs::VFilePtr &fTGA)
{
	TGAInfo tga;
	if(fTGA->Read(&tga.header, sizeof(tga.header)) == 0)
		return false;
	auto width = tga.header[1] * 256 + tga.header[0];
	auto height = tga.header[3] * 256 + tga.header[2];

	auto bpp = tga.header[4];
	tga.Width = width;
	tga.Height = height;
	tga.Bpp = bpp;

	if((width <= 0) || (height <= 0) || ((bpp != 24) && (bpp != 32)))
		return false;
	if(bpp == 24)
		texture = pragma::image::ImageBuffer::Create(width, height, pragma::image::Format::RGB8);
	else
		texture = pragma::image::ImageBuffer::Create(width, height, pragma::image::Format::RGBA8);

	tga.bytesPerPixel = (tga.Bpp / 8);
	tga.imageSize = (tga.bytesPerPixel * tga.Width * tga.Height);
	assert(tga.imageSize == texture->GetSize());
	if(tga.imageSize != texture->GetSize())
		throw std::logic_error {"Image Buffer size does not match expected tga size!"};
	if(fTGA->Read(texture->GetData(), tga.imageSize) != tga.imageSize)
		return false;
	auto *data = static_cast<uint8_t *>(texture->GetData());
	for(uint32_t cswap = 0; cswap < (int32_t)tga.imageSize; cswap += tga.bytesPerPixel) {
		data[cswap] ^= data[cswap + 2] ^= data[cswap] ^= data[cswap + 2];
	}
	return true;
}

static bool load_compressed_tga(std::shared_ptr<pragma::image::ImageBuffer> &texture, pragma::fs::VFilePtr &fTGA)
{
	TGAInfo tga {};
	if(fTGA->Read(&tga.header[0], sizeof(tga.header)) == 0)
		return false;
	auto width = tga.header[1] * 256 + tga.header[0];
	auto height = tga.header[3] * 256 + tga.header[2];
	auto bpp = tga.header[4];
	tga.Width = width;
	tga.Height = height;
	tga.Bpp = bpp;
	if((width <= 0) || (height <= 0) || ((bpp != 24) && (bpp != 32)))
		return false;
	if(bpp == 24)
		texture = pragma::image::ImageBuffer::Create(width, height, pragma::image::Format::RGB8);
	else
		texture = pragma::image::ImageBuffer::Create(width, height, pragma::image::Format::RGBA8);

	tga.bytesPerPixel = (tga.Bpp / 8);
	tga.imageSize = (tga.bytesPerPixel * tga.Width * tga.Height);
	assert(tga.imageSize == texture->GetSize());
	if(tga.imageSize != texture->GetSize())
		throw std::logic_error {"Image Buffer size does not match expected tga size!"};
	auto pixelcount = tga.Height * tga.Width;
	auto *data = static_cast<uint8_t *>(texture->GetData());
	uint32_t currentpixel = 0;
	uint32_t currentbyte = 0;
	std::vector<uint8_t> colorbuffer(tga.bytesPerPixel);
	do {
		uint8_t chunkheader = 0;
		if(fTGA->Read(&chunkheader, sizeof(uint8_t)) == 0)
			return false;
		if(chunkheader < 128) {
			chunkheader++;
			for(int16_t counter = 0; counter < chunkheader; counter++) {
				if(fTGA->Read(colorbuffer.data(), tga.bytesPerPixel) != tga.bytesPerPixel)
					return false;
				data[currentbyte] = colorbuffer[2];
				data[currentbyte + 1] = colorbuffer[1];
				data[currentbyte + 2] = colorbuffer[0];
				if(tga.bytesPerPixel == 4)
					data[currentbyte + 3] = colorbuffer[3];
				currentbyte += tga.bytesPerPixel;
				currentpixel++;
				if(currentpixel > pixelcount)
					return false;
			}
		}
		else {
			chunkheader -= 127;
			if(fTGA->Read(colorbuffer.data(), tga.bytesPerPixel) != tga.bytesPerPixel)
				return false;

			for(int16_t counter = 0; counter < chunkheader; counter++) {
				data[currentbyte] = colorbuffer[2];
				data[currentbyte + 1] = colorbuffer[1];
				data[currentbyte + 2] = colorbuffer[0];
				if(tga.bytesPerPixel == 4)
					data[currentbyte + 3] = colorbuffer[3];
				currentbyte += tga.bytesPerPixel;
				currentpixel++;
				if(currentpixel > pixelcount)
					return false;
			}
		}
	} while(currentpixel < pixelcount);
	return true;
}

static bool load(pragma::fs::VFilePtr &fTGA, std::shared_ptr<pragma::image::ImageBuffer> &texture)
{
	texture = nullptr;
	TGAHeader tgaheader;
	if(fTGA->Read(&tgaheader, sizeof(TGAHeader)) == 0)
		return false;
	if(memcmp(uTGAcompare, &tgaheader, sizeof(tgaheader)) == 0)
		return load_uncompressed_tga(texture, fTGA);
	else if(memcmp(cTGAcompare, &tgaheader, sizeof(tgaheader)) == 0)
		return load_compressed_tga(texture, fTGA);
	return false;
}

static bool load(const std::string &path, std::shared_ptr<pragma::image::ImageBuffer> &texture)
{
	texture = nullptr;
	auto fTGA = pragma::fs::open_file(path.c_str(), pragma::fs::FileMode::Read | pragma::fs::FileMode::Binary);
	if(fTGA == nullptr)
		return false;
	return load(fTGA, texture);
}

std::shared_ptr<pragma::image::ImageBuffer> pragma::image::impl::load_tga_image(std::shared_ptr<fs::VFilePtrInternal> &file)
{
	std::shared_ptr<ImageBuffer> image = nullptr;
	auto bSuccess = load(file, image);
	return bSuccess ? image : nullptr;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "compressors/ispctc.hpp"
#include "compressor.hpp"
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/scope_guard.h>
#include "util_image.hpp"
#include "util_image_buffer.hpp"

#include "gli/texture.hpp"
#include "gli/texture2d.hpp"
#include "gli/texture_cube.hpp"
#include "gli/type.hpp"
#include "ispc_texcomp.h"
#include "mathutil/umath.h"
#include "util_texture_info.hpp"
#include <gli/gli.hpp>
#include <gli/generate_mipmaps.hpp>

#include "nv_dds/nv_dds.h"

// BC1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C

// BC1a
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D

// BC2
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E

enum class TextureFormat {
	BC1 = 0,
	BC1a,
	BC2,
	BC3,
	BC4,
	BC5,
	BC6H,
	BC7,
};

struct TextureImageInfo {
	gli::texture texture;
	bool srgb = false;
	bool cubemap = false;
};

static bool save_nv_dds(TextureFormat format, const std::string &filename, const TextureImageInfo &imgInfo, std::string &outErr)
{
	uint32_t ddsFormat;
	uint32_t numComponents;
	switch(format) {
	case TextureFormat::BC1:
		ddsFormat = imgInfo.srgb ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		numComponents = 3;
		break;
	case TextureFormat::BC1a:
		ddsFormat = imgInfo.srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		numComponents = 4;
		break;
	case TextureFormat::BC2:
		ddsFormat = imgInfo.srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		numComponents = 4;
		break;
	default:
		outErr = "Unsupported texture format for DDS: " + std::string {magic_enum::enum_name(format)};
		return false;
	}

	nv_dds::CDDSImage ddsImg;
	auto &inTex = imgInfo.texture;
	auto extent = inTex.extent(0);
	if(imgInfo.cubemap) {
		if(inTex.layers() != 6) {
			outErr = "Cubemap texture must have exactly 6 layers, but has " + std::to_string(inTex.layers());
			return false;
		}
		std::array<nv_dds::CTexture, 6> cubeSides;
		for(uint32_t l = 0; l < cubeSides.size(); ++l) {
			cubeSides[l] = nv_dds::CTexture {extent.x, extent.y, 1, inTex.size(0), static_cast<const uint8_t *>(inTex.data(l, 0, 0))};
			for(size_t m = 1; m < inTex.levels(); ++m) {
				auto mipExtent = inTex.extent(m);
				cubeSides[l].add_mipmap(nv_dds::CSurface {mipExtent.x, mipExtent.y, l, inTex.size(m), static_cast<const uint8_t *>(inTex.data(l, 0, m))});
			}
		}
		ddsImg.create_textureCubemap(ddsFormat, numComponents, cubeSides[0], cubeSides[1], cubeSides[2], cubeSides[3], cubeSides[4], cubeSides[5]);
	}
	else {
		nv_dds::CTexture baseTex {extent.x, extent.y, 1, inTex.size(0), static_cast<const uint8_t *>(inTex.data(0, 0, 0))};
		for(size_t l = 0; l < inTex.layers(); ++l) {
			for(size_t m = 1; m < inTex.levels(); ++m) {
				extent = inTex.extent(m);
				baseTex.add_mipmap(nv_dds::CSurface {extent.x, extent.y, l, inTex.size(m), static_cast<const uint8_t *>(inTex.data(l, 0, m))});
			}
		}
		if(inTex.layers() == 1)
			ddsImg.create_textureFlat(ddsFormat, numComponents, baseTex);
		else
			ddsImg.create_texture3D(ddsFormat, numComponents, baseTex);
	}
	ddsImg.save(filename, false);

	return true;
}
static gli::format to_gli_format(TextureFormat format, bool srgb)
{
	switch(format) {
	case TextureFormat::BC1:
		return srgb ? gli::format::FORMAT_RGB_DXT1_SRGB_BLOCK8 : gli::format::FORMAT_RGB_DXT1_UNORM_BLOCK8;
	case TextureFormat::BC1a:
		return srgb ? gli::format::FORMAT_RGBA_DXT1_SRGB_BLOCK8 : gli::format::FORMAT_RGBA_DXT1_UNORM_BLOCK8;
		break;
	case TextureFormat::BC2:
		return srgb ? gli::format::FORMAT_RGBA_DXT3_SRGB_BLOCK16 : gli::format::FORMAT_RGBA_DXT3_UNORM_BLOCK16;
		break;
	case TextureFormat::BC3:
		return srgb ? gli::format::FORMAT_RGBA_DXT5_SRGB_BLOCK16 : gli::format::FORMAT_RGBA_DXT5_UNORM_BLOCK16;
	case TextureFormat::BC4:
		return gli::format::FORMAT_R_ATI1N_UNORM_BLOCK8;
		break;
	case TextureFormat::BC5:
		return gli::format::FORMAT_RG_ATI2N_UNORM_BLOCK16;
		break;
	default:
		break;
	}
	throw std::runtime_error("Unsupported texture format for gli: " + std::string {magic_enum::enum_name(format)});
	return {};
}
static gli::format to_gli_format(uimg::TextureInfo::InputFormat inputFormat)
{
	switch(inputFormat) {
	case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
		return gli::format::FORMAT_RGBA8_UNORM_PACK8;
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
		return gli::format::FORMAT_RGBA8_UNORM_PACK8;
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
		return gli::format::FORMAT_RGBA16_SFLOAT_PACK16;
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
		return gli::format::FORMAT_RGBA32_SFLOAT_PACK32;
	case uimg::TextureInfo::InputFormat::R32_Float:
		return gli::format::FORMAT_R32_SFLOAT_PACK32;
	}
	throw std::runtime_error("Unsupported input format for gli: " + std::string {magic_enum::enum_name(inputFormat)});
	return {};
}
static uimg::Format to_uimg_format(uimg::TextureInfo::InputFormat inputFormat)
{
	switch(inputFormat) {
	case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
		return uimg::Format::RGBA8;
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
		return uimg::Format::RGBA16;
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
		return uimg::Format::RGBA32;
	case uimg::TextureInfo::InputFormat::R32_Float:
		return uimg::Format::R32;
	}
	throw std::runtime_error("Unsupported input format for uimg: " + std::string {magic_enum::enum_name(inputFormat)});
	return {};
}
static bool save_gli(TextureFormat format, const std::string &filename, const TextureImageInfo &imgInfo, bool saveAsKtx, std::string &outErr)
{
	auto saveSuccess = false;
	if(saveAsKtx)
		saveSuccess = gli::save_ktx(imgInfo.texture, filename);
	else
		saveSuccess = gli::save_dds(imgInfo.texture, filename);
	if(!saveSuccess) {
		outErr = "Failed to save DDS file: " + filename;
		return false;
	}
	return true;
}

enum class DdsLibrary : uint8_t {
	Gli = 0,
	NvDds,
};
static bool save_dds(DdsLibrary ddsLib, TextureFormat format, const std::string &filename, const TextureImageInfo &imgInfo, std::string &outErr)
{
	switch(ddsLib) {
	case DdsLibrary::Gli:
		return save_gli(format, filename, imgInfo, false, outErr);
	case DdsLibrary::NvDds:
		return save_nv_dds(format, filename, imgInfo, outErr);
	}
	outErr = "Unsupported DDS library: " + std::string {magic_enum::enum_name(ddsLib)};
	return false;
}
static bool save_ktx(TextureFormat format, const std::string &filename, const TextureImageInfo &imgInfo, std::string &outErr) { return save_gli(format, filename, imgInfo, true, outErr); }

std::optional<uimg::ITextureCompressor::ResultData> uimg::IspctcTextureCompressor::Compress(const CompressInfo &compressInfo)
{
	auto &texInfo = compressInfo.textureSaveInfo.texInfo;
	if(umath::to_integral(texInfo.outputFormat) < umath::to_integral(uimg::TextureInfo::OutputFormat::BCFirst) || umath::to_integral(texInfo.outputFormat) > umath::to_integral(uimg::TextureInfo::OutputFormat::BCLast))
		return {};

	uint32_t srcSizePerPixel;
	uint32_t dstSizePerBlock;
	TextureFormat dstTexFormat;

	auto outputFormat = texInfo.outputFormat;
	// BC1a and BC3n are not supported by ISPC TC, so we have to fall back
	if(outputFormat == uimg::TextureInfo::OutputFormat::BC1a)
		outputFormat = uimg::TextureInfo::OutputFormat::BC2;
	else if(outputFormat == uimg::TextureInfo::OutputFormat::BC3n)
		outputFormat = uimg::TextureInfo::OutputFormat::BC5;

	switch(outputFormat) {
	case uimg::TextureInfo::OutputFormat::BC1:
		srcSizePerPixel = 4;
		dstSizePerBlock = 8;
		dstTexFormat = TextureFormat::BC1;
		break;
	case uimg::TextureInfo::OutputFormat::BC1a:
		srcSizePerPixel = 4;
		dstSizePerBlock = 8;
		dstTexFormat = TextureFormat::BC1a;
		break;
	case uimg::TextureInfo::OutputFormat::BC2:
		srcSizePerPixel = 4;
		dstSizePerBlock = 16;
		dstTexFormat = TextureFormat::BC2;
		break;
	case uimg::TextureInfo::OutputFormat::BC3:
		srcSizePerPixel = 4;
		dstSizePerBlock = 16;
		dstTexFormat = TextureFormat::BC3;
		break;
	case uimg::TextureInfo::OutputFormat::BC4:
		srcSizePerPixel = 1;
		dstSizePerBlock = 8;
		dstTexFormat = TextureFormat::BC4;
		break;
	case uimg::TextureInfo::OutputFormat::BC5:
		srcSizePerPixel = 2;
		dstSizePerBlock = 16;
		dstTexFormat = TextureFormat::BC5;
		break;
	case uimg::TextureInfo::OutputFormat::BC6:
		srcSizePerPixel = 8;
		dstSizePerBlock = 16;
		dstTexFormat = TextureFormat::BC6H;
		break;
	case uimg::TextureInfo::OutputFormat::BC7:
		srcSizePerPixel = 4;
		dstSizePerBlock = 16;
		dstTexFormat = TextureFormat::BC7;
		break;
	default:
		return {};
	}

	gli::texture inputTex;
	auto srgb = umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::SRGB);

	TextureImageInfo dstImageInfo {};
	auto &outputTex = dstImageInfo.texture;
	dstImageInfo.srgb = srgb;
	dstImageInfo.cubemap = compressInfo.textureSaveInfo.cubemap;

	auto gliFormat = to_gli_format(texInfo.inputFormat);
	auto numDstMipmaps = compressInfo.numMipmaps;
	auto genMipmaps = umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::GenerateMipmaps);
	if(genMipmaps)
		numDstMipmaps = uimg::calculate_mipmap_count(compressInfo.width, compressInfo.height);
	if(compressInfo.numLayers == 1) {
		inputTex = gli::texture2d {gliFormat, gli::extent2d {compressInfo.width, compressInfo.height}, numDstMipmaps};
		outputTex = gli::texture2d {to_gli_format(dstTexFormat, srgb), gli::extent2d {compressInfo.width, compressInfo.height}, numDstMipmaps};
	}
	else if(dstImageInfo.cubemap) {
		inputTex = gli::texture_cube {gliFormat, gli::extent3d {compressInfo.width, compressInfo.height, compressInfo.numLayers}, numDstMipmaps};
		outputTex = gli::texture_cube {to_gli_format(dstTexFormat, srgb), gli::extent3d {compressInfo.width, compressInfo.height, compressInfo.numLayers}, numDstMipmaps};
	}
	else {
		inputTex = gli::texture3d {gliFormat, gli::extent3d {compressInfo.width, compressInfo.height, compressInfo.numLayers}, numDstMipmaps};
		outputTex = gli::texture3d {to_gli_format(dstTexFormat, srgb), gli::extent3d {compressInfo.width, compressInfo.height, compressInfo.numLayers}, numDstMipmaps};
	}
	auto swapRedBlue = (texInfo.inputFormat == uimg::TextureInfo::InputFormat::B8G8R8A8_UInt);
	auto uimgFormat = to_uimg_format(texInfo.inputFormat);
	for(auto l = decltype(compressInfo.numLayers) {0u}; l < compressInfo.numLayers; ++l) {
		for(auto m = decltype(compressInfo.numMipmaps) {0u}; m < compressInfo.numMipmaps; ++m) {
			std::function<void(void)> deleter = nullptr;
			auto *data = compressInfo.getImageData(l, m, deleter);
			assert(data != nullptr);
			auto extent = inputTex.extent(m);
			auto imgBuf = uimg::ImageBuffer::Create(const_cast<unsigned char *>(data), extent.x, extent.y, uimgFormat, true);
			if(swapRedBlue) {
				// Copy the buffer to not pollute the original data
				imgBuf = imgBuf->Copy();
				imgBuf->SwapChannels(uimg::Channel::Red, uimg::Channel::Blue);
			}
			memcpy(inputTex.data(l, 0, m), imgBuf->GetData(), inputTex.size(m));
			if(deleter)
				deleter();
		}
	}
	if(genMipmaps) {
		// TODO: Choose Filter depending on texture type?
		auto filter = gli::filter::FILTER_LINEAR;
		switch(inputTex.target()) {
		case gli::target::TARGET_2D:
			inputTex = gli::generate_mipmaps(gli::texture2d {inputTex}, filter);
			break;
		case gli::target::TARGET_3D:
			inputTex = gli::generate_mipmaps(gli::texture3d {inputTex}, filter);
			break;
		case gli::target::TARGET_CUBE:
			inputTex = gli::generate_mipmaps(gli::texture_cube {inputTex}, filter);
			break;
		}
	}

	for(auto l = decltype(compressInfo.numLayers) {0u}; l < compressInfo.numLayers; ++l) {
		for(auto m = decltype(numDstMipmaps) {0u}; m < numDstMipmaps; ++m) {
			auto extent = inputTex.extent(m);
			auto wMipmap = extent.x;
			auto hMipmap = extent.y;

			rgba_surface src = {};
			src.width = wMipmap;
			src.height = hMipmap;
			src.stride = wMipmap * srcSizePerPixel;
			src.ptr = static_cast<uint8_t *>(inputTex.data(l, 0, m));

			// Each block is always 4x4 pixels
			auto blocksX = (wMipmap + 3) / 4;
			auto blocksY = (hMipmap + 3) / 4;
			auto numBlocks = blocksX * blocksY;

			auto *dstData = static_cast<uint8_t *>(outputTex.data(l, 0, m));
			switch(dstTexFormat) {
			case TextureFormat::BC1:
				CompressBlocksBC1(&src, dstData);
				break;
			case TextureFormat::BC3:
				CompressBlocksBC3(&src, dstData);
				break;
			case TextureFormat::BC4:
				CompressBlocksBC4(&src, dstData);
				break;
			case TextureFormat::BC5:
				CompressBlocksBC5(&src, dstData);
				break;
			case TextureFormat::BC6H:
				{
					bc6h_enc_settings settings;
					GetProfile_bc6h_slow(&settings);
					CompressBlocksBC6H(&src, dstData, &settings);
					break;
				}
			case TextureFormat::BC7:
				{
					bc7_enc_settings settings;
					GetProfile_slow(&settings);
					CompressBlocksBC7(&src, dstData, &settings);
					break;
				}
			}
		}
	}

	auto &outputHandler = compressInfo.outputHandler;
	if(outputHandler.index() == 0) {
		auto &texOutputHandler = std::get<uimg::TextureOutputHandler>(outputHandler);
		auto &tex = dstImageInfo.texture;
		for(size_t l = 0; l < tex.layers(); ++l) {
			for(size_t m = 0; m < tex.levels(); ++m) {
				auto extent = tex.extent(m);
				texOutputHandler.beginImage(tex.size(m), extent.x, extent.y, l, 0, m);
				texOutputHandler.writeData(tex.data(l, 0, m), tex.size(m));
				texOutputHandler.endImage();
			}
		}
		ResultData resultData {};
		return resultData;
	}
	else {
		auto &fileName = std::get<std::string>(outputHandler);
		std::string outputFilePath = compressInfo.absoluteFileName ? fileName.c_str() : get_absolute_path(fileName, texInfo.containerFormat).c_str();

		std::string err;
		auto saveSuccess = false;
		switch(texInfo.containerFormat) {
		case uimg::TextureInfo::ContainerFormat::DDS:
			{
				auto ddsLib = DdsLibrary::Gli;
				// Note: We don't use gli for the older BC formats because it uses a header that isn't recognized by most DDS loaders.
				// nv_dds is also significantly faster, however does not support the newer DDS formats (BC6, BC7, ...), so for those
				// we fall back to gli.
				switch(dstTexFormat) {
				case TextureFormat::BC1:
				case TextureFormat::BC1a:
				case TextureFormat::BC2:
				case TextureFormat::BC3:
					ddsLib = DdsLibrary::NvDds;
					break;
				}
				saveSuccess = save_dds(ddsLib, dstTexFormat, outputFilePath, dstImageInfo, err);
				break;
			}
		case uimg::TextureInfo::ContainerFormat::KTX:
			saveSuccess = save_ktx(dstTexFormat, outputFilePath, dstImageInfo, err);
			break;
		default:
			err = "Unsupported container format: " + std::string {magic_enum::enum_name(texInfo.containerFormat)};
			saveSuccess = false;
			break;
		}

		if(!saveSuccess)
			return {};

		ResultData resultData {};
		resultData.outputFilePath = outputFilePath;
		return resultData;
	}
	return {};
}
std::unique_ptr<uimg::ITextureCompressor> uimg::IspctcTextureCompressor::Create() { return std::make_unique<IspctcTextureCompressor>(); }

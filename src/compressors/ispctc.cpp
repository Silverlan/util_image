// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "compressors/ispctc.hpp"
#include "compressor.hpp"
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/scope_guard.h>
#include "gli/format.hpp"
#include "util_image.hpp"
#include "util_image_buffer.hpp"

#include "gli/texture.hpp"
#include "gli/texture2d.hpp"
#include "gli/texture_cube.hpp"
#include "gli/type.hpp"
#include "ispc_texcomp.h"
#include "mathutil/umath.h"
#include "util_image_types.hpp"
#include "util_texture_info.hpp"
#include <gli/gli.hpp>
#include <gli/generate_mipmaps.hpp>

#pragma clang optimize off
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

static gli::format to_gli_format(TextureFormat format, bool srgb)
{
	switch(format) {
	case TextureFormat::BC1:
		return srgb ? gli::format::FORMAT_RGBA_DXT1_SRGB_BLOCK8 : gli::format::FORMAT_RGBA_DXT1_UNORM_BLOCK8;
	case TextureFormat::BC1a:
		return srgb ? gli::format::FORMAT_RGBA_DXT1_SRGB_BLOCK8 : gli::format::FORMAT_RGBA_DXT1_UNORM_BLOCK8;
	case TextureFormat::BC2:
		return srgb ? gli::format::FORMAT_RGBA_DXT3_SRGB_BLOCK16 : gli::format::FORMAT_RGBA_DXT3_UNORM_BLOCK16;
	case TextureFormat::BC3:
		return srgb ? gli::format::FORMAT_RGBA_DXT5_SRGB_BLOCK16 : gli::format::FORMAT_RGBA_DXT5_UNORM_BLOCK16;
	case TextureFormat::BC4:
		return gli::format::FORMAT_R_ATI1N_UNORM_BLOCK8;
	case TextureFormat::BC5:
		return gli::format::FORMAT_RG_ATI2N_UNORM_BLOCK16;
	case TextureFormat::BC6H:
		return gli::format::FORMAT_RGB_BP_UFLOAT_BLOCK16;
	case TextureFormat::BC7:
		return srgb ? gli::format::FORMAT_RGBA_BP_SRGB_BLOCK16 : gli::format::FORMAT_RGBA_BP_UNORM_BLOCK16;
	default:
		break;
	}
	throw std::runtime_error("Unsupported texture format for gli: " + std::string {magic_enum::enum_name(format)});
	return {};
}
static gli::format to_gli_format(uimg::Format format)
{
	switch(format) {
	case uimg::Format::RGBA8:
		return gli::format::FORMAT_RGBA8_UNORM_PACK8;
	case uimg::Format::R8:
		return gli::format::FORMAT_R8_UNORM_PACK8;
	case uimg::Format::RG8:
		return gli::format::FORMAT_RG8_UNORM_PACK8;
	case uimg::Format::RGBA16:
		return gli::format::FORMAT_RGBA16_SFLOAT_PACK16;
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
};
static bool save_dds(DdsLibrary ddsLib, TextureFormat format, const std::string &filename, const TextureImageInfo &imgInfo, std::string &outErr)
{
	switch(ddsLib) {
	case DdsLibrary::Gli:
		return save_gli(format, filename, imgInfo, false, outErr);
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

	TextureFormat dstTexFormat;

	auto outputFormat = texInfo.outputFormat;
	// BC1a and BC3n are not supported by ISPC TC, so we have to fall back
	// BC2 is also not supported, but BC3 is usually a better choice anyway.
	if(outputFormat == uimg::TextureInfo::OutputFormat::BC1a || outputFormat == uimg::TextureInfo::OutputFormat::BC2)
		outputFormat = uimg::TextureInfo::OutputFormat::BC3;
	else if(outputFormat == uimg::TextureInfo::OutputFormat::BC3n)
		outputFormat = uimg::TextureInfo::OutputFormat::BC5;

	uimg::Format expectedInputFormat;
	switch(outputFormat) {
	case uimg::TextureInfo::OutputFormat::BC1:
		dstTexFormat = TextureFormat::BC1;
		expectedInputFormat = uimg::Format::RGBA8;
		break;
	case uimg::TextureInfo::OutputFormat::BC3:
		dstTexFormat = TextureFormat::BC3;
		expectedInputFormat = uimg::Format::RGBA8;
		break;
	case uimg::TextureInfo::OutputFormat::BC4:
		dstTexFormat = TextureFormat::BC4;
		expectedInputFormat = uimg::Format::R8;
		break;
	case uimg::TextureInfo::OutputFormat::BC5:
		dstTexFormat = TextureFormat::BC5;
		expectedInputFormat = uimg::Format::RG8;
		break;
	case uimg::TextureInfo::OutputFormat::BC6:
		dstTexFormat = TextureFormat::BC6H;
		expectedInputFormat = uimg::Format::RGBA16;
		break;
	case uimg::TextureInfo::OutputFormat::BC7:
		dstTexFormat = TextureFormat::BC7;
		expectedInputFormat = uimg::Format::RGBA8;
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

	auto gliFormat = to_gli_format(expectedInputFormat);
	auto numDstMipmaps = compressInfo.numMipmaps;
	auto genMipmaps = umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::GenerateMipmaps);
	if(genMipmaps)
		numDstMipmaps = uimg::calculate_mipmap_count(compressInfo.width, compressInfo.height);
	if(compressInfo.numLayers == 1) {
		inputTex = gli::texture2d {gliFormat, gli::extent2d {compressInfo.width, compressInfo.height}, numDstMipmaps};
		outputTex = gli::texture2d {to_gli_format(dstTexFormat, srgb), gli::extent2d {compressInfo.width, compressInfo.height}, numDstMipmaps};
	}
	else if(dstImageInfo.cubemap) {
		inputTex = gli::texture_cube {gliFormat, gli::extent3d {compressInfo.width, compressInfo.height, 1}, numDstMipmaps};
		outputTex = gli::texture_cube {to_gli_format(dstTexFormat, srgb), gli::extent3d {compressInfo.width, compressInfo.height, 1}, numDstMipmaps};
	}
	else {
		inputTex = gli::texture2d_array {gliFormat, gli::extent3d {compressInfo.width, compressInfo.height, 1}, compressInfo.numLayers, numDstMipmaps};
		outputTex = gli::texture2d_array {to_gli_format(dstTexFormat, srgb), gli::extent3d {compressInfo.width, compressInfo.height, 1}, compressInfo.numLayers, numDstMipmaps};
	}
	auto swapRedBlue = false; // (texInfo.inputFormat == uimg::TextureInfo::InputFormat::B8G8R8A8_UInt);
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

			// Make sure the image is in the expected input format
			switch(outputFormat) {
			case uimg::TextureInfo::OutputFormat::BC1:
			case uimg::TextureInfo::OutputFormat::BC3:
			case uimg::TextureInfo::OutputFormat::BC7:
				imgBuf->Convert(uimg::Format::RGBA8);
				break;
			case uimg::TextureInfo::OutputFormat::BC4:
				imgBuf->Convert(uimg::Format::R8);
				break;
			case uimg::TextureInfo::OutputFormat::BC5:
				imgBuf->Convert(uimg::Format::RG8);
				break;
			case uimg::TextureInfo::OutputFormat::BC6:
				imgBuf->Convert(uimg::Format::RGBA16);
				break;
			default:
				return {};
			}

			memcpy(inputTex.data(dstImageInfo.cubemap ? 0 : l, dstImageInfo.cubemap ? l : 0, m), imgBuf->GetData(), inputTex.size(m));
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

	uint32_t srcSizePerPixel = gli::block_size(inputTex.format());
	for(auto l = decltype(compressInfo.numLayers) {0u}; l < compressInfo.numLayers; ++l) {
		for(auto m = decltype(numDstMipmaps) {0u}; m < numDstMipmaps; ++m) {
			auto extent = inputTex.extent(m);
			auto wMipmap = extent.x;
			auto hMipmap = extent.y;

			rgba_surface src = {};
			src.width = wMipmap;
			src.height = hMipmap;
			src.stride = wMipmap * srcSizePerPixel;
			src.ptr = static_cast<uint8_t *>(inputTex.data(dstImageInfo.cubemap ? 0 : l, dstImageInfo.cubemap ? l : 0, m));

			// Each block is always 4x4 pixels
			auto blocksX = (wMipmap + 3) / 4;
			auto blocksY = (hMipmap + 3) / 4;
			auto numBlocks = blocksX * blocksY;

			auto *dstData = static_cast<uint8_t *>(outputTex.data(dstImageInfo.cubemap ? 0 : l, dstImageInfo.cubemap ? l : 0, m));
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
				texOutputHandler.writeData(tex.data(dstImageInfo.cubemap ? 0 : l, dstImageInfo.cubemap ? l : 0, m), tex.size(m));
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
				saveSuccess = save_dds(DdsLibrary::Gli, dstTexFormat, outputFilePath, dstImageInfo, err);
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

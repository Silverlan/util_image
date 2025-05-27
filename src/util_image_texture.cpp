/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifdef UIMG_ENABLE_NVTT
#include "util_image.hpp"
#include "util_image_buffer.hpp"
#include "util_texture_info.hpp"
#include "util_image_texture_compressor.hpp"
#include <fsys/filesystem.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_path.hpp>
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/scope_guard.h>
#include <variant>
#include <cstring>

std::string uimg::get_absolute_path(const std::string &fileName, uimg::TextureInfo::ContainerFormat containerFormat)
{
	auto path = ufile::get_path_from_filename(fileName);
	filemanager::create_path(path);
	auto fileNameWithExt = fileName;
	ufile::remove_extension_from_filename(fileNameWithExt, std::array<std::string, 2> {"dds", "ktx"});
	switch(containerFormat) {
	case uimg::TextureInfo::ContainerFormat::DDS:
		fileNameWithExt += ".dds";
		break;
	case uimg::TextureInfo::ContainerFormat::KTX:
		fileNameWithExt += ".ktx";
		break;
	}
	return filemanager::get_program_path() + '/' + fileNameWithExt;
}

static uint32_t calculate_mipmap_size(uint32_t v, uint32_t level)
{
	auto r = v;
	auto scale = static_cast<int>(std::pow(2, level));
	r /= scale;
	if(r == 0)
		r = 1;
	return r;
}

static bool compress_texture(const std::variant<uimg::TextureOutputHandler, std::string> &outputHandler, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &pfGetImgData, const uimg::TextureSaveInfo &texSaveInfo,
  const std::function<void(const std::string &)> &errorHandler = nullptr, bool absoluteFileName = false)
{
	auto r = false;
	{
		auto &texInfo = texSaveInfo.texInfo;
		uimg::ChannelMask channelMask {};
		if(texSaveInfo.channelMask.has_value())
			channelMask = *texSaveInfo.channelMask;

#if TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_NVTT
		switch(texInfo.inputFormat) {
		case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
		case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
			channelMask.SwapChannels(uimg::Channel::Red, uimg::Channel::Blue);
			break;
		}
#endif

		auto numMipmaps = umath::max(texSaveInfo.numMipmaps, static_cast<uint32_t>(1));
		auto numLayers = texSaveInfo.numLayers;
		auto width = texSaveInfo.width;
		auto height = texSaveInfo.height;
		auto szPerPixel = texSaveInfo.szPerPixel;
		auto cubemap = texSaveInfo.cubemap;
		std::vector<std::shared_ptr<uimg::ImageBuffer>> imgBuffers;
		auto fGetImgData = pfGetImgData;
		if(channelMask != uimg::ChannelMask {}) {
			imgBuffers.resize(numLayers * numMipmaps);
			uint32_t idx = 0;

			uimg::Format uimgFormat;
			switch(texSaveInfo.texInfo.inputFormat) {
			case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
			case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
				uimgFormat = uimg::Format::RGBA8;
				break;
			case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
				uimgFormat = uimg::Format::RGBA16;
				break;
			case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
				uimgFormat = uimg::Format::RGBA32;
				break;
			case uimg::TextureInfo::InputFormat::R32_Float:
				uimgFormat = uimg::Format::R32;
				break;
			default:
				throw std::runtime_error {"Texture compression error: Unsupported format " + std::string {magic_enum::enum_name(texSaveInfo.texInfo.inputFormat)}};
			}

			for(auto i = decltype(numLayers) {0u}; i < numLayers; ++i) {
				for(auto m = decltype(numMipmaps) {0u}; m < numMipmaps; ++m) {
					std::function<void()> deleter = nullptr;
					auto *data = fGetImgData(i, m, deleter);
					auto &imgBuf = imgBuffers[idx++] = uimg::ImageBuffer::Create(data, calculate_mipmap_size(width, m), calculate_mipmap_size(height, m), uimgFormat);

					imgBuf->SwapChannels(channelMask);
					if(deleter)
						deleter();
				}
			}
			fGetImgData = [&imgBuffers, numMipmaps](uint32_t layer, uint32_t mip, std::function<void()> &deleter) -> const uint8_t * {
				deleter = nullptr;
				auto idx = layer * numMipmaps + mip;
				auto &imgBuf = imgBuffers[idx];
				return imgBuf ? static_cast<uint8_t *>(imgBuf->GetData()) : nullptr;
			};
		}

		auto size = width * height * szPerPixel;

		uimg::TextureCompressor::CompressInfo compressInfo {};
		compressInfo.getImageData = fGetImgData;
        compressInfo.errorHandler = (errorHandler != nullptr) ? errorHandler : [](const std::string &err) {std::cout<<"Compression failure: "<<err<<std::endl;};
		compressInfo.textureSaveInfo = texSaveInfo;
		compressInfo.outputHandler = outputHandler;
		compressInfo.width = width;
		compressInfo.height = height;
		compressInfo.numMipmaps = numMipmaps;
		compressInfo.numLayers = numLayers;
		compressInfo.absoluteFileName = absoluteFileName;

		auto compressor = uimg::TextureCompressor::Create();
		auto resultData = compressor->Compress(compressInfo);
		if(resultData) {
			if(resultData->outputFilePath)
				filemanager::update_file_index_cache(*resultData->outputFilePath, true);
		}
		r = resultData.has_value();
	}
	return r;
}

bool uimg::compress_texture(const TextureOutputHandler &outputHandler, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const uimg::TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler)
{
	return ::compress_texture(outputHandler, fGetImgData, texSaveInfo, errorHandler);
}

bool uimg::compress_texture(std::vector<std::vector<std::vector<uint8_t>>> &outputData, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const uimg::TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler)
{
	outputData.resize(texSaveInfo.numLayers, std::vector<std::vector<uint8_t>>(texSaveInfo.numMipmaps, std::vector<uint8_t> {}));
	auto iLevel = std::numeric_limits<uint32_t>::max();
	auto iMipmap = std::numeric_limits<uint32_t>::max();
	uimg::TextureOutputHandler outputHandler {};
	outputHandler.beginImage = [&outputData, &iLevel, &iMipmap](int size, int width, int height, int depth, int face, int miplevel) {
		iLevel = face;
		iMipmap = miplevel;
		outputData[iLevel][iMipmap].resize(size);
	};
	outputHandler.writeData = [&outputData, &iLevel, &iMipmap](const void *data, int size) -> bool {
		if(iLevel == std::numeric_limits<uint32_t>::max() || iMipmap == std::numeric_limits<uint32_t>::max())
			return true;
		std::memcpy(outputData[iLevel][iMipmap].data(), data, size);
		return true;
	};
	outputHandler.endImage = [&iLevel, &iMipmap]() {
		iLevel = std::numeric_limits<uint32_t>::max();
		iMipmap = std::numeric_limits<uint32_t>::max();
	};
	return ::compress_texture(outputHandler, fGetImgData, texSaveInfo, errorHandler);
}

bool uimg::save_texture(const std::string &fileName, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const uimg::TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler, bool absoluteFileName)
{
	return ::compress_texture(fileName, fGetImgData, texSaveInfo, errorHandler, absoluteFileName);
}

bool uimg::save_texture(const std::string &fileName, uimg::ImageBuffer &imgBuffer, const uimg::TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler, bool absoluteFileName)
{
	constexpr auto numLayers = 1u;
	constexpr auto numMipmaps = 1u;

	auto newTexSaveInfo = texSaveInfo;
	auto swapRedBlue = (texSaveInfo.texInfo.inputFormat == uimg::TextureInfo::InputFormat::R8G8B8A8_UInt);
	if(swapRedBlue) {
		imgBuffer.SwapChannels(uimg::Channel::Red, uimg::Channel::Blue);
		newTexSaveInfo.texInfo.inputFormat = uimg::TextureInfo::InputFormat::B8G8R8A8_UInt;
	}
	newTexSaveInfo.width = imgBuffer.GetWidth();
	newTexSaveInfo.height = imgBuffer.GetHeight();
	newTexSaveInfo.szPerPixel = imgBuffer.GetPixelSize();
	newTexSaveInfo.numLayers = numLayers;
	newTexSaveInfo.numMipmaps = numMipmaps;
	auto success = save_texture(
	  fileName, [&imgBuffer](uint32_t iLayer, uint32_t iMipmap, std::function<void(void)> &outDeleter) -> const uint8_t * { return static_cast<uint8_t *>(imgBuffer.GetData()); }, newTexSaveInfo, errorHandler, absoluteFileName);
	if(swapRedBlue)
		imgBuffer.SwapChannels(uimg::Channel::Red, uimg::Channel::Blue);
	return success;
}

bool uimg::save_texture(const std::string &fileName, const std::vector<std::vector<const void *>> &imgLayerMipmapData, const uimg::TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler, bool absoluteFileName)
{
	auto numLayers = imgLayerMipmapData.size();
	auto numMipmaps = imgLayerMipmapData.empty() ? 1 : imgLayerMipmapData.front().size();
	auto ltexSaveInfo = texSaveInfo;
	ltexSaveInfo.numLayers = numLayers;
	ltexSaveInfo.numMipmaps = numMipmaps;
	return save_texture(
	  fileName,
	  [&imgLayerMipmapData](uint32_t iLayer, uint32_t iMipmap, std::function<void(void)> &outDeleter) -> const uint8_t * {
		  if(iLayer >= imgLayerMipmapData.size())
			  return nullptr;
		  auto &mipmapData = imgLayerMipmapData.at(iLayer);
		  if(iMipmap >= mipmapData.size())
			  return nullptr;
		  return static_cast<const uint8_t *>(mipmapData.at(iMipmap));
	  },
	  ltexSaveInfo, errorHandler, absoluteFileName);
}
#endif
#pragma optimize("", on)

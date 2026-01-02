// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module pragma.image;

import :compressor;
import pragma.filesystem;

#ifdef UIMG_ENABLE_TEXTURE_COMPRESSION
std::string pragma::image::get_absolute_path(const std::string &fileName, TextureInfo::ContainerFormat containerFormat)
{
	auto path = ufile::get_path_from_filename(fileName);
	fs::create_path(path);
	auto fileNameWithExt = fileName;
	ufile::remove_extension_from_filename(fileNameWithExt, std::array<std::string, 2> {"dds", "ktx"});
	switch(containerFormat) {
	case TextureInfo::ContainerFormat::DDS:
		fileNameWithExt += ".dds";
		break;
	case TextureInfo::ContainerFormat::KTX:
		fileNameWithExt += ".ktx";
		break;
	}
	return fs::get_program_write_path() + '/' + fileNameWithExt;
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

static bool compress_texture(const std::variant<pragma::image::TextureOutputHandler, std::string> &outputHandler, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &pfGetImgData, const pragma::image::TextureSaveInfo &texSaveInfo,
  const std::function<void(const std::string &)> &errorHandler = nullptr, bool absoluteFileName = false)
{
	auto r = false;
	{
		auto &texInfo = texSaveInfo.texInfo;
		pragma::image::ChannelMask channelMask {};
		if(texSaveInfo.channelMask.has_value())
			channelMask = *texSaveInfo.channelMask;

		auto numMipmaps = pragma::math::max(texSaveInfo.numMipmaps, static_cast<uint32_t>(1));
		auto numLayers = texSaveInfo.numLayers;
		auto width = texSaveInfo.width;
		auto height = texSaveInfo.height;
		auto szPerPixel = texSaveInfo.szPerPixel;
		auto cubemap = texSaveInfo.cubemap;
		std::vector<std::shared_ptr<pragma::image::ImageBuffer>> imgBuffers;
		auto fGetImgData = pfGetImgData;

		auto size = width * height * szPerPixel;

		pragma::image::ITextureCompressor::CompressInfo compressInfo {};
		compressInfo.getImageData = fGetImgData;
		compressInfo.errorHandler = (errorHandler != nullptr) ? errorHandler : [](const std::string &err) { std::cout << "Compression failure: " << err << std::endl; };
		compressInfo.textureSaveInfo = texSaveInfo;
		compressInfo.outputHandler = outputHandler;
		compressInfo.width = width;
		compressInfo.height = height;
		compressInfo.numMipmaps = numMipmaps;
		compressInfo.numLayers = numLayers;
		compressInfo.absoluteFileName = absoluteFileName;

		std::vector<pragma::image::CompressorLibrary> libTypes;
		if(texSaveInfo.compressorLibrary)
			libTypes.push_back(*texSaveInfo.compressorLibrary);
		else {
			// Compressor libraries in order of priority.
			// We evaluate nvtt last, because it may require some preprocessing (swapping red and blue channels)
			libTypes = {
			  pragma::image::CompressorLibrary::Ispctc,
			  pragma::image::CompressorLibrary::Compressonator,
			  pragma::image::CompressorLibrary::Nvtt, // Nvtt needs to be last!
			};
		}

		for(auto compressorLib : libTypes) {
			if(compressorLib == pragma::image::CompressorLibrary::Nvtt) {
				switch(texInfo.inputFormat) {
				case pragma::image::TextureInfo::InputFormat::R8G8B8A8_UInt:
				case pragma::image::TextureInfo::InputFormat::B8G8R8A8_UInt:
					channelMask.SwapChannels(pragma::image::Channel::Red, pragma::image::Channel::Blue);
					break;
				}

				if(channelMask != pragma::image::ChannelMask {}) {
					imgBuffers.resize(numLayers * numMipmaps);
					uint32_t idx = 0;

					pragma::image::Format uimgFormat;
					switch(texSaveInfo.texInfo.inputFormat) {
					case pragma::image::TextureInfo::InputFormat::R8G8B8A8_UInt:
					case pragma::image::TextureInfo::InputFormat::B8G8R8A8_UInt:
						uimgFormat = pragma::image::Format::RGBA8;
						break;
					case pragma::image::TextureInfo::InputFormat::R16G16B16A16_Float:
						uimgFormat = pragma::image::Format::RGBA16;
						break;
					case pragma::image::TextureInfo::InputFormat::R32G32B32A32_Float:
						uimgFormat = pragma::image::Format::RGBA32;
						break;
					case pragma::image::TextureInfo::InputFormat::R32_Float:
						uimgFormat = pragma::image::Format::R32;
						break;
					default:
						throw std::runtime_error {"Texture compression error: Unsupported format " + std::string {magic_enum::enum_name(texSaveInfo.texInfo.inputFormat)}};
					}

					for(auto i = decltype(numLayers) {0u}; i < numLayers; ++i) {
						for(auto m = decltype(numMipmaps) {0u}; m < numMipmaps; ++m) {
							std::function<void()> deleter = nullptr;
							auto *data = fGetImgData(i, m, deleter);
							auto &imgBuf = imgBuffers[idx++] = pragma::image::ImageBuffer::Create(data, calculate_mipmap_size(width, m), calculate_mipmap_size(height, m), uimgFormat);

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
			}
			auto compressor = pragma::image::ITextureCompressor::Create(compressorLib);
			if(!compressor)
				continue;
			auto resultData = compressor->Compress(compressInfo);
			if(resultData) {
				if(resultData->outputFilePath)
					pragma::fs::update_file_index_cache(*resultData->outputFilePath, true);
				r = true;
				break;
			}
		}
	}
	return r;
}

bool pragma::image::compress_texture(const TextureOutputHandler &outputHandler, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler)
{
	return ::compress_texture(outputHandler, fGetImgData, texSaveInfo, errorHandler);
}

bool pragma::image::compress_texture(std::vector<std::vector<std::vector<uint8_t>>> &outputData, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler)
{
	outputData.resize(texSaveInfo.numLayers, std::vector<std::vector<uint8_t>>(texSaveInfo.numMipmaps, std::vector<uint8_t> {}));
	auto iLevel = std::numeric_limits<uint32_t>::max();
	auto iMipmap = std::numeric_limits<uint32_t>::max();
	TextureOutputHandler outputHandler {};
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

bool pragma::image::save_texture(const std::string &fileName, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler, bool absoluteFileName)
{
	return ::compress_texture(fileName, fGetImgData, texSaveInfo, errorHandler, absoluteFileName);
}

bool pragma::image::save_texture(const std::string &fileName, ImageBuffer &imgBuffer, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler, bool absoluteFileName)
{
	constexpr auto numLayers = 1u;
	constexpr auto numMipmaps = 1u;

	auto newTexSaveInfo = texSaveInfo;
	auto swapRedBlue = (texSaveInfo.texInfo.inputFormat == TextureInfo::InputFormat::R8G8B8A8_UInt);
	if(swapRedBlue) {
		imgBuffer.SwapChannels(Channel::Red, Channel::Blue);
		newTexSaveInfo.texInfo.inputFormat = TextureInfo::InputFormat::B8G8R8A8_UInt;
	}
	newTexSaveInfo.width = imgBuffer.GetWidth();
	newTexSaveInfo.height = imgBuffer.GetHeight();
	newTexSaveInfo.szPerPixel = imgBuffer.GetPixelSize();
	newTexSaveInfo.numLayers = numLayers;
	newTexSaveInfo.numMipmaps = numMipmaps;
	auto success = save_texture(fileName, [&imgBuffer](uint32_t iLayer, uint32_t iMipmap, std::function<void(void)> &outDeleter) -> const uint8_t * { return static_cast<uint8_t *>(imgBuffer.GetData()); }, newTexSaveInfo, errorHandler, absoluteFileName);
	if(swapRedBlue)
		imgBuffer.SwapChannels(Channel::Red, Channel::Blue);
	return success;
}

bool pragma::image::save_texture(const std::string &fileName, const std::vector<std::vector<const void *>> &imgLayerMipmapData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler, bool absoluteFileName)
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

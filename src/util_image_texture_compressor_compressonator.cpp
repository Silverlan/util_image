/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image_texture_compressor.hpp"

#if UIMG_ENABLE_NVTT && TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_COMPRESSONATOR

#include <compressonator.h>
#include <common.h>
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/scope_guard.h>

static std::optional<CMP_FORMAT> to_cmp_enum(uimg::TextureInfo::OutputFormat format)
{
	switch(format) {
	case uimg::TextureInfo::OutputFormat::RGB:
		return CMP_FORMAT_RGB_888;
	case uimg::TextureInfo::OutputFormat::RGBA:
		return CMP_FORMAT_RGBA_8888;
	case uimg::TextureInfo::OutputFormat::DXT1:
		return CMP_FORMAT_DXT1;
	case uimg::TextureInfo::OutputFormat::DXT1a:
		return CMP_FORMAT_DXT1;
	case uimg::TextureInfo::OutputFormat::DXT3:
		return CMP_FORMAT_DXT3;
	case uimg::TextureInfo::OutputFormat::DXT5:
		return CMP_FORMAT_DXT5;
	case uimg::TextureInfo::OutputFormat::DXT5n:
		return CMP_FORMAT_DXT5;
	case uimg::TextureInfo::OutputFormat::BC1:
		return CMP_FORMAT_BC1;
	case uimg::TextureInfo::OutputFormat::BC1a:
		return CMP_FORMAT_BC1;
	case uimg::TextureInfo::OutputFormat::BC2:
		return CMP_FORMAT_BC2;
	case uimg::TextureInfo::OutputFormat::BC3:
		return CMP_FORMAT_BC3;
	case uimg::TextureInfo::OutputFormat::BC3n:
		return CMP_FORMAT_BC3;
	case uimg::TextureInfo::OutputFormat::BC4:
		return CMP_FORMAT_BC4;
	case uimg::TextureInfo::OutputFormat::BC5:
		return CMP_FORMAT_BC5;
	case uimg::TextureInfo::OutputFormat::DXT1n:
		return CMP_FORMAT_BC1;
	case uimg::TextureInfo::OutputFormat::CTX1:
		return CMP_FORMAT_BC1;
	case uimg::TextureInfo::OutputFormat::BC6:
		return CMP_FORMAT_BC6H;
	case uimg::TextureInfo::OutputFormat::BC7:
		return CMP_FORMAT_BC7;
	case uimg::TextureInfo::OutputFormat::BC3_RGBM:
		return CMP_FORMAT_BC3;
	case uimg::TextureInfo::OutputFormat::ETC1:
		return CMP_FORMAT_ETC_RGB;
	case uimg::TextureInfo::OutputFormat::ETC2_R:
		return CMP_FORMAT_ETC2_RGB;
	case uimg::TextureInfo::OutputFormat::ETC2_RG:
		return CMP_FORMAT_ETC2_RGB;
	case uimg::TextureInfo::OutputFormat::ETC2_RGB:
		return CMP_FORMAT_ETC2_RGB;
	case uimg::TextureInfo::OutputFormat::ETC2_RGBA:
		return CMP_FORMAT_ETC2_RGBA;
	case uimg::TextureInfo::OutputFormat::ETC2_RGB_A1:
		return CMP_FORMAT_ETC2_RGBA1;
	case uimg::TextureInfo::OutputFormat::ETC2_RGBM:
		return {};
	case uimg::TextureInfo::OutputFormat::KeepInputImageFormat:
		break;
	}

	// Default case, returning an invalid format
	return {};
}

static std::string cmp_result_to_string(CMP_ERROR err)
{
	switch(err) {
	case CMP_OK:
		return "Ok.";
	case CMP_ABORTED:
		return "The conversion was aborted.";
	case CMP_ERR_INVALID_SOURCE_TEXTURE:
		return "The source texture is invalid.";
	case CMP_ERR_INVALID_DEST_TEXTURE:
		return "The destination texture is invalid.";
	case CMP_ERR_UNSUPPORTED_SOURCE_FORMAT:
		return "The source format is not a supported format.";
	case CMP_ERR_UNSUPPORTED_DEST_FORMAT:
		return "The destination format is not a supported format.";
	case CMP_ERR_UNSUPPORTED_GPU_ASTC_DECODE:
		return "The GPU hardware is not supported for ASTC decoding.";
	case CMP_ERR_UNSUPPORTED_GPU_BASIS_DECODE:
		return "The GPU hardware is not supported for Basis decoding.";
	case CMP_ERR_SIZE_MISMATCH:
		return "The source and destination texture sizes do not match.";
	case CMP_ERR_UNABLE_TO_INIT_CODEC:
		return "Compressonator was unable to initialize the codec needed for conversion.";
	case CMP_ERR_UNABLE_TO_INIT_DECOMPRESSLIB:
		return "GPU_Decode Lib was unable to initialize the codec needed for decompression.";
	case CMP_ERR_UNABLE_TO_INIT_COMPUTELIB:
		return "Compute Lib was unable to initialize the codec needed for compression.";
	case CMP_ERR_CMP_DESTINATION:
		return "Error in compressing destination texture.";
	case CMP_ERR_MEM_ALLOC_FOR_MIPSET:
		return "Memory Error: allocating MIPSet compression level data buffer.";
	case CMP_ERR_UNKNOWN_DESTINATION_FORMAT:
		return "The destination Codec Type is unknown! In SDK refer to GetCodecType().";
	case CMP_ERR_FAILED_HOST_SETUP:
		return "Failed to setup Host for processing.";
	case CMP_ERR_PLUGIN_FILE_NOT_FOUND:
		return "The required plugin library was not found.";
	case CMP_ERR_UNABLE_TO_LOAD_FILE:
		return "The requested file was not loaded.";
	case CMP_ERR_UNABLE_TO_CREATE_ENCODER:
		return "Request to create an encoder failed.";
	case CMP_ERR_UNABLE_TO_LOAD_ENCODER:
		return "Unable to load an encode library.";
	case CMP_ERR_NOSHADER_CODE_DEFINED:
		return "No shader code is available for the requested framework.";
	case CMP_ERR_GPU_DOESNOT_SUPPORT_COMPUTE:
		return "The GPU device selected does not support compute.";
	case CMP_ERR_NOPERFSTATS:
		return "No Performance Stats are available.";
	case CMP_ERR_GPU_DOESNOT_SUPPORT_CMP_EXT:
		return "The GPU does not support the requested compression extension!";
	case CMP_ERR_GAMMA_OUTOFRANGE:
		return "Gamma value set for processing is out of range.";
	case CMP_ERR_PLUGIN_SHAREDIO_NOT_SET:
		return "The plugin C_PluginSetSharedIO call was not set and is required for this plugin to operate.";
	case CMP_ERR_UNABLE_TO_INIT_D3DX:
		return "Unable to initialize DirectX SDK or get a specific DX API.";
	case CMP_FRAMEWORK_NOT_INITIALIZED:
		return "CMP_InitFramework failed or not called.";
	case CMP_ERR_GENERIC:
		return "An unknown error occurred.";
	default:
		return "Unknown error code.";
	}
}

static std::optional<CMP_ChannelFormat> get_cmp_channel_format(uimg::TextureInfo::InputFormat inputFormat)
{
	switch(inputFormat) {
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
	case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
		return CMP_ChannelFormat::CF_8bit;
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
		return CMP_ChannelFormat::CF_16bit;
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
	case uimg::TextureInfo::InputFormat::R32_Float:
		return CMP_ChannelFormat::CF_Float32;
	}
	return {};
}

static std::optional<uint8_t> get_cmp_format_pixel_size(uimg::TextureInfo::InputFormat inputFormat)
{
	switch(inputFormat) {
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
	case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
		return 4;
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
		return 8;
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
		return 16;
	case uimg::TextureInfo::InputFormat::R32_Float:
		return 4;
	}
	return {};
}

static std::optional<CMP_TextureDataType> get_cmp_texture_data_type(const uimg::TextureInfo &texInfo)
{
	if(texInfo.IsNormalMap())
		return CMP_TextureDataType::TDT_NORMAL_MAP;
	switch(texInfo.inputFormat) {
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
	case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
		return CMP_TextureDataType::TDT_ARGB;
	case uimg::TextureInfo::InputFormat::R32_Float:
		return CMP_TextureDataType::TDT_R;
	}
	return {};
}

static std::optional<CMP_TextureType> get_cmp_texture_type(const uimg::TextureSaveInfo &texInfo)
{
	if(texInfo.cubemap)
		return CMP_TextureType::TT_CubeMap;
	return CMP_TextureType::TT_2D;
}

namespace uimg {
	class TextureCompressorCompressonator : public TextureCompressor {
	  public:
		virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) override
		{
			CMP_InitFramework();

			auto &textureSaveInfo = compressInfo.textureSaveInfo;
			auto &texInfo = compressInfo.textureSaveInfo.texInfo;
			auto cmpFormat = to_cmp_enum(texInfo.outputFormat);
			if(!cmpFormat) {
				compressInfo.errorHandler("Failed to determine compressonator format for output format " + std::string {magic_enum::enum_name(texInfo.outputFormat)} + "!");
				return {};
			}

			auto channelFormat = get_cmp_channel_format(texInfo.inputFormat);
			if(!channelFormat) {
				compressInfo.errorHandler("Failed to determine compressonator channel format for input format " + std::string {magic_enum::enum_name(texInfo.inputFormat)} + "!");
				return {};
			}
			auto texDataType = get_cmp_texture_data_type(texInfo);
			if(!texDataType) {
				compressInfo.errorHandler("Failed to determine compressonator texture data type for input image!");
				return {};
			}
			auto texType = get_cmp_texture_type(textureSaveInfo);
			if(!texType) {
				compressInfo.errorHandler("Failed to determine compressonator texture type for input image!");
				return {};
			}
			auto pixelSize = get_cmp_format_pixel_size(texInfo.inputFormat);
			if(!pixelSize) {
				compressInfo.errorHandler("Failed to determine pixel size for input format " + std::string {magic_enum::enum_name(texInfo.inputFormat)} + "!");
				return {};
			}

			KernelOptions kernel_options;
			memset(&kernel_options, 0, sizeof(KernelOptions));
			kernel_options.encodeWith = CMP_GPU_OCL;
			kernel_options.format = *cmpFormat; // Set the format to process
			kernel_options.fquality = 1.f;      // Set the quality of the result
			kernel_options.threads = 0;         // Auto setting
			CMIPS cmips {};

			MipSet ms {};
			auto res = cmips.AllocateMipSet(&ms, *channelFormat, *texDataType, *texType, compressInfo.width, compressInfo.height, compressInfo.numLayers);
			if(!res) {
				compressInfo.errorHandler("Failed to allocate mip set with " + std::to_string(compressInfo.numMipmaps) + " mipmaps and " + std::to_string(compressInfo.numLayers) + " layers!");
				return {};
			}
			util::ScopeGuard sgMs {[&cmips, &ms]() { cmips.FreeMipSet(&ms); }};

			for(auto l = decltype(compressInfo.numLayers) {0u}; l < compressInfo.numLayers; ++l) {
				for(auto m = decltype(compressInfo.numMipmaps) {0u}; m < compressInfo.numMipmaps; ++m) {
					std::function<void(void)> deleter = nullptr;
					auto *data = compressInfo.getImageData(l, m, deleter);
					if(!data) {
						compressInfo.errorHandler("Failed to retrieve mipmap data for mipmap " + std::to_string(m) + " of layer " + std::to_string(l) + "!");
						return {};
					}
					auto success = cmips.AllocateMipLevelData(cmips.GetMipLevel(&ms, m, l), ms.m_nWidth, ms.m_nHeight, *channelFormat, *texDataType);
					if(!success) {
						compressInfo.errorHandler("Failed to allocate memory for mipmap " + std::to_string(m) + " of layer " + std::to_string(l) + "!");
						return {};
					}
					auto *pMipLv = cmips.GetMipLevel(&ms, m, l);
					CMP_BYTE *pData = pMipLv->m_pbData;

					auto expectedSize = ms.m_nWidth * ms.m_nHeight * (*pixelSize);
					CMP_DWORD dwSize = pMipLv->m_dwLinearSize;
					if(dwSize != expectedSize) {
						compressInfo.errorHandler("Size mismatch for mipmap " + std::to_string(m) + " of layer " + std::to_string(l) + ": Expected " + std::to_string(expectedSize) + ", got " + std::to_string(dwSize) + "!");
						return {};
					}
					memcpy(pData, data, dwSize);

					if(deleter)
						deleter();
				}
			}
			ms.m_nMipLevels = compressInfo.numMipmaps;

			if(umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::GenerateMipmaps)) {
				auto resMip = CMP_GenerateMIPLevels(&ms, 1);
				if(resMip != CMP_OK) {
					// CMP_GenerateMIPLevels returns a CMP_INT, even though the return values are CMP_ERROR. Reason unclear.
					compressInfo.errorHandler(cmp_result_to_string(static_cast<CMP_ERROR>(resMip)));
					return {};
				}
			}

			MipSet msProcessed {};
			memset(&msProcessed, 0, sizeof(msProcessed));
			auto err = CMP_ProcessTexture(
			  &ms, &msProcessed, kernel_options, +[](CMP_FLOAT fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2) -> bool {
				  auto abort = false;
				  return abort;
			  });
			if(err != CMP_OK) {
				compressInfo.errorHandler(cmp_result_to_string(err));
				return {};
			}
			util::ScopeGuard sgMsProcessed {[&msProcessed]() { CMP_FreeMipSet(&msProcessed); }};
			ResultData resultData {};
			auto &outputHandler = compressInfo.outputHandler;
			if(outputHandler.index() == 0) {
				auto &texOutputHandler = std::get<uimg::TextureOutputHandler>(outputHandler);
				auto &r = const_cast<uimg::TextureOutputHandler &>(texOutputHandler);
				for(auto l = decltype(compressInfo.numLayers) {0u}; l < compressInfo.numLayers; ++l) {
					for(auto m = decltype(compressInfo.numMipmaps) {0u}; m < compressInfo.numMipmaps; ++m) {
						auto *pMipLv = cmips.GetMipLevel(&msProcessed, m, l);
						CMP_BYTE *pData = pMipLv->m_pbData;
						CMP_DWORD dwSize = pMipLv->m_dwLinearSize;
						r.beginImage(dwSize, ms.m_nWidth, ms.m_nHeight, 1, l, m);
						r.writeData(pData, dwSize);
						r.endImage();
					}
				}
			}
			else {
				auto &fileName = std::get<std::string>(outputHandler);
				std::string outputFilePath = compressInfo.absoluteFileName ? fileName.c_str() : get_absolute_path(fileName, texInfo.containerFormat).c_str();

				// TODO: This does not work with KTX
				err = CMP_SaveTexture(outputFilePath.c_str(), &msProcessed);
				if(err != CMP_OK) {
					compressInfo.errorHandler(cmp_result_to_string(err));
					return {};
				}

				resultData.outputFilePath = outputFilePath;
			}
			return resultData;
		}
	};
};

std::unique_ptr<uimg::TextureCompressor> uimg::TextureCompressor::Create() { return std::make_unique<TextureCompressorCompressonator>(); }

#endif

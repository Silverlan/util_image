/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define TEX_COMPRESSION_LIBRARY_NVTT 0
#define TEX_COMPRESSION_LIBRARY_COMPRESSONATOR 1

#ifndef TEX_COMPRESSION_LIBRARY

#define TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_NVTT

#endif

#ifdef UIMG_ENABLE_NVTT
#include "util_image.hpp"
#include "util_image_buffer.hpp"
#include "util_texture_info.hpp"
#if TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_NVTT
#include <nvtt/nvtt.h>
#else
#include <compressonator.h>
#include <common.h>
#endif
#include <fsys/filesystem.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_path.hpp>
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/scope_guard.h>
#include <variant>
#include <cstring>

#pragma optimize("", off)

class TextureCompressor {
  public:
	struct CompressInfo {
		std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> getImageData;
		std::function<void(const std::string &)> errorHandler;
		uimg::TextureSaveInfo textureSaveInfo;
		std::variant<uimg::TextureOutputHandler, std::string> outputHandler;
		uint32_t width;
		uint32_t height;
		uint32_t numMipmaps;
		uint32_t numLayers;
		bool absoluteFileName;
	};
	struct ResultData {
		std::optional<std::string> outputFilePath;
	};
	virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) = 0;
};

#if TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_NVTT
struct OutputHandler : public nvtt::OutputHandler {
	OutputHandler(VFilePtrReal f) : m_file {f} {}
	virtual ~OutputHandler() override {}

	// Indicate the start of a new compressed image that's part of the final texture.
	virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override {}

	// Output data. Compressed data is output as soon as it's generated to minimize memory allocations.
	virtual bool writeData(const void *data, int size) override { return m_file->Write(data, size) == size; }

	// Indicate the end of the compressed image. (New in NVTT 2.1)
	virtual void endImage() override {}
  private:
	VFilePtrReal m_file = nullptr;
};

struct ErrorHandler : public nvtt::ErrorHandler {
	ErrorHandler(const std::function<void(const std::string &)> &errorHandler) : m_errorHandler {errorHandler} {}
	virtual ~ErrorHandler() override {};

	// Signal error.
	virtual void error(nvtt::Error e) override
	{
		if(m_errorHandler == nullptr)
			return;
		switch(e) {
		case nvtt::Error_Unknown:
			m_errorHandler("Unknown error");
			break;
		case nvtt::Error_InvalidInput:
			m_errorHandler("Invalid input");
			break;
		case nvtt::Error_UnsupportedFeature:
			m_errorHandler("Unsupported feature");
			break;
		case nvtt::Error_CudaError:
			m_errorHandler("Cuda error");
			break;
		case nvtt::Error_FileOpen:
			m_errorHandler("File open error");
			break;
		case nvtt::Error_FileWrite:
			m_errorHandler("File write error");
			break;
		case nvtt::Error_UnsupportedOutputFormat:
			m_errorHandler("Unsupported output format");
			break;
		}
	}
  private:
	std::function<void(const std::string &)> m_errorHandler = nullptr;
};

static nvtt::Format to_nvtt_enum(uimg::TextureInfo::OutputFormat format)
{
	switch(format) {
	case uimg::TextureInfo::OutputFormat::RGB:
		return nvtt::Format_RGB;
	case uimg::TextureInfo::OutputFormat::RGBA:
		return nvtt::Format_RGBA;
	case uimg::TextureInfo::OutputFormat::DXT1:
		return nvtt::Format_DXT1;
	case uimg::TextureInfo::OutputFormat::DXT1a:
		return nvtt::Format_DXT1a;
	case uimg::TextureInfo::OutputFormat::DXT3:
		return nvtt::Format_DXT3;
	case uimg::TextureInfo::OutputFormat::DXT5:
		return nvtt::Format_DXT5;
	case uimg::TextureInfo::OutputFormat::DXT5n:
		return nvtt::Format_DXT5n;
	case uimg::TextureInfo::OutputFormat::BC1:
		return nvtt::Format_BC1;
	case uimg::TextureInfo::OutputFormat::BC1a:
		return nvtt::Format_BC1a;
	case uimg::TextureInfo::OutputFormat::BC2:
		return nvtt::Format_BC2;
	case uimg::TextureInfo::OutputFormat::BC3:
		return nvtt::Format_BC3;
	case uimg::TextureInfo::OutputFormat::BC3n:
		return nvtt::Format_BC3n;
	case uimg::TextureInfo::OutputFormat::BC4:
		return nvtt::Format_BC4;
	case uimg::TextureInfo::OutputFormat::BC5:
		return nvtt::Format_BC5;
	case uimg::TextureInfo::OutputFormat::DXT1n:
		return nvtt::Format_DXT1n;
	case uimg::TextureInfo::OutputFormat::CTX1:
		return nvtt::Format_CTX1;
	case uimg::TextureInfo::OutputFormat::BC6:
		return nvtt::Format_BC6;
	case uimg::TextureInfo::OutputFormat::BC7:
		return nvtt::Format_BC7;
	case uimg::TextureInfo::OutputFormat::BC3_RGBM:
		return nvtt::Format_BC3_RGBM;
	case uimg::TextureInfo::OutputFormat::ETC1:
		return nvtt::Format_ETC1;
	case uimg::TextureInfo::OutputFormat::ETC2_R:
		return nvtt::Format_ETC2_R;
	case uimg::TextureInfo::OutputFormat::ETC2_RG:
		return nvtt::Format_ETC2_RG;
	case uimg::TextureInfo::OutputFormat::ETC2_RGB:
		return nvtt::Format_ETC2_RGB;
	case uimg::TextureInfo::OutputFormat::ETC2_RGBA:
		return nvtt::Format_ETC2_RGBA;
	case uimg::TextureInfo::OutputFormat::ETC2_RGB_A1:
		return nvtt::Format_ETC2_RGB_A1;
	case uimg::TextureInfo::OutputFormat::ETC2_RGBM:
		return nvtt::Format_ETC2_RGBM;
	case uimg::TextureInfo::OutputFormat::KeepInputImageFormat:
		break;
	}
	static_assert(umath::to_integral(uimg::TextureInfo::OutputFormat::Count) == 27);
	return {};
}

static nvtt::Container to_nvtt_enum(uimg::TextureInfo::ContainerFormat format, uimg::TextureInfo::OutputFormat imgFormat)
{
	switch(format) {
	case uimg::TextureInfo::ContainerFormat::DDS:
		{
			if(imgFormat == uimg::TextureInfo::OutputFormat::BC6 || imgFormat == uimg::TextureInfo::OutputFormat::BC7)
				return nvtt::Container_DDS10; // These formats are only supported by DDS10
			return nvtt::Container_DDS;
		}
	case uimg::TextureInfo::ContainerFormat::KTX:
		return nvtt::Container_KTX;
	}
	static_assert(umath::to_integral(uimg::TextureInfo::ContainerFormat::Count) == 2);
	return {};
}

static nvtt::MipmapFilter to_nvtt_enum(uimg::TextureInfo::MipmapFilter filter)
{
	switch(filter) {
	case uimg::TextureInfo::MipmapFilter::Box:
		return nvtt::MipmapFilter_Box;
	case uimg::TextureInfo::MipmapFilter::Kaiser:
		return nvtt::MipmapFilter_Kaiser;
	}
	static_assert(umath::to_integral(uimg::TextureInfo::ContainerFormat::Count) == 2);
	return {};
}

static nvtt::WrapMode to_nvtt_enum(uimg::TextureInfo::WrapMode wrapMode)
{
	switch(wrapMode) {
	case uimg::TextureInfo::WrapMode::Clamp:
		return nvtt::WrapMode_Clamp;
	case uimg::TextureInfo::WrapMode::Repeat:
		return nvtt::WrapMode_Repeat;
	case uimg::TextureInfo::WrapMode::Mirror:
		return nvtt::WrapMode_Mirror;
	}
	static_assert(umath::to_integral(uimg::TextureInfo::ContainerFormat::Count) == 2);
	return {};
}

static nvtt::InputFormat get_nvtt_format(uimg::TextureInfo::InputFormat format)
{
	nvtt::InputFormat nvttFormat;
	switch(format) {
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
	case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
		nvttFormat = nvtt::InputFormat_BGRA_8UB;
		break;
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
		nvttFormat = nvtt::InputFormat_RGBA_16F;
		break;
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
		nvttFormat = nvtt::InputFormat_RGBA_32F;
		break;
	case uimg::TextureInfo::InputFormat::R32_Float:
		nvttFormat = nvtt::InputFormat_R_32F;
		break;
	case uimg::TextureInfo::InputFormat::KeepInputImageFormat:
		break;
	}
	static_assert(umath::to_integral(uimg::TextureInfo::InputFormat::Count) == 6);
	return nvttFormat;
}

class TextureCompressorNvtt : public TextureCompressor {
  public:
	virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) override
	{
		auto &textureSaveInfo = compressInfo.textureSaveInfo;
		auto &texInfo = compressInfo.textureSaveInfo.texInfo;
		auto nvttFormat = get_nvtt_format(texInfo.inputFormat);
		nvtt::InputOptions inputOptions {};
		inputOptions.reset();
		inputOptions.setTextureLayout(nvtt::TextureType_2D, compressInfo.width, compressInfo.height);
		inputOptions.setFormat(nvttFormat);
		inputOptions.setWrapMode(to_nvtt_enum(texInfo.wrapMode));
		inputOptions.setMipmapFilter(to_nvtt_enum(texInfo.mipMapFilter));

		if(umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::GenerateMipmaps))
			inputOptions.setMipmapGeneration(true);
		else
			inputOptions.setMipmapGeneration(compressInfo.numMipmaps > 1, compressInfo.numMipmaps - 1u);

		auto texType = textureSaveInfo.cubemap ? nvtt::TextureType_Cube : nvtt::TextureType_2D;
		auto alphaMode = texInfo.alphaMode;
		if(texInfo.outputFormat == uimg::TextureInfo::OutputFormat::BC6)
			alphaMode = uimg::TextureInfo::AlphaMode::Transparency;
		inputOptions.setTextureLayout(texType, compressInfo.width, compressInfo.height);
		for(auto iLayer = decltype(compressInfo.numLayers) {0u}; iLayer < compressInfo.numLayers; ++iLayer) {
			for(auto iMipmap = decltype(compressInfo.numMipmaps) {0u}; iMipmap < compressInfo.numMipmaps; ++iMipmap) {
				std::function<void(void)> deleter = nullptr;
				auto *data = compressInfo.getImageData(iLayer, iMipmap, deleter);
				if(data == nullptr)
					continue;
				uint32_t wMipmap, hMipmap;
				uimg::calculate_mipmap_size(compressInfo.width, compressInfo.height, wMipmap, hMipmap, iMipmap);
				inputOptions.setMipmapData(data, wMipmap, hMipmap, 1, iLayer, iMipmap);

				if(alphaMode == uimg::TextureInfo::AlphaMode::Auto) {
					// Determine whether there are any alpha values < 1
					auto numPixels = wMipmap * hMipmap;
					for(auto i = decltype(numPixels) {0u}; i < numPixels; ++i) {
						float alpha = 1.f;
						switch(nvttFormat) {
						case nvtt::InputFormat_BGRA_8UB:
							alpha = (data + i * 4)[3] / static_cast<float>(std::numeric_limits<uint8_t>::max());
							break;
						case nvtt::InputFormat_RGBA_16F:
							alpha = umath::float16_to_float32_glm((reinterpret_cast<const uint16_t *>(data) + i * 4)[3]);
							break;
						case nvtt::InputFormat_RGBA_32F:
							alpha = (reinterpret_cast<const float *>(data) + i * 4)[3];
							break;
						case nvtt::InputFormat_R_32F:
							break;
						}
						if(alpha < 0.999f) {
							alphaMode = uimg::TextureInfo::AlphaMode::Transparency;
							break;
						}
					}
				}
				if(deleter)
					deleter();
			}
		}
		inputOptions.setAlphaMode((alphaMode == uimg::TextureInfo::AlphaMode::Transparency) ? nvtt::AlphaMode::AlphaMode_Transparency : nvtt::AlphaMode::AlphaMode_None);

		static_assert(umath::to_integral(uimg::TextureInfo::ContainerFormat::Count) == 2);
		/*auto f = FileManager::OpenFile<VFilePtrReal>(fileNameWithExt.c_str(),"wb");
		if(f == nullptr)
		{
		if(errorHandler)
		errorHandler("Unable to write file!");
		return false;
		}*/

		ErrorHandler errHandler {compressInfo.errorHandler};
		//OutputHandler outputHandler {f};

		struct NvttOutputHandler : public nvtt::OutputHandler {
			NvttOutputHandler(uimg::TextureOutputHandler &handler) : m_outputHandler {handler} {}
			virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override { m_outputHandler.beginImage(size, width, height, depth, face, miplevel); }
			virtual bool writeData(const void *data, int size) override { return m_outputHandler.writeData(data, size); }
			virtual void endImage() override { m_outputHandler.endImage(); }
		  private:
			uimg::TextureOutputHandler &m_outputHandler;
		};

		std::unique_ptr<NvttOutputHandler> nvttOutputHandler = nullptr;
		nvtt::OutputOptions outputOptions {};
		outputOptions.reset();
		outputOptions.setContainer(to_nvtt_enum(texInfo.containerFormat, texInfo.outputFormat));
		outputOptions.setSrgbFlag(umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::SRGB));

		ResultData resultData {};

		auto &outputHandler = compressInfo.outputHandler;
		if(outputHandler.index() == 0) {
			auto &texOutputHandler = std::get<uimg::TextureOutputHandler>(outputHandler);
			auto &r = const_cast<uimg::TextureOutputHandler &>(texOutputHandler);
			nvttOutputHandler = std::unique_ptr<NvttOutputHandler> {new NvttOutputHandler {r}};
			outputOptions.setOutputHandler(nvttOutputHandler.get());
		}
		else {
			auto &fileName = std::get<std::string>(outputHandler);
			std::string outputFilePath = compressInfo.absoluteFileName ? fileName.c_str() : get_absolute_path(fileName, texInfo.containerFormat).c_str();
			outputOptions.setFileName(outputFilePath.c_str());
			resultData.outputFilePath = outputFilePath;
		}
		outputOptions.setErrorHandler(&errHandler);

		auto nvttOutputFormat = to_nvtt_enum(texInfo.outputFormat);
		nvtt::CompressionOptions compressionOptions {};
		compressionOptions.reset();
		compressionOptions.setFormat(nvttOutputFormat);
		compressionOptions.setQuality(nvtt::Quality_Production);

		// These settings are from the standalone nvtt nvcompress application
		switch(nvttOutputFormat) {
		case nvtt::Format_BC2:
			// Dither alpha when using BC2.
			compressionOptions.setQuantization(/*color dithering*/ false, /*alpha dithering*/ true, /*binary alpha*/ false);
			break;
		case nvtt::Format_BC1a:
			// Binary alpha when using BC1a.
			compressionOptions.setQuantization(/*color dithering*/ false, /*alpha dithering*/ true, /*binary alpha*/ true, 127);
			break;
		case nvtt::Format_BC6:
			compressionOptions.setPixelType(nvtt::PixelType_UnsignedFloat);
			break;
		case nvtt::Format_DXT1n:
			compressionOptions.setColorWeights(1, 1, 0);
			break;
		}

		if(texInfo.IsNormalMap()) {
			inputOptions.setNormalMap(true);
			inputOptions.setConvertToNormalMap(false);
			inputOptions.setGamma(1.0f, 1.0f);
			inputOptions.setNormalizeMipmaps(true);
		}
		else if(umath::is_flag_set(texInfo.flags, uimg::TextureInfo::Flags::ConvertToNormalMap)) {
			inputOptions.setNormalMap(false);
			inputOptions.setConvertToNormalMap(true);
			inputOptions.setHeightEvaluation(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f);
			//inputOptions.setNormalFilter(1.0f, 0, 0, 0);
			//inputOptions.setNormalFilter(0.0f, 0, 0, 1);
			inputOptions.setGamma(1.0f, 1.0f);
			inputOptions.setNormalizeMipmaps(true);
		}
		else {
			inputOptions.setNormalMap(false);
			inputOptions.setConvertToNormalMap(false);
			inputOptions.setGamma(2.2f, 2.2f);
			inputOptions.setNormalizeMipmaps(false);
		}

		nvtt::Compressor compressor {};
		compressor.enableCudaAcceleration(true);
		auto res = compressor.process(inputOptions, compressionOptions, outputOptions);
		return res ? resultData : std::optional<ResultData> {};
	}
};

#else

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
				++ms.m_nMipLevels;

				if(deleter)
					deleter();
			}
		}

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

#endif

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
		else if(texInfo.inputFormat == uimg::TextureInfo::InputFormat::R8G8B8A8_UInt)
			channelMask.SwapChannels(uimg::Channel::Red, uimg::Channel::Blue);

		switch(texInfo.inputFormat) {
		case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
		case uimg::TextureInfo::InputFormat::B8G8R8A8_UInt:
			channelMask.SwapChannels(uimg::Channel::Red, uimg::Channel::Blue);
			break;
		}

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

		TextureCompressor::CompressInfo compressInfo {};
		compressInfo.getImageData = fGetImgData;
		compressInfo.errorHandler = errorHandler;
		compressInfo.textureSaveInfo = texSaveInfo;
		compressInfo.outputHandler = outputHandler;
		compressInfo.width = width;
		compressInfo.height = height;
		compressInfo.numMipmaps = numMipmaps;
		compressInfo.numLayers = numLayers;
		compressInfo.absoluteFileName = absoluteFileName;

#if TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_NVTT
		TextureCompressorNvtt compressor {};
#else
		TextureCompressorCompressonator compressor {};
#endif
		auto resultData = compressor.Compress(compressInfo);
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

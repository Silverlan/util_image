/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image_texture_compressor.hpp"

#if UIMG_ENABLE_NVTT && TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_NVTT

#include <nvtt/nvtt.h>
#include <fsys/filesystem.h>
#include <sharedutils/util_string.h>

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
		default:
			m_errorHandler("Unknown error");
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
	default:
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
	default:
		break;
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
	default:
		break;
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

namespace uimg {
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

			#ifdef _WIN32
			FILE *fileHandle;
			#endif

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
				#ifdef _WIN32
				std::wstring wideOutputFilePath = ustring::string_to_wstring(outputFilePath);
				_wfopen_s(&fileHandle, wideOutputFilePath.c_str(), L"wb");
				outputOptions.setFileHandle(fileHandle);
				#else
				outputOptions.setFileName(outputFilePath.c_str());
				#endif
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
			default:
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
			#ifdef _WIN32
			//NVTT does not close the handle. We need to close this ourselves.
			fclose(fileHandle);
			#endif
			return res ? resultData : std::optional<ResultData> {};
		}
	};
};

std::unique_ptr<uimg::TextureCompressor> uimg::TextureCompressor::Create() { return std::make_unique<TextureCompressorNvtt>(); }

#endif

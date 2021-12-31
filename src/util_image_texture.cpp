/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef UIMG_ENABLE_NVTT
#include "util_image.hpp"
#include "util_image_buffer.hpp"
#include "util_texture_info.hpp"
#include <nvtt/nvtt.h>
#include <fsys/filesystem.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_path.hpp>
#include <sharedutils/magic_enum.hpp>
#include <variant>
#include <cstring>

#pragma optimize("",off)
static nvtt::Format to_nvtt_enum(uimg::TextureInfo::OutputFormat format)
{
	switch(format)
	{
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

static nvtt::Container to_nvtt_enum(uimg::TextureInfo::ContainerFormat format,uimg::TextureInfo::OutputFormat imgFormat)
{
	switch(format)
	{
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
	switch(filter)
	{
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
	switch(wrapMode)
	{
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

struct OutputHandler
	: public nvtt::OutputHandler
{
	OutputHandler(VFilePtrReal f)
		: m_file{f}
	{}
	virtual ~OutputHandler() override {}

	// Indicate the start of a new compressed image that's part of the final texture.
	virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override {}

	// Output data. Compressed data is output as soon as it's generated to minimize memory allocations.
	virtual bool writeData(const void * data, int size) override
	{
		return m_file->Write(data,size) == size;
	}

	// Indicate the end of the compressed image. (New in NVTT 2.1)
	virtual void endImage() override {}

private:
	VFilePtrReal m_file = nullptr;
};

struct ErrorHandler
	: public nvtt::ErrorHandler
{
	ErrorHandler(const std::function<void(const std::string&)> &errorHandler)
		: m_errorHandler{errorHandler}
	{}
	virtual ~ErrorHandler() override {};

	// Signal error.
	virtual void error(nvtt::Error e) override
	{
		if(m_errorHandler == nullptr)
			return;
		switch(e)
		{
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
	std::function<void(const std::string&)> m_errorHandler = nullptr;
};

std::string uimg::get_absolute_path(const std::string &fileName,uimg::TextureInfo::ContainerFormat containerFormat)
{
	auto path = ufile::get_path_from_filename(fileName);
	FileManager::CreatePath(path.c_str());
	auto fileNameWithExt = fileName;
	ufile::remove_extension_from_filename(fileNameWithExt,std::array<std::string,2>{"dds","ktx"});
	switch(containerFormat)
	{
	case uimg::TextureInfo::ContainerFormat::DDS:
		fileNameWithExt += ".dds";
		break;
	case uimg::TextureInfo::ContainerFormat::KTX:
		fileNameWithExt += ".ktx";
		break;
	}
	return FileManager::GetProgramPath() +'/' +fileNameWithExt;
}

static nvtt::InputFormat get_nvtt_format(uimg::TextureInfo::InputFormat format)
{
	nvtt::InputFormat nvttFormat;
	switch(format)
	{
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

static uint32_t calculate_mipmap_size(uint32_t v,uint32_t level)
{
	auto r = v;
	auto scale = static_cast<int>(std::pow(2,level));
	r /= scale;
	if(r == 0)
		r = 1;
	return r;
}

static bool compress_texture(
	const std::variant<uimg::TextureOutputHandler,std::string> &outputHandler,const std::function<const uint8_t*(uint32_t,uint32_t,std::function<void()>&)> &pfGetImgData,
	const uimg::TextureSaveInfo &texSaveInfo,const std::function<void(const std::string&)> &errorHandler=nullptr,
	bool absoluteFileName=false
)
{
	std::function<void()> updateFileCache = nullptr;
	auto r = false;
	{
		auto &texInfo = texSaveInfo.texInfo;
		uimg::ChannelMask channelMask {};
		if(texSaveInfo.channelMask.has_value())
			channelMask = *texSaveInfo.channelMask;
		else if(texInfo.inputFormat == uimg::TextureInfo::InputFormat::R8G8B8A8_UInt)
			channelMask.SwapChannels(uimg::Channel::Red,uimg::Channel::Blue);

		auto nvttFormat = get_nvtt_format(texInfo.inputFormat);
		switch(nvttFormat)
		{
		case nvtt::InputFormat_BGRA_8UB:
			channelMask.SwapChannels(uimg::Channel::Red,uimg::Channel::Blue);
			break;
		}

		auto numMipmaps = umath::max(texSaveInfo.numMipmaps,static_cast<uint32_t>(1));
		auto numLayers = texSaveInfo.numLayers;
		auto width = texSaveInfo.width;
		auto height = texSaveInfo.height;
		auto szPerPixel = texSaveInfo.szPerPixel;
		auto cubemap = texSaveInfo.cubemap;
		std::vector<std::shared_ptr<uimg::ImageBuffer>> imgBuffers;
		auto fGetImgData = pfGetImgData;
		if(channelMask != uimg::ChannelMask{})
		{
			imgBuffers.resize(numLayers *numMipmaps);
			uint32_t idx = 0;

			uimg::Format uimgFormat;
			switch(texSaveInfo.texInfo.inputFormat)
			{
			case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
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
				throw std::runtime_error{"Texture compression error: Unsupported format " +std::string{magic_enum::enum_name(texSaveInfo.texInfo.inputFormat)}};
			}

			for(auto i=decltype(numLayers){0u};i<numLayers;++i)
			{
				for(auto m=decltype(numMipmaps){0u};m<numMipmaps;++m)
				{
					std::function<void()> deleter = nullptr;
					auto *data = fGetImgData(i,m,deleter);
					auto &imgBuf = imgBuffers[idx++] = uimg::ImageBuffer::Create(data,calculate_mipmap_size(width,m),calculate_mipmap_size(height,m),uimgFormat);
				
					imgBuf->SwapChannels(channelMask);
					if(deleter)
						deleter();
				}
			}
			fGetImgData = [&imgBuffers,numMipmaps](uint32_t layer,uint32_t mip,std::function<void()> &deleter) -> const uint8_t* {
				deleter = nullptr;
				auto idx = layer *numMipmaps +mip;
				auto &imgBuf = imgBuffers[idx];
				return imgBuf ? static_cast<uint8_t*>(imgBuf->GetData()) : nullptr;
			};
		}

		auto size = width *height *szPerPixel;

		nvtt::InputOptions inputOptions {};
		inputOptions.reset();
		inputOptions.setTextureLayout(nvtt::TextureType_2D,width,height);
		inputOptions.setFormat(nvttFormat);
		inputOptions.setWrapMode(to_nvtt_enum(texInfo.wrapMode));
		inputOptions.setMipmapFilter(to_nvtt_enum(texInfo.mipMapFilter));

		if(umath::is_flag_set(texInfo.flags,uimg::TextureInfo::Flags::GenerateMipmaps))
			inputOptions.setMipmapGeneration(true);
		else
			inputOptions.setMipmapGeneration(numMipmaps > 1,numMipmaps -1u);

		auto texType = cubemap ? nvtt::TextureType_Cube : nvtt::TextureType_2D;
		auto alphaMode = texInfo.alphaMode;
		if(texInfo.outputFormat == uimg::TextureInfo::OutputFormat::BC6)
			alphaMode = uimg::TextureInfo::AlphaMode::Transparency;
		inputOptions.setTextureLayout(texType,width,height);
		for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
		{
			for(auto iMipmap=decltype(numMipmaps){0u};iMipmap<numMipmaps;++iMipmap)
			{
				std::function<void(void)> deleter = nullptr;
				auto *data = fGetImgData(iLayer,iMipmap,deleter);
				if(data == nullptr)
					continue;
				uint32_t wMipmap,hMipmap;
				uimg::calculate_mipmap_size(width,height,wMipmap,hMipmap,iMipmap);
				inputOptions.setMipmapData(data,wMipmap,hMipmap,1,iLayer,iMipmap);

				if(alphaMode == uimg::TextureInfo::AlphaMode::Auto)
				{
					// Determine whether there are any alpha values < 1
					auto numPixels = wMipmap *hMipmap;
					for(auto i=decltype(numPixels){0u};i<numPixels;++i)
					{
						float alpha = 1.f;
						switch(nvttFormat)
						{
						case nvtt::InputFormat_BGRA_8UB:
							alpha = (data +i *4)[3] /static_cast<float>(std::numeric_limits<uint8_t>::max());
							break;
						case nvtt::InputFormat_RGBA_16F:
							alpha = umath::float16_to_float32_glm((reinterpret_cast<const uint16_t*>(data) +i *4)[3]);
							break;
						case nvtt::InputFormat_RGBA_32F:
							alpha = (reinterpret_cast<const float*>(data) +i *4)[3];
							break;
						case nvtt::InputFormat_R_32F:
							break;
						}
						if(alpha < 0.999f)
						{
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

		ErrorHandler errHandler {errorHandler};
		//OutputHandler outputHandler {f};

		struct NvttOutputHandler
			: public nvtt::OutputHandler
		{
			NvttOutputHandler(uimg::TextureOutputHandler &handler)
				: m_outputHandler{handler}
			{}
			virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override
			{
				m_outputHandler.beginImage(size,width,height,depth,face,miplevel);
			}
			virtual bool writeData(const void * data, int size) override
			{
				return m_outputHandler.writeData(data,size);
			}
			virtual void endImage() override
			{
				m_outputHandler.endImage();
			}
		private:
			uimg::TextureOutputHandler &m_outputHandler;
		};

		std::unique_ptr<NvttOutputHandler> nvttOutputHandler = nullptr;
		nvtt::OutputOptions outputOptions {};
		outputOptions.reset();
		outputOptions.setContainer(to_nvtt_enum(texInfo.containerFormat,texInfo.outputFormat));
		outputOptions.setSrgbFlag(umath::is_flag_set(texInfo.flags,uimg::TextureInfo::Flags::SRGB));
		if(outputHandler.index() == 0)
		{
			auto &texOutputHandler = std::get<uimg::TextureOutputHandler>(outputHandler);
			auto &r = const_cast<uimg::TextureOutputHandler&>(texOutputHandler);
			nvttOutputHandler = std::unique_ptr<NvttOutputHandler>{new NvttOutputHandler{r}};
			outputOptions.setOutputHandler(nvttOutputHandler.get());
		}
		else
		{
			auto &fileName = std::get<std::string>(outputHandler);
			std::string outputFilePath = absoluteFileName ? fileName.c_str() : get_absolute_path(fileName,texInfo.containerFormat).c_str();
			outputOptions.setFileName(outputFilePath.c_str());
			updateFileCache = [outputFilePath]() {
				filemanager::update_file_index_cache(outputFilePath,true);
			};
		}
		outputOptions.setErrorHandler(&errHandler);

		auto nvttOutputFormat = to_nvtt_enum(texInfo.outputFormat);
		nvtt::CompressionOptions compressionOptions {};
		compressionOptions.reset();
		compressionOptions.setFormat(nvttOutputFormat);
		compressionOptions.setQuality(nvtt::Quality_Production);

		// These settings are from the standalone nvtt nvcompress application
		switch(nvttOutputFormat)
		{
		case nvtt::Format_BC2:
			// Dither alpha when using BC2.
			compressionOptions.setQuantization(/*color dithering*/false, /*alpha dithering*/true, /*binary alpha*/false);
			break;
		case nvtt::Format_BC1a:
			// Binary alpha when using BC1a.
			compressionOptions.setQuantization(/*color dithering*/false, /*alpha dithering*/true, /*binary alpha*/true, 127);
			break;
		case nvtt::Format_BC6:
			compressionOptions.setPixelType(nvtt::PixelType_UnsignedFloat);
			break;
		case nvtt::Format_DXT1n:
			compressionOptions.setColorWeights(1, 1, 0);
			break;
		}

		if(texInfo.IsNormalMap())
		{
			inputOptions.setNormalMap(true);
			inputOptions.setConvertToNormalMap(false);
			inputOptions.setGamma(1.0f, 1.0f);
			inputOptions.setNormalizeMipmaps(true);
		}
		else if(umath::is_flag_set(texInfo.flags,uimg::TextureInfo::Flags::ConvertToNormalMap))
		{
			inputOptions.setNormalMap(false);
			inputOptions.setConvertToNormalMap(true);
			inputOptions.setHeightEvaluation(1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f, 0.0f);
			//inputOptions.setNormalFilter(1.0f, 0, 0, 0);
			//inputOptions.setNormalFilter(0.0f, 0, 0, 1);
			inputOptions.setGamma(1.0f, 1.0f);
			inputOptions.setNormalizeMipmaps(true);
		}
		else
		{
			inputOptions.setNormalMap(false);
			inputOptions.setConvertToNormalMap(false);
			inputOptions.setGamma(2.2f, 2.2f);
			inputOptions.setNormalizeMipmaps(false);
		}

		nvtt::Compressor compressor {};
		compressor.enableCudaAcceleration(true);
		r = compressor.process(inputOptions,compressionOptions,outputOptions);
	}
	if(r && updateFileCache)
		updateFileCache();
	return r;
}

bool uimg::compress_texture(
	const TextureOutputHandler &outputHandler,const std::function<const uint8_t*(uint32_t,uint32_t,std::function<void()>&)> &fGetImgData,
	const uimg::TextureSaveInfo &texSaveInfo,const std::function<void(const std::string&)> &errorHandler
)
{
	return ::compress_texture(outputHandler,fGetImgData,texSaveInfo,errorHandler);
}

bool uimg::compress_texture(
	std::vector<std::vector<std::vector<uint8_t>>> &outputData,const std::function<const uint8_t*(uint32_t,uint32_t,std::function<void()>&)> &fGetImgData,
	const uimg::TextureSaveInfo &texSaveInfo,const std::function<void(const std::string&)> &errorHandler
)
{
	outputData.resize(texSaveInfo.numLayers,std::vector<std::vector<uint8_t>>(texSaveInfo.numMipmaps,std::vector<uint8_t>{}));
	auto iLevel = std::numeric_limits<uint32_t>::max();
	auto iMipmap = std::numeric_limits<uint32_t>::max();
	uimg::TextureOutputHandler outputHandler {};
	outputHandler.beginImage = [&outputData,&iLevel,&iMipmap](int size, int width, int height, int depth, int face, int miplevel) {
		iLevel = face;
		iMipmap = miplevel;
		outputData[iLevel][iMipmap].resize(size);
	};
	outputHandler.writeData = [&outputData,&iLevel,&iMipmap](const void * data, int size) -> bool {
		if(iLevel == std::numeric_limits<uint32_t>::max() || iMipmap == std::numeric_limits<uint32_t>::max())
			return true;
		std::memcpy(outputData[iLevel][iMipmap].data(),data,size);
		return true;
	};
	outputHandler.endImage = [&iLevel,&iMipmap]() {
		iLevel = std::numeric_limits<uint32_t>::max();
		iMipmap = std::numeric_limits<uint32_t>::max();
	};
	return ::compress_texture(outputHandler,fGetImgData,texSaveInfo,errorHandler);
}

bool uimg::save_texture(
	const std::string &fileName,const std::function<const uint8_t*(uint32_t,uint32_t,std::function<void()>&)> &fGetImgData,
	const uimg::TextureSaveInfo &texSaveInfo,const std::function<void(const std::string&)> &errorHandler,bool absoluteFileName
)
{
	return ::compress_texture(fileName,fGetImgData,texSaveInfo,errorHandler,absoluteFileName);
}

bool uimg::save_texture(
	const std::string &fileName,uimg::ImageBuffer &imgBuffer,const uimg::TextureSaveInfo &texSaveInfo,const std::function<void(const std::string&)> &errorHandler,bool absoluteFileName
)
{
	constexpr auto numLayers = 1u;
	constexpr auto numMipmaps = 1u;

	auto newTexSaveInfo = texSaveInfo;
	auto swapRedBlue = (texSaveInfo.texInfo.inputFormat == uimg::TextureInfo::InputFormat::R8G8B8A8_UInt);
	if(swapRedBlue)
	{
		imgBuffer.SwapChannels(uimg::Channel::Red,uimg::Channel::Blue);
		newTexSaveInfo.texInfo.inputFormat = uimg::TextureInfo::InputFormat::B8G8R8A8_UInt;
	}
	newTexSaveInfo.width = imgBuffer.GetWidth();
	newTexSaveInfo.height = imgBuffer.GetHeight();
	newTexSaveInfo.szPerPixel = imgBuffer.GetPixelSize();
	newTexSaveInfo.numLayers = numLayers;
	newTexSaveInfo.numMipmaps = numMipmaps;
	auto success = save_texture(fileName,[&imgBuffer](uint32_t iLayer,uint32_t iMipmap,std::function<void(void)> &outDeleter) -> const uint8_t* {
		return static_cast<uint8_t*>(imgBuffer.GetData());
	},newTexSaveInfo,errorHandler,absoluteFileName);
	if(swapRedBlue)
		imgBuffer.SwapChannels(uimg::Channel::Red,uimg::Channel::Blue);
	return success;
}

bool uimg::save_texture(
	const std::string &fileName,const std::vector<std::vector<const void*>> &imgLayerMipmapData,
	const uimg::TextureSaveInfo &texSaveInfo,const std::function<void(const std::string&)> &errorHandler,bool absoluteFileName
)
{
	auto numLayers = imgLayerMipmapData.size();
	auto numMipmaps = imgLayerMipmapData.empty() ? 1 : imgLayerMipmapData.front().size();
	auto ltexSaveInfo = texSaveInfo;
	ltexSaveInfo.numLayers = numLayers;
	ltexSaveInfo.numMipmaps = numMipmaps;
	return save_texture(fileName,[&imgLayerMipmapData](uint32_t iLayer,uint32_t iMipmap,std::function<void(void)> &outDeleter) -> const uint8_t* {
		if(iLayer >= imgLayerMipmapData.size())
			return nullptr;
		auto &mipmapData = imgLayerMipmapData.at(iLayer);
		if(iMipmap >= mipmapData.size())
			return nullptr;
		return static_cast<const uint8_t*>(mipmapData.at(iMipmap));
	},ltexSaveInfo,errorHandler,absoluteFileName);
}
#endif
#pragma optimize("",on)

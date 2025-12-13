// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "stb_image_write.h"
#include "stb_image.h"
#ifdef UIMG_ENABLE_SVG
#include <lunasvg.h>
#endif

module pragma.image;

import :core;
import pragma.filesystem;

void pragma::image::ChannelMask::Reverse() { *this = GetReverse(); }
pragma::image::ChannelMask pragma::image::ChannelMask::GetReverse() const
{
	ChannelMask rmask;
	rmask.red = alpha;
	rmask.green = blue;
	rmask.blue = green;
	rmask.alpha = red;
	return rmask;
}
void pragma::image::ChannelMask::SwapChannels(Channel a, Channel b) { pragma::math::swap((*this)[pragma::math::to_integral(a)], (*this)[pragma::math::to_integral(b)]); }
std::optional<uint8_t> pragma::image::ChannelMask::GetChannelIndex(Channel channel) const
{
	if(red == channel)
		return 0;
	if(green == channel)
		return 1;
	if(blue == channel)
		return 2;
	if(alpha == channel)
		return 3;
	return {};
}

///////////

void pragma::image::calculate_mipmap_size(uint32_t w, uint32_t h, uint32_t &outWMipmap, uint32_t &outHMipmap, uint32_t level)
{
	outWMipmap = w;
	outHMipmap = h;
	int scale = static_cast<int>(std::pow(2, level));
	outWMipmap /= scale;
	outHMipmap /= scale;
	if(outWMipmap == 0)
		outWMipmap = 1;
	if(outHMipmap == 0)
		outHMipmap = 1;
}

uint32_t pragma::image::calculate_mipmap_count(uint32_t w, uint32_t h) { return 1 + static_cast<uint32_t>(floor(log2(fmaxf(static_cast<float>(w), static_cast<float>(h))))); }

std::optional<pragma::image::ImageFormat> pragma::image::string_to_image_output_format(const std::string &str)
{
	if(pragma::string::compare<std::string>(str, "PNG", false))
		return ImageFormat::PNG;
	else if(pragma::string::compare<std::string>(str, "BMP", false))
		return ImageFormat::BMP;
	else if(pragma::string::compare<std::string>(str, "TGA", false))
		return ImageFormat::TGA;
	else if(pragma::string::compare<std::string>(str, "JPG", false))
		return ImageFormat::JPG;
	else if(pragma::string::compare<std::string>(str, "HDR", false))
		return ImageFormat::HDR;
	return {};
}

std::string pragma::image::get_image_output_format_extension(ImageFormat format)
{
	switch(format) {
	case ImageFormat::PNG:
		return "png";
	case ImageFormat::BMP:
		return "bmp";
	case ImageFormat::TGA:
		return "tga";
	case ImageFormat::JPG:
		return "jpg";
	case ImageFormat::HDR:
		return "hdr";
	default:
		break;
	}
	return "";
}

std::optional<pragma::image::ToneMapping> pragma::image::string_to_tone_mapping(const std::string &str)
{
	if(pragma::string::compare<std::string>(str, "gamma_correction", false))
		return ToneMapping::GammaCorrection;
	else if(pragma::string::compare<std::string>(str, "reinhard", false))
		return ToneMapping::Reinhard;
	else if(pragma::string::compare<std::string>(str, "hejil_richard", false))
		return ToneMapping::HejilRichard;
	else if(pragma::string::compare<std::string>(str, "uncharted", false))
		return ToneMapping::Uncharted;
	else if(pragma::string::compare<std::string>(str, "aces", false))
		return ToneMapping::Aces;
	else if(pragma::string::compare<std::string>(str, "gran_turismo", false))
		return ToneMapping::GranTurismo;
	return {};
}

std::string pragma::image::get_file_extension(ImageFormat format)
{
	switch(format) {
	case ImageFormat::PNG:
		return "png";
	case ImageFormat::BMP:
		return "bmp";
	case ImageFormat::TGA:
		return "tga";
	case ImageFormat::JPG:
		return "jpg";
	case ImageFormat::HDR:
		return "hdr";
	default:
		break;
	}
	static_assert(pragma::math::to_integral(ImageFormat::Count) == 5);
	return "";
}

std::shared_ptr<pragma::image::ImageBuffer> pragma::image::load_image(const std::string &fileName, PixelFormat pixelFormat, bool flipVertically)
{
	auto fp = fs::open_file(fileName.c_str(), fs::FileMode::Read | fs::FileMode::Binary);
	if(fp == nullptr)
		return nullptr;
	fs::File f {fp};
	return load_image(f, pixelFormat, flipVertically);
}

std::shared_ptr<pragma::image::ImageBuffer> pragma::image::load_image(ufile::IFile &f, PixelFormat pixelFormat, bool flipVertically)
{
	int width, height, nrComponents;
	stbi_io_callbacks ioCallbacks {};
	ioCallbacks.read = [](void *user, char *data, int size) -> int { return static_cast<ufile::IFile *>(user)->Read(data, size); };
	ioCallbacks.skip = [](void *user, int n) -> void {
		auto *f = static_cast<ufile::IFile *>(user);
		f->Seek(f->Tell() + n);
	};
	ioCallbacks.eof = [](void *user) -> int { return static_cast<ufile::IFile *>(user)->Eof(); };
	stbi_set_flip_vertically_on_load(flipVertically);
	std::shared_ptr<ImageBuffer> imgBuffer = nullptr;
	switch(pixelFormat) {
	case PixelFormat::LDR:
		{
			auto *data = stbi_load_from_callbacks(&ioCallbacks, &f, &width, &height, &nrComponents, 4);
			imgBuffer = data ? ImageBuffer::CreateWithCustomDeleter(data, width, height, Format::RGBA8, [](void *data) { stbi_image_free(data); }) : nullptr;
			break;
		}
	case PixelFormat::HDR:
		{
			auto *data = stbi_load_16_from_callbacks(&ioCallbacks, &f, &width, &height, &nrComponents, 4);
			imgBuffer = data ? ImageBuffer::CreateWithCustomDeleter(data, width, height, Format::RGBA16, [](void *data) { stbi_image_free(data); }) : nullptr;
			break;
		}
	case PixelFormat::Float:
		{
			auto *data = stbi_loadf_from_callbacks(&ioCallbacks, &f, &width, &height, &nrComponents, 4);
			imgBuffer = data ? ImageBuffer::CreateWithCustomDeleter(data, width, height, Format::RGBA32, [](void *data) { stbi_image_free(data); }) : nullptr;
			break;
		}
	}
	stbi_set_flip_vertically_on_load(false); // Reset
	return imgBuffer;
}

bool pragma::image::save_image(ufile::IFile &f, ImageBuffer &imgBuffer, ImageFormat format, float quality, bool flipVertically)
{
	auto *fptr = &f;

	auto imgFormat = imgBuffer.GetFormat();
	// imgFormat = ::pragma::image::ImageBuffer::ToRGBFormat(imgFormat);
	if(format != ImageFormat::HDR)
		imgFormat = ImageBuffer::ToLDRFormat(imgFormat);
	else
		imgFormat = ImageBuffer::ToFloatFormat(imgFormat);
	imgBuffer.Convert(imgFormat);
	auto w = imgBuffer.GetWidth();
	auto h = imgBuffer.GetHeight();
	auto *data = imgBuffer.GetData();
	auto numChannels = imgBuffer.GetChannelCount();
	int result = 0;
	stbi_flip_vertically_on_write(flipVertically);
	switch(format) {
	case ImageFormat::PNG:
		if(quality >= 0.9f)
			stbi_write_png_compression_level = 0;
		else if(quality >= 0.75f)
			stbi_write_png_compression_level = 6;
		else if(quality >= 0.5f)
			stbi_write_png_compression_level = 8;
		else if(quality >= 0.25f)
			stbi_write_png_compression_level = 10;
		else
			stbi_write_png_compression_level = 12;
		result = stbi_write_png_to_func([](void *context, void *data, int size) { static_cast<ufile::IFile *>(context)->Write(data, size); }, fptr, w, h, numChannels, data, imgBuffer.GetPixelSize() * w);
		break;
	case ImageFormat::BMP:
		result = stbi_write_bmp_to_func([](void *context, void *data, int size) { static_cast<ufile::IFile *>(context)->Write(data, size); }, fptr, w, h, numChannels, data);
		break;
	case ImageFormat::TGA:
		result = stbi_write_tga_to_func([](void *context, void *data, int size) { static_cast<ufile::IFile *>(context)->Write(data, size); }, fptr, w, h, numChannels, data);
		break;
	case ImageFormat::JPG:
		result = stbi_write_jpg_to_func([](void *context, void *data, int size) { static_cast<ufile::IFile *>(context)->Write(data, size); }, fptr, w, h, numChannels, data, static_cast<int32_t>(quality * 100.f));
		break;
	case ImageFormat::HDR:
		result = stbi_write_hdr_to_func([](void *context, void *data, int size) { static_cast<ufile::IFile *>(context)->Write(data, size); }, fptr, w, h, numChannels, reinterpret_cast<float *>(data));
		break;
	default:
		break;
	}
	stbi_flip_vertically_on_write(false); // Reset
	return result != 0;
}

#ifdef UIMG_ENABLE_SVG
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::load_svg(ufile::IFile &f, const SvgImageInfo &svgInfo)
{
	std::vector<uint8_t> data;
	data.resize(f.GetSize());
	if(f.Read(data.data(), data.size()) != data.size())
		return nullptr;
	auto document = lunasvg::Document::loadFromData(reinterpret_cast<char *>(data.data()), data.size());
	if(document == nullptr)
		return nullptr;
	if(!svgInfo.styleSheet.empty())
		document->applyStyleSheet(svgInfo.styleSheet);
	auto bitmap = document->renderToBitmap(svgInfo.width ? static_cast<int32_t>(*svgInfo.width) : -1, svgInfo.height ? static_cast<int32_t>(*svgInfo.height) : -1);
	if(bitmap.isNull())
		return nullptr;
	bitmap.convertToRGBA();
	return ImageBuffer::Create(bitmap.data(), bitmap.width(), bitmap.height(), Format::RGBA8, false);
}
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::load_svg(const std::string &fileName, const SvgImageInfo &svgInfo)
{
	auto fp = fs::open_file(fileName.c_str(), fs::FileMode::Read | fs::FileMode::Binary);
	if(fp == nullptr)
		return nullptr;
	fs::File f {fp};
	return load_svg(f, svgInfo);
}
#endif

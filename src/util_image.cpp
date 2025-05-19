/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image.hpp"
#include "util_image_buffer.hpp"
#include "stb_image_write.h"
#include "stb_image.h"
#include <sharedutils/util_string.h>
#include <fsys/filesystem.h>
#include <fsys/ifile.hpp>
#include <cmath>
#ifdef UIMG_ENABLE_SVG
#include <lunasvg.h>
#endif

void uimg::ChannelMask::Reverse() { *this = GetReverse(); }
uimg::ChannelMask uimg::ChannelMask::GetReverse() const
{
	uimg::ChannelMask rmask;
	rmask.red = alpha;
	rmask.green = blue;
	rmask.blue = green;
	rmask.alpha = red;
	return rmask;
}
void uimg::ChannelMask::SwapChannels(Channel a, Channel b) { umath::swap((*this)[umath::to_integral(a)], (*this)[umath::to_integral(b)]); }
std::optional<uint8_t> uimg::ChannelMask::GetChannelIndex(Channel channel) const
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

void uimg::calculate_mipmap_size(uint32_t w, uint32_t h, uint32_t &outWMipmap, uint32_t &outHMipmap, uint32_t level)
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

std::optional<uimg::ImageFormat> uimg::string_to_image_output_format(const std::string &str)
{
	if(ustring::compare<std::string>(str, "PNG", false))
		return ImageFormat::PNG;
	else if(ustring::compare<std::string>(str, "BMP", false))
		return ImageFormat::BMP;
	else if(ustring::compare<std::string>(str, "TGA", false))
		return ImageFormat::TGA;
	else if(ustring::compare<std::string>(str, "JPG", false))
		return ImageFormat::JPG;
	else if(ustring::compare<std::string>(str, "HDR", false))
		return ImageFormat::HDR;
	return {};
}

std::string uimg::get_image_output_format_extension(ImageFormat format)
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

std::optional<uimg::ToneMapping> uimg::string_to_tone_mapping(const std::string &str)
{
	if(ustring::compare<std::string>(str, "gamma_correction", false))
		return uimg::ToneMapping::GammaCorrection;
	else if(ustring::compare<std::string>(str, "reinhard", false))
		return uimg::ToneMapping::Reinhard;
	else if(ustring::compare<std::string>(str, "hejil_richard", false))
		return uimg::ToneMapping::HejilRichard;
	else if(ustring::compare<std::string>(str, "uncharted", false))
		return uimg::ToneMapping::Uncharted;
	else if(ustring::compare<std::string>(str, "aces", false))
		return uimg::ToneMapping::Aces;
	else if(ustring::compare<std::string>(str, "gran_turismo", false))
		return uimg::ToneMapping::GranTurismo;
	return {};
}

std::string uimg::get_file_extension(ImageFormat format)
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
	static_assert(umath::to_integral(ImageFormat::Count) == 5);
	return "";
}

std::shared_ptr<::uimg::ImageBuffer> uimg::load_image(const std::string &fileName, PixelFormat pixelFormat, bool flipVertically)
{
	auto fp = FileManager::OpenFile(fileName.c_str(), "rb");
	if(fp == nullptr)
		return nullptr;
	fsys::File f {fp};
	return load_image(f, pixelFormat, flipVertically);
}

std::shared_ptr<::uimg::ImageBuffer> uimg::load_image(ufile::IFile &f, PixelFormat pixelFormat, bool flipVertically)
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
	std::shared_ptr<::uimg::ImageBuffer> imgBuffer = nullptr;
	switch(pixelFormat) {
	case PixelFormat::LDR:
		{
			auto *data = stbi_load_from_callbacks(&ioCallbacks, &f, &width, &height, &nrComponents, 4);
			imgBuffer = data ? uimg::ImageBuffer::CreateWithCustomDeleter(data, width, height, uimg::Format::RGBA8, [](void *data) { stbi_image_free(data); }) : nullptr;
			break;
		}
	case PixelFormat::HDR:
		{
			auto *data = stbi_load_16_from_callbacks(&ioCallbacks, &f, &width, &height, &nrComponents, 4);
			imgBuffer = data ? uimg::ImageBuffer::CreateWithCustomDeleter(data, width, height, uimg::Format::RGBA16, [](void *data) { stbi_image_free(data); }) : nullptr;
			break;
		}
	case PixelFormat::Float:
		{
			auto *data = stbi_loadf_from_callbacks(&ioCallbacks, &f, &width, &height, &nrComponents, 4);
			imgBuffer = data ? uimg::ImageBuffer::CreateWithCustomDeleter(data, width, height, uimg::Format::RGBA32, [](void *data) { stbi_image_free(data); }) : nullptr;
			break;
		}
	}
	stbi_set_flip_vertically_on_load(false); // Reset
	return imgBuffer;
}

bool uimg::save_image(ufile::IFile &f, ::uimg::ImageBuffer &imgBuffer, ImageFormat format, float quality, bool flipVertically)
{
	auto *fptr = &f;

	auto imgFormat = imgBuffer.GetFormat();
	// imgFormat = ::uimg::ImageBuffer::ToRGBFormat(imgFormat);
	if(format != ImageFormat::HDR)
		imgFormat = ::uimg::ImageBuffer::ToLDRFormat(imgFormat);
	else
		imgFormat = ::uimg::ImageBuffer::ToFloatFormat(imgFormat);
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
std::shared_ptr<uimg::ImageBuffer> uimg::load_svg(ufile::IFile &f, const SvgImageInfo &svgInfo)
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
	return uimg::ImageBuffer::Create(bitmap.data(), bitmap.width(), bitmap.height(), uimg::Format::RGBA8, false);
}
std::shared_ptr<uimg::ImageBuffer> uimg::load_svg(const std::string &fileName, const SvgImageInfo &svgInfo)
{
	auto fp = filemanager::open_file(fileName.c_str(), filemanager::FileMode::Read | filemanager::FileMode::Binary);
	if(fp == nullptr)
		return nullptr;
	fsys::File f {fp};
	return load_svg(f, svgInfo);
}
#endif

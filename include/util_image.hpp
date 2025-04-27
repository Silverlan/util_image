/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_IMAGE_HPP__
#define __UTIL_IMAGE_HPP__

#include "util_image_definitions.hpp"
#include "util_image_types.hpp"
#include "util_texture_info.hpp"
#include <mathutil/uvec.h>
#include <string>
#include <memory>
#include <functional>
#include <cinttypes>
#include <optional>

namespace ufile {
	struct IFile;
};
namespace uimg {
	class ImageBuffer;
	struct TextureInfo;

	DLLUIMG bool read_image_size(const std::string &file, uint32_t &pixelWidth, uint32_t &pixelHeight);
	DLLUIMG void calculate_mipmap_size(uint32_t w, uint32_t h, uint32_t &outWMipmap, uint32_t &outHMipmap, uint32_t level);

	enum class ImageFormat : uint8_t { PNG = 0, BMP, TGA, JPG, HDR, Count };
	enum class PixelFormat : uint8_t { LDR = 0, HDR, Float };
	DLLUIMG std::string get_file_extension(ImageFormat format);
	DLLUIMG std::shared_ptr<ImageBuffer> load_image(ufile::IFile &f, PixelFormat pixelFormat = PixelFormat::LDR, bool flipVertically = false);
	DLLUIMG std::shared_ptr<ImageBuffer> load_image(const std::string &fileName, PixelFormat pixelFormat = PixelFormat::LDR, bool flipVertically = false);
	DLLUIMG bool save_image(ufile::IFile &f, ImageBuffer &imgBuffer, ImageFormat format, float quality = 1.f, bool flipVertically = false);
#ifdef UIMG_ENABLE_SVG
	struct DLLUIMG SvgImageInfo {
		std::string styleSheet {};
		std::optional<uint32_t> width {};
		std::optional<uint32_t> height {};
	};
	DLLUIMG std::shared_ptr<ImageBuffer> load_svg(ufile::IFile &f, const SvgImageInfo &svgInfo = {});
	DLLUIMG std::shared_ptr<ImageBuffer> load_svg(const std::string &fileName, const SvgImageInfo &svgInfo = {});
#endif
#ifdef UIMG_ENABLE_NVTT
	struct DLLUIMG TextureOutputHandler {
		std::function<void(int size, int width, int height, int depth, int face, int miplevel)> beginImage = nullptr;
		std::function<bool(const void *data, int size)> writeData = nullptr;
		std::function<void()> endImage = nullptr;
	};
	struct DLLUIMG TextureSaveInfo {
		uimg::TextureInfo texInfo {};

		// Only needed if no image buffer was specified
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t szPerPixel = 0;

		// Only needed if no layer data was specified
		uint32_t numLayers = 0;
		uint32_t numMipmaps = 0;

		bool cubemap = false;
		std::optional<uimg::ChannelMask> channelMask {};
	};
	DLLUIMG bool compress_texture(const TextureOutputHandler &outputHandler, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler = nullptr);
	DLLUIMG bool compress_texture(std::vector<std::vector<std::vector<uint8_t>>> &outputData, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const TextureSaveInfo &texSaveInfo,
	  const std::function<void(const std::string &)> &errorHandler = nullptr);
	DLLUIMG bool save_texture(const std::string &fileName, const std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> &fGetImgData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler = nullptr,
	  bool absoluteFileName = false);
	DLLUIMG bool save_texture(ufile::IFile &f, ImageBuffer &imgBuffer, const TextureSaveInfo &texInfo);
	DLLUIMG bool save_texture(const std::string &fileName, uimg::ImageBuffer &imgBuffer, const TextureSaveInfo &texInfo, const std::function<void(const std::string &)> &errorHandler = nullptr, bool absoluteFileName = false);
	DLLUIMG bool save_texture(const std::string &fileName, const std::vector<std::vector<const void *>> &imgLayerMipmapData, const TextureSaveInfo &texSaveInfo, const std::function<void(const std::string &)> &errorHandler = nullptr, bool absoluteFileName = false);
#endif
	DLLUIMG std::optional<ImageFormat> string_to_image_output_format(const std::string &str);
	DLLUIMG std::string get_image_output_format_extension(ImageFormat format);

	DLLUIMG void bake_margin(ImageBuffer &imgBuffer, std::vector<uint8_t> &mask, const int margin);
	DLLUIMG Vector3 linear_to_srgb(const Vector3 &color);
	DLLUIMG Vector3 srgb_to_linear(const Vector3 &srgbIn);
};

#endif

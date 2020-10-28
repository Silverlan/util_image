/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_IMAGE_HPP__
#define __UTIL_IMAGE_HPP__

#include "util_image_definitions.hpp"
#include <string>
#include <memory>
#include <functional>
#include <cinttypes>
#include <optional>

class VFilePtrInternal;
class VFilePtrInternalReal;
namespace uimg
{
	class ImageBuffer;
	struct TextureInfo;

	DLLUIMG bool read_image_size(const std::string &file,uint32_t &pixelWidth,uint32_t &pixelHeight);
	DLLUIMG void calculate_mipmap_size(uint32_t w,uint32_t h,uint32_t &outWMipmap,uint32_t &outHMipmap,uint32_t level);

	enum class ImageFormat : uint8_t
	{
		PNG = 0,
		BMP,
		TGA,
		JPG,
		HDR,
		Count
	};
	enum class PixelFormat : uint8_t
	{
		LDR = 0,
		HDR,
		Float
	};
	DLLUIMG std::string get_file_extension(ImageFormat format);
	DLLUIMG std::shared_ptr<ImageBuffer> load_image(std::shared_ptr<VFilePtrInternal> f,PixelFormat pixelFormat=PixelFormat::LDR);
	DLLUIMG std::shared_ptr<ImageBuffer> load_image(const std::string &fileName,PixelFormat pixelFormat=PixelFormat::LDR);
	DLLUIMG bool save_image(std::shared_ptr<VFilePtrInternalReal> f,ImageBuffer &imgBuffer,ImageFormat format,float quality=1.f);
#ifdef UIMG_ENABLE_NVTT
	DLLUIMG bool save_texture(
		const std::string &fileName,const std::function<const uint8_t*(uint32_t,uint32_t,std::function<void()>&)> &fGetImgData,uint32_t width,uint32_t height,uint32_t szPerPixel,
		uint32_t numLayers,uint32_t numMipmaps,bool cubemap,
		const uimg::TextureInfo &texInfo,const std::function<void(const std::string&)> &errorHandler=nullptr,
		bool absoluteFileName=false
	);
	DLLUIMG bool save_texture(std::shared_ptr<VFilePtrInternalReal> f,ImageBuffer &imgBuffer,const TextureInfo &texInfo);
	DLLUIMG bool save_texture(
		const std::string &fileName,uimg::ImageBuffer &imgBuffer,const uimg::TextureInfo &texInfo,bool cubemap,const std::function<void(const std::string&)> &errorHandler=nullptr,bool absoluteFileName=false
	);
	DLLUIMG bool save_texture(
		const std::string &fileName,const std::vector<std::vector<const void*>> &imgLayerMipmapData,uint32_t width,uint32_t height,uint32_t sizePerPixel,
		const uimg::TextureInfo &texInfo,bool cubemap,const std::function<void(const std::string&)> &errorHandler=nullptr,bool absoluteFileName=false
	);
#endif
	DLLUIMG std::optional<ImageFormat> string_to_image_output_format(const std::string &str);
	DLLUIMG std::string get_image_output_format_extension(ImageFormat format);
};

#endif

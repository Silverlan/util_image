/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_IMAGE_H__
#define __UTIL_IMAGE_H__

#include <string>
#include <vector>
#include <memory>
#include <functional>

class VFilePtrInternal;
namespace uimg
{
	enum class ImageType : uint32_t
	{
		Invalid = 0u,
		RGB,
		RGBA,
		Alpha
	};
	enum class ImageFormat : uint32_t
	{
		Invalid = 0u,
		TGA,
		PNG
	};
	enum class Channel : uint8_t
	{
		Red = 0,
		Green = 1,
		Blue = 2,
		Alpha = 3
	};
	class Image
	{
	public:
		Image()=default;
		bool Initialize(const std::function<bool(std::vector<uint8_t>&,ImageType&,uint32_t&,uint32_t&)> &fInitializer);

		void FlipVertically();
		bool SwapChannels(Channel channelA,Channel channelB);
		bool ConvertToRGBA();
		uint32_t GetChannelCount() const;
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		ImageType GetType() const;
		const std::vector<uint8_t> &GetData() const;
	private:
		std::vector<uint8_t> m_data;
		ImageType m_type = ImageType::Invalid;
		uint32_t m_width = 0u;
		uint32_t m_height = 0u;
	};
	bool read_image_size(const std::string &file,uint32_t &pixelWidth,uint32_t &pixelHeight);
	std::shared_ptr<Image> load_image(const std::string &fileName);
	std::shared_ptr<Image> load_image(std::shared_ptr<VFilePtrInternal> &file,ImageFormat format);
};

#endif

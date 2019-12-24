/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image.h"
#include <mathutil/umath.h>

bool uimg::Image::ConvertToRGBA()
{
	switch(m_type)
	{
		case ImageType::RGBA:
			return true;
		case ImageType::RGB:
		{
			auto numPixels = m_width *m_height;
			auto rgbData = m_data;
			m_data.resize(numPixels *4u);
			auto *rgbaData = m_data.data();
			for(auto i=decltype(numPixels){0u};i<numPixels;++i)
			{
				auto srcPxIdx = i *3u;
				auto dstPxIdx = i *4u;
				for(auto j=0u;j<3u;++j)
					rgbaData[dstPxIdx +j] = rgbData[srcPxIdx +j];
				rgbaData[dstPxIdx +3u] = std::numeric_limits<uint8_t>::max();
			}
			return true;
		}
	}
	return false;
}

uint32_t uimg::Image::GetChannelCount() const
{
	switch(m_type)
	{
		case ImageType::Alpha:
			return 1;
		case ImageType::RGB:
			return 3;
		case ImageType::RGBA:
			return 4;
	}
	return 0;
}

uint32_t uimg::Image::GetWidth() const {return m_width;}
uint32_t uimg::Image::GetHeight() const {return m_height;}
uimg::ImageType uimg::Image::GetType() const {return m_type;}
const std::vector<uint8_t> &uimg::Image::GetData() const {return m_data;}

void uimg::Image::FlipVertically()
{
	auto numChannels = GetChannelCount();
	for(auto i=decltype(m_data.size()){0u};i<((m_height /2) *m_width);++i)
	{
		auto pxIdx0 = i;
		auto pxIdx1 = (m_width *m_height) -m_width +(i %m_width) -(i /m_width) *m_width;
		pxIdx0 *= numChannels;
		pxIdx1 *= numChannels;
		for(auto j=0u;j<numChannels;++j)
			umath::swap(m_data.at(pxIdx0 +j),m_data.at(pxIdx1 +j));
	}
}

bool uimg::Image::SwapChannels(Channel channelA,Channel channelB)
{
	auto numChannels = GetChannelCount();
	if(umath::to_integral(channelA) >= numChannels || umath::to_integral(channelB) >= numChannels)
		return false;
	for(auto i=decltype(m_data.size()){0u};i<(m_width *m_height);++i)
	{
		auto pxIdx = i *numChannels;
		umath::swap(m_data.at(pxIdx +umath::to_integral(channelA)),m_data.at(pxIdx +umath::to_integral(channelB)));
	}
	return true;
}

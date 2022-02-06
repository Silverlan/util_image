/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image_buffer.hpp"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#include <sharedutils/util.h>
#include <sharedutils/magic_enum.hpp>
#include <mathutil/color.h>
#include <mathutil/umath.h>
#include <thread>
#include <stdexcept>
#pragma optimize("",off)
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Create(const void *data,uint32_t width,uint32_t height,Format format)
{
	return Create(const_cast<void*>(data),width,height,format,false);
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::CreateWithCustomDeleter(void *data,uint32_t width,uint32_t height,Format format,const std::function<void(void*)> &customDeleter)
{
	if(customDeleter == nullptr)
	{
		auto buf = Create(width,height,format);
		if(data)
			buf->Write(0,width *height *GetPixelSize(format),data);
		return buf;
	}
	auto ptrData = std::shared_ptr<void>{data,customDeleter};
	return std::shared_ptr<ImageBuffer>{new ImageBuffer{ptrData,width,height,format}};
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Create(void *data,uint32_t width,uint32_t height,Format format,bool ownedExternally)
{
	if(ownedExternally == false)
		return CreateWithCustomDeleter(data,width,height,format,nullptr);
	return CreateWithCustomDeleter(data,width,height,format,[](void*) {});
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Create(uint32_t width,uint32_t height,Format format)
{
	auto buf = std::shared_ptr<ImageBuffer>{new ImageBuffer{nullptr,width,height,format}};
	buf->Reallocate();
	return buf;
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Create(ImageBuffer &parent,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{
	auto xMax = parent.GetWidth();
	auto yMax = parent.GetHeight();
	x = umath::min(x,xMax);
	y = umath::min(y,yMax);
	w = umath::min(x +w,xMax) -x;
	h = umath::min(y +h,yMax) -y;

	auto buf = std::shared_ptr<ImageBuffer>{new ImageBuffer{nullptr,w,h,parent.GetFormat()}};
	buf->m_offsetRelToParent = {x,y};
	buf->m_parent = parent.shared_from_this();
	buf->m_data = parent.m_data;
	return buf;
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::CreateCubemap(const std::array<std::shared_ptr<ImageBuffer>,6> &cubemapSides)
{
	auto &img0 = cubemapSides.front();
	auto w = img0->GetWidth();
	auto h = img0->GetHeight();
	auto format = img0->GetFormat();
	for(auto &img : cubemapSides)
	{
		// Make sure all sides have the same format
		if(img->GetWidth() != w || img->GetHeight() != h || img->GetFormat() != format)
			return nullptr;
	}
	auto cubemap = Create(w *4,h *3,format);
	cubemap->Clear(Vector4{0.f,0.f,0.f,0.f});

	std::array<std::pair<uint32_t,uint32_t>,6> pxOffsetCoords = {
		std::pair<uint32_t,uint32_t>{w *2,h}, // Right
		std::pair<uint32_t,uint32_t>{0,h}, // Left
		std::pair<uint32_t,uint32_t>{w,0}, // Top
		std::pair<uint32_t,uint32_t>{w,h *2}, // Bottom
		std::pair<uint32_t,uint32_t>{w,h}, // Front
		std::pair<uint32_t,uint32_t>{w *3,h} // Back
	};
	std::array<std::thread,6> threads {};
	for(auto i=decltype(pxOffsetCoords.size()){0u};i<pxOffsetCoords.size();++i)
	{
		auto coords = pxOffsetCoords.at(i);
		auto imgSide = cubemapSides.at(i);
		threads.at(i) = std::thread{[i,coords,cubemap,&cubemapSides,w,h]() {
			cubemapSides.at(i)->Copy(*cubemap,0,0,coords.first,coords.second,w,h);
		}};
	}
	for(auto &t : threads)
		t.join();
	return cubemap;
}
uimg::ImageBuffer::LDRValue uimg::ImageBuffer::ToLDRValue(HDRValue value)
{
	return umath::clamp<uint32_t>(
		umath::float16_to_float32_glm(value) *static_cast<float>(std::numeric_limits<uint8_t>::max()),
		0,std::numeric_limits<uint8_t>::max()
		);
}
uimg::ImageBuffer::LDRValue uimg::ImageBuffer::ToLDRValue(FloatValue value)
{
	return umath::clamp<uint32_t>(
		value *static_cast<float>(std::numeric_limits<uint8_t>::max()),
		0,std::numeric_limits<uint8_t>::max()
		);
}
uimg::ImageBuffer::HDRValue uimg::ImageBuffer::ToHDRValue(LDRValue value)
{
	return ToHDRValue(ToFloatValue(value));
}
uimg::ImageBuffer::HDRValue uimg::ImageBuffer::ToHDRValue(FloatValue value)
{
	return umath::float32_to_float16_glm(value);
}
uimg::ImageBuffer::FloatValue uimg::ImageBuffer::ToFloatValue(LDRValue value)
{
	return value /static_cast<float>(std::numeric_limits<LDRValue>::max());
}
uimg::ImageBuffer::FloatValue uimg::ImageBuffer::ToFloatValue(HDRValue value)
{
	return umath::float16_to_float32_glm(value);
}
uimg::Format uimg::ImageBuffer::ToLDRFormat(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return Format::R8;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return Format::RG8;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB8;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA8;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return Format::None;
}
uimg::Format uimg::ImageBuffer::ToHDRFormat(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return Format::R16;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return Format::RG16;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB16;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA16;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return Format::None;
}
uimg::Format uimg::ImageBuffer::ToFloatFormat(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return Format::R32;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return Format::RG32;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB32;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA32;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return Format::None;
}
uimg::Format uimg::ImageBuffer::ToRGBFormat(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return Format::RGB8;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return Format::RGB16;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return Format::RGB32;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return Format::None;
}
uimg::Format uimg::ImageBuffer::ToRGBAFormat(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return Format::RGBA8;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return Format::RGBA16;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return Format::RGBA32;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return Format::None;
}
size_t uimg::ImageBuffer::GetRowStride() const {return GetPixelSize() *GetWidth();}
size_t uimg::ImageBuffer::GetPixelStride() const {return GetPixelSize();}
void uimg::ImageBuffer::FlipHorizontally()
{
	auto w = GetWidth();
	auto h = GetHeight();
	auto rowStride = GetRowStride();
	auto pxStride = GetPixelStride();
	auto *p = static_cast<uint8_t*>(GetData());
	for(auto y=decltype(h){0u};y<h;++y)
	{
		util::flip_item_sequence(p,w *pxStride,w,pxStride);
		p += rowStride;
	}
}
void uimg::ImageBuffer::FlipVertically()
{
	auto w = GetWidth();
	auto h = GetHeight();
	util::flip_item_sequence(GetData(),w *h *GetPixelStride(),h,GetRowStride());
}
void uimg::ImageBuffer::Flip(bool horizontally,bool vertically)
{
	if(horizontally && vertically)
	{
		// Optimized algorithm for flipping both axes at once
		auto w = GetWidth();
		auto h = GetHeight();
		auto rowStride = GetRowStride();
		auto pxStride = GetPixelStride();
		auto *sequence = GetData();
		auto *tmp = new uint8_t[rowStride];
		auto *row0 = static_cast<uint8_t*>(sequence);
		auto *row1 = row0 +GetSize() -rowStride;
		for(auto y=decltype(h){0u};y<h /2;++y)
		{
			memcpy(tmp,row1,rowStride);
			memcpy(row1,row0,rowStride);
			memcpy(row0,tmp,rowStride);

			// Flip both rows horizontally
			for(auto *rowStart : {row0,row1})
			{
				auto *px0 = rowStart;
				auto *px1 = rowStart +rowStride -pxStride;
				for(auto x=decltype(w){0u};x<w /2;++x)
				{
					memcpy(tmp,px1,pxStride);
					memcpy(px1,px0,pxStride);
					memcpy(px0,tmp,pxStride);

					px0 += pxStride;
					px1 -= pxStride;
				}
			}

			row0 += rowStride;
			row1 -= rowStride;
		}
		if((h %2) != 0)
		{
			// Uneven height; We still have to flip the center row horizontally
			auto *p = static_cast<uint8_t*>(sequence) +(h /2) *GetRowStride();
			util::flip_item_sequence(p,GetRowStride(),GetWidth(),GetPixelStride());
		}
		delete[] tmp;
		return;
	}
	if(horizontally)
		FlipHorizontally();
	if(vertically)
		FlipVertically();
}
void uimg::ImageBuffer::Crop(uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *optOutCroppedData)
{
	std::shared_ptr<void> newData = nullptr;
	void *newDataPtr = optOutCroppedData;
	if(optOutCroppedData == nullptr)
	{
		newData = std::shared_ptr<void>{new uint8_t[w *h *GetPixelStride()],[](const void *ptr) {
			delete[] static_cast<const uint8_t*>(ptr);
		}};
		newDataPtr = newData.get();
	}
	auto *srcData = static_cast<uint8_t*>(GetData());
	auto *dstData = static_cast<uint8_t*>(newDataPtr);
	auto srcRowStride = GetRowStride();
	auto dstRowStride = w *GetPixelStride();
	for(auto yc=y;yc<(y +h);++yc)
	{
		memmove(dstData,srcData,dstRowStride);
		srcData += srcRowStride;
		dstData += dstRowStride;
	}
	if(newData)
		m_data = newData;
	m_width = w;
	m_height = h;
}
void uimg::ImageBuffer::InitPixelView(uint32_t x,uint32_t y,PixelView &pxView)
{
	pxView.m_offset = GetPixelOffset(x,y);
}
uimg::ImageBuffer::PixelView uimg::ImageBuffer::GetPixelView(Offset offset) {return PixelView{*this,offset};}
uimg::ImageBuffer::PixelView uimg::ImageBuffer::GetPixelView(uint32_t x,uint32_t y) {return GetPixelView(GetPixelOffset(x,y));}
void uimg::ImageBuffer::SetPixelColor(uint32_t x,uint32_t y,const std::array<uint8_t,4> &color) {SetPixelColor(GetPixelIndex(x,y),color);}
void uimg::ImageBuffer::SetPixelColor(PixelIndex index,const std::array<uint8_t,4> &color)
{
	auto pxView = GetPixelView(GetPixelOffset(index));
	for(uint8_t i=0;i<4;++i)
		pxView.SetValue(static_cast<uimg::Channel>(i),color.at(i));
}
void uimg::ImageBuffer::SetPixelColor(uint32_t x,uint32_t y,const std::array<uint16_t,4> &color) {SetPixelColor(GetPixelIndex(x,y),color);}
void uimg::ImageBuffer::SetPixelColor(PixelIndex index,const std::array<uint16_t,4> &color)
{
	auto pxView = GetPixelView(GetPixelOffset(index));
	for(uint8_t i=0;i<4;++i)
		pxView.SetValue(static_cast<uimg::Channel>(i),color.at(i));
}
void uimg::ImageBuffer::SetPixelColor(uint32_t x,uint32_t y,const Vector4 &color) {SetPixelColor(GetPixelIndex(x,y),color);}
void uimg::ImageBuffer::SetPixelColor(PixelIndex index,const Vector4 &color)
{
	auto pxView = GetPixelView(GetPixelOffset(index));
	for(uint8_t i=0;i<4;++i)
		pxView.SetValue(static_cast<uimg::Channel>(i),color[i]);
}

uimg::ImageBuffer *uimg::ImageBuffer::GetParent() {return (m_parent.expired() == false) ? m_parent.lock().get() : nullptr;}
const std::pair<uint64_t,uint64_t> &uimg::ImageBuffer::GetPixelCoordinatesRelativeToParent() const {return m_offsetRelToParent;}
uimg::ImageBuffer::Offset uimg::ImageBuffer::GetAbsoluteOffset(Offset localOffset) const
{
	if(m_parent.expired())
		return localOffset;
	auto parent = m_parent.lock();
	auto pxCoords = GetPixelCoordinates(localOffset);
	pxCoords.first += m_offsetRelToParent.first;
	pxCoords.second += m_offsetRelToParent.second;
	return parent->GetAbsoluteOffset(parent->GetPixelOffset(pxCoords.first,pxCoords.second));
}
float uimg::calc_luminance(const Vector3 &color)
{
	return 0.212671 *color.r +0.71516 *color.g +0.072169 *color.b;
}
void uimg::ImageBuffer::CalcLuminance(float &outAvgLuminance,float &outMinLuminance,float &outMaxLuminance,Vector3 &outAvgIntensity,float *optOutLogAvgLuminance) const
{
	float delta = 1e-4f;
	outAvgLuminance = 0.f;
	if(optOutLogAvgLuminance)
		*optOutLogAvgLuminance = 0.f;
	outMinLuminance = std::numeric_limits<float>::max();
	outMaxLuminance = std::numeric_limits<float>::lowest();
	outAvgIntensity = Vector3{};

	for(auto &pxView : const_cast<ImageBuffer&>(*this))
	{
		Vector3 col {
			pxView.GetFloatValue(Channel::Red),
			pxView.GetFloatValue(Channel::Green),
			pxView.GetFloatValue(Channel::Blue)
		};
		outAvgIntensity += col;
		auto lum = calc_luminance(col);
		outAvgLuminance += lum;
		if(optOutLogAvgLuminance)
			*optOutLogAvgLuminance += std::log(delta +lum);
		if(lum > outMaxLuminance)
			outMaxLuminance = lum;
		if(lum < outMinLuminance)
			outMinLuminance = lum;
	}

	auto numPixels = static_cast<float>(GetPixelCount());
	outAvgIntensity = outAvgIntensity /numPixels;
	outAvgLuminance = outAvgLuminance /numPixels;
	if(optOutLogAvgLuminance)
		*optOutLogAvgLuminance = std::exp(*optOutLogAvgLuminance /numPixels);
}
uint8_t uimg::ImageBuffer::GetChannelCount(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return 1;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return 2;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return 3;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return 4;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return 0;
}
uint8_t uimg::ImageBuffer::GetChannelSize(Format format)
{
	switch(format)
	{
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return 1;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return 2;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return 4;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return 0;
}
uimg::ImageBuffer::Size uimg::ImageBuffer::GetPixelSize(Format format)
{
	return GetChannelCount(format) *GetChannelSize(format);
}
uimg::ImageBuffer::ImageBuffer(const std::shared_ptr<void> &data,uint32_t width,uint32_t height,Format format)
	: m_data{data},m_width{width},m_height{height},m_format{format}
{}
std::pair<uint32_t,uint32_t> uimg::ImageBuffer::GetPixelCoordinates(Offset offset) const
{
	offset /= GetPixelSize();
	auto x = offset %GetWidth();
	auto y = offset /GetWidth();
	return {x,y};
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Copy() const
{
	return uimg::ImageBuffer::Create(m_data.get(),m_width,m_height,m_format,false);
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Copy(Format format) const
{
	// Optimized copy that performs copy +format change in one go
	auto cpy = uimg::ImageBuffer::Create(m_width,m_height,m_format);
	Convert(const_cast<ImageBuffer&>(*this),*cpy,format);
	return cpy;
}
bool uimg::ImageBuffer::Copy(ImageBuffer &dst) const
{
	auto w = GetWidth();
	auto h = GetHeight();
	if(w != dst.GetWidth() || h != dst.GetHeight())
		return false;
	Copy(dst,0,0,0,0,w,h);
	return true;
}
void uimg::ImageBuffer::Copy(ImageBuffer &dst,uint32_t xSrc,uint32_t ySrc,uint32_t xDst,uint32_t yDst,uint32_t w,uint32_t h) const
{
	if(GetFormat() == dst.GetFormat() && xSrc == 0 && ySrc == 0 && xDst == 0 && yDst == 0 && w == GetWidth() && h == GetHeight())
	{
		memcpy(dst.GetData(),GetData(),GetSize());
		return;
	}
	auto imgViewSrc = Create(const_cast<ImageBuffer&>(*this),xSrc,ySrc,w,h);
	auto imgViewDst = Create(dst,xDst,yDst,w,h);
	for(auto &px : *imgViewSrc)
	{
		auto pxDst = imgViewDst->GetPixelView(px.GetX(),px.GetY());
		pxDst.CopyValues(px);
	}
}
uimg::Format uimg::ImageBuffer::GetFormat() const {return m_format;}
uint32_t uimg::ImageBuffer::GetWidth() const {return m_width;}
uint32_t uimg::ImageBuffer::GetHeight() const {return m_height;}
uimg::ImageBuffer::Size uimg::ImageBuffer::GetPixelSize() const {return GetPixelSize(GetFormat());}
uint32_t uimg::ImageBuffer::GetPixelCount() const {return m_width *m_height;}
bool uimg::ImageBuffer::HasAlphaChannel() const {return GetChannelCount() >= umath::to_integral(Channel::Alpha) +1;}
bool uimg::ImageBuffer::IsLDRFormat() const
{
	switch(GetFormat())
	{
	case Format::R_LDR:
	case Format::RG_LDR:
	case Format::RGB_LDR:
	case Format::RGBA_LDR:
		return true;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return false;
}
bool uimg::ImageBuffer::IsHDRFormat() const
{
	switch(GetFormat())
	{
	case Format::R_HDR:
	case Format::RG_HDR:
	case Format::RGB_HDR:
	case Format::RGBA_HDR:
		return true;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return false;
}
bool uimg::ImageBuffer::IsFloatFormat() const
{
	switch(GetFormat())
	{
	case Format::R_FLOAT:
	case Format::RG_FLOAT:
	case Format::RGB_FLOAT:
	case Format::RGBA_FLOAT:
		return true;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return false;
}
uint8_t uimg::ImageBuffer::GetChannelCount() const {return GetChannelCount(GetFormat());}
uint8_t uimg::ImageBuffer::GetChannelSize() const {return GetChannelSize(GetFormat());}
uimg::ImageBuffer::PixelIndex uimg::ImageBuffer::GetPixelIndex(uint32_t x,uint32_t y) const {return y *GetWidth() +x;}
uimg::ImageBuffer::Offset uimg::ImageBuffer::GetPixelOffset(uint32_t x,uint32_t y) const {return GetPixelOffset(GetPixelIndex(x,y));}
uimg::ImageBuffer::Offset uimg::ImageBuffer::GetPixelOffset(PixelIndex index) const {return index *GetPixelSize();}
const void *uimg::ImageBuffer::GetData() const {return const_cast<ImageBuffer*>(this)->GetData();}
void *uimg::ImageBuffer::GetData() {return m_data.get();}
void uimg::ImageBuffer::Reallocate()
{
	m_data = std::shared_ptr<void>{new uint8_t[GetSize()],[](const void *ptr) {
		delete[] static_cast<const uint8_t*>(ptr);
	}};
}
uimg::ImageBuffer::PixelIterator uimg::ImageBuffer::begin()
{
	return PixelIterator{*this,0};
}
uimg::ImageBuffer::PixelIterator uimg::ImageBuffer::end()
{
	return PixelIterator{*this,GetSize()};
}
void uimg::ImageBuffer::Convert(ImageBuffer &srcImg,ImageBuffer &dstImg,Format targetFormat)
{
	if(dstImg.GetFormat() != targetFormat)
	{
		dstImg.m_format = targetFormat;
		dstImg.Reallocate();
	}
	auto itSrc = srcImg.begin();
	auto itDst = dstImg.begin();
	while(itSrc != srcImg.end())
	{
		auto &srcPixel = *itSrc;
		auto &dstPixel = *itDst;
		auto numChannels = umath::to_integral(Channel::Count);
		for(auto i=decltype(numChannels){0u};i<numChannels;++i)
		{
			auto channel = static_cast<Channel>(i);
			switch(targetFormat)
			{
			case Format::R8:
			case Format::RG8:
			case Format::RGB8:
			case Format::RGBA8:
				dstPixel.SetValue(channel,srcPixel.GetLDRValue(channel));
				break;
			case Format::R16:
			case Format::RG16:
			case Format::RGB16:
			case Format::RGBA16:
				dstPixel.SetValue(channel,srcPixel.GetHDRValue(channel));
				break;
			case Format::R32:
			case Format::RG32:
			case Format::RGB32:
			case Format::RGBA32:
				dstPixel.SetValue(channel,srcPixel.GetFloatValue(channel));
				break;
			}
			static_assert(umath::to_integral(Format::Count) == 13u);
		}
		++itSrc;
		++itDst;
	}
}
void uimg::ImageBuffer::Convert(Format targetFormat)
{
	if(GetFormat() == targetFormat)
		return;
	auto cpy = *this;
	Convert(cpy,*this,targetFormat);
}
void uimg::ImageBuffer::Convert(ImageBuffer &dst)
{
	Convert(*this,dst,dst.GetFormat());
}
void uimg::ImageBuffer::SwapChannels(ChannelMask swizzle)
{
	if(swizzle == ChannelMask{})
		return;
	auto channelSize = GetChannelSize();
	auto numChannels = GetChannelCount();
	auto pxSize = channelSize *numChannels;
	std::vector<uint8_t> tmpChannelData {};
	tmpChannelData.resize(pxSize);
	for(auto &px : *this)
	{
		auto *pxData = px.GetPixelData();
		auto *channelData = static_cast<uint8_t*>(pxData);

		for(auto i=decltype(numChannels){0u};i<numChannels;++i)
			memcpy(tmpChannelData.data() +i *channelSize,channelData +umath::to_integral(swizzle[i]) *channelSize,channelSize);

		memcpy(channelData,tmpChannelData.data(),pxSize);
	}
}
void uimg::ImageBuffer::SwapChannels(Channel channel0,Channel channel1)
{
	auto channelSize = GetChannelSize();
	std::vector<uint8_t> tmpChannelData {};
	tmpChannelData.resize(channelSize);
	for(auto &px : *this)
	{
		auto *pxData = px.GetPixelData();
		auto &channelData0 = *(static_cast<uint8_t*>(pxData) +umath::to_integral(channel0) *channelSize);
		auto &channelData1 = *(static_cast<uint8_t*>(pxData) +umath::to_integral(channel1) *channelSize);
		memcpy(tmpChannelData.data(),&channelData0,channelSize);
		memcpy(&channelData0,&channelData1,channelSize);
		memcpy(&channelData1,tmpChannelData.data(),channelSize);
	}
}
void uimg::ImageBuffer::ToLDR()
{
	switch(m_format)
	{
	case Format::R16:
	case Format::R32:
		Convert(Format::R8);
		return;
	case Format::RG16:
	case Format::RG32:
		Convert(Format::RG8);
		return;
	case Format::RGB16:
	case Format::RGB32:
		Convert(Format::RGB8);
		return;
	case Format::RGBA16:
	case Format::RGBA32:
		Convert(Format::RGBA8);
		return;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
void uimg::ImageBuffer::ToHDR()
{
	switch(m_format)
	{
	case Format::R8:
	case Format::R32:
		Convert(Format::R16);
		return;
	case Format::RG8:
	case Format::RG32:
		Convert(Format::RG16);
		return;
	case Format::RGB8:
	case Format::RGB32:
		Convert(Format::RGB16);
		return;
	case Format::RGBA8:
	case Format::RGBA32:
		Convert(Format::RGBA16);
		return;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
void uimg::ImageBuffer::ToFloat()
{
	switch(m_format)
	{
	case Format::R8:
	case Format::R16:
		Convert(Format::R32);
		return;
	case Format::RG8:
	case Format::RG16:
		Convert(Format::RG32);
		return;
	case Format::RGB8:
	case Format::RGB16:
		Convert(Format::RGB32);
		return;
	case Format::RGBA8:
	case Format::RGBA16:
		Convert(Format::RGBA32);
		return;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
void uimg::ImageBuffer::Clear(const Color &color) {Clear(color.ToVector4());}
void uimg::ImageBuffer::Clear(const Vector4 &color)
{
	for(auto &px : *this)
	{
		for(uint8_t i=0;i<4;++i)
			px.SetValue(static_cast<Channel>(i),color[i]);
	}
}
void uimg::ImageBuffer::ClearAlpha(LDRValue alpha)
{
	if(HasAlphaChannel() == false)
		return;
	if(IsLDRFormat())
	{
		for(auto &px : *this)
			px.SetValue(Channel::Alpha,alpha);
		return;
	}
	if(IsHDRFormat())
	{
		auto value = ToHDRValue((alpha /static_cast<float>(std::numeric_limits<LDRValue>::max())));// *std::numeric_limits<float>::max());
		for(auto &px : *this)
			px.SetValue(Channel::Alpha,value);
		return;
	}
	if(IsFloatFormat())
	{
		auto value = (alpha /static_cast<float>(std::numeric_limits<LDRValue>::max()));// *std::numeric_limits<float>::max();
		for(auto &px : *this)
			px.SetValue(Channel::Alpha,value);
		return;
	}
}
uimg::ImageBuffer::Size uimg::ImageBuffer::GetSize() const
{
	return GetPixelCount() *GetPixelSize(GetFormat());
}
void uimg::ImageBuffer::Read(Offset offset,Size size,void *outData)
{
	auto *srcPtr = static_cast<uint8_t*>(m_data.get()) +offset;
	memcpy(outData,srcPtr,size);
}
void uimg::ImageBuffer::Write(Offset offset,Size size,const void *inData)
{
	auto *dstPtr = static_cast<uint8_t*>(m_data.get()) +offset;
	memcpy(dstPtr,inData,size);
}
void uimg::ImageBuffer::Resize(Size width,Size height,EdgeAddressMode addressMode,Filter filter,ColorSpace colorSpace)
{
	if(width == m_width && height == m_height)
		return;
	auto imgResized = Create(width,height,GetFormat());
	stbir_datatype stformat;
	if(IsLDRFormat()) stformat = stbir_datatype::STBIR_TYPE_UINT8;
	else if(IsHDRFormat()) stformat = stbir_datatype::STBIR_TYPE_UINT16;
	else if(IsFloatFormat()) stformat = stbir_datatype::STBIR_TYPE_FLOAT;
	else return;

	stbir_edge stedge;
	switch(addressMode)
	{
	case EdgeAddressMode::Clamp:
		stedge = stbir_edge::STBIR_EDGE_CLAMP;
		break;
	case EdgeAddressMode::Reflect:
		stedge = stbir_edge::STBIR_EDGE_REFLECT;
		break;
	case EdgeAddressMode::Wrap:
		stedge = stbir_edge::STBIR_EDGE_WRAP;
		break;
	case EdgeAddressMode::Zero:
		stedge = stbir_edge::STBIR_EDGE_ZERO;
		break;
	}
	static_assert(umath::to_integral(EdgeAddressMode::Count) == 4);

	stbir_filter stfilter;
	switch(filter)
	{
	case Filter::Default:
		stfilter = stbir_filter::STBIR_FILTER_DEFAULT;
		break;
	case Filter::Box:
		stfilter = stbir_filter::STBIR_FILTER_BOX;
		break;
	case Filter::Triangle:
		stfilter = stbir_filter::STBIR_FILTER_TRIANGLE;
		break;
	case Filter::CubicBSpline:
		stfilter = stbir_filter::STBIR_FILTER_CUBICBSPLINE;
		break;
	case Filter::CatmullRom:
		stfilter = stbir_filter::STBIR_FILTER_CATMULLROM;
		break;
	case Filter::Mitchell:
		stfilter = stbir_filter::STBIR_FILTER_MITCHELL;
		break;
	}
	static_assert(umath::to_integral(Filter::Count) == 6);

	stbir_colorspace stColorspace;
	switch(colorSpace)
	{
	case ColorSpace::Auto:
		stColorspace = !IsLDRFormat() ? stbir_colorspace::STBIR_COLORSPACE_LINEAR : stbir_colorspace::STBIR_COLORSPACE_SRGB;
		break;
	case ColorSpace::Linear:
		stColorspace = stbir_colorspace::STBIR_COLORSPACE_LINEAR;
		break;
	case ColorSpace::SRGB:
		stColorspace = stbir_colorspace::STBIR_COLORSPACE_SRGB;
		break;
	}
	static_assert(umath::to_integral(ColorSpace::Count) == 3);

	auto res = stbir_resize(
		static_cast<uint8_t*>(GetData()),GetWidth(),GetHeight(),0 /* stride */,
		static_cast<uint8_t*>(imgResized->GetData()),imgResized->GetWidth(),imgResized->GetHeight(),0 /* stride */,
		stformat,GetChannelCount(),HasAlphaChannel() ? umath::to_integral(Channel::Alpha) : STBIR_ALPHA_CHANNEL_NONE,0 /* flags */,stedge,stedge,stfilter,stfilter,stColorspace,nullptr
	);
	if(res == 0)
		return;
	*this = *imgResized;
}

std::ostream &operator<<(std::ostream &out,const uimg::ImageBuffer &o)
{
	out<<"ImageBuffer";
	out<<"[Format:"<<magic_enum::enum_name(o.GetFormat())<<"]";
	out<<"[Resolution:"<<o.GetWidth()<<"x"<<o.GetHeight()<<"]";
	out<<"[Channels:"<<o.GetChannelCount()<<"]";
	return out;
}
#pragma optimize("",on)

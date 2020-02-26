/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image_buffer.hpp"
#include <mathutil/color.h>
#include <mathutil/umath.h>
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
uimg::ImageBuffer::Format uimg::ImageBuffer::ToLDRFormat(Format format)
{
	switch(format)
	{
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB8;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA8;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return Format::None;
}
uimg::ImageBuffer::Format uimg::ImageBuffer::ToHDRFormat(Format format)
{
	switch(format)
	{
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB16;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA16;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return Format::None;
}
uimg::ImageBuffer::Format uimg::ImageBuffer::ToFloatFormat(Format format)
{
	switch(format)
	{
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB32;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA32;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return Format::None;
}
uimg::ImageBuffer::Format uimg::ImageBuffer::ToRGBFormat(Format format)
{
	switch(format)
	{
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return format;
	case Format::RGBA8:
		return Format::RGB8;
	case Format::RGBA16:
		return Format::RGB16;
	case Format::RGBA32:
		return Format::RGB32;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return Format::None;
}
uimg::ImageBuffer::Format uimg::ImageBuffer::ToRGBAFormat(Format format)
{
	switch(format)
	{
	case Format::RGB8:
		return Format::RGBA8;
	case Format::RGB16:
		return Format::RGBA16;
	case Format::RGB32:
		return Format::RGBA32;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return format;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return Format::None;
}
void uimg::ImageBuffer::FlipHorizontally()
{
	auto imgFlipped = Copy();
	auto srcPxView = imgFlipped->GetPixelView();
	auto dstPxView = GetPixelView();
	auto w = GetWidth();
	auto h = GetHeight();
	for(auto x=decltype(w){0u};x<w;++x)
	{
		for(auto y=decltype(h){0};y<h;++y)
		{
			imgFlipped->InitPixelView(w -x -1,y,srcPxView);
			InitPixelView(x,y,dstPxView);
			dstPxView.CopyValues(srcPxView);
		}
	}
}
void uimg::ImageBuffer::FlipVertically()
{
	auto imgFlipped = Copy();
	auto srcPxView = imgFlipped->GetPixelView();
	auto dstPxView = GetPixelView();
	auto w = GetWidth();
	auto h = GetHeight();
	for(auto x=decltype(w){0u};x<w;++x)
	{
		for(auto y=decltype(h){0};y<h;++y)
		{
			imgFlipped->InitPixelView(x,h -y -1,srcPxView);
			InitPixelView(x,y,dstPxView);
			dstPxView.CopyValues(srcPxView);
		}
	}
}
void uimg::ImageBuffer::InitPixelView(uint32_t x,uint32_t y,PixelView &pxView)
{
	pxView.m_offset = GetPixelOffset(x,y);
}
uimg::ImageBuffer::PixelView uimg::ImageBuffer::GetPixelView(Offset offset)
{
	return PixelView{*this,offset};
}
uint8_t uimg::ImageBuffer::GetChannelCount(Format format)
{
	switch(format)
	{
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return 3;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return 4;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return 0;
}
uint8_t uimg::ImageBuffer::GetChannelSize(Format format)
{
	switch(format)
	{
	case Format::RGB8:
	case Format::RGBA8:
		return 1;
	case Format::RGB16:
	case Format::RGBA16:
		return 2;
	case Format::RGB32:
	case Format::RGBA32:
		return 4;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
	return 0;
}
uimg::ImageBuffer::Size uimg::ImageBuffer::GetPixelSize(Format format)
{
	return GetChannelCount(format) *GetChannelSize(format);
}
uimg::ImageBuffer::ImageBuffer(const std::shared_ptr<void> &data,uint32_t width,uint32_t height,Format format)
	: m_data{data},m_width{width},m_height{height},m_format{format}
{}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Copy() const
{
	return uimg::ImageBuffer::Create(m_data.get(),m_width,m_height,m_format,false);
}
std::shared_ptr<uimg::ImageBuffer> uimg::ImageBuffer::Copy(Format format) const
{
	// Optimized copy that performs copy +format change in one go
	auto cpy = uimg::ImageBuffer::Create(nullptr,m_width,m_height,m_format,true);
	Convert(const_cast<ImageBuffer&>(*this),*cpy,format);
	return cpy;
}
uimg::ImageBuffer::Format uimg::ImageBuffer::GetFormat() const {return m_format;}
uint32_t uimg::ImageBuffer::GetWidth() const {return m_width;}
uint32_t uimg::ImageBuffer::GetHeight() const {return m_height;}
uimg::ImageBuffer::Size uimg::ImageBuffer::GetPixelSize() const {return GetPixelSize(GetFormat());}
uint32_t uimg::ImageBuffer::GetPixelCount() const {return m_width *m_height;}
bool uimg::ImageBuffer::HasAlphaChannel() const {return GetChannelCount() >= umath::to_integral(Channel::Alpha) +1;}
bool uimg::ImageBuffer::IsLDRFormat() const
{
	switch(GetFormat())
	{
	case Format::RGB_LDR:
	case Format::RGBA_LDR:
		return true;
	}
	return false;
}
bool uimg::ImageBuffer::IsHDRFormat() const
{
	switch(GetFormat())
	{
	case Format::RGB_HDR:
	case Format::RGBA_HDR:
		return true;
	}
	return false;
}
bool uimg::ImageBuffer::IsFloatFormat() const
{
	switch(GetFormat())
	{
	case Format::RGB_FLOAT:
	case Format::RGBA_FLOAT:
		return true;
	}
	return false;
}
uint8_t uimg::ImageBuffer::GetChannelCount() const {return GetChannelCount(GetFormat());}
uint8_t uimg::ImageBuffer::GetChannelSize() const {return GetChannelSize(GetFormat());}
uimg::ImageBuffer::PixelIndex uimg::ImageBuffer::GetPixelIndex(uint32_t x,uint32_t y) const {return y *GetWidth() +x;}
uimg::ImageBuffer::Offset uimg::ImageBuffer::GetPixelOffset(uint32_t x,uint32_t y) const {return GetPixelIndex(x,y) *GetPixelSize();}
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
	if(dstImg.GetFormat() == targetFormat)
		return;
	dstImg.m_format = targetFormat;
	dstImg.Reallocate();
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
			case Format::RGB8:
			case Format::RGBA8:
				dstPixel.SetValue(channel,srcPixel.GetLDRValue(channel));
				break;
			case Format::RGB16:
			case Format::RGBA16:
				dstPixel.SetValue(channel,srcPixel.GetHDRValue(channel));
				break;
			case Format::RGB32:
			case Format::RGBA32:
				dstPixel.SetValue(channel,srcPixel.GetFloatValue(channel));
				break;
			}
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
	case Format::RGB16:
	case Format::RGB32:
		Convert(Format::RGB8);
		return;
	case Format::RGBA16:
	case Format::RGBA32:
		Convert(Format::RGBA8);
		return;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
}
void uimg::ImageBuffer::ToHDR()
{
	switch(m_format)
	{
	case Format::RGB8:
	case Format::RGB32:
		Convert(Format::RGB16);
		return;
	case Format::RGBA8:
	case Format::RGBA32:
		Convert(Format::RGBA16);
		return;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
}
void uimg::ImageBuffer::ToFloat()
{
	switch(m_format)
	{
	case Format::RGB8:
	case Format::RGB16:
		Convert(Format::RGB32);
		return;
	case Format::RGBA8:
	case Format::RGBA16:
		Convert(Format::RGBA32);
		return;
	}
	static_assert(umath::to_integral(Format::Count) == 7u);
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
		auto value = ToHDRValue((alpha /static_cast<float>(std::numeric_limits<LDRValue>::max())) *std::numeric_limits<float>::max());
		for(auto &px : *this)
			px.SetValue(Channel::Alpha,value);
		return;
	}
	if(IsFloatFormat())
	{
		auto value = (alpha /static_cast<float>(std::numeric_limits<LDRValue>::max())) *std::numeric_limits<float>::max();
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
void uimg::ImageBuffer::Resize(Size width,Size height)
{
	// TODO
	throw std::runtime_error{"Resizing images not yet implemented!"};
}
#pragma optimize("",on)

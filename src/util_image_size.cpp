/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image.hpp"
#include <fsys/filesystem.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
#include <array>
#include <cstring>
#ifdef __linux__
#include <arpa/inet.h> // Required for ntohl
#endif

#pragma comment(lib, "Ws2_32.lib") // Required for ntohl

namespace uimg {
	bool read_ktx_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight);
	bool read_dds_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight);
	bool read_png_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight);
	bool read_vtf_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight);
	bool read_tga_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight);
};

static bool compare_header(const char *src, const char *dst, size_t len) { return (strncmp(src, dst, len) == 0) ? true : false; }

bool uimg::read_ktx_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight)
{
	// See https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/#2.10
	std::array<char, 12> identifier;
	f->Read(identifier.data(), identifier.size());
	const std::array<char, 12> cmpIdentifier = {'\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n'};
	if(compare_header(identifier.data(), cmpIdentifier.data(), identifier.size()) == false)
		return false;                          // Incorrect header
	f->Seek(f->Tell() + sizeof(uint32_t) * 6); // Skip all header information which we don't need
	pixelWidth = f->Read<uint32_t>();
	pixelHeight = f->Read<uint32_t>();
	return true;
}

bool uimg::read_dds_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight)
{
	// See https://msdn.microsoft.com/de-de/library/windows/desktop/bb943991(v=vs.85).aspx#File_Layout1
	std::array<char, 4> dwMagic;
	f->Read(dwMagic.data(), dwMagic.size());
	const std::array<char, 4> cmpMagic = {'D', 'D', 'S', ' '};
	if(compare_header(dwMagic.data(), cmpMagic.data(), dwMagic.size()) == false)
		return false;                          // Incorrect header
	f->Seek(f->Tell() + sizeof(uint32_t) * 2); // Skip all header information which we don't need
	pixelHeight = f->Read<uint32_t>();
	pixelWidth = f->Read<uint32_t>();
	return true;
}

bool uimg::read_png_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight)
{
	std::array<char, 8> dwMagic;
	f->Read(dwMagic.data(), dwMagic.size());
	const std::array<char, 8> cmpMagic = {'\211', 'P', 'N', 'G', '\r', '\n', '\032', '\n'};
	if(compare_header(dwMagic.data(), cmpMagic.data(), dwMagic.size()) == false)
		return false;                          // Incorrect header
	f->Seek(f->Tell() + sizeof(uint32_t) * 1); // Skip all header information which we don't need
	std::array<char, 4> chunkType;
	f->Read(chunkType.data(), chunkType.size());
	const std::array<char, 4> chunkIHDR = {'I', 'H', 'D', 'R'};
	if(compare_header(chunkType.data(), chunkIHDR.data(), chunkType.size()) == false)
		return false;
	pixelWidth = ntohl(f->Read<uint32_t>());
	pixelHeight = ntohl(f->Read<uint32_t>());
	return true;
}

bool uimg::read_vtf_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight)
{
	std::array<char, 4> signature;
	f->Read(signature.data(), signature.size());
	const std::array<char, 4> cmpSignature = {'V', 'T', 'F', '\0'};
	if(compare_header(signature.data(), cmpSignature.data(), signature.size()) == false)
		return false; // Incorrect header
	f->Seek(f->Tell() + sizeof(uint32_t) * 3);
	pixelWidth = static_cast<uint32_t>(f->Read<uint16_t>());
	pixelHeight = static_cast<uint32_t>(f->Read<uint16_t>());
	return true;
}

bool uimg::read_tga_size(VFilePtr &f, uint32_t &pixelWidth, uint32_t &pixelHeight)
{
	f->Seek(f->Tell() + sizeof(int8_t) * 4 + sizeof(int16_t) * 4);
	pixelWidth = static_cast<uint32_t>(f->Read<int16_t>());
	pixelHeight = static_cast<uint32_t>(f->Read<int16_t>());
	return true;
}

bool uimg::read_image_size(const std::string &file, uint32_t &pixelWidth, uint32_t &pixelHeight)
{
	std::string ext;
	if(ufile::get_extension(file, &ext) == false)
		return false; // TODO: Search available extensions? (Which order?)
	ustring::to_lower(ext);
	if(ext == "ktx") {
		auto f = FileManager::OpenFile(file.c_str(), "rb");
		return (f != nullptr && read_ktx_size(f, pixelWidth, pixelHeight) == true) ? true : false;
	}
	else if(ext == "dds") {
		auto f = FileManager::OpenFile(file.c_str(), "rb");
		return (f != nullptr && read_dds_size(f, pixelWidth, pixelHeight) == true) ? true : false;
	}
	else if(ext == "png") {
		auto f = FileManager::OpenFile(file.c_str(), "rb");
		return (f != nullptr && read_png_size(f, pixelWidth, pixelHeight) == true) ? true : false;
	}
	else if(ext == "vtf") {
		auto f = FileManager::OpenFile(file.c_str(), "rb");
		return (f != nullptr && read_vtf_size(f, pixelWidth, pixelHeight) == true) ? true : false;
	}
	else if(ext == "tga") {
		auto f = FileManager::OpenFile(file.c_str(), "rb");
		return (f != nullptr && read_tga_size(f, pixelWidth, pixelHeight) == true) ? true : false;
	}
	return false;
}

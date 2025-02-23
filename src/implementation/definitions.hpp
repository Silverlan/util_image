/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_IMAGE_DEFINITIONS_HPP__
#define __UTIL_IMAGE_DEFINITIONS_HPP__

#ifdef UIMG_STATIC
#define DLLUIMG
#elif UIMG_DLL
#ifdef __linux__
#define DLLUIMG __attribute__((visibility("default")))
#else
#define DLLUIMG __declspec(dllexport)
#endif
#else
#ifdef __linux__
#define DLLUIMG
#else
#define DLLUIMG __declspec(dllimport)
#endif
#endif

#ifdef UIMG_ENABLE_NVTT

#define TEX_COMPRESSION_LIBRARY_NVTT 0
#define TEX_COMPRESSION_LIBRARY_COMPRESSONATOR 1

#ifndef TEX_COMPRESSION_LIBRARY

#define TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_NVTT

#endif

#endif

#endif

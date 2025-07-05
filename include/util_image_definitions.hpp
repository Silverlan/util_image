// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

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

#endif

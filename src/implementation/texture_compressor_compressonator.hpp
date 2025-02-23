/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TEXTURE_COMPRESSOR_COMPRESSONATOR_HPP__
#define __TEXTURE_COMPRESSOR_COMPRESSONATOR_HPP__

#include "definitions.hpp"
#if UIMG_ENABLE_NVTT && TEX_COMPRESSION_LIBRARY == TEX_COMPRESSION_LIBRARY_COMPRESSONATOR

#include <compressonator.h>
#include <common.h>
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/scope_guard.h>
#ifdef _WIN32
#include <fileapi.h>

#include <sharedutils/util_string.h>
#include <sharedutils/util.h>
#include <filesystem>
#endif

#endif

#endif

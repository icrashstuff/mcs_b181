/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef TETRA_UTIL_STBI_H
#define TETRA_UTIL_STBI_H
#include "physfs/physfs.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

////////////////////////////////////
//
// 8-bits-per-channel interface
//

STBIDEF stbi_uc* stbi_physfs_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
STBIDEF stbi_uc* stbi_physfs_load_from_file(PHYSFS_File* fd, int* x, int* y, int* channels_in_file, int desired_channels);

////////////////////////////////////
//
// 16-bits-per-channel interface
//

STBIDEF stbi_us* stbi_physfs_load_16(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
STBIDEF stbi_us* stbi_physfs_load_from_file_16(PHYSFS_File* f, int* x, int* y, int* channels_in_file, int desired_channels);

////////////////////////////////////
//
// float-per-channel interface
//
#ifndef STBI_NO_LINEAR
STBIDEF float* stbi_physfs_loadf(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
STBIDEF float* stbi_physfs_loadf_from_file(PHYSFS_File* f, int* x, int* y, int* channels_in_file, int desired_channels);
#endif

STBIDEF int stbi_physfs_is_hdr(char const* filename);
STBIDEF int stbi_physfs_is_hdr_from_file(PHYSFS_File* f);

STBIDEF int stbi_physfs_info(char const* filename, int* x, int* y, int* comp);
STBIDEF int stbi_physfs_info_from_file(PHYSFS_File* f, int* x, int* y, int* comp);
STBIDEF int stbi_physfs_is_16_bit(char const* filename);
STBIDEF int stbi_physfs_is_16_bit_from_file(PHYSFS_File* f);

////////////////////////////////////
//
// STBIW interface
//
STBIWDEF int stbi_physfs_write_png(char const* filename, int w, int h, int channels, const void* data, int stride_in_bytes);
STBIWDEF int stbi_physfs_write_bmp(char const* filename, int w, int h, int channels, const void* data);
STBIWDEF int stbi_physfs_write_tga(char const* filename, int w, int h, int channels, const void* data);
STBIWDEF int stbi_physfs_write_hdr(char const* filename, int w, int h, int channels, const float* data);
STBIWDEF int stbi_physfs_write_jpg(char const* filename, int x, int y, int channels, const void* data, int quality);

#endif

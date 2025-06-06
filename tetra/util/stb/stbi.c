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
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "../stbi.h"

#ifdef STB_IMAGE_IMPLEMENTATION

static int stbi_io_callback_physfs_read(void* user, char* data, int size) { return PHYSFS_readBytes((PHYSFS_File*)user, data, size); }

static void stbi_io_callback_physfs_skip(void* user, int n)
{
    PHYSFS_File* fd = (PHYSFS_File*)user;
    PHYSFS_seek(fd, PHYSFS_tell(fd) + n);
}

static int stbi_io_callback_physfs_eof(void* user) { return PHYSFS_eof((PHYSFS_File*)user); }

static const stbi_io_callbacks stbi_io_callbacks_physfs = {
    .read = stbi_io_callback_physfs_read,
    .skip = stbi_io_callback_physfs_skip,
    .eof = stbi_io_callback_physfs_eof,
};

////////////////////////////////////
//
// 8-bits-per-channel interface
//

STBIDEF stbi_uc* stbi_physfs_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels)
{
    PHYSFS_File* fd = PHYSFS_openRead(filename);
    if (!fd)
        return stbi__errpuc("can't PHYSFS_openRead", "Unable to open file");
    stbi_uc* result = stbi_physfs_load_from_file(fd, x, y, channels_in_file, desired_channels);
    PHYSFS_close(fd);
    return result;
}
STBIDEF stbi_uc* stbi_physfs_load_from_file(PHYSFS_File* f, int* x, int* y, int* channels_in_file, int desired_channels)
{
    return stbi_load_from_callbacks(&stbi_io_callbacks_physfs, f, x, y, channels_in_file, desired_channels);
}

////////////////////////////////////
//
// 16-bits-per-channel interface
//

STBIDEF stbi_us* stbi_physfs_load_16(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels)
{
    PHYSFS_File* fd = PHYSFS_openRead(filename);
    if (!fd)
        return (stbi_us*)stbi__errpuc("can't PHYSFS_openRead", "Unable to open file");
    stbi_us* result = stbi_physfs_load_from_file_16(fd, x, y, channels_in_file, desired_channels);
    PHYSFS_close(fd);
    return result;
}
STBIDEF stbi_us* stbi_physfs_load_from_file_16(PHYSFS_File* f, int* x, int* y, int* channels_in_file, int desired_channels)
{
    return stbi_load_16_from_callbacks(&stbi_io_callbacks_physfs, f, x, y, channels_in_file, desired_channels);
}

////////////////////////////////////
//
// float-per-channel interface
//
#ifndef STBI_NO_LINEAR
STBIDEF float* stbi_physfs_loadf(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels)
{
    PHYSFS_File* fd = PHYSFS_openRead(filename);
    if (!fd)
        return (float*)stbi__errpuc("can't PHYSFS_openRead", "Unable to open file");
    float* result = stbi_physfs_loadf_from_file(fd, x, y, channels_in_file, desired_channels);
    PHYSFS_close(fd);
    return result;
}
STBIDEF float* stbi_physfs_loadf_from_file(PHYSFS_File* f, int* x, int* y, int* channels_in_file, int desired_channels)
{
    return stbi_loadf_from_callbacks(&stbi_io_callbacks_physfs, f, x, y, channels_in_file, desired_channels);
}
#endif

STBIDEF int stbi_physfs_is_hdr(char const* filename)
{
    PHYSFS_File* fd = PHYSFS_openRead(filename);
    if (!fd)
    {
        stbi__errpuc("can't PHYSFS_openRead", "Unable to open file");
        return 0;
    }
    int result = stbi_physfs_is_hdr_from_file(fd);
    PHYSFS_close(fd);
    return result;
}
STBIDEF int stbi_physfs_is_hdr_from_file(PHYSFS_File* f) { return stbi_is_hdr_from_callbacks(&stbi_io_callbacks_physfs, f); }

STBIDEF int stbi_physfs_info(char const* filename, int* x, int* y, int* comp)
{
    PHYSFS_File* fd = PHYSFS_openRead(filename);
    if (!fd)
    {
        stbi__errpuc("can't PHYSFS_openRead", "Unable to open file");
        return 0;
    }
    int result = stbi_physfs_info_from_file(fd, x, y, comp);
    PHYSFS_close(fd);
    return result;
}
STBIDEF int stbi_physfs_info_from_file(PHYSFS_File* f, int* x, int* y, int* comp) { return stbi_info_from_callbacks(&stbi_io_callbacks_physfs, f, x, y, comp); }

STBIDEF int stbi_physfs_is_16_bit(char const* filename)
{
    PHYSFS_File* fd = PHYSFS_openRead(filename);
    if (!fd)
    {
        stbi__errpuc("can't PHYSFS_openRead", "Unable to open file");
        return 0;
    }
    int result = stbi_physfs_is_16_bit_from_file(fd);
    PHYSFS_close(fd);
    return result;
}
STBIDEF int stbi_physfs_is_16_bit_from_file(PHYSFS_File* f) { return stbi_is_16_bit_from_callbacks(&stbi_io_callbacks_physfs, f); }

#endif // #ifdef STB_IMAGE_IMPLEMENTATION

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

static void stbi_physfs_write_func(void* context, void* data, int size) { PHYSFS_writeBytes(context, data, size); }

int stbi_physfs_write_png(const char* filename, int w, int h, int channels, const void* data, int stride_in_bytes)
{
    PHYSFS_File* fd = PHYSFS_openWrite(filename);
    int result = 0;
    if (!fd)
        return result;
    result = stbi_write_png_to_func(stbi_physfs_write_func, fd, w, h, channels, data, stride_in_bytes);
    PHYSFS_close(fd);
    return result;
}
STBIWDEF int stbi_physfs_write_bmp(char const* filename, int w, int h, int channels, const void* data)
{
    PHYSFS_File* fd = PHYSFS_openWrite(filename);
    int result = 0;
    if (!fd)
        return result;
    result = stbi_write_bmp_to_func(stbi_physfs_write_func, fd, w, h, channels, data);
    PHYSFS_close(fd);
    return result;
}
STBIWDEF int stbi_physfs_write_tga(char const* filename, int w, int h, int channels, const void* data)
{
    PHYSFS_File* fd = PHYSFS_openWrite(filename);
    int result = 0;
    if (!fd)
        return result;
    result = stbi_write_tga_to_func(stbi_physfs_write_func, fd, w, h, channels, data);
    PHYSFS_close(fd);
    return result;
}
STBIWDEF int stbi_physfs_write_hdr(char const* filename, int w, int h, int channels, const float* data)
{
    PHYSFS_File* fd = PHYSFS_openWrite(filename);
    int result = 0;
    if (!fd)
        return result;
    result = stbi_write_hdr_to_func(stbi_physfs_write_func, fd, w, h, channels, data);
    PHYSFS_close(fd);
    return result;
}
STBIWDEF int stbi_physfs_write_jpg(char const* filename, int x, int y, int channels, const void* data, int quality)
{
    PHYSFS_File* fd = PHYSFS_openWrite(filename);
    int result = 0;
    if (!fd)
        return result;
    result = stbi_write_jpg_to_func(stbi_physfs_write_func, fd, x, y, channels, data, quality);
    PHYSFS_close(fd);
    return result;
}

#endif // #ifdef STB_IMAGE_WRITE_IMPLEMENTATION

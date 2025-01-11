/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
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
#ifndef TETRA_CLIENT_CHUNK_CUBIC_H
#define TETRA_CLIENT_CHUNK_CUBIC_H

#include <SDL3/SDL_opengl.h>

#include "shared/misc.h"

/* TODO-OPT: Spin this out to shared? */
struct chunk_cubic_t
{
    bool dirty = 1;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    /**
     * One of GL_UNSIGNED_INT, GL_UNSIGNED_SHORT, GL_UNSIGNED_BYTE, or GL_NONE
     */
    GLenum index_type = GL_NONE;
    size_t index_count = 0;
    size_t index_offset_water = 0;
    size_t index_count_water = 0;

    int chunk_x = 0;
    int chunk_y = 0;
    int chunk_z = 0;

    std::vector<Uint8> data;

    chunk_cubic_t() { data.resize(SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * 5 / 2, 0); }

    ~chunk_cubic_t() { free_gl(); }

    void free_gl()
    {
        vao = 0;
        vbo = 0;
        ebo = 0;
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    }

    inline Uint8 get_type(int x, int y, int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z));

        return data[index];
    }

    inline Uint8 get_type_fallback(int x, int y, int z, Uint8 fallback)
    {
        if (x < 0 || y < 0 || z < 0 || x >= SUBCHUNK_SIZE_X || y >= SUBCHUNK_SIZE_Y || z >= SUBCHUNK_SIZE_Z)
            return fallback;

        int index = y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z));

        return data[index];
    }

    inline void set_type(int x, int y, int z, Uint8 type)
    {
        dirty = true;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z));

        if (type <= BLOCK_ID_MAX)
            data[index] = type;
        else
            data[index] = 0;
    }

    inline Uint8 get_metadata(int x, int y, int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = (y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * SUBCHUNK_SIZE_X * 2;

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    inline void set_metadata(int x, int y, int z, Uint8 metadata)
    {
        dirty = true;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = (y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * SUBCHUNK_SIZE_X * 2;

        if (index % 2 == 1)
            data[index / 2] = ((metadata & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (metadata & 0x0F) | (data[index / 2] & 0xF0);
    }

    inline Uint8 get_light_block(int x, int y, int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = (y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * SUBCHUNK_SIZE_X * 3;

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    inline void set_light_block(int x, int y, int z, Uint8 level)
    {
        dirty = true;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = (y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * SUBCHUNK_SIZE_X * 3;

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }

    inline Uint8 get_light_sky(int x, int y, int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = (y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * SUBCHUNK_SIZE_X * 4;

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    inline void set_light_sky(int x, int y, int z, Uint8 level)
    {
        dirty = true;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        int index = (y + (z * (SUBCHUNK_SIZE_Y)) + (x * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * SUBCHUNK_SIZE_X * 4;

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }
};

#endif

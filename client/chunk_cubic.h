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
#ifndef MCS_B181_CLIENT_CHUNK_CUBIC_H
#define MCS_B181_CLIENT_CHUNK_CUBIC_H

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include "shared/misc.h"

/* Like SDL_FORCE_INLINE but without static */
#ifndef CHUNK_CUBIC_INLINE
#ifdef _MSC_VER
#define CHUNK_CUBIC_INLINE __forceinline
#elif ((defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__))
#define CHUNK_CUBIC_INLINE __attribute__((always_inline)) __inline__
#else
#define CHUNK_CUBIC_INLINE SDL_INLINE
#endif
#endif /* CHUNK_CUBIC_INLINE not defined */

#define SUBCHUNK_INDEX_OFFSET(OFF_MULT) (SUBCHUNK_SIZE_VOLUME * OFF_MULT)
#define SUBCHUNK_INDEX(X, Y, Z, OFF_MULT) (((Y) + ((Z) * (SUBCHUNK_SIZE_Y)) + ((X) * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z))) + SUBCHUNK_INDEX_OFFSET(OFF_MULT))

/* TODO-OPT: Spin this out to shared? */
struct chunk_cubic_t
{
    enum dirty_level_t
    {
        DIRTY_LEVEL_NONE,
        /** Internal use only */
        DIRTY_LEVEL_MESH,
        /** Internal use only */
        DIRTY_LEVEL_LIGHT_PASS_EXT_1,
        /** Internal use only */
        DIRTY_LEVEL_LIGHT_PASS_EXT_0,
        /** Set to this if in doubt */
        DIRTY_LEVEL_LIGHT_PASS_INTERNAL,
    };
    dirty_level_t dirty_level = DIRTY_LEVEL_LIGHT_PASS_INTERNAL;

    /**
     * To be filled in by a culling pass
     */
    bool visible = 1;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo_translucent = 0;
    /**
     * One of GL_UNSIGNED_INT, GL_UNSIGNED_SHORT, GL_UNSIGNED_BYTE, or GL_NONE
     */
    GLenum index_type = GL_NONE;
    size_t index_count = 0;

    /**
     * One of GL_UNSIGNED_INT, GL_UNSIGNED_SHORT, GL_UNSIGNED_BYTE, or GL_NONE
     */
    GLenum index_type_translucent = GL_NONE;
    size_t index_count_translucent = 0;

    const Uint64 time_creation = SDL_GetTicks();

    glm::ivec3 pos = { 0, 0, 0 };

    std::vector<Uint8> data_buf;
    Uint8* data;

    chunk_cubic_t()
    {
        data_buf.resize(SUBCHUNK_SIZE_VOLUME * 5 / 2, 0);
        data = data_buf.data();
    }

    ~chunk_cubic_t() { free_gl(); }

    void free_gl()
    {
        vao = 0;
        vbo = 0;
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    }

    CHUNK_CUBIC_INLINE Uint8 get_type(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 0);

        return data[index];
    }

    CHUNK_CUBIC_INLINE Uint8 get_type_fallback(const int x, const int y, const int z, const Uint8 fallback)
    {
        if (x < 0 || y < 0 || z < 0 || x >= SUBCHUNK_SIZE_X || y >= SUBCHUNK_SIZE_Y || z >= SUBCHUNK_SIZE_Z)
            return fallback;

        const int index = SUBCHUNK_INDEX(x, y, z, 0);

        return data[index];
    }

    CHUNK_CUBIC_INLINE void set_type(const int x, const int y, const int z, const Uint8 type)
    {
        dirty_level = DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 0);

        if (type <= BLOCK_ID_MAX)
            data[index] = type;
        else
            data[index] = 0;
    }

    CHUNK_CUBIC_INLINE Uint8 get_metadata(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 2);

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    CHUNK_CUBIC_INLINE void set_metadata(const int x, const int y, const int z, const Uint8 metadata)
    {
        dirty_level = DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(metadata < 16);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 2);

        if (index % 2 == 1)
            data[index / 2] = ((metadata & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (metadata & 0x0F) | (data[index / 2] & 0xF0);
    }

    CHUNK_CUBIC_INLINE Uint8 get_light_block(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 3);

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    CHUNK_CUBIC_INLINE void clear_light_block(const Uint8 clear_level = 0)
    {
        assert(clear_level <= 0x0F);

        Uint8* data_ptr = data + SUBCHUNK_INDEX_OFFSET(3 / 2);
        SDL_memset(data_ptr, (clear_level & 0x0F) | ((clear_level & 0x0F) << 4), SUBCHUNK_SIZE_VOLUME / 2);
    }

    CHUNK_CUBIC_INLINE void set_light_block(const int x, const int y, const int z, const Uint8 level)
    {
        dirty_level = DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(level < 16);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 3);

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }

    CHUNK_CUBIC_INLINE Uint8 get_light_sky(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 4);

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    CHUNK_CUBIC_INLINE void clear_light_sky(const Uint8 clear_level = 0)
    {
        assert(clear_level <= 0x0F);

        Uint8* data_ptr = data + SUBCHUNK_INDEX_OFFSET(4 / 2);
        SDL_memset(data_ptr, (clear_level & 0x0F) | ((clear_level & 0x0F) << 4), SUBCHUNK_SIZE_VOLUME / 2);
    }

    CHUNK_CUBIC_INLINE void set_light_sky(const int x, const int y, const int z, const Uint8 level)
    {
        dirty_level = DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(level < 16);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z, 4);

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }
};

#endif

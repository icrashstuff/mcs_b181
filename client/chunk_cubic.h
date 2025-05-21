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

#include "gpu/subdiv_buffer.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include "shared/ids.h"
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

#endif /* #ifndef CHUNK_CUBIC_INLINE */

#define SUBCHUNK_INDEX(X, Y, Z) ((Y) + ((Z) * (SUBCHUNK_SIZE_Y)) + ((X) * (SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z)))

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

    struct
    {
        /**
         * To be set to 1 by a renderer hint pass
         * To be set to 0 by a block or metadata update
         */
        bool hints_set : 1;

        /**
         * Chunk is uniformly made of air
         *
         * If this is true then remapping DIRTY_LEVEL_MESH -> DIRTY_LEVEL_NONE is possible
         *
         * To be filled in by a renderer hint pass
         */
        bool uniform_air : 1;

        /**
         * Sides of chunk are fully opaque
         *
         * If this is true then remapping [DIRTY_LEVEL_LIGHT_PASS_EXT_0, DIRTY_LEVEL_LIGHT_PASS_EXT_2] -> DIRTY_LEVEL_MESH is possible
         *
         * To be filled in by a renderer hint pass
         */
        bool opaque_sides : 1;

        /**
         * Chunk is fully opaque
         *
         * If this is true then remapping [DIRTY_LEVEL_LIGHT_PASS_EXT_0, DIRTY_LEVEL_LIGHT_PASS_EXT_2] -> DIRTY_LEVEL_MESH is possible
         *
         * If this is true and the surrounding +XYZ, -XYZ chunks have renderer_hint_opaque_sides or renderer_hint_uniform_opaque then remapping
         * [DIRTY_LEVEL_LIGHT_PASS_EXT_0, DIRTY_LEVEL_MESH] -> DIRTY_LEVEL_NONE is possible
         *
         * To be filled in by a renderer hint pass
         */
        bool uniform_opaque : 1;

        /** Face of blocks where (x = SUBCHUNK_SIZE_X - 1) is opaque */
        bool opaque_face_pos_x : 1;

        /** Face of blocks where (y = SUBCHUNK_SIZE_Y - 1) is opaque */
        bool opaque_face_pos_y : 1;

        /** Face of blocks where (z = SUBCHUNK_SIZE_Z - 1) is opaque */
        bool opaque_face_pos_z : 1;

        /** Face of blocks where (x = 0) is opaque */
        bool opaque_face_neg_x : 1;

        /** Face of blocks where (y = 0) is opaque */
        bool opaque_face_neg_y : 1;

        /** Face of blocks where (z = 0) is opaque */
        bool opaque_face_neg_z : 1;
    } renderer_hints = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    struct
    {
        chunk_cubic_t* pos_x = NULL;
        chunk_cubic_t* pos_y = NULL;
        chunk_cubic_t* pos_z = NULL;
        chunk_cubic_t* neg_x = NULL;
        chunk_cubic_t* neg_y = NULL;
        chunk_cubic_t* neg_z = NULL;
    } neighbors;

    gpu::subdiv_buffer_allocation_t* mesh_handle = nullptr;

    Uint64 last_mesh_update_time = 0;

    Uint32 quad_count = 0;
    Uint32 quad_count_overlay = 0;
    Uint32 quad_count_translucent = 0;

    const Uint64 time_creation = SDL_GetTicks();

    glm::ivec3 pos = { 0, 0, 0 };

    /* The 128 byte alignment is to assist auto-vectorization */
    alignas(128) Uint8 data_block[SUBCHUNK_SIZE_VOLUME];
    alignas(128) Uint8 data_light_block[SUBCHUNK_SIZE_VOLUME / 2];
    alignas(128) Uint8 data_light_sky[SUBCHUNK_SIZE_VOLUME / 2];
    alignas(128) Uint8 data_metadata[SUBCHUNK_SIZE_VOLUME / 2];

    chunk_cubic_t()
    {
        memset(data_block, 0, sizeof(data_block));
        memset(data_light_block, 0, sizeof(data_light_block));
        memset(data_light_sky, 0, sizeof(data_light_sky));
        memset(data_metadata, 0, sizeof(data_metadata));
    }

    ~chunk_cubic_t() { free_renderer_resources(); }

    /**
     * Update renderer hints
     *
     * This must be called before any of the lighting pass functions
     */
    void update_renderer_hints();

    /**
     * Sets block light levels according to block type
     *
     * Pseudo-code for block light passes (where chunks is a container with all the chunks in the level):
     * 1) for(c: chunks) c->clear_light_block()
     * 2) for(c: chunks) c->light_pass_block_set_according_to_type()
     * 3) for(c: chunks) c->light_pass_block_propagate_internals()
     * 4) for(i in range(4))
     * 4.1) for(c: chunks) c->light_pass_block_grab_from_neighbors()
     * 4.2) for(c: chunks) c->light_pass_block_propagate_internals()
     */
    void light_pass_block_setup();

    /**
     * Internally propagate block light
     */
    void light_pass_block_propagate_internals();

    /**
     * Grab block light from neighbors
     */
    void light_pass_block_grab_from_neighbors();

    /**
     * Grab sky light from neighbors
     *
     * Pseudo-code for block light passes (where chunks is a container with all the chunks in the level):
     * 1) for(c: chunks) c->clear_light_sky()
     * 2) for(c: chunks) c->light_pass_sky_propagate_internals()
     * 3) for(i in range(4))
     * 3.1) for(c: chunks) c->light_pass_sky_grab_from_neighbors()
     * 3.2) for(c: chunks) c->light_pass_sky_propagate_internals()
     */

    void light_pass_sky_grab_from_neighbors();

    /**
     * Internally propagate sky light
     */
    void light_pass_sky_propagate_internals();

    /**
     * Check if light can propagate from this chunk to others
     *
     * NOTE: Something to keep in mind is that if this function returns true,
     * that does not mean that the neighbors don't need their meshes updated
     * (TLDR: Be cautious when using this for skipping the mesh stage)
     *
     * @returns True if light can propagate (out of or into) the chunk, False otherwise
     */
    CHUNK_CUBIC_INLINE bool can_light_leave() const
    {
        bool ret = 1;

        /* Pseudo-code:
         * 1) If face opaque then visible = 0, else continue
         * 2) If not neighbor then visible = 1, if neighbor then continue
         * 3) If neighbor then visible = (bordering face from neighbor is transparent)
         */
        /*                Check for transparency on face   || (Check for neighbor || Check for transparency on neighbors face ) */
        bool vis_pos_x = (!renderer_hints.opaque_face_pos_x || (!neighbors.pos_x || !neighbors.pos_x->renderer_hints.opaque_face_neg_x));
        bool vis_pos_y = (!renderer_hints.opaque_face_pos_y || (!neighbors.pos_y || !neighbors.pos_y->renderer_hints.opaque_face_neg_y));
        bool vis_pos_z = (!renderer_hints.opaque_face_pos_z || (!neighbors.pos_z || !neighbors.pos_z->renderer_hints.opaque_face_neg_z));
        bool vis_neg_x = (!renderer_hints.opaque_face_neg_x || (!neighbors.neg_x || !neighbors.neg_x->renderer_hints.opaque_face_pos_x));
        bool vis_neg_y = (!renderer_hints.opaque_face_neg_y || (!neighbors.neg_y || !neighbors.neg_y->renderer_hints.opaque_face_pos_y));
        bool vis_neg_z = (!renderer_hints.opaque_face_neg_z || (!neighbors.neg_z || !neighbors.neg_z->renderer_hints.opaque_face_pos_z));

        if ((vis_pos_x | vis_pos_y | vis_pos_z | vis_neg_x | vis_neg_y | vis_neg_z) == 0)
            ret = 0;

        return ret;
    }

    void free_renderer_resources();

    /**
     * Finds a chunk by recursively searching from the origin
     *
     * @param origin Chunk to start from
     * @param target Coordinates of target chunk
     *
     * @returns Target chunk if found, NULL otherwise
     */
    static inline chunk_cubic_t* find_chunk(chunk_cubic_t* const origin, const decltype(chunk_cubic_t::pos) target)
    {
        if (!origin)
            return NULL;
        const decltype(chunk_cubic_t::pos) diff = target - origin->pos;
        chunk_cubic_t* t = NULL;

        if (diff == decltype(chunk_cubic_t::pos)(0))
            return origin;

        if (origin->neighbors.pos_x && diff.x > 0 && (t = find_chunk(origin->neighbors.pos_x, target)) && t->pos == target)
            return t;
        if (origin->neighbors.pos_y && diff.y > 0 && (t = find_chunk(origin->neighbors.pos_y, target)) && t->pos == target)
            return t;
        if (origin->neighbors.pos_z && diff.z > 0 && (t = find_chunk(origin->neighbors.pos_z, target)) && t->pos == target)
            return t;
        if (origin->neighbors.neg_x && diff.x < 0 && (t = find_chunk(origin->neighbors.neg_x, target)) && t->pos == target)
            return t;
        if (origin->neighbors.neg_y && diff.y < 0 && (t = find_chunk(origin->neighbors.neg_y, target)) && t->pos == target)
            return t;
        if (origin->neighbors.neg_z && diff.z < 0 && (t = find_chunk(origin->neighbors.neg_z, target)) && t->pos == target)
            return t;

        return NULL;
    }

    CHUNK_CUBIC_INLINE Uint8 get_type(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z);

        return data_block[index];
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

        const int index = SUBCHUNK_INDEX(x, y, z);

        renderer_hints.hints_set = 0;

        /* We don't assert because this function may process uninitialized data */
        if (type < BLOCK_ID_NUM_USED)
            data_block[index] = type;
        else
            data_block[index] = 0;
    }

    CHUNK_CUBIC_INLINE Uint8 get_metadata(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z);

        if (index % 2 == 1)
            return (data_metadata[index / 2] >> 4) & 0x0F;
        else
            return data_metadata[index / 2] & 0x0F;
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

        const int index = SUBCHUNK_INDEX(x, y, z);

        renderer_hints.hints_set = 0;

        if (index % 2 == 1)
            data_metadata[index / 2] = ((metadata & 0x0F) << 4) | (data_metadata[index / 2] & 0x0F);
        else
            data_metadata[index / 2] = (metadata & 0x0F) | (data_metadata[index / 2] & 0xF0);
    }

    CHUNK_CUBIC_INLINE Uint8 get_light_block(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z);

        if (index % 2 == 1)
            return (data_light_block[index / 2] >> 4) & 0x0F;
        else
            return data_light_block[index / 2] & 0x0F;
    }

    CHUNK_CUBIC_INLINE void clear_light_block(const Uint8 clear_level = 0)
    {
        assert(clear_level <= 0x0F);

        SDL_memset(data_light_block, (clear_level & 0x0F) | ((clear_level & 0x0F) << 4), sizeof(data_light_block));
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

        const int index = SUBCHUNK_INDEX(x, y, z);

        if (index % 2 == 1)
            data_light_block[index / 2] = ((level & 0x0F) << 4) | (data_light_block[index / 2] & 0x0F);
        else
            data_light_block[index / 2] = (level & 0x0F) | (data_light_block[index / 2] & 0xF0);
    }

    CHUNK_CUBIC_INLINE Uint8 get_light_sky(const int x, const int y, const int z)
    {
        assert(x >= 0);
        assert(y >= 0);
        assert(z >= 0);
        assert(x < SUBCHUNK_SIZE_X);
        assert(y < SUBCHUNK_SIZE_Y);
        assert(z < SUBCHUNK_SIZE_Z);

        const int index = SUBCHUNK_INDEX(x, y, z);

        if (index % 2 == 1)
            return (data_light_sky[index / 2] >> 4) & 0x0F;
        else
            return data_light_sky[index / 2] & 0x0F;
    }

    CHUNK_CUBIC_INLINE void clear_light_sky(const Uint8 clear_level = 0)
    {
        assert(clear_level <= 0x0F);

        SDL_memset(data_light_sky, (clear_level & 0x0F) | ((clear_level & 0x0F) << 4), sizeof(data_light_sky));
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

        const int index = SUBCHUNK_INDEX(x, y, z);

        if (index % 2 == 1)
            data_light_sky[index / 2] = ((level & 0x0F) << 4) | (data_light_sky[index / 2] & 0x0F);
        else
            data_light_sky[index / 2] = (level & 0x0F) | (data_light_sky[index / 2] & 0xF0);
    }
};

#endif

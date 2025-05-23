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
#include "chunk_cubic.h"
#include "state.h"

#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(X) (int(SDL_arraysize(X)))
#endif

void chunk_cubic_t::free_renderer_resources(const chunk_cubic_t::dirty_level_t new_dirty_level)
{
    quad_count = 0;
    quad_count_overlay = 0;
    quad_count_translucent = 0;

    if (mesh_handle)
    {
        mesh_handle->release();
        delete mesh_handle;
        mesh_handle = nullptr;
    }

    dirty_level = new_dirty_level;
}

/* There is no special reason that this function uses |= instead of +=, other than if one happens to be faster (which is unlikely) it would probably be |= */
void chunk_cubic_t::update_renderer_hints()
{
    bool is_trans[256];

    for (size_t i = 0; i < SDL_arraysize(is_trans); i++)
        is_trans[i] = mc_id::is_transparent(i);

    memset(&renderer_hints, 0, sizeof(renderer_hints));

    /* Very important to set this *AFTER* setting all the fields to zero, otherwise you will be confused where your frame rate went (*facepalm*) */
    renderer_hints.hints_set = 1;

    static_assert(SDL_arraysize(data_block) == SUBCHUNK_SIZE_VOLUME);
    static_assert(SDL_arraysize(data_block) % sizeof(Uint64) == 0);

    /* Check for a uniform chunk of air (This loop will likely be auto-vectorized) */
    Uint32 total_block = 0;
    for (int i = 0; i < SUBCHUNK_SIZE_VOLUME; i++)
        total_block |= data_block[i];
    if (total_block == 0)
    {
        renderer_hints.uniform_air = 1;
        renderer_hints.uniform_opaque = 0;
        renderer_hints.opaque_sides = 0;
        renderer_hints.opaque_face_pos_x = 0;
        renderer_hints.opaque_face_pos_y = 0;
        renderer_hints.opaque_face_pos_z = 0;
        renderer_hints.opaque_face_neg_x = 0;
        renderer_hints.opaque_face_neg_y = 0;
        renderer_hints.opaque_face_neg_z = 0;
        return;
    }

    /* Check for a uniform chunk of opaque to light blocks */
    Uint32 total_trans = 0;
    for (int i = 0; i < SUBCHUNK_SIZE_VOLUME; i++)
        total_trans |= is_trans[data_block[i]];
    if (total_trans == 0)
    {
        renderer_hints.uniform_air = 0;
        renderer_hints.uniform_opaque = 1;
        renderer_hints.opaque_sides = 1;
        renderer_hints.opaque_face_pos_x = 1;
        renderer_hints.opaque_face_pos_y = 1;
        renderer_hints.opaque_face_pos_z = 1;
        renderer_hints.opaque_face_neg_x = 1;
        renderer_hints.opaque_face_neg_y = 1;
        renderer_hints.opaque_face_neg_z = 1;
        return;
    }

    Uint32 has_trans_pos_x = 0;
    Uint32 has_trans_pos_y = 0;
    Uint32 has_trans_pos_z = 0;
    Uint32 has_trans_neg_x = 0;
    Uint32 has_trans_neg_y = 0;
    Uint32 has_trans_neg_z = 0;

    /* Check faces for transparency */
    // int y = (dat_it) & 0x0F;
    // int z = (dat_it >> 4) & 0x0F;
    // int x = (dat_it >> 8) & 0x0F;
    // clang-format off
    for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_pos_x |= is_trans[data_block[SUBCHUNK_INDEX(SUBCHUNK_SIZE_X - 1, y, z)]];
    for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_neg_x |= is_trans[data_block[SUBCHUNK_INDEX(0, y, z)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) has_trans_pos_y |= is_trans[data_block[SUBCHUNK_INDEX(x, SUBCHUNK_SIZE_Y - 1, z)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) has_trans_neg_y |= is_trans[data_block[SUBCHUNK_INDEX(x, 0, z)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_pos_z |= is_trans[data_block[SUBCHUNK_INDEX(x, y, SUBCHUNK_SIZE_Z - 1)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_neg_z |= is_trans[data_block[SUBCHUNK_INDEX(x, y, 0)]];
    // clang-format on

    renderer_hints.opaque_face_pos_x = (has_trans_pos_x == 0);
    renderer_hints.opaque_face_pos_y = (has_trans_pos_y == 0);
    renderer_hints.opaque_face_pos_z = (has_trans_pos_z == 0);
    renderer_hints.opaque_face_neg_x = (has_trans_neg_x == 0);
    renderer_hints.opaque_face_neg_y = (has_trans_neg_y == 0);
    renderer_hints.opaque_face_neg_z = (has_trans_neg_z == 0);

    /* Check if all faces are opaque and set the appropriate hint */
    if ((has_trans_pos_x | has_trans_pos_y | has_trans_pos_z | has_trans_neg_x | has_trans_neg_y | has_trans_neg_z) == 0)
        renderer_hints.opaque_sides = 1;
}

void chunk_cubic_t::light_pass_block_setup()
{
    Uint8 light_levels[256];

    for (int i = 0; i < IM_ARRAYSIZE(light_levels); i++)
        light_levels[i] = mc_id::get_light_level(i);

    clear_light_block();

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it++)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        set_light_block(x, y, z, light_levels[get_type(x, y, z)]);
    }
}

/* TODO: Grab from 3x3x3 */
void chunk_cubic_t::light_pass_block_grab_from_neighbors()
{
    bool is_transparent[256];
    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    /* Get block light from face neighbors */
    /* <x0,y0,z0> refers to an internal block, <x1,y1,z1> refers to it's neighbor in a different chunk */
    /* l0 refers to the light value of an internal block, l1 refers to it's neighbor in a different chunk */
    if (neighbors.pos_x)
        for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = SUBCHUNK_SIZE_X - 1, y0 = y, z0 = z;
                const int x1 = 0, y1 = y, z1 = z;
                int l0 = get_light_block(x0, y0, z0), l1 = neighbors.pos_x->get_light_block(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_block(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.neg_x)
        for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = 0, y0 = y, z0 = z;
                const int x1 = SUBCHUNK_SIZE_X - 1, y1 = y, z1 = z;
                int l0 = get_light_block(x0, y0, z0), l1 = neighbors.neg_x->get_light_block(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_block(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.pos_y)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int x0 = x, y0 = SUBCHUNK_SIZE_Y - 1, z0 = z;
                const int x1 = x, y1 = 0, z1 = z;
                int l0 = get_light_block(x0, y0, z0), l1 = neighbors.pos_y->get_light_block(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_block(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.neg_y)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int x0 = x, y0 = 0, z0 = z;
                const int x1 = x, y1 = SUBCHUNK_SIZE_Y - 1, z1 = z;
                int l0 = get_light_block(x0, y0, z0), l1 = neighbors.neg_y->get_light_block(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_block(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.pos_z)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = x, y0 = y, z0 = SUBCHUNK_SIZE_Z - 1;
                const int x1 = x, y1 = y, z1 = 0;
                int l0 = get_light_block(x0, y0, z0), l1 = neighbors.pos_z->get_light_block(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_block(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.neg_z)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = x, y0 = y, z0 = 0;
                const int x1 = x, y1 = y, z1 = SUBCHUNK_SIZE_Z - 1;
                int l0 = get_light_block(x0, y0, z0), l1 = neighbors.neg_z->get_light_block(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_block(x0, y0, z0, SDL_max(l0, l1));
            }
}

void chunk_cubic_t::light_pass_block_propagate_internals()
{
    bool is_transparent[256];

    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    /* Propagate light (Block) (Backwards, then forwards) */
    for (int dat_it = SUBCHUNK_SIZE_VOLUME - 1; dat_it > -SUBCHUNK_SIZE_VOLUME; dat_it--)
    {
        const int vol_it = abs(dat_it) & 0xFFF;
        const int y = (vol_it) & 0x0F;
        const int z = (vol_it >> 4) & 0x0F;
        const int x = (vol_it >> 8) & 0x0F;

        if (!is_transparent[get_type(x, y, z)])
            continue;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        Uint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Uint8(get_light_block(x + 1, y, z));
            sur_levels[3] = 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = 0;
            sur_levels[3] = Uint8(get_light_block(x - 1, y, z));
            break;
        default:
            sur_levels[0] = Uint8(get_light_block(x + 1, y, z));
            sur_levels[3] = Uint8(get_light_block(x - 1, y, z));
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Uint8(get_light_block(x, y + 1, z));
            sur_levels[4] = 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            sur_levels[1] = 0;
            sur_levels[4] = Uint8(get_light_block(x, y - 1, z));
            break;
        default:
            sur_levels[1] = Uint8(get_light_block(x, y + 1, z));
            sur_levels[4] = Uint8(get_light_block(x, y - 1, z));
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Uint8(get_light_block(x, y, z + 1));
            sur_levels[5] = 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = 0;
            sur_levels[5] = Uint8(get_light_block(x, y, z - 1));
            break;
        default:
            sur_levels[2] = Uint8(get_light_block(x, y, z + 1));
            sur_levels[5] = Uint8(get_light_block(x, y, z - 1));
            break;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        Uint8 lvl = get_light_block(x, y, z) + 1;

        /* This SDL_max() isn't necessary because lvl is guaranteed to be at least one by now */
        // lvl = SDL_max(1, lvl);

        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[1]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        /* Move lvl from range [1,16] to [0,15] */
        lvl--;

        set_light_block(x, y, z, lvl);
    }
}

/* TODO: Grab from 3x3x3 */
void chunk_cubic_t::light_pass_sky_grab_from_neighbors()
{
    Uint8 decaying_types[256];
    memset(decaying_types, 0, sizeof(decaying_types));

    /* Move this out to something like mc_id::get_sky_light_attenuation() */
    decaying_types[BLOCK_ID_LEAVES] = 1;
    decaying_types[BLOCK_ID_WATER_FLOWING] = 1;
    decaying_types[BLOCK_ID_WATER_SOURCE] = 1;

    bool is_transparent[256];
    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    /* Get block light from face neighbors */
    /* <x0,y0,z0> refers to an internal block, <x1,y1,z1> refers to it's neighbor in a different chunk */
    /* l0 refers to the light value of an internal block, l1 refers to it's neighbor in a different chunk */
    if (neighbors.pos_x)
        for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = SUBCHUNK_SIZE_X - 1, y0 = y, z0 = z;
                const int x1 = 0, y1 = y, z1 = z;
                int l0 = get_light_sky(x0, y0, z0), l1 = neighbors.pos_x->get_light_sky(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.neg_x)
        for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = 0, y0 = y, z0 = z;
                const int x1 = SUBCHUNK_SIZE_X - 1, y1 = y, z1 = z;
                int l0 = get_light_sky(x0, y0, z0), l1 = neighbors.neg_x->get_light_sky(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }

    if (neighbors.pos_y)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int x0 = x, y0 = SUBCHUNK_SIZE_Y - 1, z0 = z;
                const int x1 = x, y1 = 0, z1 = z;
                Uint8 type = get_type(x0, y0, z0);
                int l0 = get_light_sky(x0, y0, z0), l1 = neighbors.pos_y->get_light_sky(x1, y1, z1) - decaying_types[type];
                if (is_transparent[type])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }
    else
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int x0 = x, y0 = SUBCHUNK_SIZE_Y - 1, z0 = z;
                const int x1 = x, y1 = 0, z1 = z;
                int l0 = get_light_sky(x0, y0, z0), l1 = 15;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }

    if (neighbors.neg_y)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int x0 = x, y0 = 0, z0 = z;
                const int x1 = x, y1 = SUBCHUNK_SIZE_Y - 1, z1 = z;
                int l0 = get_light_sky(x0, y0, z0), l1 = neighbors.neg_y->get_light_sky(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.pos_z)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = x, y0 = y, z0 = SUBCHUNK_SIZE_Z - 1;
                const int x1 = x, y1 = y, z1 = 0;
                int l0 = get_light_sky(x0, y0, z0), l1 = neighbors.pos_z->get_light_sky(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }
    if (neighbors.neg_z)
        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
            for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
            {
                const int x0 = x, y0 = y, z0 = 0;
                const int x1 = x, y1 = y, z1 = SUBCHUNK_SIZE_Z - 1;
                int l0 = get_light_sky(x0, y0, z0), l1 = neighbors.neg_z->get_light_sky(x1, y1, z1) - 1;
                if (is_transparent[get_type(x0, y0, z0)])
                    set_light_sky(x0, y0, z0, SDL_max(l0, l1));
            }
}

void chunk_cubic_t::light_pass_sky_propagate_internals()
{
    Uint8 non_decaying_types[256];
    memset(non_decaying_types, 1, sizeof(non_decaying_types));

    /* Move this out to something like mc_id::get_sky_light_attenuation() */
    non_decaying_types[BLOCK_ID_LEAVES] = 0;
    non_decaying_types[BLOCK_ID_WATER_FLOWING] = 0;
    non_decaying_types[BLOCK_ID_WATER_SOURCE] = 0;

    bool is_transparent[256];
    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    /* Propagate light (Block) (Backwards, then forwards) */
    for (int dat_it = SUBCHUNK_SIZE_VOLUME - 1; dat_it > -SUBCHUNK_SIZE_VOLUME; dat_it--)
    {
        const int vol_it = abs(dat_it) & 0xFFF;
        const int y = (vol_it) & 0x0F;
        const int z = (vol_it >> 4) & 0x0F;
        const int x = (vol_it >> 8) & 0x0F;

        Uint8 type = get_type(x, y, z);

        if (!is_transparent[type])
            continue;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        Uint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Uint8(get_light_sky(x + 1, y, z));
            sur_levels[3] = 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = 0;
            sur_levels[3] = Uint8(get_light_sky(x - 1, y, z));
            break;
        default:
            sur_levels[0] = Uint8(get_light_sky(x + 1, y, z));
            sur_levels[3] = Uint8(get_light_sky(x - 1, y, z));
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Uint8(get_light_sky(x, y + 1, z));
            sur_levels[4] = 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            sur_levels[1] = 0;
            sur_levels[4] = Uint8(get_light_sky(x, y - 1, z));
            break;
        default:
            sur_levels[1] = Uint8(get_light_sky(x, y + 1, z));
            sur_levels[4] = Uint8(get_light_sky(x, y - 1, z));
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Uint8(get_light_sky(x, y, z + 1));
            sur_levels[5] = 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = 0;
            sur_levels[5] = Uint8(get_light_sky(x, y, z - 1));
            break;
        default:
            sur_levels[2] = Uint8(get_light_sky(x, y, z + 1));
            sur_levels[5] = Uint8(get_light_sky(x, y, z - 1));
            break;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        Uint8 lvl = get_light_sky(x, y, z) + 1;

        /* This SDL_max() isn't necessary because lvl is guaranteed to be at least one by now */
        // lvl = SDL_max(1, lvl);

        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        /* Sky light does not decay downward through air */
        lvl = SDL_max(lvl, sur_levels[1] + non_decaying_types[type]);

        /* Move lvl from range [1,16] to [0,15] */
        lvl--;

        set_light_sky(x, y, z, lvl);
    }
}

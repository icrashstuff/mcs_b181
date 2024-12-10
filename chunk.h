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
#ifndef MCS_B181_CHUNK_H
#define MCS_B181_CHUNK_H

#include "ids.h"
#include "misc.h"
#include <vector>

struct param_ore_t
{
    /**
     * Block to generate
     */
    block_id_t block_id;

    /**
     * Rarity value in the range [0, 1]
     */
    float rarity;

    /**
     * Ore vein size in terms of min and max 2x2 arrangements
     */
    range_t vein_size;

    /**
     * Zone of max abundance (unused)
     */
    range_t zone_y;

    /**
     * Zone of possible generation
     */
    range_t gen_y;

    /**
     * Blocks that can be replaced by the vein
     */
    block_id_t can_replace[4];
};

enum cutter_type_t : Uint8
{
    CUTTER_CAVE = 0,
    CUTTER_RAVINE = 1,
    CUTTER_CAVE_NO_DECOR = 2,
    CUTTER_RAVINE_NO_DECOR = 3,
};

/**
 * TODO: Will be involved with cave/ravine gen
 *
 * Cutters will be able to cut through anything, but must start in terrain
 */
struct param_cutter_t
{
    float rarity;
    Uint8 block_id;
    range_t size;
    range_t gen_y;

    cutter_type_t cutter;
};

/* A 16 * WORLD_HEIGHT * 16 chunk */
class chunk_t
{
public:
    chunk_t();

    /**
     * Goes through and sets the appropriate light levels for each block,
     *
     * Lighting is something I don't really understand nor something I feel
     * like currently putting in the effort to understand right now
     */
    void correct_lighting(int generator);

    /**
     * TODO: Multi stage generation
     *
     * 0: Simple noise (BLOCK_ID_STONE, BLOCK_ID_NETHERRACK)
     * 1: Mountains (Still basic building blocks) (If biomes are implemented then they should be used here)
     * 2: Biomes (Grass, dirt, sand, water, and the likes)
     * 3: Ores
     * 4: Cutters
     * 5: Structures (includes trees)
     */
    void generate_from_seed_over(long seed, int cx, int cz);

    void generate_ores(long seed, int cx, int cz, param_ore_t* ores, Uint8 ore_count);

    void generate_from_seed_nether(long seed, int cx, int cz);

    void generate_special_ascending_type(int max_y);

    void generate_special_metadata();

    /**
     * Attempts to find a suitable place to put a player in a chunk
     *
     * Returns true if a suitable location was found, false if a fallback location at world height was selected
     */
    bool find_spawn_point(double& x, double& y, double& z);

    inline Uint8 get_type(int x, int y, int z)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));

        return data[index];
    }

    inline void set_type(int x, int y, int z, Uint8 type)
    {
        changed = true;
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));

        if (type <= BLOCK_ID_MAX)
            data[index] = type;
        else
            data[index] = 0;
    }

    inline Uint8 get_metadata(int x, int y, int z)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 2;

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    inline void set_metadata(int x, int y, int z, Uint8 metadata)
    {
        changed = true;
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 2;

        if (index % 2 == 1)
            data[index / 2] = ((metadata & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (metadata & 0x0F) | (data[index / 2] & 0xF0);
    }

    inline Uint8 get_light_block(int x, int y, int z)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 3;

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    inline void set_light_block(int x, int y, int z, Uint8 level)
    {
        changed = true;
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 3;

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }

    inline Uint8 get_light_sky(int x, int y, int z)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 4;

        if (index % 2 == 1)
            return (data[index / 2] >> 4) & 0x0F;
        else
            return data[index / 2] & 0x0F;
    }

    inline void set_light_sky(int x, int y, int z, Uint8 level)
    {
        changed = true;
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 4;

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }

    bool compress_to_buf(std::vector<Uint8>& out);

    bool decompress_from_buf(std::vector<Uint8>& in);

    bool changed = false;

private:
    Uint64 r_state;
    std::vector<Uint8> data;
};

#endif

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
#include <atomic>
#include <vector>

struct param_ore_t
{
    /**
     * Block to generate
     */
    block_id_t block_id;

    float bias_horiz;

    float bias_vert;

    /**
     * Rarity value in the range [0, 1]
     */
    float rarity;

    /**
     * Ore vein size in terms of min and max 2x2 arrangements
     */
    range_t vein_size;

    /**
     * Zone of max abundance
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
 * Cutters will be able to cut through anything (except bedrock), but must start in terrain
 */
struct param_cutter_t
{
    float rarity;
    Uint8 block_id;

    /**
     * For caves [0: Big sphere, 1: Smaller sphere, 2: Smallest sphere]
     */
    range_t radius;

    /**
     * Cutter vein size in terms of min and max cut planes
     */
    range_t vein_size;

    /**
     * Zone of possible centerpoints
     */
    range_t gen_y;

    cutter_type_t cutter;
};

/**
 * A 16 * WORLD_HEIGHT * 16 chunk
 *
 * For memory reasons the best way to iterate over the chunk is as follows:
 * for(int x = 0; x < CHUNK_SIZE_X; x++)
 * for(int z = 0; z < CHUNK_SIZE_Z; z++)
 * for(int y = 0; y < CHUNK_SIZE_Y; y++)
 */
class chunk_t
{
public:
    chunk_t();

    /**
     * Signifies that this chunk is ready to be sent to players (ie. loaded or generated)
     */
    std::atomic<bool> ready = { false };

    /**
     * Goes through and sets the appropriate light levels for each block,
     *
     * Lighting is something I don't really understand nor something I feel
     * like currently putting in the effort to understand right now
     */
    void correct_lighting(int generator);

    /**
     * Goes through and makes any dirt with only air above it grass
     */
    void correct_grass();

    /**
     * TODO: Finish or redo multi stage generation
     *
     * 0: Simple noise (BLOCK_ID_STONE, BLOCK_ID_NETHERRACK)
     * 1: Mountains (Still basic building blocks) (If biomes are implemented then they should be used here)
     * 2: Biomes (Grass, dirt, sand, water, and the likes)
     * 3: Ores
     * 4: Cutters
     * 5: Structures (includes trees)
     */
    void generate_from_seed_over(const long int seed, const int cx, const int cz);

    void generate_biome_toppings(const long int seed, const int cx, const int cz);

    void generate_ores(long seed, int cx, int cz, param_ore_t* ores, Uint8 ore_count);

    void generate_cutters(long seed, int cx, int cz, param_cutter_t* cutters, Uint8 cutter_count);

    void generate_from_seed_nether(long seed, int cx, int cz);

    void generate_special_ascending_type(int max_y);

    void generate_special_metadata();

    void generate_special_fluid(bool random_water, bool random_lava);

    /**
     * Returns an estimate on of memory footprint of a chunk_t object
     */
    size_t get_mem_size();

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

    inline Uint8 get_type_fallback(int x, int y, int z, Uint8 fallback)
    {
        if (x < 0 || y < 0 || z < 0 || x >= CHUNK_SIZE_X || y >= CHUNK_SIZE_Y || z >= CHUNK_SIZE_Z)
            return fallback;

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

        if (type <= BLOCK_ID_NUM_USED)
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
    void generate_biome_data(const long seed, const int cx, const int cz);

    Uint64 r_state_spawn;
    std::vector<Uint8> data;
    float temperatures[CHUNK_SIZE_X * CHUNK_SIZE_Z];
    float humidties[CHUNK_SIZE_X * CHUNK_SIZE_Z];
    float blends[CHUNK_SIZE_X * CHUNK_SIZE_Z];
};

#endif

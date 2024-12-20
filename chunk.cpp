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
#include "chunk.h"
#include "simplex_noise/SimplexNoise.h"
#include "tetra/util/convar.h"

#include <zlib.h>

static param_ore_t ore_params[] = {
    { BLOCK_ID_GRAVEL, 0.3f, { 3, 7 }, { 20, 96 }, { 0, 127 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_DIRT, 0.25f, { 2, 6 }, { 18, 96 }, { 0, 127 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_CLAY, 0.35f, { 2, 5 }, { 5, 52 }, { 40, 72 }, { BLOCK_ID_DIRT, -1, -1, -1 } },
    { BLOCK_ID_ORE_COAL, 0.85f, { 2, 7 }, { 5, 96 }, { 0, 127 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_ORE_COAL, 0.5f, { 2, 7 }, { 96, 127 }, { 80, 127 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_ORE_LAPIS, 0.35f, { 1, 1 }, { 13, 17 }, { 0, 34 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_ORE_IRON, 0.65f, { 1, 2 }, { 5, 64 }, { 0, 72 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_ORE_GOLD, 0.5f, { 1, 1 }, { 5, 29 }, { 0, 34 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_ORE_REDSTONE_OFF, 0.5f, { 1, 2 }, { 5, 12 }, { 0, 16 }, { BLOCK_ID_STONE, -1, -1, -1 } },
    { BLOCK_ID_ORE_DIAMOND, 0.35f, { 1, 1 }, { 5, 12 }, { 0, 16 }, { BLOCK_ID_STONE, -1, -1, -1 } },
};

static Uint8 ore_2r[] = { 0x3f, 0x7f, 0xff, 0x7d, 0xbf, 0x77, 0xff };

static convar_int_t strip_stone("strip_stone", 0, 0, 1, "Strip stone after generating terrain", CONVAR_FLAG_HIDDEN | CONVAR_FLAG_INT_IS_BOOL);

/* Each layer is on the x z plane */
static const Uint16 cutters_layers[][10] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {
        0b0000000000000000,
        0b0000000000000000,
        0b0000000000000000,
        0b0000000000000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000000000000,
        0b0000000000000000,
        0b0000000000000000,
        0b0000000000000000,
    },
    {
        0b0000000000000000,
        0b0000000000000000,
        0b0000000000000000,
        0b0000000110000000,
        0b0000001111000000,
        0b0000001111000000,
        0b0000000110000000,
        0b0000000000000000,
        0b0000000000000000,
        0b0000000000000000,
    },
    {
        0b0000000000000000,
        0b0000000000000000,
        0b0000001111000000,
        0b0000011111100000,
        0b0000011111100000,
        0b0000011111100000,
        0b0000011111100000,
        0b0000001111000000,
        0b0000000000000000,
        0b0000000000000000,
    },
    {
        0b0000000000000000,
        0b0000000110000000,
        0b0000011111100000,
        0b0000011111100000,
        0b0000111111110000,
        0b0000111111110000,
        0b0000011111100000,
        0b0000011111100000,
        0b0000000110000000,
        0b0000000000000000,
    },
    {
        0b0000000110000000,
        0b0000011111100000,
        0b0000111111110000,
        0b0000111111110000,
        0b0001111111111000,
        0b0001111111111000,
        0b0000111111110000,
        0b0000111111110000,
        0b0000011111100000,
        0b0000000110000000,
    },
};

/**
 * TODO: Will be involved with cave/ravine gen
 *
 * Cutters will be able to cut through anything, but must start in terrain
 */
static param_cutter_t cutter_params[] = {
    { 0.15f, BLOCK_ID_AIR, { 0, 1 }, { 2, 6 }, { 8, 13 }, CUTTER_CAVE },
    { 0.20f, BLOCK_ID_AIR, { 0, 2 }, { 10, 80 }, { 8, 63 }, CUTTER_CAVE },
    { 0.35f, BLOCK_ID_AIR, { 1, 1 }, { 10, 80 }, { 8, 63 }, CUTTER_CAVE },
    { 0.15f, BLOCK_ID_AIR, { 2, 2 }, { 10, 80 }, { 8, 63 }, CUTTER_CAVE },
    { 0.1f, BLOCK_ID_AIR, { 0, 2 }, { 24, 80 }, { 32, 80 }, CUTTER_CAVE },
    { 0.07f, BLOCK_ID_AIR, { 8, 10 }, { 20, 80 }, { 8, 72 }, CUTTER_RAVINE_NO_DECOR },
    { 0.025f, BLOCK_ID_AIR, { 8, 10 }, { 20, 80 }, { 8, 72 }, CUTTER_RAVINE_NO_DECOR },
};

chunk_t::chunk_t()
{
    data.resize(CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 5 / 2, 0);
    assert(data.size());
    Uint8* ptr = data.data();
    Uint64* rptr = (Uint64*)this;
    r_state = *(Uint64*)&ptr + *(Uint32*)this + *(Uint64*)&rptr;
}

void chunk_t::correct_lighting(int generator)
{
    if (!changed)
        return;

    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
        {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--)
            {
                set_light_sky(x, y, z, 15);
                Uint8 level = mc_id::get_light_level(get_type(x, y, z));
                set_light_block(x, y, z, level);
            }
        }
    }
    changed = false;
}

void chunk_t::correct_grass()
{

    int found_air = 0;
    Uint8 last_type[2] = { 0, 0 };

    for (int ix = 0; ix < CHUNK_SIZE_X; ix++)
    {
        for (int iz = 0; iz < CHUNK_SIZE_Z; iz++)
        {
            int cx = (ix) % CHUNK_SIZE_X;
            int cz = (iz) % CHUNK_SIZE_Z;
            for (int i = CHUNK_SIZE_Y; i > 0; i--)
            {
                int index = i - 1 + (cz * (CHUNK_SIZE_Y)) + (cx * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));
                Uint8 type = data[index];
                if (type == 0)
                    found_air++;

                if (type > 0 && found_air > 2 && last_type[0] == 0 && last_type[1] == 0 && type != BLOCK_ID_LAVA_FLOWING && type != BLOCK_ID_LAVA_SOURCE)
                {
                    if (type == BLOCK_ID_DIRT)
                        set_type(cx, i, cz, BLOCK_ID_GRASS);
                    goto next_y_column;
                }
                last_type[1] = last_type[0];
                last_type[0] = type;
            }
        next_y_column:;
        }
    }
}

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
void chunk_t::generate_from_seed_over(long seed, int cx, int cz)
{
    if (((convar_int_t*)convar_t::get_convar("dev"))->get() && cx == 0 && cz == 0)
    {
        generate_special_metadata();

        correct_lighting(0);
        ready = true;
        return;
    }
    SimplexNoise noise;

    Uint64 seed_r = *(Uint64*)&seed;

    Uint32 rc1 = SDL_rand_bits_r(&seed_r);
    Uint32 rc2 = SDL_rand_bits_r(&seed_r);

    double x_diff = cast_to_sint32((rc1 & 0xF05A0FA5) | (rc2 & 0x0FA5F05A)) / 4096.0;
    double z_diff = cast_to_sint32((rc1 & 0x0F0F0F0F) | (rc2 & 0xF0F0F0F0)) / 4096.0;

    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
        {
            double fx = x + cx * CHUNK_SIZE_X + x_diff;
            double fz = z + cz * CHUNK_SIZE_Z + z_diff;
            int height_grass = (noise.fractal(2, fx / 100, fz / 100) + 1.0) + 2;
            double height = (noise.fractal(4, fx / 100, fz / 100) + 1.0 + noise.noise((fx + 10.0) / 100, (fz + 10.0) / 100) + 1.0) * 0.05 * CHUNK_SIZE_Y + 56
                - height_grass;
            double aggressive = noise.fractal(4, fx / 150, fz / 150) + 1.0;
            if (aggressive > 1.05)
                height = height * (noise.fractal(3, fx / 150, fz / 150) + 1.0);
            if (aggressive > 1.5)
                height = height * 1.5 / (noise.fractal(2, fx / 150, fz / 150) + 1.0);
            else
                height = height + 1.5 / (noise.fractal(2, fx / 150, fz / 150) + 1.0);

            for (int i = 1; i < height && i < CHUNK_SIZE_Y; i++)
                set_type(x, i, z, BLOCK_ID_STONE);
            for (int i = height; i < height + height_grass && i < CHUNK_SIZE_Y; i++)
                set_type(x, i, z, BLOCK_ID_DIRT);
            set_type(x, height + height_grass, z, BLOCK_ID_GRASS);
            for (int i = height - 2; i < CHUNK_SIZE_Y; i++)
                set_light_sky(x, i, z, 15);
            set_type(x, 0, z, BLOCK_ID_BEDROCK);
        }
    }

    generate_ores(seed, cx, cz, ore_params, ARR_SIZE(ore_params));
    generate_cutters(seed, cx, cz, cutter_params, ARR_SIZE(cutter_params));

    if (((convar_int_t*)convar_t::get_convar("dev"))->get())
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
            {
                if (x == 0 && z == 0)
                    set_type(x, 0, z, BLOCK_ID_WOOL);
                else if (SDL_abs(cx) % 2 == SDL_abs(cz) % 2)
                    set_type(x, 0, z, BLOCK_ID_BEDROCK);
                else
                    set_type(x, 0, z, BLOCK_ID_BRICKS_STONE);
            }
        }
    }

    if (strip_stone.get())
        for (int x = 0; x < CHUNK_SIZE_X; x++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
                for (int i = 0; i < CHUNK_SIZE_Y; i++)
                    if (get_type(x, i, z) == BLOCK_ID_STONE)
                        set_type(x, i, z, BLOCK_ID_AIR);

    correct_grass();
    correct_lighting(0);
    ready = true;
}

static void generate_ore_chunk_vals(Uint64 arr[NUM_ORE_CHANCE], int cx, int cz, Uint64 seed_r)
{
    seed_r += cx * CHUNK_SIZE_X;
    seed_r += ((Sint64)cz * CHUNK_SIZE_Z) << 32;

    for (int i = 0; i < NUM_ORE_CHANCE; i++)
        arr[i] = ((Uint64)SDL_rand_bits_r(&seed_r) << 32) | (Uint64)SDL_rand_bits_r(&seed_r);
}

void chunk_t::generate_cutters(long seed, int cx, int cz, param_cutter_t* cutters, Uint8 cutter_count)
{
    SimplexNoise noise;

    Uint64 seed_r = *(Uint64*)&seed;

    Uint32 rc1 = SDL_rand_bits_r(&seed_r);
    Uint32 rc2 = SDL_rand_bits_r(&seed_r);

    int x_diff = cast_to_sint32((rc2 & 0xF05A0FA5) | (rc1 & 0x0FA5F05A)) >> 12;
    int z_diff = cast_to_sint32((rc2 & 0x0F0F0F0F) | (rc1 & 0xF0F0F0F0)) >> 12;

    seed_r += SDL_rand_bits_r(&seed_r);

    /**
     * To access: (cutter_sphere[which][y][x] >> z) & 1
     */
    Uint16 cutter_sphere[3][8][10];

    int sphere_radi[] = { 4, 3, 2 };
    {
        int j = 0;
        int l = 2;
        memcpy(cutter_sphere[0][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[0][j++], cutters_layers[--l], sizeof(cutters_layers[0]));

        j = 0;
        l = 2;
        memcpy(cutter_sphere[1][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[1][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[1][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[1][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[1][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[1][j++], cutters_layers[--l], sizeof(cutters_layers[0]));

        j = 0;
        l = 1;
        memcpy(cutter_sphere[2][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[2][j++], cutters_layers[l++], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[2][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
        memcpy(cutter_sphere[2][j++], cutters_layers[--l], sizeof(cutters_layers[0]));
    }

    for (int ic = -8; ic < 8; ic++)
    {
        for (int jc = -8; jc < 8; jc++)
        {
            Uint64 cvals[NUM_ORE_CHANCE];
            generate_ore_chunk_vals(cvals, cx + ic + x_diff, cz + jc + z_diff, seed_r);

            int num_chances = (cvals[0] % 5) + 2;

            for (int cval_it = 0; cval_it < num_chances; cval_it++)
            {
                Uint64 d = cvals[cval_it];
                jshort x = ((jshort)(d & 0x0f)) + (ic - 1) * CHUNK_SIZE_X;
                jshort z = ((jshort)(d >> 10) & 0x0f) + (jc - 1) * CHUNK_SIZE_Z;
                jshort y = (d >> 20) & 0x7f;
                jubyte which = ((d >> 28) & 0xff) % cutter_count;
                float rarity = (float)(((d >> 36) & 0xff) + ((d >> 20) & 0xff)) / 512.0f;
                bool direction_x = (d >> 45) & 1;
                int direction_move = ((d >> 46) & 1) ? -1 : 1;
                int direction_move_y = (((d >> 58) & 1) ? -1 : 1) * ((d >> 57) & 1);
                int direction_side = (((d >> 48) & 1) ? -1 : 1) * ((d >> 47) & 1);

                param_cutter_t p = cutters[which];

                for (jubyte off = 0; off < cutter_count; off++)
                {
                    p = cutters[which];
                    if (p.gen_y.max < y || p.gen_y.min > y || (p.cutter != CUTTER_CAVE_NO_DECOR && p.cutter != CUTTER_CAVE))
                        which = (which + 3) % cutter_count;
                    else
                        off = cutter_count;
                }

                if (p.gen_y.max < y || p.gen_y.min > y || (p.cutter != CUTTER_CAVE_NO_DECOR && p.cutter != CUTTER_CAVE))
                    continue;

                if (p.rarity <= rarity)
                    continue;

                Uint8 times = p.vein_size.min;
                if (p.vein_size.max != p.vein_size.min)
                    times += d % (p.vein_size.max - p.vein_size.min);

                if (times > 100)
                    times = 100;

                Uint64 jitter_var = ROTATE_UINT64(d, d & 0xff);

                jubyte which_sphere = p.radius.min;
                if (p.radius.max != p.radius.min)
                    which_sphere += jitter_var % (p.radius.max - p.radius.min);

                for (int time_it = 0; time_it < times; time_it++)
                {
                    if (p.gen_y.max < y || p.gen_y.min > y)
                    {
                        time_it = times;
                        continue;
                    }
                    jitter_var = ROTATE_UINT64(jitter_var, 5);
                    if ((jitter_var >> 5) & 1)
                        direction_side = (((jitter_var >> 48) & 1) ? -1 : 1) * ((jitter_var >> 47) & 1);
                    if ((jitter_var >> 24) & 1)
                        direction_x = !direction_x;
                    if (((jitter_var >> 28) & 3) == 3)
                        direction_move_y = (((jitter_var >> 58) & 1) ? -1 : 1) * ((jitter_var >> 57) & 1);

                    if (direction_x)
                    {
                        if (direction_side != 0)
                        {
                            x += direction_move;
                            z += direction_side * ((jitter_var >> 4) & 1);
                        }
                        else
                        {
                            x += direction_move;
                            z -= ((jitter_var >> 3) & 1);
                            z += ((jitter_var >> 2) & 1);
                        }
                    }
                    else
                    {
                        if (direction_side != 0)
                        {
                            x += direction_side * ((jitter_var >> 4) & 1);
                            z += direction_move;
                        }
                        else
                        {
                            z += direction_move;
                            x += ((jitter_var >> 2) & 1);
                            x -= ((jitter_var >> 3) & 1);
                        }
                    }

                    if (direction_move_y != 0)
                    {
                        y += direction_move_y * (1 + ((jitter_var >> 62) & 1));
                    }
                    else
                    {
                        y += ((jitter_var >> 0) & 3) == 3;
                        y -= ((jitter_var >> 1) & 3) == 3;
                    }

                    if (x < -CHUNK_SIZE_X * 2 || x >= CHUNK_SIZE_X * 3)
                        continue;
                    if (z < -CHUNK_SIZE_Z * 2 || z >= CHUNK_SIZE_Z * 3)
                        continue;
                    if (y < -CHUNK_SIZE_Y * 2 || y >= CHUNK_SIZE_Y * 3)
                        continue;

                    for (int y_off = 0; y_off < sphere_radi[which_sphere] * 2; y_off++)
                        for (int x_off = 0; x_off < 10; x_off++)
                            for (int z_off = 0; z_off < 16; z_off++)
                            {
                                if (!((cutter_sphere[which_sphere][y_off][x_off] >> z_off) & 1))
                                    continue;
                                jshort jx = x, jy = y + y_off - sphere_radi[which_sphere], jz = z;

                                jx += (x_off - 5);
                                jz += (z_off - 8);

                                if (jx < 0 || jx >= CHUNK_SIZE_X)
                                    continue;
                                if (jz < 0 || jz >= CHUNK_SIZE_Z)
                                    continue;
                                if (jy < 0 || jy >= CHUNK_SIZE_Y)
                                    continue;
                                Uint8 existing = get_type(jx, jy, jz);
                                if (existing != BLOCK_ID_BEDROCK && existing != BLOCK_ID_LAVA_SOURCE && existing != BLOCK_ID_LAVA_FLOWING
                                    && existing != BLOCK_ID_WATER_SOURCE && existing != BLOCK_ID_WATER_FLOWING)
                                {
                                    if (jy < 13 && p.cutter == CUTTER_CAVE)
                                        set_type(jx, jy, jz, BLOCK_ID_LAVA_SOURCE);
                                    else
                                        set_type(jx, jy, jz, p.block_id);
                                }
                            }
                }
            }
        }
    }
}
void chunk_t::generate_ores(long seed, int cx, int cz, param_ore_t* ores, Uint8 ore_count)
{
    SimplexNoise noise;

    Uint64 seed_r = *(Uint64*)&seed;

    Uint32 rc1 = SDL_rand_bits_r(&seed_r);
    Uint32 rc2 = SDL_rand_bits_r(&seed_r);

    int x_diff = cast_to_sint32((rc1 & 0xF05A0FA5) | (rc2 & 0x0FA5F05A)) >> 12;
    int z_diff = cast_to_sint32((rc1 & 0x0F0F0F0F) | (rc2 & 0xF0F0F0F0)) >> 12;

    seed_r += SDL_rand_bits_r(&seed_r);

    for (int ic = -1; ic < 2; ic++)
    {
        for (int jc = -1; jc < 2; jc++)
        {
            Uint64 cvals[NUM_ORE_CHANCE];
            generate_ore_chunk_vals(cvals, cx + ic + x_diff, cz + jc + z_diff, seed_r);

            int num_chances = (seed_r % (NUM_ORE_CHANCE / 4)) + NUM_ORE_CHANCE * 3 / 4;

            for (int cval_it = 0; cval_it < num_chances; cval_it++)
            {
                Uint64 d = cvals[cval_it];
                jshort x = ((jbyte)(d & 0x0f)) + (ic - 1) * CHUNK_SIZE_X;
                jshort z = ((jbyte)(d >> 4) & 0x0f) + (jc - 1) * CHUNK_SIZE_Z;
                jubyte y = (d >> 8) & 0x7f;
                jubyte which = ((d >> 16) & 0xff) % ore_count;
                float rarity = (float)(((d >> 24) & 0xff) + ((d >> 36) & 0xff)) / 512.0f;
                bool direction_x = (d >> 45) & 1;
                int direction_move = ((d >> 46) & 1) ? -1 : 1;
                int direction_side = (((d >> 48) & 1) ? -1 : 1) * ((d >> 47) & 1);

                for (jubyte off = 0; off < ore_count; off++)
                {
                    if (ores[which].gen_y.max < y || ores[which].gen_y.min > y)
                        which = (which + 3) % ore_count;
                    else
                        off = ore_count;
                }

                param_ore_t p = ores[which];

                if (p.gen_y.max < y || p.gen_y.min > y)
                    continue;

                if (y < p.zone_y.min)
                    p.rarity *= (float)(y - p.gen_y.min) / (float)(p.zone_y.min - p.gen_y.min);
                if (y > p.zone_y.max)
                    p.rarity *= (float)(p.gen_y.max - y) / (float)(p.gen_y.max - p.zone_y.max);

                if (p.rarity <= rarity)
                    continue;

                Uint8 times = p.vein_size.min;
                if (p.vein_size.max != p.vein_size.min)
                    times += d % (p.vein_size.max - p.vein_size.min);

                Uint64 jitter_var = ROTATE_UINT64(d, d & 0xff);

                for (int time_it = 0; time_it < times; time_it++)
                {
                    jitter_var = ROTATE_UINT64(jitter_var, 7);
                    Uint8 pos_ore_2r = ((d >> 45) + time_it) % ARR_SIZE(ore_2r);
                    if (direction_x)
                    {
                        if (direction_side != 0)
                        {
                            x += direction_move;
                            z += direction_side * ((jitter_var >> 4) & 1);
                        }
                        else
                        {
                            x += direction_move;
                            z -= ((jitter_var >> 3) & 1);
                            z += ((jitter_var >> 2) & 1);
                        }
                    }
                    else
                    {
                        if (direction_side != 0)
                        {
                            x += direction_side * ((jitter_var >> 4) & 1);
                            z += direction_move;
                        }
                        else
                        {
                            z += direction_move;
                            x += ((jitter_var >> 2) & 1);
                            x -= ((jitter_var >> 3) & 1);
                        }
                    }

                    y += ((jitter_var >> 0) & 1);
                    y -= ((jitter_var >> 1) & 1);

                    for (int shift = 0; shift < 8; shift++)
                    {
                        Uint16 shifty = (Uint16)ore_2r[pos_ore_2r] << 8 | ore_2r[pos_ore_2r];
                        Uint8 val = (shifty >> ((shift + ((d >> 33) & 0xff) * 2) % 8)) & 1;
                        if (!val)
                            continue;
                        jshort jx = x + shift / 4, jy = y + shift % 2, jz = z + (shift % 4) / 2;
                        if (jx < 0 || jx >= CHUNK_SIZE_X)
                            continue;
                        if (jz < 0 || jz >= CHUNK_SIZE_Z)
                            continue;
                        if (jy < 0 || jy >= CHUNK_SIZE_Y)
                            continue;
                        for (int k = 0; k < ARR_SIZE_I(p.can_replace); k++)
                            if (get_type(jx, jy, jz) == p.can_replace[k])
                                set_type(jx, jy, jz, p.block_id);
                    }
                }
            }
        }
    }
}

void chunk_t::generate_from_seed_nether(long seed, int cx, int cz)
{
    SimplexNoise noise;

    Uint64 seed_r = *(Uint64*)&seed;

    Uint32 rc1 = SDL_rand_bits_r(&seed_r);
    Uint32 rc2 = SDL_rand_bits_r(&seed_r);

    double x_diff = cast_to_sint32((rc1 & 0xF05A0FA5) | (rc2 & 0x0FA5F05A)) / 4096.0;
    double y_diff = cast_to_sint32((rc1 & 0x0F0F0F0F) | (rc2 & 0xF0F0F0F0)) / 4096.0;

    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
        {
            double fx = x + cx * CHUNK_SIZE_X + x_diff;
            double fz = z + cz * CHUNK_SIZE_Z + y_diff;
            int height = (noise.fractal(4, fx / 100, fz / 100) + 1.0) * 0.1 * CHUNK_SIZE_Y + 24;
            double noise2 = (noise.fractal(4, fx / 200, fz / 200) + 1.0) * 0.1 * CHUNK_SIZE_Y + 4;

            int height2 = CHUNK_SIZE_Y - noise2;
            for (int i = 1; i < height; i++)
                set_type(x, i, z, BLOCK_ID_NETHERRACK);
            for (int i = height - 2; i < height2; i++)
            {
                if (i < 32)
                {
                    set_type(x, i, z, BLOCK_ID_LAVA_FLOWING);
                    set_light_block(x, i, z, mc_id::get_light_level(BLOCK_ID_LAVA_FLOWING));
                }
                else
                    set_light_sky(x, i, z, 15);
            }
            for (int i = height2; i < CHUNK_SIZE_Y; i++)
                set_type(x, i, z, BLOCK_ID_NETHERRACK);
            set_type(x, 0, z, BLOCK_ID_BEDROCK);
            set_type(x, CHUNK_SIZE_Y - 1, z, BLOCK_ID_BEDROCK);
            set_light_sky(x, CHUNK_SIZE_Y - 1, z, 15);
            for (int i = 0; i < CHUNK_SIZE_Y; i++)
                set_light_block(x, i, z, 15);
        }
    }

    correct_lighting(-1);
}

void chunk_t::generate_special_ascending_type(int max_y)
{
    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
            {
                if (y < BLOCK_ID_MAX && y < max_y)
                {
                    set_type(x, y, z, y);
                }
                set_light_block(x, y, z, 15);
                set_light_sky(x, y, z, 15);
            }
        }
    }
}

void chunk_t::generate_special_metadata()
{
    for (int x = 0; x < CHUNK_SIZE_X; x++)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
            {
                if (y < BLOCK_ID_MAX)
                {
                    if (z == x)
                    {
                        set_type(x, y, z, y);
                        set_metadata(x, y, z, x);
                    }
                }
                set_light_block(x, y, z, 15);
                set_light_sky(x, y, z, 15);
            }
        }
    }
}

/**
 * Attempts to find a suitable place to put a player in a chunk
 *
 * Returns true if a suitable location was found, false if a fallback location at world height was selected
 */
bool chunk_t::find_spawn_point(double& x, double& y, double& z)
{
    Uint32 pos = ((int)(x * 3) << 24) + ((int)(y * 3) << 12) + (int)(z * 3);
    pos += SDL_rand_bits_r(&r_state);

    int cx_s = (pos >> 16) % CHUNK_SIZE_X;
    int cz_s = pos % CHUNK_SIZE_Z;

    int found_air = 0;
    Uint8 last_type[2] = { 0, 0 };

    for (int ix = 0; ix < CHUNK_SIZE_X; ix++)
    {
        for (int iz = 0; iz < CHUNK_SIZE_Z; iz++)
        {
            int cx = (ix + cx_s) % CHUNK_SIZE_X;
            int cz = (iz + cz_s) % CHUNK_SIZE_Z;
            TRACE("checking %d %d", cx, cz);
            for (int i = CHUNK_SIZE_Y; i > 0; i--)
            {
                int index = i - 1 + (cz * (CHUNK_SIZE_Y)) + (cx * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));
                Uint8 type = data[index];
                if (type == 0)
                    found_air++;

                if (type > 0 && found_air > 2 && last_type[0] == 0 && last_type[1] == 0 && type != BLOCK_ID_LAVA_FLOWING && type != BLOCK_ID_LAVA_SOURCE)
                {
                    x = cx + 0.5;
                    y = i + 1.8;
                    z = cz + 0.5;
                    return true;
                }
                last_type[1] = last_type[0];
                last_type[0] = type;
            }
        }
    }

    x = cx_s + 0.5;
    y = CHUNK_SIZE_Y + 1.8;
    z = cz_s + 0.5;
    return false;
}

size_t chunk_t::get_mem_size() { return sizeof(*this) + data.capacity(); }

bool chunk_t::compress_to_buf(std::vector<Uint8>& out)
{
    out.resize(compressBound(data.size()));

    uLongf compressed_len = out.size();
    int result = compress(out.data(), &compressed_len, data.data(), data.size());
    out.resize(compressed_len);

    return result == Z_OK;
}

bool chunk_t::decompress_from_buf(std::vector<Uint8>& in)
{
    std::vector<Uint8> temp;
    temp.resize(data.size());

    uLongf compressed_len = temp.size();
    int result = uncompress(temp.data(), &compressed_len, in.data(), in.size());

    if (result != Z_OK || compressed_len != data.size())
        return false;

    memcpy(data.data(), temp.data(), data.size());

    return true;
}

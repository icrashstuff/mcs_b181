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
#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL3/SDL_opengl.h>

#include "level.h"

#define PASS_TIMER_START()             \
    do                                 \
    {                                  \
        built = 0;                     \
        tick_start = SDL_GetTicksNS(); \
    } while (0)
#define PASS_TIMER_STOP(fmt, ...)                \
    do                                           \
    {                                            \
        elapsed = SDL_GetTicksNS() - tick_start; \
        if (built)                               \
            dc_log(fmt, ##__VA_ARGS__);          \
    } while (0)

void level_t::build_dirty_meshes()
{
    size_t built;
    Uint64 elapsed;
    Uint64 tick_start;

    /* First Light Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (!chunks[i]->dirty)
            continue;
        chunks[i]->clear_light_block(0);
        chunks[i]->clear_light_sky(0);
        light_pass(chunks[i]->chunk_x, chunks[i]->chunk_y, chunks[i]->chunk_z, true);
        built++;
    }
    PASS_TIMER_STOP("Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 1)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Second Light Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (!chunks[i]->dirty)
            continue;
        light_pass(chunks[i]->chunk_x, chunks[i]->chunk_y, chunks[i]->chunk_z, false);
        built++;
    }
    PASS_TIMER_STOP("Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 2)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Third Light Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (!chunks[i]->dirty)
            continue;
        light_pass(chunks[i]->chunk_x, chunks[i]->chunk_y, chunks[i]->chunk_z, false);
        built++;
    }
    PASS_TIMER_STOP("Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 3)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Mesh Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (!chunks[i]->dirty)
            continue;
        build_mesh(chunks[i]->chunk_x, chunks[i]->chunk_y, chunks[i]->chunk_z);
        chunks[i]->dirty = 0;
        built++;
    }
    PASS_TIMER_STOP("Built %zu meshes in %.2f ms (%.2f ms per)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
}

/**
 * Force inlining in debug builds
 *
 * Without this debug builds are *very*, *very* slow (~6x slower)
 */
#pragma GCC push_options
#pragma GCC optimize("Ofast")

struct chunk_cross_t
{
    chunk_cubic_t* c = NULL;
    chunk_cubic_t* pos_x = NULL;
    chunk_cubic_t* pos_y = NULL;
    chunk_cubic_t* pos_z = NULL;
    chunk_cubic_t* neg_x = NULL;
    chunk_cubic_t* neg_y = NULL;
    chunk_cubic_t* neg_z = NULL;

    chunk_cross_t() { }
};

void level_t::light_pass(int chunk_x, int chunk_y, int chunk_z, bool local_only)
{
    /** Index: C +XYZ -XYZ */
    chunk_cross_t cross;

    for (chunk_cubic_t* c : chunks)
    {
        if (!c)
            continue;

        if (c->chunk_x == chunk_x && c->chunk_y == chunk_y && c->chunk_z == chunk_z)
            cross.c = c;

        if (local_only)
            continue;
        else if (c->chunk_x == chunk_x + 1 && c->chunk_y == chunk_y && c->chunk_z == chunk_z)
            cross.pos_x = c;
        else if (c->chunk_x == chunk_x - 1 && c->chunk_y == chunk_y && c->chunk_z == chunk_z)
            cross.neg_x = c;
        else if (c->chunk_x == chunk_x && c->chunk_y == chunk_y + 1 && c->chunk_z == chunk_z)
            cross.pos_y = c;
        else if (c->chunk_x == chunk_x && c->chunk_y == chunk_y - 1 && c->chunk_z == chunk_z)
            cross.neg_y = c;
        else if (c->chunk_x == chunk_x && c->chunk_y == chunk_y && c->chunk_z == chunk_z + 1)
            cross.pos_z = c;
        else if (c->chunk_x == chunk_x && c->chunk_y == chunk_y && c->chunk_z == chunk_z - 1)
            cross.neg_z = c;
    }

    if (!cross.c)
    {
        dc_log_error("Attempt made to update lighting for unloaded chunk at chunk coordinates: %d, %d, %d", chunk_x, chunk_y, chunk_z);
        return;
    }

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z; dat_it++)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;
        cross.c->set_light_block(x, y, z, mc_id::get_light_level(cross.c->get_type(x, y, z)));
    }

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * 16; dat_it++)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        if (!mc_id::is_transparent(cross.c->get_type(x, y, z)))
            continue;

        /** Index: +XYZ -XYZ */
        Sint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Sint8(cross.c->get_light_block(x + 1, y, z)) - 1;
            sur_levels[3] = cross.neg_x ? (Sint8(cross.neg_x->get_light_block(SUBCHUNK_SIZE_X - 1, y, z)) - 1) : 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = cross.pos_x ? (Sint8(cross.pos_x->get_light_block(0, y, z)) - 1) : 0;
            sur_levels[3] = Sint8(cross.c->get_light_block(x - 1, y, z)) - 1;
            break;
        default:
            sur_levels[0] = Sint8(cross.c->get_light_block(x + 1, y, z)) - 1;
            sur_levels[3] = Sint8(cross.c->get_light_block(x - 1, y, z)) - 1;
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Sint8(cross.c->get_light_block(x, y + 1, z)) - 1;
            sur_levels[4] = cross.neg_y ? (Sint8(cross.neg_y->get_light_block(x, SUBCHUNK_SIZE_Y - 1, z)) - 1) : 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            sur_levels[1] = cross.pos_y ? (Sint8(cross.pos_y->get_light_block(x, 0, z)) - 1) : 0;
            sur_levels[4] = Sint8(cross.c->get_light_block(x, y - 1, z)) - 1;
            break;
        default:
            sur_levels[1] = Sint8(cross.c->get_light_block(x, y + 1, z)) - 1;
            sur_levels[4] = Sint8(cross.c->get_light_block(x, y - 1, z)) - 1;
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Sint8(cross.c->get_light_block(x, y, z + 1)) - 1;
            sur_levels[5] = cross.neg_z ? (Sint8(cross.neg_z->get_light_block(x, y, SUBCHUNK_SIZE_Z - 1)) - 1) : 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = cross.pos_z ? (Sint8(cross.pos_z->get_light_block(x, y, 0)) - 1) : 0;
            sur_levels[5] = Sint8(cross.c->get_light_block(x, y, z - 1)) - 1;
            break;
        default:
            sur_levels[2] = Sint8(cross.c->get_light_block(x, y, z + 1)) - 1;
            sur_levels[5] = Sint8(cross.c->get_light_block(x, y, z - 1)) - 1;
            break;
        }

        Uint8 lvl = cross.c->get_light_block(x, y, z);
        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[1]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        cross.c->set_light_block(x, y, z, lvl);
    }
}
#pragma GCC pop_options

void level_t::build_mesh(int chunk_x, int chunk_y, int chunk_z)
{
    /** Index: [x+1][y+1][z+1] */
    chunk_cubic_t* rubik[3][3][3];

    for (int i = 0; i < 27; i++)
    {
        const int ix = (i / 9) % 3 - 1;
        const int iy = (i / 3) % 3 - 1;
        const int iz = i % 3 - 1;
        rubik[ix + 1][iy + 1][iz + 1] = NULL;
        for (chunk_cubic_t* c : chunks)
        {
            if (!c || c->chunk_x != (ix + chunk_x) || c->chunk_y != (iy + chunk_y) || c->chunk_z != (iz + chunk_z))
                continue;
            rubik[ix + 1][iy + 1][iz + 1] = c;
        }
    }

    chunk_cubic_t* center = rubik[1][1][1];

    if (!center)
    {
        dc_log_error("Attempt made to build unloaded chunk at chunk coordinates: %d, %d, %d", chunk_x, chunk_y, chunk_z);
        return;
    }

    std::vector<terrain_vertex_t> vtx;

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z; dat_it++)
    {
        /* This is to keep the loop body 8 spaces closer to the left margin */
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        block_id_t type = rubik[1][1][1]->get_type(x, y, z);
        if (type == BLOCK_ID_AIR)
            continue;

        Uint8 metadata = rubik[1][1][1]->get_metadata(x, y, z);

        float r = float((type) & 3) / 3.0f, g = float((type >> 2) & 3) / 3.0f, b = float((type >> 4) & 3) / 3.0f;
        r = 1.0, g = 1.0, b = 1.0;

        /** Index: [x+1][y+1][z+1] */
        block_id_t stypes[3][3][3];
        Uint8 slight_block[3][3][3];
        Uint8 slight_sky[3][3][3];
        for (int i = -1; i < 2; i++)
        {
            for (int j = -1; j < 2; j++)
            {
                for (int k = -1; k < 2; k++)
                {
                    int chunk_ix = 1, chunk_iy = 1, chunk_iz = 1;
                    int local_x = x + i, local_y = y + j, local_z = z + k;

                    if (local_x < 0)
                    {
                        local_x += SUBCHUNK_SIZE_X;
                        chunk_ix--;
                    }
                    else if (local_x >= SUBCHUNK_SIZE_X)
                    {
                        local_x -= SUBCHUNK_SIZE_X;
                        chunk_ix++;
                    }

                    if (local_y < 0)
                    {
                        local_y += SUBCHUNK_SIZE_Y;
                        chunk_iy--;
                    }
                    else if (local_y >= SUBCHUNK_SIZE_Y)
                    {
                        local_y -= SUBCHUNK_SIZE_Y;
                        chunk_iy++;
                    }

                    if (local_z < 0)
                    {
                        local_z += SUBCHUNK_SIZE_Z;
                        chunk_iz--;
                    }
                    else if (local_z >= SUBCHUNK_SIZE_Z)
                    {
                        local_z -= SUBCHUNK_SIZE_Z;
                        chunk_iz++;
                    }

                    chunk_cubic_t* c = rubik[chunk_ix][chunk_iy][chunk_iz];
                    stypes[i + 1][j + 1][k + 1] = c == NULL ? BLOCK_ID_AIR : c->get_type_fallback(local_x, local_y, local_z, BLOCK_ID_AIR);
                    slight_block[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_light_block(local_x, local_y, local_z);
                    slight_sky[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_light_sky(local_x, local_y, local_z);
                }
            }
        }

        /**
         * Ordered +XYZ then -XYZ for simple blocks
         */
        mc_id::terrain_face_t faces[6];
#define BLOCK_SIMPLE(ID_BLOCK, ID_FACE)        \
    case ID_BLOCK:                             \
    {                                          \
        faces[0] = terrain->get_face(ID_FACE); \
        faces[1] = faces[0];                   \
        faces[2] = faces[0];                   \
        faces[3] = faces[0];                   \
        faces[4] = faces[0];                   \
        faces[5] = faces[0];                   \
        break;                                 \
    }
#define BLOCK_TNT(ID_BLOCK, ID_FACE_TOP, ID_FACE_BOTTOM, ID_FACE_SIDE) \
    case ID_BLOCK:                                                     \
    {                                                                  \
        faces[0] = terrain->get_face(ID_FACE_SIDE);                    \
        faces[1] = terrain->get_face(ID_FACE_TOP);                     \
        faces[2] = faces[0];                                           \
        faces[3] = faces[0];                                           \
        faces[4] = terrain->get_face(ID_FACE_BOTTOM);                  \
        faces[5] = faces[0];                                           \
        break;                                                         \
    }
#define BLOCK_LOG(ID_BLOCK, ID_FACE_TOP, ID_FACE_SIDE) \
    case ID_BLOCK:                                     \
    {                                                  \
        faces[0] = terrain->get_face(ID_FACE_SIDE);    \
        faces[1] = terrain->get_face(ID_FACE_TOP);     \
        faces[2] = faces[0];                           \
        faces[3] = faces[0];                           \
        faces[4] = faces[1];                           \
        faces[5] = faces[0];                           \
        break;                                         \
    }

        switch ((item_id_t_)type)
        {
            BLOCK_SIMPLE(BLOCK_ID_AIR, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STONE, mc_id::FACE_STONE);
            BLOCK_SIMPLE(BLOCK_ID_GRASS, mc_id::FACE_GRASS_TOP);
            BLOCK_SIMPLE(BLOCK_ID_DIRT, mc_id::FACE_DIRT);
            BLOCK_SIMPLE(BLOCK_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
            BLOCK_SIMPLE(BLOCK_ID_WOOD_PLANKS, mc_id::FACE_PLANKS_OAK);
            BLOCK_SIMPLE(BLOCK_ID_SAPLING, mc_id::FACE_SAPLING_OAK); //----
            BLOCK_SIMPLE(BLOCK_ID_BEDROCK, mc_id::FACE_BEDROCK);

            BLOCK_SIMPLE(BLOCK_ID_WATER_FLOWING, mc_id::FACE_WATER_FLOW_STRAIGHT);
            BLOCK_SIMPLE(BLOCK_ID_WATER_SOURCE, mc_id::FACE_WATER_STILL);
            BLOCK_SIMPLE(BLOCK_ID_LAVA_FLOWING, mc_id::FACE_LAVA_FLOW_STRAIGHT);
            BLOCK_SIMPLE(BLOCK_ID_LAVA_SOURCE, mc_id::FACE_LAVA_STILL);

            BLOCK_SIMPLE(BLOCK_ID_SAND, mc_id::FACE_SAND);
            BLOCK_SIMPLE(BLOCK_ID_GRAVEL, mc_id::FACE_GRAVEL);
            BLOCK_SIMPLE(BLOCK_ID_ORE_GOLD, mc_id::FACE_GOLD_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_IRON, mc_id::FACE_IRON_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_COAL, mc_id::FACE_COAL_ORE);
            BLOCK_LOG(BLOCK_ID_LOG, mc_id::FACE_LOG_OAK_TOP, mc_id::FACE_LOG_OAK); //----
            BLOCK_SIMPLE(BLOCK_ID_LEAVES, mc_id::FACE_LEAVES_OAK);
            BLOCK_SIMPLE(BLOCK_ID_SPONGE, mc_id::FACE_SPONGE);
            BLOCK_SIMPLE(BLOCK_ID_GLASS, mc_id::FACE_GLASS);
            BLOCK_SIMPLE(BLOCK_ID_ORE_LAPIS, mc_id::FACE_LAPIS_ORE);

            BLOCK_SIMPLE(BLOCK_ID_LAPIS, mc_id::FACE_LAPIS_BLOCK);
            BLOCK_SIMPLE(BLOCK_ID_DISPENSER, mc_id::FACE_DISPENSER_FRONT_HORIZONTAL);
            BLOCK_TNT(BLOCK_ID_SANDSTONE, mc_id::FACE_SANDSTONE_TOP, mc_id::FACE_SANDSTONE_BOTTOM, mc_id::FACE_SANDSTONE_NORMAL)
            BLOCK_SIMPLE(BLOCK_ID_NOTE_BLOCK, mc_id::FACE_NOTEBLOCK);
            BLOCK_SIMPLE(BLOCK_ID_BED, mc_id::FACE_BED_HEAD_TOP);
            BLOCK_SIMPLE(BLOCK_ID_RAIL_POWERED, mc_id::FACE_RAIL_GOLDEN_POWERED);
            BLOCK_SIMPLE(BLOCK_ID_RAIL_DETECTOR, mc_id::FACE_RAIL_DETECTOR);
            BLOCK_SIMPLE(BLOCK_ID_PISTON_STICKY, mc_id::FACE_PISTON_TOP_STICKY);
            BLOCK_SIMPLE(BLOCK_ID_COBWEB, mc_id::FACE_WEB); //----
            BLOCK_SIMPLE(BLOCK_ID_FOLIAGE, mc_id::FACE_TALLGRASS);
            BLOCK_SIMPLE(BLOCK_ID_DEAD_BUSH, mc_id::FACE_DEADBUSH);
            BLOCK_SIMPLE(BLOCK_ID_PISTON, mc_id::FACE_PISTON_TOP_NORMAL);
            BLOCK_SIMPLE(BLOCK_ID_PISTON_HEAD, mc_id::FACE_PISTON_TOP_NORMAL);
            BLOCK_SIMPLE(BLOCK_ID_WOOL, mc_id::FACE_WOOL_COLORED_WHITE);

            BLOCK_SIMPLE(BLOCK_ID_UNKNOWN, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_FLOWER_YELLOW, mc_id::FACE_FLOWER_DANDELION);
            BLOCK_SIMPLE(BLOCK_ID_FLOWER_RED, mc_id::FACE_FLOWER_ROSE);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_BLAND, mc_id::FACE_MUSHROOM_BROWN);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_RED, mc_id::FACE_MUSHROOM_RED);

            BLOCK_SIMPLE(BLOCK_ID_GOLD, mc_id::FACE_GOLD_BLOCK);
            BLOCK_SIMPLE(BLOCK_ID_IRON, mc_id::FACE_IRON_BLOCK);

            BLOCK_SIMPLE(BLOCK_ID_SLAB_DOUBLE, mc_id::FACE_STONE_SLAB_SIDE);
            BLOCK_SIMPLE(BLOCK_ID_SLAB_SINGLE, mc_id::FACE_STONE_SLAB_SIDE);
            BLOCK_SIMPLE(BLOCK_ID_BRICKS, mc_id::FACE_BRICK);
            BLOCK_TNT(BLOCK_ID_TNT, mc_id::FACE_TNT_TOP, mc_id::FACE_TNT_BOTTOM, mc_id::FACE_TNT_SIDE)
            BLOCK_SIMPLE(BLOCK_ID_BOOKSHELF, mc_id::FACE_BOOKSHELF);
            BLOCK_SIMPLE(BLOCK_ID_COBBLESTONE_MOSSY, mc_id::FACE_COBBLESTONE_MOSSY);
            BLOCK_SIMPLE(BLOCK_ID_OBSIDIAN, mc_id::FACE_OBSIDIAN);

            BLOCK_SIMPLE(BLOCK_ID_TORCH, mc_id::FACE_TORCH_ON);
            BLOCK_SIMPLE(BLOCK_ID_FIRE, mc_id::FACE_FIRE_LAYER_0);
            BLOCK_SIMPLE(BLOCK_ID_SPAWNER, mc_id::FACE_MOB_SPAWNER);

            BLOCK_SIMPLE(BLOCK_ID_STAIRS_WOOD, mc_id::FACE_PLANKS_OAK); //----
            BLOCK_SIMPLE(BLOCK_ID_CHEST, mc_id::FACE_DEBUG); //----
            BLOCK_SIMPLE(BLOCK_ID_REDSTONE, mc_id::FACE_REDSTONE_DUST_LINE); //----
            BLOCK_SIMPLE(BLOCK_ID_ORE_DIAMOND, mc_id::FACE_DIAMOND_ORE);
            BLOCK_SIMPLE(BLOCK_ID_DIAMOND, mc_id::FACE_DIAMOND_BLOCK);
        case BLOCK_ID_CRAFTING_TABLE:
        {
            faces[0] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_FRONT);
            faces[1] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_FRONT);
            faces[4] = terrain->get_face(mc_id::FACE_PLANKS_OAK);
            faces[5] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_PLANT_FOOD, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_DIRT_TILLED, mc_id::FACE_DEBUG);
        case BLOCK_ID_FURNACE_OFF:
        {
            faces[0] = terrain->get_face(mc_id::FACE_FURNACE_FRONT_OFF);
            faces[1] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_FURNACE_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_FURNACE_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[5] = terrain->get_face(mc_id::FACE_FURNACE_SIDE);
            break;
        }
        case BLOCK_ID_FURNACE_ON:
        {
            faces[0] = terrain->get_face(mc_id::FACE_FURNACE_FRONT_ON);
            faces[1] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_FURNACE_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_FURNACE_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[5] = terrain->get_face(mc_id::FACE_FURNACE_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_SIGN_STANDING, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_DOOR_WOOD, mc_id::FACE_DOOR_WOOD_UPPER);
            BLOCK_SIMPLE(BLOCK_ID_LADDER, mc_id::FACE_LADDER);
            BLOCK_SIMPLE(BLOCK_ID_RAIL, mc_id::FACE_RAIL_NORMAL);
            BLOCK_SIMPLE(BLOCK_ID_STAIRS_COBBLESTONE, mc_id::FACE_COBBLESTONE); //----
            BLOCK_SIMPLE(BLOCK_ID_SIGN_WALL, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_LEVER, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_PRESSURE_PLATE_STONE, mc_id::FACE_STONE);
            BLOCK_SIMPLE(BLOCK_ID_DOOR_IRON, mc_id::FACE_DOOR_IRON_UPPER);
            BLOCK_SIMPLE(BLOCK_ID_PRESSURE_PLATE_WOOD, mc_id::FACE_PLANKS_OAK);
            BLOCK_SIMPLE(BLOCK_ID_ORE_REDSTONE_OFF, mc_id::FACE_REDSTONE_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_REDSTONE_ON, mc_id::FACE_REDSTONE_ORE);
            BLOCK_SIMPLE(BLOCK_ID_TORCH_REDSTONE_OFF, mc_id::FACE_REDSTONE_TORCH_OFF);
            BLOCK_SIMPLE(BLOCK_ID_TORCH_REDSTONE_ON, mc_id::FACE_REDSTONE_TORCH_ON);
            BLOCK_SIMPLE(BLOCK_ID_BUTTON_STONE, mc_id::FACE_STONE);
            BLOCK_SIMPLE(BLOCK_ID_SNOW, mc_id::FACE_SNOW);
            BLOCK_SIMPLE(BLOCK_ID_ICE, mc_id::FACE_ICE);
            BLOCK_SIMPLE(BLOCK_ID_SNOW_BLOCK, mc_id::FACE_SNOW);
            BLOCK_SIMPLE(BLOCK_ID_CACTUS, mc_id::FACE_CACTUS_SIDE);
            BLOCK_SIMPLE(BLOCK_ID_CLAY, mc_id::FACE_CLAY);
            BLOCK_SIMPLE(BLOCK_ID_SUGAR_CANE, mc_id::FACE_DEBUG);
        case BLOCK_ID_JUKEBOX:
        {
            faces[0] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            faces[1] = terrain->get_face(mc_id::FACE_JUKEBOX_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            faces[5] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_FENCE_WOOD, mc_id::FACE_DEBUG);
        case BLOCK_ID_PUMPKIN:
        {
            faces[0] = terrain->get_face(mc_id::FACE_PUMPKIN_FACE_OFF);
            faces[1] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[5] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_NETHERRACK, mc_id::FACE_NETHERRACK);
            BLOCK_SIMPLE(BLOCK_ID_SOULSAND, mc_id::FACE_SOUL_SAND);
            BLOCK_SIMPLE(BLOCK_ID_GLOWSTONE, mc_id::FACE_GLOWSTONE);
            BLOCK_SIMPLE(BLOCK_ID_NETHER_PORTAL, mc_id::FACE_PORTAL);
        case BLOCK_ID_PUMPKIN_GLOWING:
        {
            faces[0] = terrain->get_face(mc_id::FACE_PUMPKIN_FACE_ON);
            faces[1] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[5] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_CAKE, mc_id::FACE_CAKE_TOP);
            BLOCK_SIMPLE(BLOCK_ID_REPEATER_OFF, mc_id::FACE_REPEATER_OFF);
            BLOCK_SIMPLE(BLOCK_ID_REPEATER_ON, mc_id::FACE_REPEATER_ON);
            BLOCK_SIMPLE(BLOCK_ID_CHEST_LOCKED, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_TRAPDOOR, mc_id::FACE_TRAPDOOR);
            BLOCK_SIMPLE(BLOCK_ID_UNKNOWN_STONE, mc_id::FACE_STONE);
            BLOCK_SIMPLE(BLOCK_ID_BRICKS_STONE, mc_id::FACE_STONEBRICK);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_BLOCK_BLAND, mc_id::FACE_MUSHROOM_BLOCK_SKIN_BROWN);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_BLOCK_RED, mc_id::FACE_MUSHROOM_BLOCK_SKIN_RED);
            BLOCK_SIMPLE(BLOCK_ID_IRON_BARS, mc_id::FACE_IRON_BARS);
            BLOCK_SIMPLE(BLOCK_ID_GLASS_PANE, mc_id::FACE_GLASS);
            BLOCK_LOG(BLOCK_ID_MELON, mc_id::FACE_MELON_TOP, mc_id::FACE_MELON_SIDE)
            BLOCK_SIMPLE(BLOCK_ID_STEM_PUMPKIN, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STEM_MELON, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_MOSS, mc_id::FACE_VINE);
            BLOCK_SIMPLE(BLOCK_ID_GATE, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STAIRS_BRICK, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STAIRS_BRICK_STONE, mc_id::FACE_DEBUG);
        default:
        {
            faces[0] = terrain->get_face(mc_id::FACE_DEBUG);
            faces[1] = faces[0];
            faces[2] = faces[0];
            faces[3] = faces[0];
            faces[4] = faces[0];
            faces[5] = faces[0];
            break;
        }
        }

        if (type == BLOCK_ID_GRASS || type == BLOCK_ID_LEAVES || type == BLOCK_ID_MOSS || type == BLOCK_ID_FOLIAGE)
            r = 0.2f, g = 1.0f, b = 0.2f;

#define UAO(X) (!mc_id::is_transparent(stypes X))
#define UBL(X) (slight_block X * mc_id::is_transparent(stypes X))

        /* Positive Y */
        if (mc_id::is_transparent(stypes[1][2][1]) && stypes[1][2][1] != type)
        {
            Uint8 ao[] = {
                Uint8(UAO([0][2][0]) + UAO([1][2][0]) + UAO([0][2][1])),
                Uint8(UAO([2][2][0]) + UAO([1][2][0]) + UAO([2][2][1])),
                Uint8(UAO([0][2][2]) + UAO([0][2][1]) + UAO([1][2][2])),
                Uint8(UAO([2][2][2]) + UAO([1][2][2]) + UAO([2][2][1])),
            };

            Uint8 bl[] = {
                Uint8((UBL([0][2][0]) + UBL([1][2][0]) + UBL([0][2][1]) + slight_block[1][2][1]) / (4 - ao[0])),
                Uint8((UBL([2][2][0]) + UBL([1][2][0]) + UBL([2][2][1]) + slight_block[1][2][1]) / (4 - ao[1])),
                Uint8((UBL([0][2][2]) + UBL([0][2][1]) + UBL([1][2][2]) + slight_block[1][2][1]) / (4 - ao[2])),
                Uint8((UBL([2][2][2]) + UBL([1][2][2]) + UBL([2][2][1]) + slight_block[1][2][1]) / (4 - ao[3])),
            };

            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] },
                { r, g, b, bl[3], slight_sky[1][2][1] },
                faces[1].corners[0],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[1] },
                { r, g, b, bl[1], slight_sky[1][2][1] },
                faces[1].corners[2],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[2] },
                { r, g, b, bl[2], slight_sky[1][2][1] },
                faces[1].corners[1],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[0] },
                { r, g, b, bl[0], slight_sky[1][2][1] },
                faces[1].corners[3],
            });
        }

        /* Negative Y */
        if (mc_id::is_transparent(stypes[1][0][1]) && stypes[1][0][1] != type)
        {
            Uint8 ao[] = {
                Uint8(UAO([0][0][0]) + UAO([1][0][0]) + UAO([0][0][1])),
                Uint8(UAO([2][0][0]) + UAO([1][0][0]) + UAO([2][0][1])),
                Uint8(UAO([0][0][2]) + UAO([0][0][1]) + UAO([1][0][2])),
                Uint8(UAO([2][0][2]) + UAO([1][0][2]) + UAO([2][0][1])),
            };

            Uint8 bl[] = {
                Uint8((UBL([0][0][0]) + UBL([1][0][0]) + UBL([0][0][1]) + slight_block[1][0][1]) / (4 - ao[0])),
                Uint8((UBL([2][0][0]) + UBL([1][0][0]) + UBL([2][0][1]) + slight_block[1][0][1]) / (4 - ao[1])),
                Uint8((UBL([0][0][2]) + UBL([0][0][1]) + UBL([1][0][2]) + slight_block[1][0][1]) / (4 - ao[2])),
                Uint8((UBL([2][0][2]) + UBL([1][0][2]) + UBL([2][0][1]) + slight_block[1][0][1]) / (4 - ao[3])),
            };

            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] },
                { r, g, b, bl[0], slight_sky[1][0][1] },
                faces[4].corners[1],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[1] },
                { r, g, b, bl[1], slight_sky[1][0][1] },
                faces[4].corners[0],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[2] },
                { r, g, b, bl[2], slight_sky[1][0][1] },
                faces[4].corners[3],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[3] },
                { r, g, b, bl[3], slight_sky[1][0][1] },
                faces[4].corners[2],
            });
        }

        /* Positive X */
        if (mc_id::is_transparent(stypes[2][1][1]) && stypes[2][1][1] != type)
        {
            Uint8 ao[] = {
                Uint8(UAO([2][0][0]) + UAO([2][1][0]) + UAO([2][0][1])),
                Uint8(UAO([2][2][0]) + UAO([2][1][0]) + UAO([2][2][1])),
                Uint8(UAO([2][0][2]) + UAO([2][0][1]) + UAO([2][1][2])),
                Uint8(UAO([2][2][2]) + UAO([2][1][2]) + UAO([2][2][1])),
            };

            Uint8 bl[] = {
                Uint8((UBL([2][0][0]) + UBL([2][0][1]) + UBL([2][1][0]) + slight_block[2][1][1]) / (4 - ao[0])),
                Uint8((UBL([2][2][0]) + UBL([2][2][1]) + UBL([2][1][0]) + slight_block[2][1][1]) / (4 - ao[1])),
                Uint8((UBL([2][0][2]) + UBL([2][0][1]) + UBL([2][1][2]) + slight_block[2][1][1]) / (4 - ao[2])),
                Uint8((UBL([2][2][2]) + UBL([2][2][1]) + UBL([2][1][2]) + slight_block[2][1][1]) / (4 - ao[3])),
            };

            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[0] },
                { r, g, b, bl[0], slight_sky[2][1][1] },
                faces[0].corners[3],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[1] },
                { r, g, b, bl[1], slight_sky[2][1][1] },
                faces[0].corners[1],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[2] },
                { r, g, b, bl[2], slight_sky[2][1][1] },
                faces[0].corners[2],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] },
                { r, g, b, bl[3], slight_sky[2][1][1] },
                faces[0].corners[0],
            });
        }

        /* Negative X */
        if (mc_id::is_transparent(stypes[0][1][1]) && stypes[0][1][1] != type)
        {
            Uint8 ao[] = {
                Uint8(UAO([0][0][0]) + UAO([0][1][0]) + UAO([0][0][1])),
                Uint8(UAO([0][2][0]) + UAO([0][1][0]) + UAO([0][2][1])),
                Uint8(UAO([0][0][2]) + UAO([0][0][1]) + UAO([0][1][2])),
                Uint8(UAO([0][2][2]) + UAO([0][1][2]) + UAO([0][2][1])),
            };

            Uint8 bl[] = {
                Uint8((UBL([0][0][0]) + UBL([0][1][0]) + UBL([0][0][1]) + slight_block[0][1][1]) / (4 - ao[0])),
                Uint8((UBL([0][2][0]) + UBL([0][1][0]) + UBL([0][2][1]) + slight_block[0][1][1]) / (4 - ao[1])),
                Uint8((UBL([0][0][2]) + UBL([0][0][1]) + UBL([0][1][2]) + slight_block[0][1][1]) / (4 - ao[2])),
                Uint8((UBL([0][2][2]) + UBL([0][1][2]) + UBL([0][2][1]) + slight_block[0][1][1]) / (4 - ao[3])),
            };

            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[3] },
                { r, g, b, bl[3], slight_sky[0][1][1] },
                faces[3].corners[1],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[1] },
                { r, g, b, bl[1], slight_sky[0][1][1] },
                faces[3].corners[0],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[2] },
                { r, g, b, bl[2], slight_sky[0][1][1] },
                faces[3].corners[3],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] },
                { r, g, b, bl[0], slight_sky[0][1][1] },
                faces[3].corners[2],
            });
        }

        /* Positive Z */
        if (mc_id::is_transparent(stypes[1][1][2]) && stypes[1][1][2] != type)
        {
            Uint8 ao[] = {
                Uint8(UAO([0][0][2]) + UAO([1][0][2]) + UAO([0][1][2])),
                Uint8(UAO([0][2][2]) + UAO([0][1][2]) + UAO([1][2][2])),
                Uint8(UAO([2][0][2]) + UAO([1][0][2]) + UAO([2][1][2])),
                Uint8(UAO([2][2][2]) + UAO([1][2][2]) + UAO([2][1][2])),
            };

            Uint8 bl[] = {
                Uint8((UBL([0][0][2]) + UBL([1][0][2]) + UBL([0][1][2]) + slight_block[1][1][2]) / (4 - ao[0])),
                Uint8((UBL([0][2][2]) + UBL([0][1][2]) + UBL([1][2][2]) + slight_block[1][1][2]) / (4 - ao[1])),
                Uint8((UBL([2][0][2]) + UBL([1][0][2]) + UBL([2][1][2]) + slight_block[1][1][2]) / (4 - ao[2])),
                Uint8((UBL([2][2][2]) + UBL([1][2][2]) + UBL([2][1][2]) + slight_block[1][1][2]) / (4 - ao[3])),
            };

            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] },
                { r, g, b, bl[3], slight_sky[1][1][2] },
                faces[2].corners[1],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[1] },
                { r, g, b, bl[1], slight_sky[1][1][2] },
                faces[2].corners[0],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[2] },
                { r, g, b, bl[2], slight_sky[1][1][2] },
                faces[2].corners[3],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[0] },
                { r, g, b, bl[0], slight_sky[1][1][2] },
                faces[2].corners[2],
            });
        }

        /* Negative Z */
        if (mc_id::is_transparent(stypes[1][1][0]) && stypes[1][1][0] != type)
        {
            Uint8 ao[] = {
                Uint8(UAO([0][0][0]) + UAO([1][0][0]) + UAO([0][1][0])),
                Uint8(UAO([0][2][0]) + UAO([0][1][0]) + UAO([1][2][0])),
                Uint8(UAO([2][0][0]) + UAO([1][0][0]) + UAO([2][1][0])),
                Uint8(UAO([2][2][0]) + UAO([1][2][0]) + UAO([2][1][0])),
            };

            Uint8 bl[] = {
                Uint8((UBL([0][0][0]) + UBL([1][0][0]) + UBL([0][1][0]) + slight_block[1][1][0]) / (4 - ao[0])),
                Uint8((UBL([0][2][0]) + UBL([0][1][0]) + UBL([1][2][0]) + slight_block[1][1][0]) / (4 - ao[1])),
                Uint8((UBL([2][0][0]) + UBL([1][0][0]) + UBL([2][1][0]) + slight_block[1][1][0]) / (4 - ao[2])),
                Uint8((UBL([2][2][0]) + UBL([1][2][0]) + UBL([2][1][0]) + slight_block[1][1][0]) / (4 - ao[3])),
            };

            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] },
                { r, g, b, bl[0], slight_sky[1][1][0] },
                faces[5].corners[3],
            });
            vtx.push_back({
                { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[1] },
                { r, g, b, bl[1], slight_sky[1][1][0] },
                faces[5].corners[1],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[2] },
                { r, g, b, bl[2], slight_sky[1][1][0] },
                faces[5].corners[2],
            });
            vtx.push_back({
                { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[3] },
                { r, g, b, bl[3], slight_sky[1][1][0] },
                faces[5].corners[0],
            });
        }
    }

    TRACE("Chunk: <%d, %d, %d>, Vertices: %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx.size(), vtx.size() / 4 * 6);

    if (!center->vao)
        terrain_vertex_t::create_vao(&center->vao);
    glBindVertexArray(center->vao);
    // TODO: Recycle VBO, and use one global chunk EBO
    center->index_type = terrain_vertex_t::create_vbo(&center->vbo, &center->ebo, vtx);
    center->index_count = vtx.size() / 4 * 6;
}

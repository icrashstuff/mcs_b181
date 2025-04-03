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
#include "migration_gl.h"

#include "level.h"

#include <algorithm>
#include <glm/ext/matrix_transform.hpp>

#include "tetra/util/convar.h"
#include <SDL3/SDL.h>

#ifndef NDEBUG
#define FORCE_OPT_MESH 1
#else
#define FORCE_OPT_MESH 0
#endif

static convar_int_t cvr_r_smooth_lighting {
    "r_smooth_lighting",
    1,
    0,
    1,
    "Use smooth lighting",
    CONVAR_FLAG_SAVE,
};

static convar_int_t cvr_r_biome_oversample {
    "r_biome_blend_limit",
    0,
    0,
    16,
    "Limit of biome blending",
    CONVAR_FLAG_SAVE,
};

#if (FORCE_OPT_MESH)
#pragma GCC push_options
#pragma GCC optimize("Og")
#endif

void level_t::build_mesh(chunk_cubic_t* const center)
{
    if (!center)
    {
        dc_log_error("Attempt made to mesh NULL chunk");
        return;
    }

    if (!terrain)
    {
        dc_log_error("A texture atlas is required to build a chunk");
        return;
    }

    const int chunk_x = center->pos.x;
    const int chunk_y = center->pos.y;
    const int chunk_z = center->pos.z;

    /** Index: [x+1][y+1][z+1] */
    chunk_cubic_t* rubik[3][3][3] = { { { 0 }, { 0 }, { 0 } }, { { 0 }, { 0 }, { 0 } }, { { 0 }, { 0 }, { 0 } } };

    rubik[1][1][1] = center;

    rubik[2][1][1] = center->neighbors.pos_x;
    rubik[1][2][1] = center->neighbors.pos_y;
    rubik[1][1][2] = center->neighbors.pos_z;
    rubik[0][1][1] = center->neighbors.neg_x;
    rubik[1][0][1] = center->neighbors.neg_y;
    rubik[1][1][0] = center->neighbors.neg_z;

    for (int i = 0; i < 27; i++)
    {
        const int ix = (i / 9) % 3 - 1;
        const int iy = (i / 3) % 3 - 1;
        const int iz = i % 3 - 1;

        /* Skip the cells we have already assigned */
        if (abs(ix) + abs(iy) + abs(iz) <= 1)
            continue;
        assert(rubik[ix + 1][iy + 1][iz + 1] == NULL);

        rubik[ix + 1][iy + 1][iz + 1] = center->find_chunk(center, center->pos + glm::ivec3(ix, iy, iz));
    }

    std::vector<terrain_vertex_t> vtx_solid;
    std::vector<terrain_vertex_t> vtx_overlay;
    std::vector<terrain_vertex_t> vtx_translucent;

    bool is_transparent[256];
    bool is_translucent[256];
    bool is_leaves_style_transparent[256];

    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    for (int i = 0; i < IM_ARRAYSIZE(is_translucent); i++)
        is_translucent[i] = mc_id::is_translucent(i);

    for (int i = 0; i < IM_ARRAYSIZE(is_leaves_style_transparent); i++)
        is_leaves_style_transparent[i] = mc_id::is_leaves_style_transparent(i);

    /** Index: [x + 1][y + 1] */
    glm::vec3 biome_colors[18][18];
    float biome_temperature[18][18];
    float biome_downfall[18][18];
    generate_climate_colors(center->pos, biome_colors, biome_temperature, biome_downfall);

    /** Index: [x+1][y+1][z+1] */
    block_id_t stypes[3][3][3];
    Uint8 smetadata[3][3][3];
    Uint8 slight_block[3][3][3];
    Uint8 slight_sky[3][3][3];

    bool skipped = true;

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z; dat_it++)
    {
        /* This is to keep the loop body 8 spaces closer to the left margin */
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        block_id_t type = rubik[1][1][1]->get_type(x, y, z);
        if (type == BLOCK_ID_AIR)
        {
            skipped = true;
            continue;
        }

        std::vector<terrain_vertex_t>* vtx = is_translucent[type] ? &vtx_translucent : &vtx_solid;

        float r = 1.0f, g = 1.0f, b = 1.0f;
        float r_overlay = r, g_overlay = g, b_overlay = b;
        float r_0x_0z = r, g_0x_0z = g, b_0x_0z = b;
        float r_0x_1z = r, g_0x_1z = g, b_0x_1z = b;
        float r_1x_0z = r, g_1x_0z = g, b_1x_0z = b;
        float r_1x_1z = r, g_1x_1z = g, b_1x_1z = b;
        float r_overlay_0x_0z = r, g_overlay_0x_0z = g, b_overlay_0x_0z = b;
        float r_overlay_0x_1z = r, g_overlay_0x_1z = g, b_overlay_0x_1z = b;
        float r_overlay_1x_0z = r, g_overlay_1x_0z = g, b_overlay_1x_0z = b;
        float r_overlay_1x_1z = r, g_overlay_1x_1z = g, b_overlay_1x_1z = b;

        /* std::move does actually increase speed in non-debug situations, don't remove it */
#define SHIFT_BLOCK_INFO(ORG, NEW)                      \
    do                                                  \
    {                                                   \
        stypes NEW = std::move(stypes ORG);             \
        smetadata NEW = std::move(smetadata ORG);       \
        slight_block NEW = std::move(slight_block ORG); \
        slight_sky NEW = std::move(slight_sky ORG);     \
    } while (0)

        /* The last set of block data was in a different vertical slice, therefore the last block should be considered skipped */
        if (y == 0)
            skipped = true;

        /* Shift data from previous slices */
        if (!skipped)
        {
            SHIFT_BLOCK_INFO([0][1][0], [0][0][0]);
            SHIFT_BLOCK_INFO([1][1][0], [1][0][0]);
            SHIFT_BLOCK_INFO([2][1][0], [2][0][0]);
            SHIFT_BLOCK_INFO([0][1][1], [0][0][1]);
            SHIFT_BLOCK_INFO([1][1][1], [1][0][1]);
            SHIFT_BLOCK_INFO([2][1][1], [2][0][1]);
            SHIFT_BLOCK_INFO([0][1][2], [0][0][2]);
            SHIFT_BLOCK_INFO([1][1][2], [1][0][2]);
            SHIFT_BLOCK_INFO([2][1][2], [2][0][2]);

            SHIFT_BLOCK_INFO([0][2][0], [0][1][0]);
            SHIFT_BLOCK_INFO([1][2][0], [1][1][0]);
            SHIFT_BLOCK_INFO([2][2][0], [2][1][0]);
            SHIFT_BLOCK_INFO([0][2][1], [0][1][1]);
            SHIFT_BLOCK_INFO([1][2][1], [1][1][1]);
            SHIFT_BLOCK_INFO([2][2][1], [2][1][1]);
            SHIFT_BLOCK_INFO([0][2][2], [0][1][2]);
            SHIFT_BLOCK_INFO([1][2][2], [1][1][2]);
            SHIFT_BLOCK_INFO([2][2][2], [2][1][2]);
        }
#undef MOVE_WINDOW

        /* When the last block was not skipped then the last slices are valid we do no need to collect the slices of information at y-1 or at y */
        for (int j = ((skipped) ? -1 : 1); j < 2; j++)
        {
            int chunk_iy = 1;
            int local_y = y + j;

            switch (local_y)
            {
            case -1:
                local_y = SUBCHUNK_SIZE_Y - 1;
                chunk_iy--;
                break;
            case SUBCHUNK_SIZE_Y:
                local_y = 0;
                chunk_iy++;
                break;
            default:
                break;
            }

            for (int i = -1; i < 2; i++)
            {
                for (int k = -1; k < 2; k++)
                {
                    int chunk_ix = 1, chunk_iz = 1;
                    int local_x = x + i, local_z = z + k;

                    switch (local_x)
                    {
                    case -1:
                        local_x = SUBCHUNK_SIZE_X - 1;
                        chunk_ix--;
                        break;
                    case SUBCHUNK_SIZE_X:
                        local_x = 0;
                        chunk_ix++;
                        break;
                    default:
                        break;
                    }

                    switch (local_z)
                    {
                    case -1:
                        local_z = SUBCHUNK_SIZE_Z - 1;
                        chunk_iz--;
                        break;
                    case SUBCHUNK_SIZE_Z:
                        local_z = 0;
                        chunk_iz++;
                        break;
                    default:
                        break;
                    }

                    chunk_cubic_t* c = rubik[chunk_ix][chunk_iy][chunk_iz];
                    stypes[i + 1][j + 1][k + 1] = c == NULL ? BLOCK_ID_AIR : c->get_type(local_x, local_y, local_z);
                    smetadata[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_metadata(local_x, local_y, local_z);
                    slight_block[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_light_block(local_x, local_y, local_z);
                    slight_sky[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_light_sky(local_x, local_y, local_z);
                }
            }
        }
        skipped = false;

        Uint8 metadata = smetadata[1][1][1];

        /** Ordered +XYZ then -XYZ for simple blocks */
        mc_id::terrain_face_t faces[6];
        /** Ordered +XYZ then -XYZ for simple blocks */
        mc_id::terrain_face_t faces_overlay[6];
        /** Ordered +XYZ then -XYZ for simple blocks */
        bool use_overlay[6] = { 0 };
#define BLOCK_SIMPLE_NO_CASE(ID_FACE)          \
    do                                         \
    {                                          \
        faces[0] = terrain->get_face(ID_FACE); \
        faces[1] = faces[0];                   \
        faces[2] = faces[0];                   \
        faces[3] = faces[0];                   \
        faces[4] = faces[0];                   \
        faces[5] = faces[0];                   \
    } while (0)
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
        case BLOCK_ID_GRASS:
        {
            BLOCK_SIMPLE_NO_CASE(mc_id::FACE_DIRT);

            faces_overlay[0] = terrain->get_face(mc_id::FACE_GRASS_SIDE_OVERLAY);
            faces_overlay[1] = terrain->get_face(mc_id::FACE_GRASS_TOP);
            faces_overlay[2] = faces_overlay[0];
            faces_overlay[3] = faces_overlay[0];
            faces_overlay[5] = faces_overlay[0];

            use_overlay[0] = true;
            use_overlay[1] = true;
            use_overlay[2] = true;
            use_overlay[3] = true;
            use_overlay[5] = true;

            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_DIRT, mc_id::FACE_DIRT);
            BLOCK_SIMPLE(BLOCK_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
        case BLOCK_ID_WOOD_PLANKS:
        {
            /* Multiple plank types are not in Minecraft Beta 1.8.x */
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_PLANKS_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_PLANKS_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_PLANKS_OAK);
            }
            break;
        }
        case BLOCK_ID_SAPLING:
        {
            switch (metadata % 4)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_SAPLING_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_SAPLING_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_SAPLING_OAK);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_BEDROCK, mc_id::FACE_BEDROCK);

            // BLOCK_SIMPLE(BLOCK_ID_WATER_FLOWING, mc_id::FACE_WATER_FLOW);
            // BLOCK_SIMPLE(BLOCK_ID_WATER_SOURCE, mc_id::FACE_WATER_STILL);
            // BLOCK_SIMPLE(BLOCK_ID_LAVA_FLOWING, mc_id::FACE_LAVA_FLOW);
            // BLOCK_SIMPLE(BLOCK_ID_LAVA_SOURCE, mc_id::FACE_LAVA_STILL);

            BLOCK_SIMPLE(BLOCK_ID_SAND, mc_id::FACE_SAND);
            BLOCK_SIMPLE(BLOCK_ID_GRAVEL, mc_id::FACE_GRAVEL);
            BLOCK_SIMPLE(BLOCK_ID_ORE_GOLD, mc_id::FACE_GOLD_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_IRON, mc_id::FACE_IRON_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_COAL, mc_id::FACE_COAL_ORE);
        case BLOCK_ID_LOG:
        {
            switch (metadata)
            {
                BLOCK_LOG(WOOD_ID_SPRUCE, mc_id::FACE_LOG_SPRUCE_TOP, mc_id::FACE_LOG_SPRUCE);
                BLOCK_LOG(WOOD_ID_BIRCH, mc_id::FACE_LOG_BIRCH_TOP, mc_id::FACE_LOG_BIRCH);
            default: /* Fall through */
                BLOCK_LOG(WOOD_ID_OAK, mc_id::FACE_LOG_OAK_TOP, mc_id::FACE_LOG_OAK);
            }
            break;
        }
        case BLOCK_ID_LEAVES:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_LEAVES_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_LEAVES_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_LEAVES_OAK);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_SPONGE, mc_id::FACE_SPONGE);
            BLOCK_SIMPLE(BLOCK_ID_GLASS, mc_id::FACE_GLASS);
            BLOCK_SIMPLE(BLOCK_ID_ORE_LAPIS, mc_id::FACE_LAPIS_ORE);

            BLOCK_SIMPLE(BLOCK_ID_LAPIS, mc_id::FACE_LAPIS_BLOCK);
        case BLOCK_ID_DISPENSER:
        {
            mc_id::terrain_face_t face_front = terrain->get_face(mc_id::FACE_DISPENSER_FRONT_HORIZONTAL);
            mc_id::terrain_face_t face_side = terrain->get_face(mc_id::FACE_FURNACE_SIDE);

            faces[1] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[4] = faces[1];

            faces[5] = metadata == 2 ? face_front : face_side;
            faces[2] = metadata == 3 ? face_front : face_side;
            faces[3] = metadata == 4 ? face_front : face_side;
            faces[0] = metadata == 5 ? face_front : face_side;

            break;
        }
            BLOCK_TNT(BLOCK_ID_SANDSTONE, mc_id::FACE_SANDSTONE_TOP, mc_id::FACE_SANDSTONE_BOTTOM, mc_id::FACE_SANDSTONE_NORMAL)
            BLOCK_SIMPLE(BLOCK_ID_NOTE_BLOCK, mc_id::FACE_NOTEBLOCK);
            BLOCK_SIMPLE(BLOCK_ID_BED, mc_id::FACE_BED_HEAD_TOP);
        case BLOCK_ID_RAIL_POWERED:
        {
            if (metadata > 7)
                BLOCK_SIMPLE_NO_CASE(mc_id::FACE_RAIL_GOLDEN_POWERED);
            else
                BLOCK_SIMPLE_NO_CASE(mc_id::FACE_RAIL_GOLDEN);
            break;
        }
        case BLOCK_ID_RAIL_DETECTOR:
        {
            if (metadata > 7)
                BLOCK_SIMPLE_NO_CASE(mc_id::FACE_RAIL_DETECTOR_POWERED);
            else
                BLOCK_SIMPLE_NO_CASE(mc_id::FACE_RAIL_DETECTOR);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_PISTON_STICKY, mc_id::FACE_PISTON_TOP_STICKY);
            BLOCK_SIMPLE(BLOCK_ID_COBWEB, mc_id::FACE_WEB);
        case BLOCK_ID_FOLIAGE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(0, mc_id::FACE_DEADBUSH);
                BLOCK_SIMPLE(2, mc_id::FACE_FERN);
            default: /* Fall through */
                BLOCK_SIMPLE(1, mc_id::FACE_TALLGRASS);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_DEAD_BUSH, mc_id::FACE_DEADBUSH);
            BLOCK_SIMPLE(BLOCK_ID_PISTON, mc_id::FACE_PISTON_TOP_NORMAL);
            BLOCK_SIMPLE(BLOCK_ID_PISTON_HEAD, mc_id::FACE_PISTON_TOP_NORMAL);
        case BLOCK_ID_WOOL:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOL_ID_WHITE, mc_id::FACE_WOOL_COLORED_WHITE)
                BLOCK_SIMPLE(WOOL_ID_ORANGE, mc_id::FACE_WOOL_COLORED_ORANGE)
                BLOCK_SIMPLE(WOOL_ID_MAGENTA, mc_id::FACE_WOOL_COLORED_MAGENTA)
                BLOCK_SIMPLE(WOOL_ID_LIGHT_BLUE, mc_id::FACE_WOOL_COLORED_LIGHT_BLUE)
                BLOCK_SIMPLE(WOOL_ID_YELLOW, mc_id::FACE_WOOL_COLORED_YELLOW)
                BLOCK_SIMPLE(WOOL_ID_LIME, mc_id::FACE_WOOL_COLORED_LIME)
                BLOCK_SIMPLE(WOOL_ID_PINK, mc_id::FACE_WOOL_COLORED_PINK)
                BLOCK_SIMPLE(WOOL_ID_GRAY, mc_id::FACE_WOOL_COLORED_GRAY)
                BLOCK_SIMPLE(WOOL_ID_LIGHT_GRAY, mc_id::FACE_WOOL_COLORED_SILVER)
                BLOCK_SIMPLE(WOOL_ID_CYAN, mc_id::FACE_WOOL_COLORED_CYAN)
                BLOCK_SIMPLE(WOOL_ID_PURPLE, mc_id::FACE_WOOL_COLORED_PURPLE)
                BLOCK_SIMPLE(WOOL_ID_BLUE, mc_id::FACE_WOOL_COLORED_BLUE)
                BLOCK_SIMPLE(WOOL_ID_BROWN, mc_id::FACE_WOOL_COLORED_BROWN)
                BLOCK_SIMPLE(WOOL_ID_GREEN, mc_id::FACE_WOOL_COLORED_GREEN)
                BLOCK_SIMPLE(WOOL_ID_RED, mc_id::FACE_WOOL_COLORED_RED)
                BLOCK_SIMPLE(WOOL_ID_BLACK, mc_id::FACE_WOOL_COLORED_BLACK)
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_UNKNOWN, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_FLOWER_YELLOW, mc_id::FACE_FLOWER_DANDELION);
            BLOCK_SIMPLE(BLOCK_ID_FLOWER_RED, mc_id::FACE_FLOWER_ROSE);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_BLAND, mc_id::FACE_MUSHROOM_BROWN);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_RED, mc_id::FACE_MUSHROOM_RED);

            BLOCK_SIMPLE(BLOCK_ID_GOLD, mc_id::FACE_GOLD_BLOCK);
            BLOCK_SIMPLE(BLOCK_ID_IRON, mc_id::FACE_IRON_BLOCK);

        case BLOCK_ID_SLAB_DOUBLE:
        case BLOCK_ID_SLAB_SINGLE:
        {
            switch (metadata)
            {
                BLOCK_LOG(SLAB_ID_SMOOTH, mc_id::FACE_STONE_SLAB_TOP, mc_id::FACE_STONE_SLAB_SIDE);
                BLOCK_TNT(SLAB_ID_SANDSTONE, mc_id::FACE_SANDSTONE_TOP, mc_id::FACE_SANDSTONE_BOTTOM, mc_id::FACE_SANDSTONE_NORMAL);
                BLOCK_SIMPLE(SLAB_ID_WOOD, mc_id::FACE_PLANKS_OAK);
                BLOCK_SIMPLE(SLAB_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
                BLOCK_SIMPLE(SLAB_ID_BRICKS, mc_id::FACE_BRICK);
                BLOCK_SIMPLE(SLAB_ID_BRICKS_STONE, mc_id::FACE_STONEBRICK);
            default: /* Fall through */
                BLOCK_SIMPLE(SLAB_ID_SMOOTH_SIDE, mc_id::FACE_STONE_SLAB_TOP);
            }
            break;
        }
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
        case BLOCK_ID_REDSTONE:
        {
            r = (metadata + 5) / 20.0f;
            g = 0.0f;
            b = 0.0f;
            faces[0] = terrain->get_face(mc_id::FACE_REDSTONE_DUST_LINE);
            faces[1] = faces[0];
            faces[2] = faces[0];
            faces[3] = faces[0];
            faces[4] = faces[0];
            faces[5] = faces[0];
            break;
        }
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
        case BLOCK_ID_FURNACE_OFF: /* Fall through */
        case BLOCK_ID_FURNACE_ON:
        {
            mc_id::terrain_face_t face_front = terrain->get_face(type == BLOCK_ID_FURNACE_OFF ? mc_id::FACE_FURNACE_FRONT_OFF : mc_id::FACE_FURNACE_FRONT_ON);
            mc_id::terrain_face_t face_side = terrain->get_face(mc_id::FACE_FURNACE_SIDE);

            faces[1] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[4] = faces[1];

            faces[5] = metadata == 2 ? face_front : face_side;
            faces[2] = metadata == 3 ? face_front : face_side;
            faces[3] = metadata == 4 ? face_front : face_side;
            faces[0] = metadata == 5 ? face_front : face_side;

            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_SIGN_STANDING, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_DOOR_WOOD, mc_id::FACE_DOOR_WOOD_UPPER);
            BLOCK_SIMPLE(BLOCK_ID_LADDER, mc_id::FACE_LADDER);
        case BLOCK_ID_RAIL:
        {
            if (metadata > 5)
                BLOCK_SIMPLE_NO_CASE(mc_id::FACE_RAIL_NORMAL_TURNED);
            else
                BLOCK_SIMPLE_NO_CASE(mc_id::FACE_RAIL_NORMAL);
            break;
        }
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
            BLOCK_TNT(BLOCK_ID_CACTUS, mc_id::FACE_CACTUS_TOP, mc_id::FACE_CACTUS_BOTTOM, mc_id::FACE_CACTUS_SIDE);
            BLOCK_SIMPLE(BLOCK_ID_CLAY, mc_id::FACE_CLAY);
            BLOCK_SIMPLE(BLOCK_ID_SUGAR_CANE, mc_id::FACE_REEDS);
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
        case BLOCK_ID_FENCE_WOOD:
        {
            /* Multiple fence types are not in Minecraft Beta 1.8.x */
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_PLANKS_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_PLANKS_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_PLANKS_OAK);
            }
            break;
        }
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
        case BLOCK_ID_UNKNOWN_STONE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(1, mc_id::FACE_COBBLESTONE);
                BLOCK_SIMPLE(2, mc_id::FACE_STONEBRICK);
            default: /* Fall through */
                BLOCK_SIMPLE(0, mc_id::FACE_STONE);
            }
            break;
        }
        case BLOCK_ID_BRICKS_STONE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(STONE_BRICK_ID_MOSSY, mc_id::FACE_STONEBRICK_MOSSY);
                BLOCK_SIMPLE(STONE_BRICK_ID_CRACKED, mc_id::FACE_STONEBRICK_CRACKED);
            default: /* Fall through */
                BLOCK_SIMPLE(STONE_BRICK_ID_REGULAR, mc_id::FACE_STONEBRICK);
            }
            break;
        }
        case BLOCK_ID_MUSHROOM_BLOCK_BLAND:
        case BLOCK_ID_MUSHROOM_BLOCK_RED:
        {
            faces[0] = terrain->get_face(mc_id::FACE_MUSHROOM_BLOCK_INSIDE);
            faces[1] = faces[0];
            faces[2] = faces[0];
            faces[3] = faces[0];
            faces[4] = faces[0];
            faces[5] = faces[0];

            if (metadata == 0 || WOOL_ID_BLUE <= metadata)
                break;
            else if (metadata == WOOL_ID_PURPLE)
            {
                faces[0] = terrain->get_face(mc_id::FACE_MUSHROOM_BLOCK_SKIN_STEM);
                faces[2] = faces[0], faces[3] = faces[0], faces[5] = faces[0];
            }
            else
            {
                bool is_red = type == BLOCK_ID_MUSHROOM_BLOCK_RED;

                mc_id::terrain_face_t mushroom_skin = terrain->get_face(is_red ? mc_id::FACE_MUSHROOM_BLOCK_SKIN_RED : mc_id::FACE_MUSHROOM_BLOCK_SKIN_BROWN);

                if (metadata > 0 && (metadata + 1) % 3 == 1)
                    faces[0] = mushroom_skin;

                if (1 <= metadata && metadata <= 9)
                    faces[1] = mushroom_skin;

                if (7 <= metadata && metadata <= 9)
                    faces[2] = mushroom_skin;

                if (metadata % 3 == 1)
                    faces[3] = mushroom_skin;

                if (1 <= metadata && metadata <= 3)
                    faces[5] = mushroom_skin;
            }

            break;
        }
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

        if (type == BLOCK_ID_GRASS || type == BLOCK_ID_LEAVES || type == BLOCK_ID_FOLIAGE || type == BLOCK_ID_MOSS)
        {
#define AVG_BIOME_COL(X, Z) glm::vec3(biome_colors[X][Z] + biome_colors[X + 1][Z] + biome_colors[X][Z + 1] + biome_colors[X + 1][Z + 1])
            glm::vec3 col_0x_0z = AVG_BIOME_COL(x + 0, z + 0) * 0.25f;
            glm::vec3 col_0x_1z = AVG_BIOME_COL(x + 0, z + 1) * 0.25f;
            glm::vec3 col_1x_0z = AVG_BIOME_COL(x + 1, z + 0) * 0.25f;
            glm::vec3 col_1x_1z = AVG_BIOME_COL(x + 1, z + 1) * 0.25f;

            if (!cvr_r_biome_oversample.get())
            {
                col_0x_0z = (col_0x_0z + col_0x_1z + col_1x_0z + col_1x_1z) * 0.25f;
                col_0x_1z = col_0x_0z, col_1x_0z = col_0x_0z, col_1x_1z = col_0x_0z;
            }

            r_overlay_0x_0z = col_0x_0z.r;
            g_overlay_0x_0z = col_0x_0z.g;
            b_overlay_0x_0z = col_0x_0z.b;

            r_overlay_0x_1z = col_0x_1z.r;
            g_overlay_0x_1z = col_0x_1z.g;
            b_overlay_0x_1z = col_0x_1z.b;

            r_overlay_1x_0z = col_1x_0z.r;
            g_overlay_1x_0z = col_1x_0z.g;
            b_overlay_1x_0z = col_1x_0z.b;

            r_overlay_1x_1z = col_1x_1z.r;
            g_overlay_1x_1z = col_1x_1z.g;
            b_overlay_1x_1z = col_1x_1z.b;
        }

        if (type == BLOCK_ID_LEAVES || (type == BLOCK_ID_FOLIAGE && metadata != 0) || type == BLOCK_ID_MOSS)
        {
            r_0x_0z = r_overlay_0x_0z, g_0x_0z = g_overlay_0x_0z, b_0x_0z = b_overlay_0x_0z;
            r_0x_1z = r_overlay_0x_1z, g_0x_1z = g_overlay_0x_1z, b_0x_1z = b_overlay_0x_1z;
            r_1x_0z = r_overlay_1x_0z, g_1x_0z = g_overlay_1x_0z, b_1x_0z = b_overlay_1x_0z;
            r_1x_1z = r_overlay_1x_1z, g_1x_1z = g_overlay_1x_1z, b_1x_1z = b_overlay_1x_1z;

            float lr, lg, lb;
            if (type == BLOCK_ID_FOLIAGE)
                goto skip_default_tint;

            if (type == BLOCK_ID_LEAVES)
            {
                switch (metadata)
                {
                case WOOD_ID_SPRUCE:
                    lr = 0.380f, lg = 0.600f, lb = 0.380f;
                    r_0x_0z = (lr + r_0x_0z) * 0.5f, g_0x_0z = (lr + g_0x_0z) * 0.5f, b_0x_0z = (lr + b_0x_0z) * 0.5f;
                    r_0x_1z = (lr + r_0x_1z) * 0.5f, g_0x_1z = (lr + g_0x_1z) * 0.5f, b_0x_1z = (lr + b_0x_1z) * 0.5f;
                    r_1x_0z = (lr + r_1x_0z) * 0.5f, g_1x_0z = (lr + g_1x_0z) * 0.5f, b_1x_0z = (lr + b_1x_0z) * 0.5f;
                    r_1x_1z = (lr + r_1x_1z) * 0.5f, g_1x_1z = (lr + g_1x_1z) * 0.5f, b_1x_1z = (lr + b_1x_1z) * 0.5f;
                    goto skip_default_tint;
                case WOOD_ID_BIRCH:
                    lr = 0.502f, lg = 0.655f, lb = 0.333f;
                    r_0x_0z = (lr + r_0x_0z) * 0.5f, g_0x_0z = (lr + g_0x_0z) * 0.5f, b_0x_0z = (lr + b_0x_0z) * 0.5f;
                    r_0x_1z = (lr + r_0x_1z) * 0.5f, g_0x_1z = (lr + g_0x_1z) * 0.5f, b_0x_1z = (lr + b_0x_1z) * 0.5f;
                    r_1x_0z = (lr + r_1x_0z) * 0.5f, g_1x_0z = (lr + g_1x_0z) * 0.5f, b_1x_0z = (lr + b_1x_0z) * 0.5f;
                    r_1x_1z = (lr + r_1x_1z) * 0.5f, g_1x_1z = (lr + g_1x_1z) * 0.5f, b_1x_1z = (lr + b_1x_1z) * 0.5f;
                    goto skip_default_tint;
                case WOOD_ID_OAK: /* Fall through */
                default:
                    break;
                }
            }

            lr = 0.900f, lg = 1.000f, lb = 0.900f;
            r_0x_0z = (lr * r_0x_0z), g_0x_0z = (lr * g_0x_0z), b_0x_0z = (lr * b_0x_0z);
            r_0x_1z = (lr * r_0x_1z), g_0x_1z = (lr * g_0x_1z), b_0x_1z = (lr * b_0x_1z);
            r_1x_0z = (lr * r_1x_0z), g_1x_0z = (lr * g_1x_0z), b_1x_0z = (lr * b_1x_0z);
            r_1x_1z = (lr * r_1x_1z), g_1x_1z = (lr * g_1x_1z), b_1x_1z = (lr * b_1x_1z);

        skip_default_tint:;
        }

#define UAO(X) (!is_transparent[stypes X])
#define UBL(X) (slight_block X * is_transparent[stypes X])
#define USL(X) (slight_sky X * is_transparent[stypes X])

        /* ============ BEGIN: IS_TORCH ============ */
        if (type == BLOCK_ID_TORCH || type == BLOCK_ID_TORCH_REDSTONE_ON || type == BLOCK_ID_TORCH_REDSTONE_OFF)
        {
            const Uint8 ao[] = { 0, 0, 0, 0 };
            const Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };
            const Uint8 sl[] = { slight_sky[1][1][1], slight_sky[1][1][1], slight_sky[1][1][1], slight_sky[1][1][1] };

            /* Positive Y */
            {
                glm::vec2 cs = faces[1].corners[3] - faces[1].corners[0];
                faces[1].corners[0] += cs * glm::vec2(0.4375f, 0.375f);
                faces[1].corners[3] = faces[1].corners[0] + cs / 8.0f;

                faces[1].corners[1] = { faces[1].corners[3].x, faces[1].corners[0].y };
                faces[1].corners[2] = { faces[1].corners[0].x, faces[1].corners[3].y };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 10), Sint16(z * 16 + 9), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 10), Sint16(z * 16 + 7), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 10), Sint16(z * 16 + 9), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 10), Sint16(z * 16 + 7), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[1].corners[3],
                });
            }

            /* Positive X */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 0), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 0), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 9), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 9), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 0), Sint16(z * 16 + 9), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 9), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 7), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 7), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 0), Sint16(z * 16 + 7), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 7), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[5].corners[0],
                });
            }
        }
        /* ============ END: IS_TORCH ============ */
        /* ============ BEGIN: IS_CROSS_BLOCK ============ */
        else if (type == BLOCK_ID_FLOWER_RED || type == BLOCK_ID_FLOWER_YELLOW || type == BLOCK_ID_COBWEB || type == BLOCK_ID_MUSHROOM_BLAND
            || type == BLOCK_ID_MUSHROOM_RED || type == BLOCK_ID_FOLIAGE || type == BLOCK_ID_DEAD_BUSH || type == BLOCK_ID_SAPLING
            || type == BLOCK_ID_SUGAR_CANE)
        {
            /* Positive X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[0], slight_sky[1][1][1] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[1], slight_sky[1][1][1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[2], slight_sky[1][1][1] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[3], slight_sky[1][1][1] },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[3], slight_sky[1][1][1] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[1], slight_sky[1][1][1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[2], slight_sky[1][1][1] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[0], slight_sky[1][1][1] },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[3], slight_sky[1][1][1] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[1], slight_sky[1][1][1] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[2], slight_sky[1][1][1] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[0], slight_sky[1][1][1] },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[0], slight_sky[1][1][1] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[1], slight_sky[1][1][1] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[2], slight_sky[1][1][1] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[3], slight_sky[1][1][1] },
                    faces[5].corners[0],
                });
            }
        }
        /* ============ END: IS_CROSS_BLOCK  ============*/
        /* ============ BEGIN: IS_FLUID ============ */
        else if (type == BLOCK_ID_LAVA_FLOWING || type == BLOCK_ID_LAVA_SOURCE || type == BLOCK_ID_WATER_FLOWING || type == BLOCK_ID_WATER_SOURCE)
        {
            /* Simplify block id checking */
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    for (int k = 0; k < 3; k++)
                    {
                        if (stypes[i][j][k] == BLOCK_ID_LAVA_FLOWING)
                            stypes[i][j][k] = BLOCK_ID_LAVA_SOURCE;
                        else if (stypes[i][j][k] == BLOCK_ID_WATER_FLOWING)
                            stypes[i][j][k] = BLOCK_ID_WATER_SOURCE;
                    }
            type = stypes[1][1][1];

            bool is_water = (type == BLOCK_ID_WATER_SOURCE);
            std::vector<terrain_vertex_t>* vtx_fluid = vtx;

            mc_id::terrain_face_t face_flow = terrain->get_face(is_water ? mc_id::FACE_WATER_FLOW : mc_id::FACE_LAVA_FLOW);
            mc_id::terrain_face_t face_still = terrain->get_face(is_water ? mc_id::FACE_WATER_STILL : mc_id::FACE_LAVA_STILL);

#define IS_SAME_FLUID(A, B) (A == B)
#define IS_FLUID(A) ((A) == BLOCK_ID_WATER_SOURCE || (A) == BLOCK_ID_LAVA_SOURCE)

            struct fluid_corners_t
            {
                Uint8 zero;
                Uint8 posx;
                Uint8 posz;
                Uint8 both;

                fluid_corners_t() { }
                fluid_corners_t(int set)
                    : fluid_corners_t(set, set, set, set)
                {
                }
                fluid_corners_t(int z, int px, int pz, int b)
                {
                    zero = SDL_clamp(z, 0, 16);
                    posx = SDL_clamp(px, 0, 16);
                    posz = SDL_clamp(pz, 0, 16);
                    both = SDL_clamp(b, 0, 16);
                }

                fluid_corners_t operator/(int a)
                {
                    fluid_corners_t t;
                    t.zero = zero / a;
                    t.posx = posx / a;
                    t.posz = posz / a;
                    t.both = both / a;
                    return t;
                }
            };
            /** Fluid depths (For deciding texture info) */
            fluid_corners_t corner_depths;
            /** Actual mesh heights*/
            fluid_corners_t corner_heights;

            int fluid_force_flow = 0;

            /* When nearby block is same override normal corner? */
            /* Block above is fluid, skip decision making*/
            if (IS_FLUID(stypes[1][2][1]))
            {
                corner_depths = 8;
                corner_heights = 16;
            }
            else
            {
#define FLUID_MAX_META 8
#define FLUID_DEPTH_FROM_METADATA(METADATA) ((METADATA < FLUID_MAX_META) ? FLUID_MAX_META - METADATA : FLUID_MAX_META)
#define FLUID_DEPTH_IF_SAME(SUBSCRIPT) (IS_SAME_FLUID(stypes SUBSCRIPT, type) ? FLUID_DEPTH_FROM_METADATA(smetadata SUBSCRIPT) : 0)
#define FLUID_DEPTH_IF_SAME_MAX(VAR, SUBSCRIPT)                                                              \
    do                                                                                                       \
    {                                                                                                        \
        Uint8 NEW_##VAR = (IS_FLUID(stypes SUBSCRIPT) ? FLUID_DEPTH_FROM_METADATA(smetadata SUBSCRIPT) : 0); \
        corner_depths.VAR = SDL_max(NEW_##VAR, corner_depths.VAR);                                           \
    } while (0)

                corner_depths = FLUID_DEPTH_FROM_METADATA(metadata);
                corner_heights = 0;

                /* Max a corner if the 2x2 region above the corner contains the same fluid */
                bool max_zero = IS_FLUID(stypes[0][2][0]) || IS_FLUID(stypes[1][2][0]) || IS_FLUID(stypes[0][2][1]);
                bool max_posx = IS_FLUID(stypes[1][2][0]) || IS_FLUID(stypes[2][2][0]) || IS_FLUID(stypes[2][2][1]);
                bool max_posz = IS_FLUID(stypes[0][2][2]) || IS_FLUID(stypes[1][2][2]) || IS_FLUID(stypes[0][2][1]);
                bool max_both = IS_FLUID(stypes[1][2][2]) || IS_FLUID(stypes[2][2][2]) || IS_FLUID(stypes[2][2][1]);

                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][0]) < corner_depths.zero)
                    fluid_force_flow = 1;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][0]) < corner_depths.posx)
                    fluid_force_flow = 1;

                if (FLUID_DEPTH_FROM_METADATA(smetadata[0][1][1]) < corner_depths.zero)
                    fluid_force_flow = 2;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[0][1][1]) < corner_depths.posz)
                    fluid_force_flow = 2;

                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][2]) < corner_depths.both)
                    fluid_force_flow = 3;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][2]) < corner_depths.posz)
                    fluid_force_flow = 3;

                if (FLUID_DEPTH_FROM_METADATA(smetadata[2][1][1]) < corner_depths.both)
                    fluid_force_flow = 4;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[2][1][1]) < corner_depths.posx)
                    fluid_force_flow = 4;

                FLUID_DEPTH_IF_SAME_MAX(zero, [0][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(zero, [1][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(zero, [0][1][1]);

                FLUID_DEPTH_IF_SAME_MAX(both, [2][1][2]);
                FLUID_DEPTH_IF_SAME_MAX(both, [1][1][2]);
                FLUID_DEPTH_IF_SAME_MAX(both, [2][1][1]);

                FLUID_DEPTH_IF_SAME_MAX(posx, [2][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(posx, [1][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(posx, [2][1][1]);

                FLUID_DEPTH_IF_SAME_MAX(posz, [0][1][2]);
                FLUID_DEPTH_IF_SAME_MAX(posz, [0][1][1]);
                FLUID_DEPTH_IF_SAME_MAX(posz, [1][1][2]);

                corner_depths.zero = max_zero ? FLUID_MAX_META : corner_depths.zero;
                corner_depths.posx = max_posx ? FLUID_MAX_META : corner_depths.posx;
                corner_depths.posz = max_posz ? FLUID_MAX_META : corner_depths.posz;
                corner_depths.both = max_both ? FLUID_MAX_META : corner_depths.both;

                corner_heights.zero = 1 + corner_depths.zero + (corner_depths.zero == FLUID_MAX_META) * 2;
                corner_heights.posx = 1 + corner_depths.posx + (corner_depths.posx == FLUID_MAX_META) * 2;
                corner_heights.posz = 1 + corner_depths.posz + (corner_depths.posz == FLUID_MAX_META) * 2;
                corner_heights.both = 1 + corner_depths.both + (corner_depths.both == FLUID_MAX_META) * 2;

#define FLUID_RAISE(X) ((stypes X) != BLOCK_ID_AIR)
                corner_heights.zero += (FLUID_RAISE([0][1][0]) + FLUID_RAISE([1][1][0]) + FLUID_RAISE([0][1][1]));
                corner_heights.posx += (FLUID_RAISE([2][1][0]) + FLUID_RAISE([1][1][0]) + FLUID_RAISE([2][1][1]));
                corner_heights.posz += (FLUID_RAISE([0][1][2]) + FLUID_RAISE([0][1][1]) + FLUID_RAISE([1][1][2]));
                corner_heights.both += (FLUID_RAISE([2][1][2]) + FLUID_RAISE([2][1][1]) + FLUID_RAISE([1][1][2]));

                corner_heights.zero = max_zero ? 16 : SDL_clamp(corner_heights.zero, 1, 16);
                corner_heights.posx = max_posx ? 16 : SDL_clamp(corner_heights.posx, 1, 16);
                corner_heights.posz = max_posz ? 16 : SDL_clamp(corner_heights.posz, 1, 16);
                corner_heights.both = max_both ? 16 : SDL_clamp(corner_heights.both, 1, 16);
            }

            glm::vec2 face_flow_tsize = (face_flow.corners[3] - face_flow.corners[0]);

            /* Positive Y */
            if (!IS_SAME_FLUID(stypes[1][2][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                int slope_left = corner_depths.zero - corner_depths.posx;
                int slope_bot = corner_depths.zero - corner_depths.posz;
                int slope_right = corner_depths.both - corner_depths.posz;
                int slope_top = corner_depths.both - corner_depths.posx;
                int slope_zeroboth = corner_depths.both - corner_depths.zero;
                int slope_posxposz = corner_depths.posx - corner_depths.posz;

                mc_id::terrain_face_t face_top;

                bool is_still = slope_left == 0 && slope_bot == 0 && slope_right == 0 && slope_top == 0;
                if (!is_still)
                    fluid_force_flow = false;
                if (fluid_force_flow)
                    is_still = false;
                float flow_rot = 0.0f;

                if (is_still)
                    face_top = face_still;
                else if (fluid_force_flow)
                {
                    switch (fluid_force_flow)
                    {
                    case 2:
                        flow_rot = 0.0f;
                        break;
                    case 3:
                        flow_rot = 90.0f;
                        break;
                    case 4:
                        flow_rot = 180.0f;
                        break;
                    case 1: /* Fall through */
                    default:
                        flow_rot = 270.0f;
                        break;
                    }
                }
                else if (slope_left == -slope_right && slope_top == -slope_bot) /* Axis aligned flow */
                {
                    if (slope_left > 0)
                        flow_rot -= 180.0f;

                    if (slope_top > 0)
                        flow_rot -= 90.0f;
                    if (slope_bot > 0)
                        flow_rot += 90.0f;
                }
                else if (slope_zeroboth != 0 || slope_posxposz != 0) /* Diagonal Flow */
                {
                    int slope_to_use = (SDL_abs(slope_posxposz) < SDL_abs(slope_zeroboth)) ? slope_zeroboth : slope_posxposz;
                    flow_rot = 45.0f;

                    if (slope_to_use < 0)
                        flow_rot += 90.0f;

                    if (slope_posxposz < slope_zeroboth)
                        flow_rot += 90.0f;

                    if (slope_posxposz < slope_zeroboth && slope_to_use > 0)
                        flow_rot += 180.0f;

                    // TODO-OPT: Add 22.5 degree adjustments
                }
                else if (slope_zeroboth == 0 && slope_posxposz == 0)
                {
                    if (corner_depths.both > corner_depths.posx)
                        flow_rot = 45.0f;
                    else
                        flow_rot = 225.0f;
                }
                else
                {
                    face_top = terrain->get_face(mc_id::FACE_DEBUG);
                    is_still = true;
                }

                if (!is_still)
                {
                    float radius = SDL_sqrtf(2.0f) * 0.25f;

                    face_top.corners[0] = glm::vec2(glm::cos(glm::radians(135.0f + flow_rot)), glm::sin(glm::radians(135.0f + flow_rot))) * radius;
                    face_top.corners[1] = glm::vec2(glm::cos(glm::radians(225.0f + flow_rot)), glm::sin(glm::radians(225.0f + flow_rot))) * radius;
                    face_top.corners[2] = glm::vec2(glm::cos(glm::radians(45.0f + flow_rot)), glm::sin(glm::radians(45.0f + flow_rot))) * radius;
                    face_top.corners[3] = glm::vec2(glm::cos(glm::radians(315.0f + flow_rot)), glm::sin(glm::radians(315.0f + flow_rot))) * radius;

                    face_top.corners[0] = (face_top.corners[0] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                    face_top.corners[1] = (face_top.corners[1] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                    face_top.corners[2] = (face_top.corners[2] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                    face_top.corners[3] = (face_top.corners[3] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                }

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.both), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][2][1] },
                    face_top.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.posx), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][2][1] },
                    face_top.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.posz), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][2][1] },
                    face_top.corners[2],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.zero), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][2][1] },
                    face_top.corners[0],
                });
            }

            /* Negative Y */
            if (is_transparent[stypes[1][0][1]] && !IS_SAME_FLUID(stypes[1][0][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][0][1] },
                    face_still.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][0][1] },
                    face_still.corners[0],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][0][1] },
                    face_still.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][0][1] },
                    face_still.corners[2],
                });
            }

            fluid_corners_t corner_tex_heights;
            corner_tex_heights.zero = 16 - corner_heights.zero;
            corner_tex_heights.posx = 16 - corner_heights.posx;
            corner_tex_heights.posz = 16 - corner_heights.posz;
            corner_tex_heights.both = 16 - corner_heights.both;

#define FLUID_CALC_SIDE_HEIGHTS(CORNER1, CORNER2)                                                                               \
    mc_id::terrain_face_t face_side;                                                                                            \
    mc_id::terrain_face_t face_side_tri;                                                                                        \
                                                                                                                                \
    Uint8 max_corner_tex_heights = SDL_max(corner_tex_heights.CORNER1, corner_tex_heights.CORNER2);                             \
    Uint8 min_corner_tex_heights = SDL_min(corner_tex_heights.CORNER1, corner_tex_heights.CORNER2);                             \
                                                                                                                                \
    Uint8 max_corner_heights = SDL_max(corner_heights.CORNER1, corner_heights.CORNER2);                                         \
    Uint8 min_corner_heights = SDL_min(corner_heights.CORNER1, corner_heights.CORNER2);                                         \
                                                                                                                                \
    bool which_height = corner_heights.CORNER1 > corner_heights.CORNER2;                                                        \
                                                                                                                                \
    face_side.corners[0] = glm::vec2(0.0f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0];     \
    face_side.corners[1] = glm::vec2(0.5f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0];     \
    face_side.corners[2] = glm::vec2(0.0f, 0.5f) * face_flow_tsize + face_flow.corners[0];                                      \
    face_side.corners[3] = glm::vec2(0.5f, 0.5f) * face_flow_tsize + face_flow.corners[0];                                      \
                                                                                                                                \
    face_side_tri.corners[0] = glm::vec2(0.0f, 0.5f * min_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0]; \
    face_side_tri.corners[1] = glm::vec2(0.5f, 0.5f * min_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0]; \
    face_side_tri.corners[2] = glm::vec2(0.0f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0]; \
    face_side_tri.corners[3] = glm::vec2(0.5f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0];

            /* Positive X */
            if (is_transparent[stypes[2][1][1]] && !IS_SAME_FLUID(stypes[2][1][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(posx, both);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[2][1][1] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[2][1][1] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[2][1][1] },
                    face_side.corners[2],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[2][1][1] },
                    face_side.corners[0],
                });

                if (max_corner_heights != min_corner_heights)
                {
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[0] },
                        { r, g, b, bl[0], slight_sky[2][1][1] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.posx), Sint16(z * 16 + 00), ao[1] },
                        { r, g, b, bl[1], slight_sky[2][1][1] },
                        face_side_tri.corners[which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[2] },
                        { r, g, b, bl[2], slight_sky[2][1][1] },
                        face_side_tri.corners[2],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.both), Sint16(z * 16 + 16), ao[3] },
                        { r, g, b, bl[3], slight_sky[2][1][1] },
                        face_side_tri.corners[which_height ? 2 : 0],
                    });
                }
            }

            /* Negative X */
            if (is_transparent[stypes[0][1][1]] && !IS_SAME_FLUID(stypes[0][1][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(zero, posz);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[0][1][1] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[0][1][1] },
                    face_side.corners[0],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[0][1][1] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[0][1][1] },
                    face_side.corners[2],
                });

                if (max_corner_heights != min_corner_heights)
                {

                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.posz), Sint16(z * 16 + 16), ao[3] },
                        { r, g, b, bl[3], slight_sky[0][1][1] },
                        face_side_tri.corners[!which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.zero), Sint16(z * 16 + 00), ao[1] },
                        { r, g, b, bl[1], slight_sky[0][1][1] },
                        face_side_tri.corners[!which_height ? 2 : 0],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[2] },
                        { r, g, b, bl[2], slight_sky[0][1][1] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[0] },
                        { r, g, b, bl[0], slight_sky[0][1][1] },
                        face_side_tri.corners[2],
                    });
                }
            }

            /* Positive Z */
            if (is_transparent[stypes[1][1][2]] && !IS_SAME_FLUID(stypes[1][1][2], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(both, posz);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][2] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][2] },
                    face_side.corners[0],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][2] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][2] },
                    face_side.corners[2],
                });

                if (max_corner_heights != min_corner_heights)
                {
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + ((which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 16), ao[3] },
                        { r, g, b, bl[3], slight_sky[1][1][2] },
                        face_side_tri.corners[which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + ((!which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 16), ao[1] },
                        { r, g, b, bl[1], slight_sky[1][1][2] },
                        face_side_tri.corners[which_height ? 2 : 0],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[2] },
                        { r, g, b, bl[2], slight_sky[1][1][2] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[0] },
                        { r, g, b, bl[0], slight_sky[1][1][2] },
                        face_side_tri.corners[2],
                    });
                }
            }
            /* Negative Z */
            if (is_transparent[stypes[1][1][0]] && !IS_SAME_FLUID(stypes[1][1][0], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(zero, posx);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][0] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][0] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][0] },
                    face_side.corners[2],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][0] },
                    face_side.corners[0],
                });

                if (max_corner_heights != min_corner_heights)
                {
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[0] },
                        { r, g, b, bl[0], slight_sky[1][1][0] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + ((which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 00), ao[1] },
                        { r, g, b, bl[1], slight_sky[1][1][0] },
                        face_side_tri.corners[which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[2] },
                        { r, g, b, bl[2], slight_sky[1][1][0] },
                        face_side_tri.corners[2],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + ((!which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 00), ao[3] },
                        { r, g, b, bl[3], slight_sky[1][1][0] },
                        face_side_tri.corners[which_height ? 2 : 0],
                    });
                }
            }
#undef FLUID_CALC_SIDE_HEIGHTS
        }
        /* ============ END: IS_FLUID ============ */
        /* ============ BEGIN: IS_CACTI ============ */
        else if (type == BLOCK_ID_CACTUS)
        {
            const Uint8 ao[] = { 0, 0, 0, 0 };
            Uint8 bl_top = slight_block[1][1][1];
            Uint8 bl_bot = slight_block[1][1][1];
            Uint8 sl_top = slight_sky[1][1][1];
            Uint8 sl_bot = slight_sky[1][1][1];

            if (stypes[1][2][1] == BLOCK_ID_CACTUS)
            {
                bl_top = (slight_block[1][2][1] + bl_top) * 0.5f;
                sl_top = (slight_sky[1][2][1] + sl_top) * 0.5f;
            }

            if (stypes[1][0][1] == BLOCK_ID_CACTUS)
            {
                bl_bot = (slight_block[1][0][1] + bl_bot) * 0.5f;
                sl_bot = (slight_sky[1][0][1] + sl_bot) * 0.5f;
            }

            /* Positive Y */
            if (stypes[1][2][1] != BLOCK_ID_CACTUS)
            {
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_top, sl_top },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_top, sl_top },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_top, sl_top },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_top, sl_top },
                    faces[1].corners[3],
                });
            }

            /* Negative Y */
            if (stypes[1][0][1] != BLOCK_ID_CACTUS)
            {
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_bot, sl_bot },
                    faces[4].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_bot, sl_bot },
                    faces[4].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_bot, sl_bot },
                    faces[4].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_bot, sl_bot },
                    faces[4].corners[2],
                });
            }

            /* Positive X */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_bot, sl_bot },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 00), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_top, sl_top },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_bot, sl_bot },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_top, sl_top },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_top, sl_top },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 00), ao[1] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_top, sl_top },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_bot, sl_bot },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_bot, sl_bot },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_top, sl_top },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[1] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_top, sl_top },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 15), ao[2] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_bot, sl_bot },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 15), ao[0] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_bot, sl_bot },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 1), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_bot, sl_bot },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_top, sl_top },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 1), ao[2] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_bot, sl_bot },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[3] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_top, sl_top },
                    faces[5].corners[0],
                });
            }
        }
        /* ============ END: IS_CACTI ============ */
        /* ============ BEGIN: IS_RAIL ============ */
        else if (mc_id::is_rail(type))
        {
            /* For powered and detector rails: only the bottom 3 bits determine model */
            Uint8 rail_meta = metadata & ((type == BLOCK_ID_RAIL) ? 0x0F : 0x07);

            /** If slanted then this is the direction it faces down towards */
            mc_id::direction_t rail_dir = mc_id::DIR_F0;

            switch (rail_meta)
            {
            case 0: /* Flat */
                rail_dir = mc_id::DIR_F2;
                break;
            case 1: /* Flat */
            case 2: /* Down towards DIR_F1 */
                rail_dir = mc_id::DIR_F1;
                break;
            case 3: /* Down towards DIR_F3 */
                rail_dir = mc_id::DIR_F3;
                break;
            case 4: /* Down towards DIR_F0 */
                rail_dir = mc_id::DIR_F0;
                break;
            case 5: /* Down towards DIR_F2 */
                rail_dir = mc_id::DIR_F2;
                break;
            case 6: /* Curved */
                rail_dir = mc_id::DIR_F0;
                break;
            case 7: /* Curved */
                rail_dir = mc_id::DIR_F1;
                break;
            case 8: /* Curved */
                rail_dir = mc_id::DIR_F2;
                break;
            case 9: /* Curved */
                rail_dir = mc_id::DIR_F3;
                break;
            default: /* Whatever goes */
                rail_dir = mc_id::DIR_F0;
                break;
            }

            bool slanted = 0;

            if (rail_meta >= 2 && rail_meta <= 5)
                slanted = 1;

            int y_x0_z0 = 1;
            int y_x1_z0 = 1;
            int y_x0_z1 = 1;
            int y_x1_z1 = 1;

            if (slanted)
            {
                switch (rail_dir)
                {
                case mc_id::DIR_TOWARDS_NEG_X:
                    y_x1_z0 = 17;
                    y_x1_z1 = 17;
                    break;
                case mc_id::DIR_TOWARDS_NEG_Z:
                    y_x0_z1 = 17;
                    y_x1_z1 = 17;
                    break;
                case mc_id::DIR_TOWARDS_POS_X:
                    y_x0_z0 = 17;
                    y_x0_z1 = 17;
                    break;
                case mc_id::DIR_TOWARDS_POS_Z:
                    y_x0_z0 = 17;
                    y_x1_z0 = 17;
                    break;
                }
            }

            switch (rail_dir)
            {
            case mc_id::DIR_F3: /* fall-through */
                faces[0].rotate_90();
            case mc_id::DIR_F0: /* fall-through */
                faces[0].rotate_90();
            case mc_id::DIR_F1: /* fall-through */
                faces[0].rotate_90();
            case mc_id::DIR_F2: /* fall-through */
                break;
            }

            Uint8 bl_x0_z0 = slight_block[1][1][1];
            Uint8 bl_x1_z0 = slight_block[1][1][1];
            Uint8 bl_x0_z1 = slight_block[1][1][1];
            Uint8 bl_x1_z1 = slight_block[1][1][1];

            Uint8 sl_x0_z0 = slight_sky[1][1][1];
            Uint8 sl_x1_z0 = slight_sky[1][1][1];
            Uint8 sl_x0_z1 = slight_sky[1][1][1];
            Uint8 sl_x1_z1 = slight_sky[1][1][1];

            vtx->push_back({
                { 1, Sint16(x * 16 + 16), Sint16(y * 16 + y_x1_z1), Sint16(z * 16 + 16), 0 },
                { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_x1_z1, sl_x1_z1 },
                faces[0].corners[0],
            });
            vtx->push_back({
                { 1, Sint16(x * 16 + 16), Sint16(y * 16 + y_x1_z0), Sint16(z * 16 + 00), 0 },
                { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_x1_z0, sl_x1_z0 },
                faces[0].corners[2],
            });
            vtx->push_back({
                { 1, Sint16(x * 16 + 00), Sint16(y * 16 + y_x0_z1), Sint16(z * 16 + 16), 0 },
                { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_x0_z1, sl_x0_z1 },
                faces[0].corners[1],
            });
            vtx->push_back({
                { 1, Sint16(x * 16 + 00), Sint16(y * 16 + y_x0_z0), Sint16(z * 16 + 00), 0 },
                { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_x0_z0, sl_x0_z0 },
                faces[0].corners[3],
            });

            vtx->push_back({
                { 1, Sint16(x * 16 + 00), Sint16(y * 16 + y_x0_z0), Sint16(z * 16 + 00), 0 },
                { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl_x0_z0, sl_x0_z0 },
                faces[0].corners[3],
            });
            vtx->push_back({
                { 1, Sint16(x * 16 + 16), Sint16(y * 16 + y_x1_z0), Sint16(z * 16 + 00), 0 },
                { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl_x1_z0, sl_x1_z0 },
                faces[0].corners[2],
            });
            vtx->push_back({
                { 1, Sint16(x * 16 + 00), Sint16(y * 16 + y_x0_z1), Sint16(z * 16 + 16), 0 },
                { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl_x0_z1, sl_x0_z1 },
                faces[0].corners[1],
            });
            vtx->push_back({
                { 1, Sint16(x * 16 + 16), Sint16(y * 16 + y_x1_z1), Sint16(z * 16 + 16), 0 },
                { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl_x1_z1, sl_x1_z1 },
                faces[0].corners[0],
            });
        }
        /* ============ END: IS_RAIL ============ */
        /* ============ BEGIN: IS_NORMAL ============ */
        else
        {
            const bool do_face_pos_y = (is_transparent[stypes[1][2][1]] && (is_leaves_style_transparent[type] || stypes[1][2][1] != type));
            const bool do_face_neg_y = (is_transparent[stypes[1][0][1]] && (is_leaves_style_transparent[type] || stypes[1][0][1] != type));
            const bool do_face_pos_x = (is_transparent[stypes[2][1][1]] && (is_leaves_style_transparent[type] || stypes[2][1][1] != type));
            const bool do_face_neg_x = (is_transparent[stypes[0][1][1]] && (is_leaves_style_transparent[type] || stypes[0][1][1] != type));
            const bool do_face_pos_z = (is_transparent[stypes[1][1][2]] && (is_leaves_style_transparent[type] || stypes[1][1][2] != type));
            const bool do_face_neg_z = (is_transparent[stypes[1][1][0]] && (is_leaves_style_transparent[type] || stypes[1][1][0] != type));

            /* Quick reject */
            if (!do_face_pos_y && !do_face_neg_y && !do_face_pos_x && !do_face_neg_x && !do_face_pos_z && !do_face_neg_z)
                continue;

            struct corner_t
            {
                Uint16 sum, divisor;
                corner_t() { sum = 0, divisor = 0; }
                corner_t(const int _sum, const int _divisor) { sum = _sum, divisor = _divisor; }
                corner_t(const Uint16 _sum, const Uint16 _divisor) { sum = _sum, divisor = _divisor; }
                void operator+=(const corner_t& rh) { sum += rh.sum, divisor += rh.divisor; }
                void operator+=(const int& rh)
                {
                    sum += rh;
                    divisor++;
                }
                Uint8 get() const { return sum / divisor; }
            };

            const std::function<std::pair<corner_t, corner_t> const(const int, const int, const int, const bool, const bool, const bool)> calc_corner
                = [&](const int _x, const int _y, const int _z, const bool f_x, const bool f_y, const bool f_z) -> std::pair<corner_t, corner_t> const {
                corner_t corner_b, corner_s;

                uint_fast8_t mask = f_x | (f_y << 1) | (f_z << 2);

                /* Check that mask has only 1 flag */
                assert(mask && !(mask & (mask - 1)));

                const int x = 1 + _x;
                const int y = 1 + _y;
                const int z = 1 + _z;

                bool v_x = 0, v_y = 0, v_z = 0;
                bool v_diag_xy = 0, v_diag_xz = 0, v_diag_zy = 0;
                bool v_diag_xzy = 0;

                switch (mask)
                {
                case 0x01:
                    v_x = is_transparent[stypes[x][1][1]];
                    break;
                case 0x02:
                    v_y = is_transparent[stypes[1][y][1]];
                    break;
                case 0x04:
                    v_z = is_transparent[stypes[1][1][z]];
                    break;
                }

                if (!cvr_r_smooth_lighting.get())
                    goto skip_smooth;

                if (mask & ~0x01)
                    v_diag_zy = (v_z || v_y) && is_transparent[stypes[1][y][z]];

                if (mask & ~0x02)
                    v_diag_xz = (v_x || v_z) && is_transparent[stypes[x][1][z]];

                if (mask & ~0x04)
                    v_diag_xy = (v_x || v_y) && is_transparent[stypes[x][y][1]];

                /* By this point, two diagonals have been calculated */
                v_diag_xzy = (v_diag_xy || v_diag_xz || v_diag_zy) && is_transparent[stypes[x][y][z]];

                if (mask & 0x01 || !v_diag_zy)
                    v_diag_zy = v_diag_xzy && is_transparent[stypes[1][y][z]];

                if (mask & 0x02 || !v_diag_xz)
                    v_diag_xz = v_diag_xzy && is_transparent[stypes[x][1][z]];

                if (mask & 0x04 || !v_diag_xy)
                    v_diag_xy = v_diag_xzy && is_transparent[stypes[x][y][1]];

                if (!v_x)
                    v_x = (v_diag_xy || v_diag_xz) && is_transparent[stypes[x][1][1]];
                if (!v_y)
                    v_y = (v_diag_xy || v_diag_zy) && is_transparent[stypes[1][y][1]];
                if (!v_z)
                    v_z = (v_diag_xz || v_diag_zy) && is_transparent[stypes[1][1][z]];

#define CORNER_IF_COND(COND, COORDS)         \
    do                                       \
        if (COND)                            \
        {                                    \
            corner_s += slight_sky COORDS;   \
            corner_b += slight_block COORDS; \
        }                                    \
    while (0)
                CORNER_IF_COND(is_transparent[stypes[1][1][1]], [1][1][1]);

                CORNER_IF_COND(v_diag_xy, [x][y][1]);
                CORNER_IF_COND(v_diag_xz, [x][1][z]);
                CORNER_IF_COND(v_diag_zy, [1][y][z]);

                CORNER_IF_COND(v_diag_xzy, [x][y][z]);

            skip_smooth:
                CORNER_IF_COND(v_x, [x][1][1]);
                CORNER_IF_COND(v_y, [1][y][1]);
                CORNER_IF_COND(v_z, [1][1][z]);

#undef CORNER_IF_COND

                return std::pair(corner_b, corner_s);
            };

            /* Positive Y */
            if (do_face_pos_y)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][2][0]) + UAO([1][2][0]) + UAO([0][2][1])),
                    Uint8(UAO([2][2][0]) + UAO([1][2][0]) + UAO([2][2][1])),
                    Uint8(UAO([0][2][2]) + UAO([0][2][1]) + UAO([1][2][2])),
                    Uint8(UAO([2][2][2]) + UAO([1][2][2]) + UAO([2][2][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, 1, -1, 0, 1, 0);
                templ[1] = calc_corner(+1, 1, -1, 0, 1, 0);
                templ[2] = calc_corner(-1, 1, +1, 0, 1, 0);
                templ[3] = calc_corner(+1, 1, +1, 0, 1, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[3].get(), sl[3].get() },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[1].get(), sl[1].get() },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[2].get(), sl[2].get() },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[0].get(), sl[0].get() },
                    faces[1].corners[3],
                });

                if (use_overlay[1])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay * r_overlay_1x_1z, g_overlay * g_overlay_1x_1z, b_overlay * b_overlay_1x_1z, bl[3].get(), sl[3].get() },
                        faces_overlay[1].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay * r_overlay_1x_0z, g_overlay * g_overlay_1x_0z, b_overlay * b_overlay_1x_0z, bl[1].get(), sl[1].get() },
                        faces_overlay[1].corners[2],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[2] },
                        { r_overlay * r_overlay_0x_1z, g_overlay * g_overlay_0x_1z, b_overlay * b_overlay_0x_1z, bl[2].get(), sl[2].get() },
                        faces_overlay[1].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[0] },
                        { r_overlay * r_overlay_0x_0z, g_overlay * g_overlay_0x_0z, b_overlay * b_overlay_0x_0z, bl[0].get(), sl[0].get() },
                        faces_overlay[1].corners[3],
                    });
                }
            }

            /* Negative Y */
            if (do_face_neg_y)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][0]) + UAO([1][0][0]) + UAO([0][0][1])),
                    Uint8(UAO([2][0][0]) + UAO([1][0][0]) + UAO([2][0][1])),
                    Uint8(UAO([0][0][2]) + UAO([0][0][1]) + UAO([1][0][2])),
                    Uint8(UAO([2][0][2]) + UAO([1][0][2]) + UAO([2][0][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, -1, 0, 1, 0);
                templ[1] = calc_corner(+1, -1, -1, 0, 1, 0);
                templ[2] = calc_corner(-1, -1, +1, 0, 1, 0);
                templ[3] = calc_corner(+1, -1, +1, 0, 1, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[0].get(), sl[0].get() },
                    faces[4].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[1].get(), sl[1].get() },
                    faces[4].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[2].get(), sl[2].get() },
                    faces[4].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[3].get(), sl[3].get() },
                    faces[4].corners[2],
                });

                if (use_overlay[4])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay * r_overlay_0x_0z, g_overlay * g_overlay_0x_0z, b_overlay * b_overlay_0x_0z, bl[0].get(), sl[0].get() },
                        faces_overlay[4].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[1] },
                        { r_overlay * r_overlay_1x_0z, g_overlay * g_overlay_1x_0z, b_overlay * b_overlay_1x_0z, bl[1].get(), sl[1].get() },
                        faces_overlay[4].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay * r_overlay_0x_1z, g_overlay * g_overlay_0x_1z, b_overlay * b_overlay_0x_1z, bl[2].get(), sl[2].get() },
                        faces_overlay[4].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[3] },
                        { r_overlay * r_overlay_1x_1z, g_overlay * g_overlay_1x_1z, b_overlay * b_overlay_1x_1z, bl[3].get(), sl[3].get() },
                        faces_overlay[4].corners[2],
                    });
                }
            }

            /* Positive X */
            if (do_face_pos_x)
            {
                Uint8 ao[] = {
                    Uint8(UAO([2][0][0]) + UAO([2][1][0]) + UAO([2][0][1])),
                    Uint8(UAO([2][2][0]) + UAO([2][1][0]) + UAO([2][2][1])),
                    Uint8(UAO([2][0][2]) + UAO([2][0][1]) + UAO([2][1][2])),
                    Uint8(UAO([2][2][2]) + UAO([2][1][2]) + UAO([2][2][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(+1, -1, -1, 1, 0, 0);
                templ[1] = calc_corner(+1, +1, -1, 1, 0, 0);
                templ[2] = calc_corner(+1, -1, +1, 1, 0, 0);
                templ[3] = calc_corner(+1, +1, +1, 1, 0, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[0].get(), sl[0].get() },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[1].get(), sl[1].get() },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[2].get(), sl[2].get() },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[3].get(), sl[3].get() },
                    faces[0].corners[0],
                });

                if (use_overlay[0])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay * r_overlay_1x_0z, g_overlay * g_overlay_1x_0z, b_overlay * b_overlay_1x_0z, bl[0].get(), sl[0].get() },
                        faces_overlay[0].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay * r_overlay_1x_0z, g_overlay * g_overlay_1x_0z, b_overlay * b_overlay_1x_0z, bl[1].get(), sl[1].get() },
                        faces_overlay[0].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay * r_overlay_1x_1z, g_overlay * g_overlay_1x_1z, b_overlay * b_overlay_1x_1z, bl[2].get(), sl[2].get() },
                        faces_overlay[0].corners[2],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay * r_overlay_1x_1z, g_overlay * g_overlay_1x_1z, b_overlay * b_overlay_1x_1z, bl[3].get(), sl[3].get() },
                        faces_overlay[0].corners[0],
                    });
                }
            }

            /* Negative X */
            if (do_face_neg_x)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][0]) + UAO([0][1][0]) + UAO([0][0][1])),
                    Uint8(UAO([0][2][0]) + UAO([0][1][0]) + UAO([0][2][1])),
                    Uint8(UAO([0][0][2]) + UAO([0][0][1]) + UAO([0][1][2])),
                    Uint8(UAO([0][2][2]) + UAO([0][1][2]) + UAO([0][2][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, -1, 1, 0, 0);
                templ[1] = calc_corner(-1, +1, -1, 1, 0, 0);
                templ[2] = calc_corner(-1, -1, +1, 1, 0, 0);
                templ[3] = calc_corner(-1, +1, +1, 1, 0, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[3].get(), sl[3].get() },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[1].get(), sl[1].get() },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[2].get(), sl[2].get() },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[0].get(), sl[0].get() },
                    faces[3].corners[2],
                });

                if (use_overlay[3])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay * r_overlay_0x_1z, g_overlay * g_overlay_0x_1z, b_overlay * b_overlay_0x_1z, bl[3].get(), sl[3].get() },
                        faces_overlay[3].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay * r_overlay_0x_0z, g_overlay * g_overlay_0x_0z, b_overlay * b_overlay_0x_0z, bl[1].get(), sl[1].get() },
                        faces_overlay[3].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay * r_overlay_0x_1z, g_overlay * g_overlay_0x_1z, b_overlay * b_overlay_0x_1z, bl[2].get(), sl[2].get() },
                        faces_overlay[3].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay * r_overlay_0x_0z, g_overlay * g_overlay_0x_0z, b_overlay * b_overlay_0x_0z, bl[0].get(), sl[0].get() },
                        faces_overlay[3].corners[2],
                    });
                }
            }

            /* Positive Z */
            if (do_face_pos_z)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][2]) + UAO([1][0][2]) + UAO([0][1][2])),
                    Uint8(UAO([0][2][2]) + UAO([0][1][2]) + UAO([1][2][2])),
                    Uint8(UAO([2][0][2]) + UAO([1][0][2]) + UAO([2][1][2])),
                    Uint8(UAO([2][2][2]) + UAO([1][2][2]) + UAO([2][1][2])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, +1, 0, 0, 1);
                templ[1] = calc_corner(+1, -1, +1, 0, 0, 1);
                templ[2] = calc_corner(-1, +1, +1, 0, 0, 1);
                templ[3] = calc_corner(+1, +1, +1, 0, 0, 1);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[3].get(), sl[3].get() },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[1] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[2].get(), sl[2].get() },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r * r_1x_1z, g * g_1x_1z, b * b_1x_1z, bl[1].get(), sl[1].get() },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[0] },
                    { r * r_0x_1z, g * g_0x_1z, b * b_0x_1z, bl[0].get(), sl[0].get() },
                    faces[2].corners[2],
                });

                if (use_overlay[2])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay * r_overlay_1x_1z, g_overlay * g_overlay_1x_1z, b_overlay * b_overlay_1x_1z, bl[3].get(), sl[3].get() },
                        faces_overlay[2].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[1] },
                        { r_overlay * r_overlay_0x_1z, g_overlay * g_overlay_0x_1z, b_overlay * b_overlay_0x_1z, bl[2].get(), sl[2].get() },
                        faces_overlay[2].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay * r_overlay_1x_1z, g_overlay * g_overlay_1x_1z, b_overlay * b_overlay_1x_1z, bl[1].get(), sl[1].get() },
                        faces_overlay[2].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[0] },
                        { r_overlay * r_overlay_0x_1z, g_overlay * g_overlay_0x_1z, b_overlay * b_overlay_0x_1z, bl[0].get(), sl[0].get() },
                        faces_overlay[2].corners[2],
                    });
                }
            }

            /* Negative Z */
            if (do_face_neg_z)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][0]) + UAO([1][0][0]) + UAO([0][1][0])),
                    Uint8(UAO([0][2][0]) + UAO([0][1][0]) + UAO([1][2][0])),
                    Uint8(UAO([2][0][0]) + UAO([1][0][0]) + UAO([2][1][0])),
                    Uint8(UAO([2][2][0]) + UAO([1][2][0]) + UAO([2][1][0])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, -1, 0, 0, 1);
                templ[1] = calc_corner(+1, -1, -1, 0, 0, 1);
                templ[2] = calc_corner(-1, +1, -1, 0, 0, 1);
                templ[3] = calc_corner(+1, +1, -1, 0, 0, 1);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[0].get(), sl[0].get() },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r * r_0x_0z, g * g_0x_0z, b * b_0x_0z, bl[2].get(), sl[2].get() },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[2] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[1].get(), sl[1].get() },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[3] },
                    { r * r_1x_0z, g * g_1x_0z, b * b_1x_0z, bl[3].get(), sl[3].get() },
                    faces[5].corners[0],
                });

                if (use_overlay[5])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay * r_overlay_0x_0z, g_overlay * g_overlay_0x_0z, b_overlay * b_overlay_0x_0z, bl[0].get(), sl[0].get() },
                        faces_overlay[5].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay * r_overlay_0x_0z, g_overlay * g_overlay_0x_0z, b_overlay * b_overlay_0x_0z, bl[2].get(), sl[2].get() },
                        faces_overlay[5].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[2] },
                        { r_overlay * r_overlay_1x_0z, g_overlay * g_overlay_1x_0z, b_overlay * b_overlay_1x_0z, bl[1].get(), sl[1].get() },
                        faces_overlay[5].corners[2],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[3] },
                        { r_overlay * r_overlay_1x_0z, g_overlay * g_overlay_1x_0z, b_overlay * b_overlay_1x_0z, bl[3].get(), sl[3].get() },
                        faces_overlay[5].corners[0],
                    });
                }
            }
        }
        /* ============ END: IS_NORMAL ============ */
    }

    TRACE("Chunk: <%d, %d, %d>, Vertices (Solid): %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx_solid.size(), vtx_solid.size() / 4 * 6);
    TRACE("Chunk: <%d, %d, %d>, Vertices (Trans): %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx_translucent.size(), vtx_translucent.size() / 4 * 6);
    TRACE("Chunk: <%d, %d, %d>, Vertices (Overlay): %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx_overlay.size(), vtx_overlay.size() / 4 * 6);

    if (!vtx_solid.size() && !vtx_translucent.size())
    {
        center->index_type = GL_NONE;
        center->index_count = 0;
        return;
    }

    if (!center->vao)
    {
        terrain_vertex_t::create_vao(&center->vao);
        tetra::gl_obj_label(GL_VERTEX_ARRAY, center->vao, "[Level][Chunk]: <%d, %d, %d>: VAO", center->pos.x, center->pos.y, center->pos.z);
    }
    glBindVertexArray(center->vao);
    if (!center->vbo)
    {
        glGenBuffers(1, &center->vbo);
        tetra::gl_obj_label(GL_BUFFER, center->vbo, "[Level][Chunk]: <%d, %d, %d>: VBO", center->pos.x, center->pos.y, center->pos.z);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    center->index_type = GL_UNSIGNED_INT;
    center->index_count = vtx_solid.size() / 4 * 6;
    center->index_count_overlay = vtx_overlay.size() / 4 * 6;
    center->index_count_translucent = vtx_translucent.size() / 4 * 6;

    /* Combine vectors into one */
    vtx_solid.reserve(vtx_solid.size() + vtx_overlay.size() + vtx_translucent.size());
    vtx_solid.insert(vtx_solid.end(), vtx_overlay.begin(), vtx_overlay.end());
    vtx_solid.insert(vtx_solid.end(), vtx_translucent.begin(), vtx_translucent.end());

    glBindBuffer(GL_ARRAY_BUFFER, center->vbo);
    glBufferData(GL_ARRAY_BUFFER, vtx_solid.size() * sizeof(vtx_solid[0]), vtx_solid.data(), GL_STATIC_DRAW);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
}

#include "shared/cubiomes/biomes.h"
#include "shared/cubiomes/generator.h"

#define GEN ((Generator*)(generator))

void level_t::generator_create()
{
    generator = new Generator;
    setupGenerator(GEN, MC_B1_8, 0);
}

void level_t::generator_destroy() { delete GEN; }

void level_t::generate_biome_ids(glm::ivec3 chunk_pos, std::vector<mc_id::biome_t>& biome_ids, int oversample)
{
    applySeed(GEN, DIM_OVERWORLD, mc_seed);

    Range r = {
        .scale = 1,
        .x = chunk_pos.x * SUBCHUNK_SIZE_X - oversample,
        .z = chunk_pos.z * SUBCHUNK_SIZE_Z - oversample,
        .sx = SUBCHUNK_SIZE_X + 2 * oversample,
        .sz = SUBCHUNK_SIZE_Z + 2 * oversample,
        .y = chunk_pos.y * SUBCHUNK_SIZE_Y,
        .sy = 1,
    };

    biome_ids.resize(getMinCacheSize(GEN, r.scale, r.sx, r.sy, r.sz));

    switch (dimension)
    {
    case mc_id::DIMENSION_OVERWORLD:
        static_assert(sizeof(mc_id::biome_t) == sizeof(int));
        genBiomes(GEN, (int*)biome_ids.data(), r);
        break;
    case mc_id::DIMENSION_NETHER:
        for (auto& it : biome_ids)
            it = mc_id::BIOME_NETHER_WASTES;
        break;
    case mc_id::DIMENSION_END:
        for (auto& it : biome_ids)
            it = mc_id::BIOME_THE_END;
        break;
    }
}

/**
 * One dimensional Gaussian blur function
 */
SDL_FORCE_INLINE float gauss(const float x, const float sigma)
{
    return (1.0f / sqrtf(SDL_PI_F * 2.0f * sigma * sigma)) * expf(-(x * x) / (2.0f * sigma * sigma));
}

SDL_FORCE_INLINE const glm::vec3 get_color_map(const float temperature, const float humidity)
{
    glm::vec3 ret(0.0f);

    float temp = SDL_clamp(temperature, 0.0f, 1.0f);
    float rain = SDL_clamp(humidity, 0.0f, 1.0f);

    rain = rain * temp;

    ret.r = ((temp + rain) * 90.0 + 30.0) / 255.0;
    ret.g = ((rain) * 55.0 + 180.0) / 255.0;
    ret.b = ((1.0 - temp) * 80.0 + 50.0) / 255.0;

    return ret;
}

void level_t::generate_climate_colors(const glm::ivec3 chunk_pos, glm::vec3 colors[18][18], float temperature[18][18], float humidity[18][18])
{
    generate_climate_parameters(chunk_pos, temperature, humidity);
    for (int i = 0; i < 18; i++)
        for (int j = 0; j < 18; j++)
            colors[i][j] = get_color_map(temperature[i][j], humidity[i][j]);
}

void level_t::generate_climate_parameters(const glm::ivec3 chunk_pos, float temperature[18][18], float humidity[18][18])
{
    const int biome_oversample = cvr_r_biome_oversample.get();
    std::vector<mc_id::biome_t> biome_ids;
    generate_biome_ids(chunk_pos, biome_ids, biome_oversample + 1);

    float get_biome_temperature[mc_id::BIOME_NUM_BIOMES];
    float get_biome_downfall[mc_id::BIOME_NUM_BIOMES];

    for (int i = 0; i < IM_ARRAYSIZE(get_biome_temperature); i++)
        get_biome_temperature[i] = mc_id::get_biome_temperature(i);

    for (int i = 0; i < IM_ARRAYSIZE(get_biome_downfall); i++)
        get_biome_downfall[i] = mc_id::get_biome_downfall(i);

    const int array_width = SUBCHUNK_SIZE_X + (1 + biome_oversample) * 2;

    /* If blurring isn't requested, then just write out the unmodified values */
    if (biome_oversample == 0)
    {
        for (int x = 0; x < array_width; x++)
            for (int z = 0; z < array_width; z++)
            {
                mc_id::biome_t id = biome_ids[z * array_width + x];
                assert(BETWEEN_INCL(id, 0, mc_id::BIOME_NUM_BIOMES - 1));
                temperature[x][z] = mc_id::get_biome_temperature(id);
                humidity[x][z] = mc_id::get_biome_downfall(id);
            }
        return;
    }

    /* Gaussian blur climate values, (This is broken) */

    std::vector<float> tmp0_temp;
    std::vector<float> tmp0_rain;
    tmp0_temp.resize(array_width * array_width, 0.0f);
    tmp0_rain.resize(array_width * array_width, 0.0f);

    /* Setup Gaussian blur */
    const float sigma = (biome_oversample + 0.5f) / 3.0f;
    std::vector<float> kernel_;
    kernel_.reserve(biome_oversample * 2 + 3);
    kernel_.push_back(0.0f);
    for (int i = -biome_oversample; i <= biome_oversample; i++)
        kernel_.push_back(gauss(i, sigma));
    kernel_.push_back(0.0f);
    const float* const kernel = kernel_.data() + biome_oversample + 1;

    /* Clear output arrays */
    for (int x = 0; x < 18; x++)
        for (int z = 0; z < 18; z++)
            temperature[x][z] = 0.0f, humidity[x][z] = 0.0f;

#define IDX(X, Z) ((Z + biome_oversample) * array_width + (X + biome_oversample))

    /* Blur on X axis */
    for (int x = 0; x < 18; x++)
        for (int z = 0; z < array_width; z++)
            for (int i = -biome_oversample; i <= biome_oversample; i++)
            {
                tmp0_temp[z * array_width + x + biome_oversample] += kernel[i] * get_biome_temperature[biome_ids[IDX(x + i, z)]];
                tmp0_rain[z * array_width + x + biome_oversample] += kernel[i] * get_biome_downfall[biome_ids[IDX(x + i, z)]];
            }

    /* Blur on Z axis and write out */
    for (int x = 0; x < 18; x++)
        for (int z = 0; z < 18; z++)
            for (int i = -biome_oversample; i <= biome_oversample; i++)
            {
                temperature[x][z] += kernel[i] * tmp0_temp[IDX(x, z + i)];
                humidity[x][z] += kernel[i] * tmp0_rain[IDX(x, z + i)];
            }
}

mc_id::biome_t level_t::get_biome_at(const glm::ivec3 pos)
{
    switch (dimension)
    {
    case mc_id::DIMENSION_OVERWORLD:
        static_assert(sizeof(mc_id::biome_t) == sizeof(int));
        applySeed(GEN, DIM_OVERWORLD, mc_seed);
        return mc_id::biome_t(getBiomeAt(GEN, 1, pos.x, pos.y, pos.z));
        break;
    case mc_id::DIMENSION_NETHER:
        return mc_id::BIOME_NETHER_WASTES;
        break;
    case mc_id::DIMENSION_END:
        return mc_id::BIOME_THE_END;
        break;
    }
    return mc_id::BIOME_OCEAN;
}

#if (FORCE_OPT_MESH)
#pragma GCC pop_options
#endif

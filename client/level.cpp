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

void level_t::clear_mesh(bool free_gl)
{
    for (chunk_cubic_t* c : chunks)
    {
        if (free_gl)
            c->free_gl();
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
    }
}

void level_t::rebuild_meshes()
{
    clear_mesh(false);

    build_dirty_meshes();
}

void level_t::build_dirty_meshes()
{
    size_t built;
    Uint64 elapsed;
    Uint64 tick_start;
    Uint64 tick_start_ms = SDL_GetTicks();

    /* Ensure chunk map is correct (TODO: level_t needs a add/remove chunk system) */
    cmap.clear();
    for (size_t i = 0; i < chunks.size(); i++)
        cmap[chunks[i]->pos] = chunks[i];

    /* First Light Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (chunks[i]->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL)
            continue;
        chunks[i]->clear_light_block(0);
        chunks[i]->clear_light_sky(0);
        light_pass(chunks[i]->pos.x, chunks[i]->pos.y, chunks[i]->pos.z, true);
        chunks[i]->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0;
        built++;
    }
    PASS_TIMER_STOP("Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 1)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Second Light Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (chunks[i]->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0)
            continue;
        light_pass(chunks[i]->pos.x, chunks[i]->pos.y, chunks[i]->pos.z, false);
        chunks[i]->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1;
        built++;
    }
    PASS_TIMER_STOP("Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 2)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Third Light Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (chunks[i]->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1)
            continue;
        light_pass(chunks[i]->pos.x, chunks[i]->pos.y, chunks[i]->pos.z, false);
        chunks[i]->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
        built++;
    }
    PASS_TIMER_STOP("Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 3)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Mesh Pass */
    PASS_TIMER_START();
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (chunks[i]->dirty_level != chunk_cubic_t::DIRTY_LEVEL_MESH)
            continue;
        build_mesh(chunks[i]->pos.x, chunks[i]->pos.y, chunks[i]->pos.z);
        chunks[i]->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
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

    auto it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(0, 0, 0));
    cross.c = ((it != cmap.end()) ? it->second : NULL);

    if (!local_only)
    {
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+1, +0, +0));
        cross.pos_x = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(-1, +0, +0));
        cross.neg_x = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +1, +0));
        cross.pos_y = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, -1, +0));
        cross.neg_y = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +0, +1));
        cross.pos_z = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +0, -1));
        cross.neg_z = it != cmap.end() ? it->second : NULL;
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

        auto it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(ix, iy, iz));
        rubik[ix + 1][iy + 1][iz + 1] = ((it != cmap.end()) ? it->second : NULL);
    }

    chunk_cubic_t* center = rubik[1][1][1];

    if (!center)
    {
        dc_log_error("Attempt made to build unloaded chunk at chunk coordinates: %d, %d, %d", chunk_x, chunk_y, chunk_z);
        return;
    }

    if (!terrain)
    {
        dc_log_error("A texture atlas is required to build a chunk");
        return;
    }

    std::vector<terrain_vertex_t> vtx_solid;
    std::vector<terrain_vertex_t> vtx_translucent;

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z; dat_it++)
    {
        /* This is to keep the loop body 8 spaces closer to the left margin */
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        block_id_t type = rubik[1][1][1]->get_type(x, y, z);
        if (type == BLOCK_ID_AIR)
            continue;

        std::vector<terrain_vertex_t>* vtx = mc_id::is_translucent(type) ? &vtx_translucent : &vtx_solid;

        Uint8 metadata = rubik[1][1][1]->get_metadata(x, y, z);

        float r = float((type) & 3) / 3.0f, g = float((type >> 2) & 3) / 3.0f, b = float((type >> 4) & 3) / 3.0f;
        r = 1.0, g = 1.0, b = 1.0;

        /** Index: [x+1][y+1][z+1] */
        block_id_t stypes[3][3][3];
        Uint8 smetadata[3][3][3];
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
                    /* TODO: Move this out? */
                    smetadata[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_metadata(local_x, local_y, local_z);
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

            BLOCK_SIMPLE(BLOCK_ID_WATER_FLOWING, mc_id::FACE_WATER_FLOW);
            BLOCK_SIMPLE(BLOCK_ID_WATER_SOURCE, mc_id::FACE_WATER_STILL);
            BLOCK_SIMPLE(BLOCK_ID_LAVA_FLOWING, mc_id::FACE_LAVA_FLOW);
            BLOCK_SIMPLE(BLOCK_ID_LAVA_SOURCE, mc_id::FACE_LAVA_STILL);

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
            BLOCK_SIMPLE(BLOCK_ID_RAIL_POWERED, mc_id::FACE_RAIL_GOLDEN_POWERED);
            BLOCK_SIMPLE(BLOCK_ID_RAIL_DETECTOR, mc_id::FACE_RAIL_DETECTOR);
            BLOCK_SIMPLE(BLOCK_ID_PISTON_STICKY, mc_id::FACE_PISTON_TOP_STICKY);
            BLOCK_SIMPLE(BLOCK_ID_COBWEB, mc_id::FACE_WEB);
        case BLOCK_ID_FOLIAGE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(0, mc_id::FACE_DEADBUSH);
                BLOCK_SIMPLE(1, mc_id::FACE_FERN);
            default: /* Fall through */
                BLOCK_SIMPLE(2, mc_id::FACE_TALLGRASS);
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

        if (type == BLOCK_ID_GRASS || type == BLOCK_ID_MOSS || type == BLOCK_ID_FOLIAGE)
            r = 0.2f, g = 0.8f, b = 0.2f;
        else if (type == BLOCK_ID_LEAVES)
        {
            switch (metadata)
            {
            case WOOD_ID_SPRUCE:
                r = 0.380f, g = 0.600f, b = 0.380f;
                break;
            case WOOD_ID_BIRCH:
                r = 0.502f, g = 0.655f, b = 0.333f;
                break;
            case WOOD_ID_OAK: /* Fall through */
            default:
                r = 0.2f, g = 1.0f, b = 0.2f;
                break;
            }
        }

#define UAO(X) (!mc_id::is_transparent(stypes X))
#define UBL(X) (slight_block X * mc_id::is_transparent(stypes X))

        /* ============ BEGIN: IS_TORCH ============ */
        if (type == BLOCK_ID_TORCH || type == BLOCK_ID_TORCH_REDSTONE_ON || type == BLOCK_ID_TORCH_REDSTONE_OFF)
        {
            /* Positive Y */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                glm::vec2 cs = faces[1].corners[3] - faces[1].corners[0];
                faces[1].corners[0] += cs * glm::vec2(0.4375f, 0.375f);
                faces[1].corners[3] = faces[1].corners[0] + cs / 8.0f;

                faces[1].corners[1] = { faces[1].corners[3].x, faces[1].corners[0].y };
                faces[1].corners[2] = { faces[1].corners[0].x, faces[1].corners[3].y };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 10), Sint16(z * 16 + 9), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][2][1] },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 10), Sint16(z * 16 + 7), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][2][1] },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 10), Sint16(z * 16 + 9), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][2][1] },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 10), Sint16(z * 16 + 7), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][2][1] },
                    faces[1].corners[3],
                });
            }

            /* Positive X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[2][1][1] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[2][1][1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 0), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[2][1][1] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[2][1][1] },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[0][1][1] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[0][1][1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 0), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[0][1][1] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[0][1][1] },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 9), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][2] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 9), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][2] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 0), Sint16(z * 16 + 9), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][2] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 9), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][2] },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 7), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][0] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 7), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][0] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 0), Sint16(z * 16 + 7), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][0] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 7), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][0] },
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
                    { r, g, b, bl[0], slight_sky[2][1][1] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[2][1][1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[2][1][1] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[2][1][1] },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[0][1][1] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[0][1][1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[0][1][1] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r, g, b, bl[0], slight_sky[0][1][1] },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][2] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][2] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][2] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][2] },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][0] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][0] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][0] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][0] },
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
            if (mc_id::is_transparent(stypes[1][0][1]) && !IS_SAME_FLUID(stypes[1][0][1], type))
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
            if (mc_id::is_transparent(stypes[2][1][1]) && !IS_SAME_FLUID(stypes[2][1][1], type))
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
            if (mc_id::is_transparent(stypes[0][1][1]) && !IS_SAME_FLUID(stypes[0][1][1], type))
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
            if (mc_id::is_transparent(stypes[1][1][2]) && !IS_SAME_FLUID(stypes[1][1][2], type))
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
            if (mc_id::is_transparent(stypes[1][1][0]) && !IS_SAME_FLUID(stypes[1][1][0], type))
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
        /* ============ BEGIN: IS_NORMAL ============ */
        else
        {
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

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][2][1] },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][2][1] },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][2][1] },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[0] },
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

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][0][1] },
                    faces[4].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][0][1] },
                    faces[4].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][0][1] },
                    faces[4].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[3] },
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

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[2][1][1] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[2][1][1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2], slight_sky[2][1][1] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
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

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3], slight_sky[0][1][1] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[0][1][1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2], slight_sky[0][1][1] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
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

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][2] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][2] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][2] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[0] },
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

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][0] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][0] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][0] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][0] },
                    faces[5].corners[0],
                });
            }
        }
        /* ============ END: IS_NORMAL ============ */
    }

    TRACE("Chunk: <%d, %d, %d>, Vertices: %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx.size(), vtx.size() / 4 * 6);

    if (!vtx_solid.size() && !vtx_translucent.size())
    {
        center->index_type = GL_NONE;
        center->index_count = 0;
        return;
    }

    if (!center->vao)
        terrain_vertex_t::create_vao(&center->vao);
    glBindVertexArray(center->vao);
    if (!center->vbo)
    {
        glGenBuffers(1, &center->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    center->index_type = GL_UNSIGNED_INT;
    center->index_count = vtx_solid.size() / 4 * 6;
    center->index_count_translucent = vtx_translucent.size() / 4 * 6;

    for (terrain_vertex_t v : vtx_translucent)
        vtx_solid.push_back(v);

    glBindBuffer(GL_ARRAY_BUFFER, center->vbo);
    glBufferData(GL_ARRAY_BUFFER, vtx_solid.size() * sizeof(vtx_solid[0]), vtx_solid.data(), GL_STATIC_DRAW);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
}

void level_t::set_block(glm::ivec3 pos, block_id_t type, Uint8 metadata)
{
    if (type >= BLOCK_ID_MAX)
    {
        dc_log_error("Preposterous id field %d:%u at <%d, %d, %d>", type, metadata, pos.x, pos.y, pos.z);
        return;
    }
    if (metadata > 15)
    {
        const char* name = mc_id::get_name_from_item_id(type, metadata);
        dc_log_error("Preposterous metadata field %d(%s):%u at <%d, %d, %d>", type, name, metadata, pos.x, pos.y, pos.z);
        return;
    }

    /* Ensure chunk map is correct (TODO: level_t needs a add/remove chunk system) */
    if (chunks.size() != cmap.size())
    {
        cmap.clear();
        for (chunk_cubic_t* c : chunks)
            cmap[c->pos] = c;
    }

    /* Attempt to find chunk */
    glm::ivec3 chunk_pos = pos >> 4;
    glm::ivec3 block_pos = pos & 0x0F;
    auto it = cmap.find(chunk_pos);
    if (it == cmap.end())
    {
        dc_log_error("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
        return;
    }

    chunk_cubic_t* c = it->second;

    /* Get existing blocks data to help determine which chunks need rebuilding */
    block_id_t old_type = c->get_type(block_pos.x, block_pos.y, block_pos.z);

    /* Set type */
    c->set_type(block_pos.x, block_pos.y, block_pos.z, type);
    c->set_metadata(block_pos.x, block_pos.y, block_pos.z, metadata);

    /* Surrounding chunks do not need updating if the replacement has an equal effect on lighting */
    if (mc_id::is_transparent(old_type) == mc_id::is_transparent(type) && mc_id::get_light_level(old_type) == mc_id::get_light_level(type))
        return;

    int diff_x = ((block_pos.x & 0x0F) < 8) ? -1 : 1;
    int diff_y = ((block_pos.y & 0x0F) < 8) ? -1 : 1;
    int diff_z = ((block_pos.z & 0x0F) < 8) ? -1 : 1;

    for (int i = 0; i < 8; i++)
    {
        const int ix = (i >> 2) & 1;
        const int iy = (i >> 1) & 1;
        const int iz = i & 1;

        c = ((it = cmap.find(chunk_pos + glm::ivec3(diff_x * ix, diff_y * iy, diff_z * iz))) != cmap.end()) ? it->second : NULL;

        chunk_cubic_t::dirty_level_t dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;

        /* Lower the amount of compute passes */
        int itotal = ix + iy + iz;
        if (itotal == 2)
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0;
        else if (itotal == 3)
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1;

        if (c && c->dirty_level < dirt_face)
            c->dirty_level = dirt_face;
    }
}

bool level_t::get_block(glm::ivec3 pos, block_id_t& type, Uint8& metadata)
{
    /* Ensure chunk map is correct (TODO: level_t needs a add/remove chunk system) */
    if (chunks.size() != cmap.size())
    {
        cmap.clear();
        for (chunk_cubic_t* c : chunks)
            cmap[c->pos] = c;
    }

    /* Attempt to find chunk */
    glm::ivec3 chunk_pos = pos >> 4;
    glm::ivec3 block_pos = pos & 0x0F;
    auto it = cmap.find(chunk_pos);
    if (it == cmap.end())
    {
        dc_log_error("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
        return false;
    }

    chunk_cubic_t* c = it->second;

    type = c->get_type(block_pos.x, block_pos.y, block_pos.z);
    metadata = c->get_metadata(block_pos.x, block_pos.y, block_pos.z);

    return true;
}

level_t::level_t(texture_terrain_t* _terrain)
{
    terrain = _terrain;

    /* Size the buffer for maximum 36 quads for every block, TODO-OPT: Would multiple smaller buffers be better? */
    size_t quads = SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * 6 * 6;
    std::vector<Uint32> ind;
    ind.reserve(quads * 6);
    for (size_t i = 0; i < quads; i++)
    {
        ind.push_back(i * 4 + 0);
        ind.push_back(i * 4 + 1);
        ind.push_back(i * 4 + 2);
        ind.push_back(i * 4 + 2);
        ind.push_back(i * 4 + 1);
        ind.push_back(i * 4 + 3);
    }

    glBindVertexArray(0);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size() * sizeof(ind[0]), ind.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

level_t::~level_t()
{
    glDeleteBuffers(1, &ebo);
    for (size_t i = 0; i < chunks.size(); i++)
        delete chunks[i];
    chunks.resize(0);
}

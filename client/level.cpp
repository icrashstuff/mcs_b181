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

void level_t::build_dirty_meshes()
{
    Uint64 tick_mesh_start = SDL_GetTicksNS();
    size_t built = 0;
    for (size_t i = 0; i < chunks.size(); i++)
    {
        if (!chunks[i]->dirty)
            continue;
        chunks[i]->dirty = 0;
        built++;
        build_mesh(chunks[i]->chunk_x, chunks[i]->chunk_y, chunks[i]->chunk_z);
    }
    Uint64 elapsed = SDL_GetTicksNS() - tick_mesh_start;
    if (built)
        dc_log("Built %zu meshes in %.2f ms (%.2f ms per)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
}

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
        for (size_t j = 0; j < chunks.size(); j++)
        {
            chunk_cubic_t* c = chunks[j];
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

        float r = float((type) & 3) / 3.0f, g = float((type >> 2) & 3) / 3.0f, b = float((type >> 4) & 3) / 3.0f;
        r = 1.0, g = 1.0, b = 1.0;

        Uint8 light_block = rubik[1][1][1]->get_light_block(x, y, z);
        Uint8 light_sky = rubik[1][1][1]->get_light_sky(x, y, z);

        /** Index: [x+1][y+1][z+1] */
        block_id_t stypes[3][3][3];
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
                }
            }
        }

        mc_id::terrain_face_t f;

        if (type == BLOCK_ID_STONE)
            f = terrain->get_face(mc_id::FACE_STONE);
        else if (type == BLOCK_ID_SAND)
            f = terrain->get_face(mc_id::FACE_SAND);
        else if (type == BLOCK_ID_GRASS)
            f = terrain->get_face(mc_id::FACE_GRASS_TOP);
        else if (type == BLOCK_ID_COBWEB)
            f = terrain->get_face(mc_id::FACE_WEB);
        else if (type == BLOCK_ID_CACTUS)
            f = terrain->get_face(mc_id::FACE_CACTUS_SIDE);
        else if (type == BLOCK_ID_COBBLESTONE)
            f = terrain->get_face(mc_id::FACE_COBBLESTONE);
        else if (type == BLOCK_ID_SPONGE)
            f = terrain->get_face(mc_id::FACE_SPONGE);
        else if (type == BLOCK_ID_GLASS)
            f = terrain->get_face(mc_id::FACE_GLASS);
        else if (type == BLOCK_ID_COBBLESTONE_MOSSY)
            f = terrain->get_face(mc_id::FACE_COBBLESTONE_MOSSY);
        else if (type == BLOCK_ID_SANDSTONE)
            f = terrain->get_face(mc_id::FACE_SANDSTONE_NORMAL);
        else if (type == BLOCK_ID_DIRT)
            f = terrain->get_face(mc_id::FACE_DIRT);
        else if (type == BLOCK_ID_GRAVEL)
            f = terrain->get_face(mc_id::FACE_GRAVEL);
        else if (type == BLOCK_ID_ORE_COAL)
            f = terrain->get_face(mc_id::FACE_COAL_ORE);
        else if (type == BLOCK_ID_ORE_IRON)
            f = terrain->get_face(mc_id::FACE_IRON_ORE);
        else if (type == BLOCK_ID_ORE_GOLD)
            f = terrain->get_face(mc_id::FACE_GOLD_ORE);
        else if (type == BLOCK_ID_ORE_LAPIS)
            f = terrain->get_face(mc_id::FACE_LAPIS_ORE);
        else if (type == BLOCK_ID_ORE_REDSTONE_ON || type == BLOCK_ID_ORE_REDSTONE_OFF)
            f = terrain->get_face(mc_id::FACE_REDSTONE_ORE);
        else if (type == BLOCK_ID_ORE_DIAMOND)
            f = terrain->get_face(mc_id::FACE_DIAMOND_ORE);
        else if (type == BLOCK_ID_BRICKS_STONE)
            f = terrain->get_face(mc_id::FACE_STONEBRICK);
        else if (type == BLOCK_ID_LAVA_SOURCE)
            f = terrain->get_face(mc_id::FACE_LAVA_STILL);
        else if (type == BLOCK_ID_LAVA_FLOWING)
            f = terrain->get_face(mc_id::FACE_LAVA_FLOW_STRAIGHT);
        else if (type == BLOCK_ID_WATER_SOURCE)
            f = terrain->get_face(mc_id::FACE_WATER_STILL);
        else if (type == BLOCK_ID_WATER_FLOWING)
            f = terrain->get_face(mc_id::FACE_WATER_FLOW_STRAIGHT);
        else
            f = terrain->get_face(mc_id::FACE_DEBUG);

        /* Positive Y */
        if (mc_id::is_transparent(stypes[1][2][1]) && stypes[1][2][1] != type)
        {
            Uint8 ao[] = {
                Uint8((stypes[0][2][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][2] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][2] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[2] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[3] });
        }

        /* Negative Y */
        if (mc_id::is_transparent(stypes[1][0][1]) && stypes[1][0][1] != type)
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][0][2] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][2] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[2] });
        }

        /* Positive X */
        if (mc_id::is_transparent(stypes[2][1][1]) && stypes[2][1][1] != type)
        {
            Uint8 ao[] = {
                Uint8((stypes[2][0][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][2] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][2] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[2] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[0] });
        }

        /* Negative X */
        if (mc_id::is_transparent(stypes[0][1][1]) && stypes[0][1][1] != type)
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][0][2] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][2] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[2] });
        }

        /* Positive Z */
        if (mc_id::is_transparent(stypes[1][1][2]) && stypes[1][1][2] != type)
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][2] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][2] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][2] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][2] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[2] });
        }

        /* Negative Z */
        if (mc_id::is_transparent(stypes[1][1][0]) && stypes[1][1][0] != type)
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 16, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[2] });
            vtx.push_back({ { 16, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[0] });
        }
    }

    dc_log_trace("Chunk: <%d, %d, %d>, Vertices: %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx.size(), vtx.size() / 4 * 6);

    if (!center->vao)
        terrain_vertex_t::create_vao(&center->vao);
    glBindVertexArray(center->vao);
    // TODO: Recycle VBO, and use one global chunk EBO
    center->index_type = terrain_vertex_t::create_vbo(&center->vbo, &center->ebo, vtx);
    center->index_count = vtx.size() / 4 * 6;
}

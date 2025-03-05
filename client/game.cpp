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

#include "game.h"

#include "tetra/util/convar.h"

#include "shared/chunk.h"
#include "shared/misc.h"

game_resources_t::game_resources_t() { reload(); }

void game_resources_t::reload()
{
    destroy();

    terrain_atlas = new texture_terrain_t("/_resources/assets/minecraft/textures/");
    terrain_shader = new shader_t("/shaders/terrain.vert", "/shaders/terrain.frag");

    terrain_shader->build();
    glUseProgram(terrain_shader->id);
    terrain_shader->set_uniform("ao_algorithm", ao_algorithm);
    terrain_shader->set_uniform("use_texture", use_texture);
}

void game_resources_t::destroy()
{
    delete terrain_atlas;
    delete terrain_shader;

    terrain_atlas = NULL;
    terrain_shader = NULL;
}

game_resources_t::~game_resources_t() { destroy(); }

static SDL_AtomicInt game_counter = { 0 };

game_t::game_t(const std::string addr, const Uint16 port, const std::string username, const game_resources_t* const _resources)
    : game_id(SDL_AddAtomicInt(&game_counter, 1))
{
    level = new level_t(_resources->terrain_atlas);
    level->lightmap.set_world_time(1000);

    connection = new connection_t();
    connection->init(addr, port, username);

    reload_resources(_resources);
}

game_t::game_t(const game_resources_t* const _resources)
    : game_id(SDL_AddAtomicInt(&game_counter, 1))
{
    level = new level_t(_resources->terrain_atlas);
    level->lightmap.set_world_time(1000);

    reload_resources(_resources);
}

/**
 * Forces a reload of all resources
 *
 * @param resources New resources struct to pull from (NULL to reuse existing one)
 * @param force_null Allows setting resources to a null object
 */
void game_t::reload_resources(const game_resources_t* const _resources, const bool force_null)
{
    if (_resources || force_null)
        resources = _resources;

    if (level)
    {
        level->set_terrain(resources ? resources->terrain_atlas : NULL);
        level->shader_terrain = resources ? resources->terrain_shader : NULL;
    }
}

game_t::~game_t()
{
    delete level;
    delete connection;
    connection = NULL;
    level = NULL;
}

static convar_int_t cvr_testworld("dev_world", 0, 0, 1, "Init to test world", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_world_size("dev_world_size", 6, 1, 32, "Side dimensions of the test world", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_world_y_off_pos("dev_world_y_off_pos", 0, 0, 32, "Positive Chunk Y offset of the test world", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_world_y_off_neg("dev_world_y_off_neg", 6, 0, 32, "Negative Chunk Y offset of the test world", CONVAR_FLAG_DEV_ONLY);
void game_t::create_testworld()
{
    level->clear();
    level->camera_pos = { -38, -12.7, -52 };
    level->camera_direction = { 1, 0, 0 };
    level->camera_right = { 1, 0, 0 };
    level->camera_up = { 0, 1, 0 };
    level->yaw = 119.0f;
    level->pitch = -45.0f;
    level->fov = -1.0f;
    level->mc_seed = 1;

    const int world_size = cvr_world_size.get();
    std::vector<chunk_cubic_t*> gen_chunks;

    gen_chunks.resize(world_size * world_size * 8, nullptr);

    util::parallel_for(0, world_size * world_size, [&gen_chunks, &world_size](const int _start, const int _end) {
        for (int i = _start; i < _end; i++)
        {
            chunk_t c_old;
            if (world_size < 4)
                c_old.generate_from_seed_over(1, i / world_size - world_size / 2, i % world_size - world_size / 2);
            else
                c_old.generate_from_seed_over(1, i / world_size - world_size + 6, i % world_size - world_size + 6);
            for (int j = 0; j < 8; j++)
            {
                chunk_cubic_t* c = new chunk_cubic_t();
                c->pos.x = i / world_size - world_size;
                c->pos.z = i % world_size - world_size;
                c->pos.y = j - cvr_world_y_off_neg.get() + cvr_world_y_off_pos.get();
                gen_chunks[i * 8 + j] = c;
                for (int x = 0; x < 16; x++)
                    for (int z = 0; z < 16; z++)
                        for (int y = 0; y < 16; y++)
                        {
                            c->set_type(x, y, z, c_old.get_type(x, y + j * 16, z));
                            c->set_metadata(x, y, z, c_old.get_metadata(x, y + j * 16, z));
                            c->set_light_block(x, y, z, c_old.get_light_block(x, y + j * 16, z));
                            c->set_light_sky(x, y, z, c_old.get_light_sky(x, y + j * 16, z));
                        }
            }
        }
    });

    for (chunk_cubic_t* c : gen_chunks)
        level->add_chunk(c);

    for (int i = 0; i < 16; i++)
    {
        chunk_cubic_t* c = new chunk_cubic_t();
        c->pos.x = i % 4;
        c->pos.y = (i / 4) % 4;
        c->pos.z = (i / 16);
        level->add_chunk(c);
        for (int x = 0; x < 16; x++)
            for (int z = 0; z < 16; z++)
                for (int y = 0; y < 16; y++)
                {
                    c->set_type(x, y, z, i + 1);
                    c->set_light_sky(x, y, z, SDL_min((SDL_fabs(x - 7.5) + SDL_fabs(z - 7.5)) * 2.2, 15));
                }

        for (int x = 4; x < 12; x++)
            for (int z = 4; z < 12; z++)
                for (int y = 2; y < 12; y++)
                    c->set_type(x, y, z, 0);
        c->set_type(7, 2, 5 + i % 4, BLOCK_ID_GLASS);
        c->set_type(8, 2, 5 + i % 4, BLOCK_ID_GLASS);
        c->set_type(7, 2, 7, BLOCK_ID_TORCH);
        c->set_type(7, 12, 5, BLOCK_ID_TORCH);
    }

    /* Test light propagation across all 3 axis jumps
     * Non of the chunks are fully filled to ensure that they don't pull face light values from adjoining ones */
    {
        glm::ivec3 pos = { 4, -2, -4 };

        chunk_cubic_t* c = new chunk_cubic_t();
        c->pos = pos;
        for (int x = 1; x < 15; x++)
            for (int z = 1; z < 15; z++)
                for (int y = 1; y < 15; y++)
                    c->set_type(x, y, z, BLOCK_ID_GLOWSTONE);

        /* This block is to verify that smooth lighting doesn't jump through seams/corners */
        c->set_type(7, 7, 7, BLOCK_ID_AIR);

        c->set_type(8, 7, 7, BLOCK_ID_DIAMOND);
        c->set_type(7, 8, 7, BLOCK_ID_DIAMOND);
        c->set_type(7, 7, 8, BLOCK_ID_DIAMOND);
        c->set_type(6, 7, 7, BLOCK_ID_DIAMOND);
        c->set_type(7, 6, 7, BLOCK_ID_DIAMOND);
        c->set_type(7, 7, 6, BLOCK_ID_DIAMOND);

        c->set_type(7, 6, 6, BLOCK_ID_AIR);
        c->set_type(6, 7, 6, BLOCK_ID_AIR);
        c->set_type(6, 6, 7, BLOCK_ID_AIR);

        level->add_chunk(c);

        c = new chunk_cubic_t();
        pos.x--;
        c->pos = pos;
        c->set_type(7, 6, 7, BLOCK_ID_TNT);
        level->add_chunk(c);

        c = new chunk_cubic_t();
        pos.y--;
        c->pos = pos;
        c->set_type(7, 6, 7, BLOCK_ID_TNT);
        level->add_chunk(c);

        c = new chunk_cubic_t();
        pos.z++;
        c->pos = pos;
        level->add_chunk(c);

        for (int x = 1; x < 15; x++)
            for (int z = 1; z < 15; z++)
                for (int y = 1; y < 15; y++)
                    c->set_type(x, y, z, BLOCK_ID_STONE);
    }

    for (int i = 0; i < 128; i++)
    {
        chunk_cubic_t* c = new chunk_cubic_t();
        c->pos.x = (i / 12);
        c->pos.y = -2;
        c->pos.z = (i % 12);
        level->add_chunk(c);
        for (int x = 0; x < 16; x++)
            for (int z = 0; z < 16; z++)
            {
                c->set_type(x, 5, z, i);
                c->set_light_sky(x, 5, z, x);
                c->set_light_block(x, 5, z, z);
            }
        if (c->pos.x == 2 && c->pos.z == 1)
            c->set_type(7, 6, 7, BLOCK_ID_TORCH);
    }

    level->inventory.items[level->inventory.armor_min + 0] = { ITEM_ID_DIAMOND_CAP };
    level->inventory.items[level->inventory.armor_min + 1] = { ITEM_ID_CHAIN_TUNIC };
    level->inventory.items[level->inventory.armor_min + 2] = { ITEM_ID_IRON_PANTS };
    level->inventory.items[level->inventory.armor_min + 3] = { ITEM_ID_GOLD_BOOTS };

    level->inventory.items[level->inventory.hotbar_min + 0] = { BLOCK_ID_DIAMOND };
    level->inventory.items[level->inventory.hotbar_min + 1] = { BLOCK_ID_TORCH };
    level->inventory.items[level->inventory.hotbar_min + 2] = { BLOCK_ID_GLOWSTONE };
}

void game_t::create_light_test_decorated_simplex(const glm::ivec3 world_size)
{
    level->clear();
    const int world_volume = world_size.x * world_size.y * world_size.z;

    std::vector<chunk_cubic_t*> chunks;
    chunks.resize(world_volume);
    std::atomic<size_t> off = 0;
    std::atomic<Uint64> elapsed_ns = 0;

    util::parallel_for(0, world_size.x * world_size.z, [&](const int _start, const int _end) {
        Uint64 start_tick = SDL_GetTicksNS();
        chunk_t c_old;
        for (int it = _start; it < _end; it++)
        {
            /* Nothing special about this seed */
            Uint64 r_state_chunk = 0x2e17d7f27f825d7f + (it << 10);

            int cx = it % world_size.x;
            int cz = it / world_size.x;

            /* Nothing special about this seed */
            /* Coordinates fed to the generator are offset to coincide with the dev chunks */
            c_old.generate_from_seed_over(0xc4891e8c5ee07c5d, cx - world_size.x / 2, cz - world_size.z / 2);
            for (int cy = 0; cy < world_size.y; cy++)
            {
                chunk_cubic_t* c = new chunk_cubic_t();
                c->pos = glm::ivec3 { cx, cy, cz };
                for (int x = 0; x < 16; x++)
                    for (int z = 0; z < 16; z++)
                        for (int y = 0; y < 16; y++)
                        {
                            c->set_type(x, y, z, c_old.get_type(x, y + (cy % 8) * 16, z));
                            c->set_metadata(x, y, z, c_old.get_metadata(x, y + (cy % 8) * 16, z));
                            c->set_light_block(x, y, z, c_old.get_light_block(x, y + (cy % 8) * 16, z));
                            c->set_light_sky(x, y, z, c_old.get_light_sky(x, y + (cy % 8) * 16, z));
                        }

                for (int decoration_it = 0; decoration_it < 20; decoration_it++)
                {
                    Uint32 rand_data = SDL_rand_bits_r(&r_state_chunk);
                    const int y = (rand_data) & 0x0F;
                    const int z = (rand_data >> 4) & 0x0F;
                    const int x = (rand_data >> 8) & 0x0F;
                    rand_data = SDL_rand_bits_r(&r_state_chunk);
                    c->set_type(x, y, z, rand_data % BLOCK_ID_NUM_USED);
                }

                chunks[off++] = c;
            }
        }
        elapsed_ns += SDL_GetTicksNS() - start_tick;
    });

    assert(size_t(off) == chunks.size());

    for (chunk_cubic_t* c : chunks)
        level->add_chunk(c);

    double elapsed_ms = double(elapsed_ns) / 1000.0 / 1000.0;
    dc_log("Construction time: %.2f ms (%.3f ms per)", elapsed_ms, elapsed_ms / double(world_volume));
}

void game_t::create_light_test_sdl_rand(const glm::ivec3 world_size, Uint64* r_state)
{
    level->clear();
    const int world_volume = world_size.x * world_size.y * world_size.z;

    /* Nothing special about this seed */
    Uint64 r_state_if_null = 0x8c5ee07d7f257c5d;
    if (!r_state)
        r_state = &r_state_if_null;

    Uint64 tstart = SDL_GetTicksNS();
    for (int cx = 0; cx < world_size.x; cx++)
        for (int cz = 0; cz < world_size.z; cz++)
            for (int cy = 0; cy < world_size.y; cy++)
            {
                chunk_cubic_t* c = new chunk_cubic_t();
                c->pos = glm::ivec3 { cx, cy, cz };
                for (int pos_it = 0; pos_it < SUBCHUNK_SIZE_VOLUME; pos_it++)
                {
                    const int y = (pos_it) & 0x0F;
                    const int z = (pos_it >> 4) & 0x0F;
                    const int x = (pos_it >> 8) & 0x0F;
                    Uint32 rand_data = SDL_rand_bits_r(r_state);
                    if (rand_data % (rand_data % 15 + 1) < 5)
                        c->set_type(x, y, z, rand_data % BLOCK_ID_NUM_USED);
                }
                level->add_chunk(c);
            }
    double elapsed_ms = double(SDL_GetTicksNS() - tstart) / 1000.0 / 1000.0;
    dc_log("Construction time: %.2f ms (%.3f ms per)", elapsed_ms, elapsed_ms / double(world_volume));
}

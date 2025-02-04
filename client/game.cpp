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

game_t::game_t(const std::string addr, const Uint16 port, const std::string username, const bool test_level, const game_resources_t* const _resources)
    : game_id(SDL_AddAtomicInt(&game_counter, 1))
{
    level = new level_t(_resources->terrain_atlas);
    level->lightmap.set_world_time(1000);

    if (test_level)
        create_testworld();
    else
    {
        connection = new connection_t();
        connection->init(addr, port, username);
    }

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
    const int world_size = cvr_world_size.get();
    for (int i = 0; i < world_size * world_size; i++)
    {
        chunk_t c_old;
        if (world_size < 4)
            c_old.generate_from_seed_over(1, i / world_size - world_size / 2, i % world_size - world_size / 2);
        else
            c_old.generate_from_seed_over(1, i / world_size - world_size + 4, i % world_size - world_size + 4);
        for (int j = 0; j < 8; j++)
        {
            chunk_cubic_t* c = new chunk_cubic_t();
            c->pos.x = i / world_size - world_size;
            c->pos.z = i % world_size - world_size;
            c->pos.y = j - cvr_world_y_off_neg.get() + cvr_world_y_off_pos.get();
            level->add_chunk(c);
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

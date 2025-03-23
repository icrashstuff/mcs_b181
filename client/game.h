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
#ifndef MCS_B181_CLIENT_GAME_H
#define MCS_B181_CLIENT_GAME_H

#include <SDL3/SDL_stdinc.h>
#include <glm/glm.hpp>
#include <string>

/* Forward declaration */
struct level_t;
struct connection_t;
struct shader_t;
struct texture_terrain_t;
struct sound_resources_t;

struct game_resources_t
{
    shader_t* terrain_shader = nullptr;
    texture_terrain_t* terrain_atlas = nullptr;

    sound_resources_t* sound_resources = nullptr;

    int ao_algorithm = 1;
    int use_texture = 1;

    game_resources_t();

    void reload();

    void destroy();

    ~game_resources_t();
};

struct game_t
{
    level_t* level = nullptr;
    connection_t* connection = nullptr;
    const game_resources_t* resources = nullptr;

    /**
     * Unique object identifier
     */
    const int game_id = 0;

    /**
     * Creates a game
     *
     * @param addr Address to connect to
     * @param port Port to connect to
     * @param username Username to use
     * @param resources Resources to use for game
     */
    game_t(const std::string addr, const Uint16 port, const std::string username, const game_resources_t* const resources);

    /**
     * Creates a internal game (No connection)
     *
     * @param resources Resources to use for game
     */
    game_t(const game_resources_t* const resources);

    /**
     * Forces the game to reload its resources
     *
     * NOTE: This does not call game_resources_t::reload()
     *
     * @param resources New resources struct to pull from (NULL to reuse existing one)
     */
    void reload_resources(const game_resources_t* const resources = nullptr, const bool force_null = false);

    ~game_t();

    /** Replace world with test world */
    void create_testworld();

    /**
     * Replace world with light test simplex world
     *
     * @param size World size
     */
    void create_light_test_decorated_simplex(const glm::ivec3 size);

    /**
     * Replace world with light test SDL_rand world
     *
     * @param size World size
     * @param r_state State for SDL_rand_bits(), if null then a fixed state is used
     */
    void create_light_test_sdl_rand(const glm::ivec3 size, Uint64* r_state = NULL);
};

#endif

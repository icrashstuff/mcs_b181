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

#include "connection.h"
#include "level.h"

struct game_resources_t
{
    shader_t* terrain_shader = nullptr;
    texture_terrain_t* terrain_atlas = nullptr;

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
     * @param test_level Connect to a test level instead of connecting to a server
     * @param resources Resources to use for game
     */
    game_t(const std::string addr, const Uint16 port, const std::string username, const bool test_level, const game_resources_t* const _resources);

    /**
     * Forces a reload of all resources
     *
     * @param resources New resources struct to pull from (NULL to reuse existing one)
     * @param force_null Allows setting resources to a null object
     */
    void reload_resources(const game_resources_t* const _resources = nullptr, const bool force_null = false);

    void create_testworld();

    ~game_t();
};

#endif

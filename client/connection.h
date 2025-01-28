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
#ifndef MCS_B181_CLIENT_CONNECTION_H
#define MCS_B181_CLIENT_CONNECTION_H

#include "level.h"

#include "sdl_net/include/SDL3_net/SDL_net.h"
#include "shared/packet.h"

#include <SDL3/SDL.h>
#include <string.h>

/**
 * Connection class
 *
 * The way this fits into the architecture is that the connection is fed a level_t which it will then modify
 */
struct connection_t
{
    enum connection_status_t
    {
        CONNECTION_UNINITIALIZED,
        CONNECTION_ADDR_RESOLVING,
        CONNECTION_ADDR_RESOLVED,
        CONNECTION_CONNECTING,
        CONNECTION_ACTIVE,
        CONNECTION_DONE,
        CONNECTION_FAILED,
    };

    inline connection_status_t get_status() { return status; }

    packet_handler_t pack_handler_client = packet_handler_t(false);
    std::string status_msg;
    std::string status_msg_sub;

    /**
     * Initialize the connection
     *
     * @param address Address to connect to
     * @param port Port to connect to
     * @param username Username to use for the connection
     * @returns True on success, False on failure
     */
    bool init(const std::string address, const Uint16 port, const std::string username);

    std::string err_str;

    Uint64 start_time = 0;

    /**
     * Runs the connection
     *
     * @param level Level to modify
     */
    void run(level_t* const level);

    ~connection_t();

private:
    void set_status_msg(const std::string _status, const std::string _sub_status = "");

    connection_status_t status = CONNECTION_UNINITIALIZED;

    bool in_world = false;

    /**
     * Steps (if possible) the state as fast a possible to CONNECTION_ACTIVE
     */
    void step_to_active();

public:
    /* This field should probably be private */
    SDLNet_StreamSocket* socket = NULL;

private:
    SDLNet_Address* addr_server = NULL;
    Uint16 port = 0;
    std::string addr;
    std::string username;

    /* I am unsure if these fields should be private */
public:
    /**
     * Used for retaining old info about blocks that exist/don't exist on the client,
     * that the server hasn't made clear it's position on
     */
    struct tentative_block_t
    {
        Uint64 timestamp = 0;
        glm::ivec3 pos = { -1, -1, -1 };
        itemstack_t old;
        bool fullfilled = 0;
    };

    /**
     * Stores blocks that the client placed/destroyed that the server will hopefully honor,
     */
    std::vector<tentative_block_t> tentative_blocks;
};

#endif

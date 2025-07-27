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

#include "shared/packet.h"
#include "shared/sdl_net/include/SDL3_net/SDL_net.h"

#include <SDL3/SDL.h>
#include <string.h>
#include <vector>

/**
 * Connection class
 *
 * The way this fits into the architecture is that the connection is fed a level_t which it will then modify
 *
 * Thread-Safety
 * It is not safe to access an instance of connection_t from multiple threads at once
 * It is probably safe to access different instances of connection_t from multiple threads at once
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

    /**
     * Used for retaining old info about blocks that exist/don't exist on the client,
     * that the server hasn't made clear it's position on
     */
    struct tentative_block_t
    {
        Uint64 timestamp = 0;
        glm::ivec3 pos = { -1, -1, -1 };
        itemstack_t old;
        bool fulfilled = 0;
    };

    inline connection_status_t get_status() const { return status; }

    /**
     * in_world means that the world has been loaded enough for interaction to commence
     *
     * @returns in_world
     */
    inline bool get_in_world() const { return in_world; }

    inline const std::string& get_username() const { return username; }

    inline jint get_max_players() const { return max_players; }

    class player_list_data_t
    {
        jshort buf[32] = { 0 };
        int filled = 0;
        int pos = 0;

    public:
        void push(const jshort x)
        {
            buf[++pos %= SDL_arraysize(buf)] = x;
            filled = SDL_min(int(SDL_arraysize(buf)), filled + 1);
        }

        inline jshort average() const
        {
            int total = 0, i = 0;
            for (; i < filled; i++)
                total += buf[i];
            return total / SDL_max(1, i);
        }
    };

    inline const std::vector<std::pair<std::string, player_list_data_t>>& get_player_list() const { return player_list; }

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
    bool init(const std::string& address, const Uint16 port, const std::string& username);

    /**
     * Runs the connection
     *
     * @param level Level to modify
     */
    void run(level_t* const level);

    /**
     * Wrapper around send_buffer()
     * NOTE: Packets are only sent if status == connection_status_t::CONNECTION_ACTIVE
     */
    bool send_packet(packet_t& pack);

    /**
     * Wrapper around send_buffer()
     * NOTE: Packets are only sent if status == connection_status_t::CONNECTION_ACTIVE
     */
    bool send_packet(packet_t* pack);

    /**
     * Pushes data into the tentative block buffer
     */
    void push_tentative_block(tentative_block_t& tblock) { tentative_blocks.push_back(tblock); };

    ~connection_t();

    /** For F3 debug screen */
    inline size_t get_size_ent_id_map() { return ent_id_map.size(); };

    /**
     * Cull dead sockets from prior instances of connection_t
     *
     * Recommended use:
     * - Call every frame with a timeout value of 0
     * - Call at application exit with a non-zero timeout value
     *
     * Thread-safety:
     * This function is safe to call from any thread
     *
     * @param timeout Number of milliseconds to wait for the sockets to die
     */
    static void cull_dead_sockets(Uint32 timeout);

    enum loading_button_t
    {
        LOADING_BUTTON_NONE,
        LOADING_BUTTON_CANCEL,
        LOADING_BUTTON_BACK_TO_MENU,
    };
    loading_button_t loading_button = LOADING_BUTTON_CANCEL;

private:
    void set_status_msg(const std::string& translation_id, const std::string& _sub_status = "");

    connection_status_t status = CONNECTION_UNINITIALIZED;

    bool in_world = false;

    jint max_players = 0;

    std::vector<std::pair<std::string, player_list_data_t>> player_list;

    /**
     * Map server ids to EnTT ids
     *
     * All tools have their purposes, not all problems are nails to be driven by a hammer
     * https://github.com/skypjack/entt/issues/337#issuecomment-543333372
     */
    std::map<eid_t, level_t::ent_id_t> ent_id_map;

    eid_t player_eid_server = -1;

    bool get_ent_id_from_server_id(const eid_t, level_t::ent_id_t& result);

    level_t::ent_id_t create_or_replace_ent_from_server_id(decltype(level_t::ecs)& reg, const eid_t);

    /**
     * Steps (if possible) the state as fast a possible to CONNECTION_ACTIVE
     */
    void step_to_active();

    /**
     * Handles the socket dying
     */
    void handle_inactive();

    SDLNet_StreamSocket* socket = NULL;

    SDLNet_Address* addr_server = NULL;
    Uint16 port = 0;
    std::string addr;
    std::string username;

    bool sent_init = false;
    Uint64 last_update_tick_build = 0;
    Uint64 last_update_tick_camera = 0;

    std::vector<Uint8> chunk_decompression_buffer;

    /**
     * Stores blocks that the client placed/destroyed that the server will hopefully honor,
     */
    std::vector<tentative_block_t> tentative_blocks;
};

#endif

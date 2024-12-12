/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
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

#if 0
#define ENABLE_TRACE
#endif

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "ids.h"
#include "java_strings.h"
#include "misc.h"
#include "packet.h"

#include "tetra/tetra.h"
#include "tetra/util/convar.h"

#include "chunk.h"

long server_seed = 0;

#define WEATHER_OFF 0
#define WEATHER_RAIN 1
#define WEATHER_THUNDER 2
#define WEATHER_THUNDER_SUPER 3

int server_weather = 0;

long server_time = 0;

int next_thunder_bolt = 0;

void update_server_time()
{
    static Uint64 last_time_update = 0;
    Uint64 sdl_tick_cur = SDL_GetTicks();

    while (last_time_update < sdl_tick_cur && sdl_tick_cur - last_time_update >= 50)
    {
        server_time++;
        last_time_update += 50;
    }
}

struct dying_socket_t
{
    SDLNet_StreamSocket* sock;
    Uint64 start_tick;
};

std::vector<dying_socket_t> dying_sockets;

void cull_dying_sockets(bool force_all = false)
{
    Uint64 cur_tick = SDL_GetTicks();
    for (auto it = dying_sockets.begin(); it != dying_sockets.end();)
    {
        int remaining = SDLNet_GetStreamSocketPendingWrites((*it).sock);
        if (remaining < 1 || cur_tick - (*it).start_tick > 60000 || force_all)
        {
            SDLNet_DestroyStreamSocket((*it).sock);
            it = dying_sockets.erase(it);
        }
        else
            it = next(it);
    }
}

void kick(SDLNet_StreamSocket* sock, std::string reason, bool log = true)
{
    packet_kick_t packet;
    packet.reason = reason;
    send_buffer(sock, packet.assemble());

    SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(sock);
    if (log)
        LOG("Kicked: %s:%u, \"%s\"", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(sock), reason.c_str());
    SDLNet_UnrefAddress(client_addr);

    dying_sockets.push_back({ sock, SDL_GetTicks() });
    cull_dying_sockets();
}

bool send_prechunk(SDLNet_StreamSocket* sock, int chunk_x, int chunk_z, bool mode)
{
    packet_chunk_cache_t packet;
    packet.chunk_x = chunk_x;
    packet.chunk_z = chunk_z;
    packet.mode = mode;
    if (!mode)
        send_buffer(sock, packet.assemble());
    return send_buffer(sock, packet.assemble());
}

class region_t;

struct chunk_thread_data_t
{
    region_t* r;
    long seed;
    int dimension;
    int cx;
    int cx_off;
    int cz_off;
};

int generate_chunk_thread_func(void* data);

class region_t
{
public:
    region_t() { }

    void generate_from_seed(long seed, int dimension, int region_x, int region_z)
    {
        Uint64 start_tick = SDL_GetTicks();
        SDL_Thread* threads[REGION_SIZE_X];
        chunk_thread_data_t d[REGION_SIZE_X];
        for (int cx = 0; cx < REGION_SIZE_X; cx++)
        {
            d[cx].r = this;
            d[cx].seed = seed;
            d[cx].dimension = dimension;
            d[cx].cx = cx;
            d[cx].cx_off = cx + region_x * REGION_SIZE_X;
            d[cx].cz_off = region_z * REGION_SIZE_Z;
        }

        for (int cx = 1; cx < REGION_SIZE_X; cx++)
            threads[cx] = SDL_CreateThread(generate_chunk_thread_func, "Chunk Thread", &d[cx]);

        generate_chunk_thread_func(&d[0]);

        for (size_t i = 1; i < ARR_SIZE(threads); i++)
        {
            int b;
            SDL_WaitThread(threads[i], &b);
            assert(b == 1);
        }
        LOG("Generated region %d %d dim[%d] in %ld ms", dimension, region_x, region_z, SDL_GetTicks() - start_tick);
    }

    chunk_t* get_chunk(int cx, int cz)
    {
        if (cx < 0 || cx > REGION_SIZE_X)
            return NULL;
        if (cz < 0 || cz > REGION_SIZE_Z)
            return NULL;

        return &chunks[cx][cz];
    }

    /**
     * Returns an estimate on of memory footprint of a region_t object
     */
    size_t get_mem_size()
    {
        size_t t = sizeof(*this);

        for (int i = 0; i < REGION_SIZE_X; i++)
            for (int j = 0; j < REGION_SIZE_Z; j++)
                t += chunks[i][j].get_mem_size();

        return t;
    }

private:
    chunk_t chunks[REGION_SIZE_X][REGION_SIZE_Z];
};

int generate_chunk_thread_func(void* data)
{
    if (!data)
        return 0;

    chunk_thread_data_t* d = (chunk_thread_data_t*)data;

    for (int cz = 0; cz < REGION_SIZE_Z; cz++)
    {
        chunk_t* c = d->r->get_chunk(d->cx, cz);
        if (!c)
            LOG("Failed to get chunk: %d %d %d", d->cx_off, d->cz_off, cz);
        assert(c);
        if (d->dimension < 0)
            c->generate_from_seed_nether(d->seed, d->cx_off, d->cz_off + cz);
        else
            c->generate_from_seed_over(d->seed, d->cx_off, d->cz_off + cz);
    }
    return 1;
}

struct dim_reg_gen_data_t
{
    region_t** r;
    long seed;
    int generator;
    int x;
    int z;
};

int dim_reg_gen_func(void* data)
{
    if (!data)
        return 0;

    dim_reg_gen_data_t* d = (dim_reg_gen_data_t*)data;

    TRACE("Generating region %d %d", (*it).x, (*it).z);
    (*d->r) = new region_t();
    if ((*d->r))
        (*d->r)->generate_from_seed(d->seed, d->generator, d->x, d->z);

    return 1;
}

class dimension_t
{
public:
    dimension_t(int terrain_generator, long seed_dim, bool update_on_init = true)
    {
        generator = terrain_generator;
        seed = seed_dim;

        Uint64* rptr = (Uint64*)this;
        r_state = seed + generator + *(Uint32*)this + *(Uint64*)&rptr;
        ;

        /* Load spawn regions */
        regions.push_back({ 0, 0, NULL, 0 });
        regions.push_back({ -1, 0, NULL, 0 });
        regions.push_back({ 0, -1, NULL, 0 });
        regions.push_back({ -1, -1, NULL, 0 });

        needs_update = true;

        if (update_on_init)
            update();
    }

    /**
     * Move internal player position that is used for determining region loading
     *
     * Call update() to perform the region loading/unloading
     */
    void move_player(int eid, int world_x, int world_y, int world_z)
    {
        (void)world_y;
        needs_update = true;
        for (size_t i = 0; i < players.size(); i++)
        {
            if (players[i].eid == eid)
            {
                players[i].x = world_x;
                players[i].z = world_z;
                return;
            }
        }

        players.push_back({ .eid = eid, .x = world_x, .z = world_z });
    }

    /**
     * Attempts to find a suitable place to put a player in the spawn regions
     *
     * Returns true if a suitable location was found, false if a fallback location at world height was selected
     */
    inline bool find_spawn_point(double& x, double& y, double& z)
    {
        Uint32 pos = ((int)(x * 3) << 24) + ((int)(y * 3) << 12) + (int)(z * 3);
        pos += SDL_rand_bits_r(&r_state);

        int plim[2] = { 0, 0 };
        for (int l = REGION_SIZE_X / 4; l > 0; l >>= 1)
        {
            int lim[2] = { REGION_SIZE_X / l - 1, REGION_SIZE_Z / l - 1 };

            int cx_s = (pos >> 16) % lim[0];
            int cz_s = pos % lim[1];

            for (int ix = 0; ix < lim[0] * 2; ix++)
            {
                if (ix == plim[0])
                    ix = plim[0] * 2;
                for (int iz = 0; iz < lim[1] * 2; iz++)
                {
                    if (iz == plim[1])
                        iz = plim[1] * 2;
                    int cx = SDL_abs((ix + cx_s) % (lim[0] * 2)) - lim[0];
                    int cz = SDL_abs((iz + cz_s) % (lim[1] * 2)) - lim[1];
                    LOG("Checking chunk %d %d", cx, cz);
                    chunk_t* c = get_chunk(cx, cz);
                    if (c && c->find_spawn_point(x, y, z))
                    {
                        TRACE("%.1f %.1f %.1f", x, y, z);
                        x += cx * CHUNK_SIZE_X;
                        z += cz * CHUNK_SIZE_Z;
                        TRACE("%.1f %.1f %.1f", x, y, z);
                        return true;
                    }
                }
            }
            LOG("Expanding search\n");
            plim[0] = lim[0];
            plim[1] = lim[1];
        }

        x = 0.5;
        y = CHUNK_SIZE_Y + 1.8;
        z = 0.5;
        return false;
    }

    chunk_t* get_chunk(int chunk_x, int chunk_z)
    {
        int rx = chunk_x >> 5;
        int rz = chunk_z >> 5;
        for (size_t i = 0; i < regions.size(); i++)
        {
            if (regions[i].x == rx && regions[i].z == rz)
            {
                if (regions[i].region)
                {
                    int cx = chunk_x % REGION_SIZE_X;
                    int cz = chunk_z % REGION_SIZE_Z;
                    if (cx < 0)
                        cx += REGION_SIZE_X;
                    if (cz < 0)
                        cz += REGION_SIZE_Z;
                    return regions[i].region->get_chunk(cx, cz);
                }
                return NULL;
            }
        }

        return NULL;
    }

    /**
     * Returns an estimate on of memory footprint of a dimension_t object
     */
    size_t get_mem_size()
    {
        size_t t = sizeof(*this);

        for (size_t i = 0; i < regions.size(); i++)
            if (regions[i].region)
                t += regions[i].region->get_mem_size();

        t += regions.capacity() * sizeof(regions[0]);
        t += players.capacity() * sizeof(players[0]);

        return t;
    }

    /**
     * Loads/unloads regions
     *
     * Note: this is a somewhat expensive function, be careful
     */
    void update()
    {
        if (!needs_update)
            return;

        needs_update = 0;

        for (size_t i = 0; i < regions.size(); i++)
        {
            regions[i].num_players = 0;
            if (regions[i].x == 0 || regions[i].x == -1)
                if (regions[i].z == 0 || regions[i].z == -1)
                    regions[i].num_players++;
        }
        for (size_t i = 0; i < players.size(); i++)
        {
            int rx_nom = ((players[i].x >> 4) + 0) >> 5;
            int rx_min = ((players[i].x >> 4) - 15) >> 5;
            int rx_max = ((players[i].x >> 4) + 15) >> 5;
            int rz_nom = ((players[i].z >> 4) + 0) >> 5;
            int rz_min = ((players[i].z >> 4) - 15) >> 5;
            int rz_max = ((players[i].z >> 4) + 15) >> 5;

            int rx_bounds_[3] = { rx_min, rx_nom, rx_max };
            int rz_bounds_[3] = { rz_min, rz_nom, rz_max };

            TRACE("%d %d %d", rx_bounds_[0], rx_bounds_[1], rx_bounds_[2]);
            TRACE("%d %d %d", rz_bounds_[0], rz_bounds_[1], rz_bounds_[2]);

            std::vector<int> rx_bounds;
            std::vector<int> rz_bounds;

            for (int j = 0; j < 3; j++)
            {
                bool dup = false;
                for (size_t k = 0; k < rx_bounds.size(); k++)
                    if (rx_bounds[k] == rx_bounds_[j])
                        dup = true;
                if (!dup)
                    rx_bounds.push_back(rx_bounds_[j]);
            }

            for (int j = 0; j < 3; j++)
            {
                bool dup = false;
                for (size_t k = 0; k < rz_bounds.size(); k++)
                    if (rz_bounds[k] == rz_bounds_[j])
                        dup = true;
                if (!dup)
                    rz_bounds.push_back(rz_bounds_[j]);
            }

            int rx_bounds_len = rx_bounds.size();
            int rz_bounds_len = rz_bounds.size();

            /* This is inefficient */
            for (int j = 0; j < rx_bounds_len; j++)
            {
                for (int k = 0; k < rz_bounds_len; k++)
                {
                    bool region_found = false;
                    for (size_t l = 0; l < regions.size() && !region_found; l++)
                    {
                        if (regions[l].x == rx_bounds[j] && regions[l].z == rz_bounds[k])
                        {
                            TRACE("Preserve %d %d", regions[l].x, regions[l].z);
                            regions[l].num_players++;
                            region_found = true;
                        }
                    }
                    if (!region_found)
                    {
                        TRACE("Create %d %d", rx_bounds[j], rz_bounds[k]);
                        regions.push_back({ rx_bounds[j], rz_bounds[k], NULL, 1 });
                    }
                }
            }
        }

        for (auto it = regions.begin(); it != regions.end();)
        {
            if ((*it).num_players == 0)
            {
                TRACE("Unloading region %d %d", (*it).x, (*it).z);
                if ((*it).region)
                    delete (*it).region;
                it = regions.erase(it);
            }
            else
            {
                it = next(it);
            }
        }

        std::vector<SDL_Thread*> threads;
        std::vector<dim_reg_gen_data_t> gen_data;
        gen_data.resize(regions.size());

        for (size_t i = 0; i < regions.size(); i++)
        {
            if (regions[i].region)
                continue;
            dim_reg_gen_data_t* d = &gen_data.data()[i];
            d->r = &regions[i].region;
            d->seed = seed;
            d->generator = generator;
            d->x = regions[i].x;
            d->z = regions[i].z;
        }

        for (size_t i = 1; i < regions.size(); i++)
        {
            if (regions[i].region)
                continue;

            dim_reg_gen_data_t* d = &gen_data.data()[i];
            threads.push_back(SDL_CreateThread(dim_reg_gen_func, "Region gen thread", d));
        }

        if (regions.size() && !regions[0].region)
        {
            dim_reg_gen_data_t* d = &gen_data.data()[0];
            dim_reg_gen_func(d);
        }

        for (size_t i = 0; i < threads.size(); i++)
            SDL_WaitThread(threads[i], NULL);

        TRACE("Dimension Update done");
    }

    size_t get_num_loaded_regions()
    {
        size_t num = 0;
        for (size_t i = 0; i < regions.size(); i++)
            if (regions[i].region)
                num++;

        return num;
    }

private:
    struct dimension_reg_dat_t
    {
        int x;
        int z;
        region_t* region;
        Uint32 num_players;
    };
    struct dimension_player_dat_t
    {
        int eid;
        int x;
        int z;
    };
    int generator;
    long seed;
    bool needs_update;
    Uint64 r_state;
    std::vector<dimension_player_dat_t> players;
    std::vector<dimension_reg_dat_t> regions;
};

bool send_chunk(SDLNet_StreamSocket* sock, chunk_t* chunk, int chunk_x, int chunk_z)
{
    if (chunk == NULL)
        return 0;

    if (!chunk->ready)
        return 0;

    packet_chunk_t packet;
    packet.block_x = chunk_x * CHUNK_SIZE_X;
    packet.block_y = 0;
    packet.block_z = chunk_z * CHUNK_SIZE_Z;
    packet.size_x = CHUNK_SIZE_X - 1;
    packet.size_y = CHUNK_SIZE_Y - 1;
    packet.size_z = CHUNK_SIZE_Z - 1;

    chunk->compress_to_buf(packet.compressed_data);

    send_prechunk(sock, chunk_x, chunk_z, 1);

    return send_buffer(sock, packet.assemble());
}

bool send_chunk(SDLNet_StreamSocket* sock, int chunk_x, int chunk_z, int max_y)
{
    chunk_t c;

    if (max_y >= BLOCK_ID_MAX)
        c.generate_special_metadata();
    else
        c.generate_special_ascending_type(max_y);

    return send_chunk(sock, &c, chunk_x, chunk_z);
}

struct chunk_coords_t
{
    int x = 0;
    int z = 0;
};

struct client_t
{
    SDLNet_StreamSocket* sock = NULL;
    packet_handler_t packet;

    Uint64 time_keep_alive_sent = 0;
    Uint64 time_keep_alive_recv = 0;

    std::string username;
    int eid = 0;
    int counter = 0;

    double old_x = 0.0;
    double old_z = 0.0;

    double player_x = 0.0;
    double player_y = 0.0;
    double player_stance = 0.0;
    double y_last_ground = 0.0;
    double player_z = 0.0;
    float player_yaw = 0.0;
    float player_pitch = 0.0;
    Uint8 player_on_ground = 0;
    int dimension = 0;
    int old_dimension = 0;
    int player_mode = 1;

    short last_health = -20;
    short health = 20;
    short food = 20;
    float food_satur = 5.0;

    bool pos_updated = 1;

    Uint32 time_invulnerable_fall_damage = 0;
    Uint64 time_last_health_update = 0;

    Uint64 time_last_food_update = 0;
    Uint64 pos_update_time;

    int update_health = 1;

    bool is_raining = 0;

    /**
     * Inventory layout
     *
     * +---+-------+ +-----+    +---+
     * | 5 |   o   | | 1 2 | -> | 0 |
     * | 6 |  ---  | | 3 4 | -> | 0 |
     * | 7 |   |   | +-----+    +---+
     * | 8 |  / \  +----------------+
     * +---+-------+ 45 (1.9+ only) |
     * +-----------+----------------+
     * |  9 10 11 12 13 14 15 16 17 |
     * | 18 19 20 21 22 23 24 25 26 |
     * | 27 28 29 30 31 32 33 34 35 |
     * +----------------------------+
     * | 36 37 38 39 40 41 42 43 44 |
     * +----------------------------+
     */
    inventory_item_t inventory[45];

    int cur_item_idx = 36;

    std::vector<chunk_coords_t> loaded_chunks;
};

bool add_to_inventory(item_id_t id, short damage, int quantity, inventory_item_t inventory[45])
{
    int max_qty = mc_id::get_max_quantity_for_id(id);
    for (int i_ = 0; i_ < 36; i_++)
    {
        int i = (i_ + 27) % 36 + 9;
        if (inventory[i].id == id && inventory[i].damage == damage && inventory[i].quantity <= max_qty)
        {
            int diff = max_qty - inventory[i].quantity;
            if (quantity < diff)
                diff = quantity;
            inventory[i].quantity += diff;
            quantity -= diff;
        }
        if (quantity == 0)
            return 1;
    }

    for (int i_ = 0; i_ < 36; i_++)
    {
        int i = (i_ + 27) % 36 + 9;
        if (inventory[i].id == -1)
        {
            int diff = max_qty - inventory[i].quantity;
            if (quantity < diff)
                diff = quantity;
            inventory[i].id = id;
            inventory[i].damage = damage;
            inventory[i].quantity += diff;
            quantity -= diff;
        }
        if (quantity == 0)
            return 1;
    }
    return quantity == 0;
}

void send_inventory(client_t* client)
{
    packet_window_items_t pack_inv;
    pack_inv.window_id = 0;
    pack_inv.payload_from_array(client->inventory, ARR_SIZE(client->inventory));

    send_buffer(client->sock, pack_inv.assemble());
}

void survival_mode_decrease_hand(client_t* client)
{
    if (client->player_mode == 0)
    {
        if (client->inventory[client->cur_item_idx].quantity > 0)
            client->inventory[client->cur_item_idx].quantity--;
        if (client->inventory[client->cur_item_idx].quantity == 0)
            client->inventory[client->cur_item_idx] = {};
    }

    packet_window_set_slot_t pack_set_slot;
    pack_set_slot.slot = client->cur_item_idx;
    pack_set_slot.item = client->inventory[client->cur_item_idx];
    send_buffer(client->sock, pack_set_slot.assemble());
}

bool is_chunk_loaded(std::vector<chunk_coords_t> loaded_chunks, int chunk_x, int chunk_z)
{
    for (size_t i = 0; i < loaded_chunks.size(); i++)
        if (loaded_chunks[i].x == chunk_x && loaded_chunks[i].z == chunk_z)
            return true;
    return false;
}

void chunk_remove_loaded(client_t* client)
{
    int chunk_x_min = ((int)client->player_x >> 4) - CHUNK_UNLOAD_DISTANCE;
    int chunk_x_max = ((int)client->player_x >> 4) + CHUNK_UNLOAD_DISTANCE;
    int chunk_z_min = ((int)client->player_z >> 4) - CHUNK_UNLOAD_DISTANCE;
    int chunk_z_max = ((int)client->player_z >> 4) + CHUNK_UNLOAD_DISTANCE;

    for (auto it = client->loaded_chunks.begin(); it != client->loaded_chunks.end();)
    {
        if ((*it).x < chunk_x_min || (*it).x > chunk_x_max || (*it).z < chunk_z_min || (*it).z > chunk_z_max)
        {
            send_prechunk(client->sock, (*it).x, (*it).z, 0);
            it = client->loaded_chunks.erase(it);
        }
        else
        {
            it = next(it);
        }
    }
}

void send_buffer_to_players(std::vector<client_t> clients, std::vector<Uint8> buf, client_t* exclude = NULL)
{
    for (size_t i = 0; i < clients.size(); i++)
        if (clients[i].sock && clients[i].username.length() > 0 && (exclude == NULL || exclude->sock != clients[i].sock))
            send_buffer(clients[i].sock, buf);
}

void send_buffer_to_players_if_dim(std::vector<client_t> clients, std::vector<Uint8> buf, int dimension, client_t* exclude = NULL)
{
    for (size_t i = 0; i < clients.size(); i++)
        if (clients[i].sock && clients[i].username.length() > 0 && (exclude == NULL || exclude->sock != clients[i].sock))
            if (clients[i].dimension == dimension)
                send_buffer(clients[i].sock, buf);
}

void send_buffer_to_players_if_coords(std::vector<client_t> clients, std::vector<Uint8> buf, int world_x, int world_z, int dimension, client_t* exclude = NULL)
{
    for (size_t i = 0; i < clients.size(); i++)
        if (clients[i].sock && clients[i].username.length() > 0 && (exclude == NULL || exclude->sock != clients[i].sock))
            if (is_chunk_loaded(clients[i].loaded_chunks, world_x >> 4, world_z >> 4) && clients[i].dimension == dimension)
                send_buffer(clients[i].sock, buf);
}

/**
 * Send chunks in a square roughly starting from the center in each axis
 */
void send_square_chunks(client_t* client, dimension_t* dimensions, int radius)
{
    int pos_cx = ((int)client->player_x) >> 4;
    int pos_cz = ((int)client->player_z) >> 4;

    for (int x_ = 1; x_ < radius * 2; x_++)
    {
        int x = (x_ / 2) * (x_ % 2 == 0 ? 1 : -1);
        for (int z_ = 1; z_ < radius * 2; z_++)
        {
            int z = (z_ / 2) * (z_ % 2 == 0 ? 1 : -1);
            chunk_coords_t coords;
            coords.x = x + pos_cx;
            coords.z = z + pos_cz;
            if (is_chunk_loaded(client->loaded_chunks, coords.x, coords.z))
                continue;
            chunk_t* c = dimensions[client->dimension < 0].get_chunk(coords.x, coords.z);
            if (!c)
                continue;
            c->correct_lighting(client->dimension);
            if (send_chunk(client->sock, c, coords.x, coords.z))
                client->loaded_chunks.push_back(coords);
        }
    }
}

/**
 * Send chunks in progressively larger vaguely circular rings
 *
 * The idea for this came from a server I saw once
 * The implementation however is my own (I imagine the other server has a higher quality implementation)
 */
void send_circle_chunks(client_t* client, dimension_t* dimensions, int radius)
{
    int off_cx = ((int)client->player_x) >> 4;
    int off_cz = ((int)client->player_z) >> 4;

    chunk_coords_t prev_coords;
    prev_coords.x = radius * 2 + off_cx;
    prev_coords.z = radius * 2 + off_cz;
    for (int i = 0; i < radius; i++)
    {
        for (float deg = 0; deg < 360; deg += 2)
        {
            float theta = (deg + client->player_yaw) * SDL_PI_F / 180.0f;
            chunk_coords_t coords;
            coords.x = ((int)(SDL_cosf(theta) * i)) + off_cx;
            coords.z = ((int)(SDL_sinf(theta) * i)) + off_cz;
            if (coords.x == prev_coords.x && coords.z == prev_coords.z)
                continue;
            if (is_chunk_loaded(client->loaded_chunks, coords.x, coords.z))
                continue;
            chunk_t* c = dimensions[client->dimension < 0].get_chunk(coords.x, coords.z);
            if (!c)
                continue;
            c->correct_lighting(client->dimension);
            if (send_chunk(client->sock, c, coords.x, coords.z))
                client->loaded_chunks.push_back(coords);
        }
    }
}

void spawn_player(std::vector<client_t> clients, client_t* client, dimension_t* dimensions)
{
    LOG("Spawning \"%s\" with eid: %d in dimension: %d", client->username.c_str(), client->eid, client->dimension);

    SDLNet_StreamSocket* sock = client->sock;

    dimensions[client->dimension < 0].find_spawn_point(client->player_x, client->player_y, client->player_z);
    client->y_last_ground = client->player_y;
    client->player_stance = client->player_y + 0.2;
    client->player_on_ground = 1;
    client->pos_updated = 1;
    client->time_invulnerable_fall_damage = 20;

    packet_ent_create_t pack_player_ent;
    packet_ent_spawn_named_t pack_player;

    pack_player_ent.eid = client->eid;
    pack_player.eid = client->eid;
    pack_player.name = client->username;
    pack_player.x = client->player_x * 32;
    pack_player.y = client->player_y * 32;
    pack_player.z = client->player_z * 32;
    pack_player.rotation = ((int)client->player_yaw) * 255 / 360;
    pack_player.pitch = client->player_pitch * 64 / 90;
    pack_player.cur_item = client->inventory[client->cur_item_idx].id;

    for (size_t i = 0; i < clients.size(); i++)
    {
        if (clients[i].sock && clients[i].username.length() > 0 && clients[i].sock != sock)
        {
            packet_ent_create_t pack_ext_player_ent;
            packet_ent_spawn_named_t pack_ext_player;

            pack_ext_player_ent.eid = clients[i].eid;
            pack_ext_player.eid = clients[i].eid;
            pack_ext_player.name = clients[i].username;
            if (client[i].dimension == client->dimension)
            {
                pack_ext_player.x = 0;
                pack_ext_player.y = SDL_MIN_SINT32 / 2;
                pack_ext_player.z = 0;
            }
            else
            {
                pack_ext_player.x = clients[i].player_x * 32;
                pack_ext_player.y = clients[i].player_y * 32;
                pack_ext_player.z = clients[i].player_z * 32;
                pack_ext_player.cur_item = clients[i].inventory[clients[i].cur_item_idx].id;
            }
            pack_ext_player.rotation = ((int)clients[i].player_yaw) * 255 / 360;
            pack_ext_player.pitch = clients[i].player_pitch * 64 / 90;

            send_buffer(sock, pack_ext_player_ent.assemble());
            send_buffer(sock, pack_ext_player.assemble());

            send_buffer(clients[i].sock, pack_player_ent.assemble());
            send_buffer(clients[i].sock, pack_player.assemble());
        }
    }

    send_inventory(client);

    send_circle_chunks(client, dimensions, CHUNK_VIEW_DISTANCE / 2);

    packet_player_pos_look_s2c_t pack_player_pos;
    pack_player_pos.x = client->player_x;
    pack_player_pos.y = client->player_y;
    pack_player_pos.stance = client->player_stance;
    pack_player_pos.z = client->player_z;
    pack_player_pos.yaw = client->player_yaw;
    pack_player_pos.pitch = client->player_pitch;
    pack_player_pos.on_ground = client->player_on_ground;

    send_square_chunks(client, dimensions, CHUNK_VIEW_DISTANCE);

    send_buffer(sock, pack_player_pos.assemble());
}

int eid_counter = 0;

int dim_gen_thread_func(void* data)
{
    if (!data)
        return 0;

    dimension_t* d = (dimension_t*)data;

    d->update();

    return 1;
}

enum command_return_t
{
    COMMAND_UNSET,
    COMMAND_OK,
    COMMAND_FAIL,
    COMMAND_FAIL_PARSE,
    COMMAND_FAIL_INTERNAL,
    COMMAND_FAIL_UNHANDLED,
};

typedef command_return_t (*command_callback_t)(const char*, std::vector<client_t>&, client_t*, dimension_t*);
#define MC_COMMAND(NAME) command_return_t command_##NAME(const char* cmdline, std::vector<client_t>& clients, client_t* client, dimension_t* dimensions)
#define MC_COMMAND_REG(NAME, CMD, PARAMS, HELP) mc_commands.push_back({ NAME, PARAMS, HELP, command_##CMD })
#define MC_COMMAND_REGB(NAME, PARAMS, HELP) MC_COMMAND_REG(#NAME, NAME, PARAMS, HELP)
#define MC_COMMAND_UNUSED() \
    do                      \
    {                       \
        (void)cmdline;      \
        (void)clients;      \
        (void)client;       \
        (void)dimensions;   \
    } while (0)

struct mc_command_t
{
    const char* name;
    const char* params;
    const char* help;
    command_callback_t cmd;
};

MC_COMMAND(smite)
{
    MC_COMMAND_UNUSED();
    if (!client)
        return COMMAND_FAIL_INTERNAL;

    packet_thunder_t pack_thunder;
    pack_thunder.eid = eid_counter++;
    pack_thunder.unknown = true;
    pack_thunder.x = client->player_x * 32;
    pack_thunder.y = client->player_y * 32;
    pack_thunder.z = client->player_z * 32;
    send_buffer_to_players_if_coords(clients, pack_thunder.assemble(), client->player_x, client->player_z, client->dimension);

    return COMMAND_OK;
}

MC_COMMAND(strip_stone)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock || !dimensions)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    int x = (int)client->player_x >> 4;
    int z = (int)client->player_z >> 4;
    LOG("Stripping stone from chunk %d %d", x, z);

    chunk_t* c = dimensions[client->dimension < 0].get_chunk(x, z);
    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
                if (c->get_type(x, y, z) == BLOCK_ID_STONE)
                    c->set_type(x, y, z, BLOCK_ID_AIR);

    c->correct_lighting(client->dimension);

    send_chunk(sock, c, x, z);

    return COMMAND_OK;
}

MC_COMMAND(stats)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    send_chat(sock, "§6============ Server stats ============");

    const char* time_states[4] = { "Sunrise", "Noon", "Sunset", "Midnight" };
    long tod = server_time % 24000;
    if (tod < 0)
        tod += 24000;
    send_chat(sock, "Time: %ld (%ld) (%s) (Day: %ld)", server_time, tod, time_states[tod / 6000], server_time / 24000);
    send_chat(sock, "  eid_counter: %d", eid_counter);

    for (int i = 0; i < 2; i++)
    {
        send_chat(sock, "§3==== dimensions[%d] ====", i);
        std::string mem_str = format_memory(dimensions[i].get_mem_size());
        send_chat(sock, "  Est. Memory footprint: %s", mem_str.c_str());
        send_chat(sock, "  Num Loaded Regions: %zu", dimensions[i].get_num_loaded_regions());
    }

    return COMMAND_OK;
}

MC_COMMAND(kill)
{
    MC_COMMAND_UNUSED();
    if (!client)
        return COMMAND_FAIL_INTERNAL;

    client->health = -100;
    client->update_health = 2;

    return COMMAND_OK;
}

MC_COMMAND(craft)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    packet_window_open_t pack_craft;
    pack_craft.window_id = 1;
    pack_craft.num_slots = 10;
    pack_craft.title = "Crafting";
    pack_craft.type = 1;
    send_buffer(sock, pack_craft.assemble());

    packet_window_set_slot_t pack_craft_set_result;
    pack_craft_set_result.window_id = 1;
    pack_craft_set_result.item.quantity = 64;

    for (int i = 1; i < 10; i++)
    {
        pack_craft_set_result.slot = i;
        pack_craft_set_result.item.id = BLOCK_ID_GOLD;
        send_buffer(sock, pack_craft_set_result.assemble());
    }

    pack_craft_set_result.slot = 5;
    pack_craft_set_result.item.id = ITEM_ID_APPLE;
    send_buffer(sock, pack_craft_set_result.assemble());

    pack_craft_set_result.slot = 0;
    pack_craft_set_result.item.quantity = 1;
    pack_craft_set_result.item.id = ITEM_ID_APPLE_GOLDEN;
    send_buffer(sock, pack_craft_set_result.assemble());

    return COMMAND_OK;
}

MC_COMMAND(unload)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;

    int x = (int)client->player_x >> 4;
    int z = (int)client->player_z >> 4;
    LOG("Unloading chunk %d %d", x, z);

    /* The notchian client does not like chunks being unloaded near it */
    for (int i = 0; i < 16; i++)
        for (size_t j = 0; j < clients.size(); j++)
            if (clients[j].sock && clients[j].username.length() > 0)
                send_prechunk(clients[j].sock, x, z, 0);

    return COMMAND_OK;
}
MC_COMMAND(dimension)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    packet_respawn_t pack_dim_change;
    pack_dim_change.seed = server_seed;
    pack_dim_change.dimension = strncmp(cmdline, "overworld", 9) == 0 ? 0 : -1;
    pack_dim_change.mode = client->player_mode;
    pack_dim_change.world_height = WORLD_HEIGHT;
    if (client->dimension != pack_dim_change.dimension)
    {
        client->loaded_chunks.clear();
        dimensions[client->dimension < 0].move_player(client->eid, 0, 0, 0);
    }
    client->dimension = pack_dim_change.dimension;
    send_buffer(sock, pack_dim_change.assemble());

    packet_time_update_t pack_time;
    pack_time.time = server_time;
    send_buffer(sock, pack_time.assemble());

    client->pos_updated = true;

    spawn_player(clients, client, dimensions);

    return COMMAND_OK;
}

MC_COMMAND(gamemode)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    client->player_mode = strncmp(cmdline, "c", 1) == 0 ? 1 : 0;

    packet_new_state_t pack_mode;
    pack_mode.reason = PACK_NEW_STATE_REASON_CHANGE_MODE;
    pack_mode.mode = client->player_mode;

    send_buffer(sock, pack_mode.assemble());

    return COMMAND_OK;
}

MC_COMMAND(time)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    std::vector<std::string> argv;
    if (!argv_from_str(argv, cmdline))
        return COMMAND_FAIL_PARSE;

    if (argv.size() < 2)
    {
        const char* time_states[4] = { "Sunrise", "Noon", "Sunset", "Midnight" };
        long tod = server_time % 24000;
        if (tod < 0)
            tod += 24000;
        send_chat(sock, "Time: %ld (%ld) (%s) (Day: %ld)", server_time, tod, time_states[tod / 6000], server_time / 24000);
        return COMMAND_OK;
    }

    if (argv[1] == "resend")
    {
        send_chat(sock, "Time: Resending time to all players");

        packet_time_update_t pack_time;
        pack_time.time = server_time;
        send_buffer_to_players(clients, pack_time.assemble());
        return COMMAND_OK;
    }

    if (argv[1] != "set" && argv[1] != "add")
    {
        send_chat(sock, "Time: \"%s\" is not a valid operation!", argv[1].c_str());
        return COMMAND_FAIL;
    }

    if (argv.size() < 3)
    {
        send_chat(sock, "Time: A number must be provided!");
        return COMMAND_FAIL;
    }

    long parse_result;
    if (!long_from_str(argv[2], parse_result))
        return COMMAND_FAIL_PARSE;

    if (argv[1] == "set")
        server_time = parse_result;
    else
        server_time += parse_result;

    send_chat(sock, "Time: Setting time to %ld", server_time);

    packet_time_update_t pack_time;
    pack_time.time = server_time;
    send_buffer_to_players(clients, pack_time.assemble());
    return COMMAND_OK;
}

MC_COMMAND(weather)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    std::vector<std::string> argv;
    if (!argv_from_str(argv, cmdline))
        return COMMAND_FAIL_PARSE;

    if (argv.size() < 2)
    {
        send_chat(sock, "Weather: state: %d", server_weather);
        send_chat(sock, "Weather: is_raining: %s", BOOL_S(server_weather > WEATHER_OFF));
        send_chat(sock, "Weather: is_thunder: %s", BOOL_S(server_weather > WEATHER_RAIN));
        if (server_weather == WEATHER_THUNDER_SUPER)
            send_chat(sock, "Weather: §7is_super: %s", BOOL_S(server_weather == WEATHER_THUNDER_SUPER));
        return COMMAND_OK;
    }

    int parse_result;
    if (!int_from_str(argv[1], parse_result))
        return COMMAND_FAIL_PARSE;

    if (parse_result < 0 || parse_result > 3)
    {
        send_chat(sock, "§cWeather: %d is not a valid weather state!", parse_result);
        return COMMAND_FAIL;
    }

    send_chat(sock, "Weather: Setting weather to %d", parse_result);
    server_weather = parse_result;

    return COMMAND_OK;
}

MC_COMMAND(health)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    std::vector<std::string> argv;
    if (!argv_from_str(argv, cmdline))
        return COMMAND_FAIL_PARSE;

    if (argv.size() < 2)
    {
        send_chat(sock, "Health: %d", client->health);
        return COMMAND_OK;
    }

    int parse_result;
    if (!int_from_str(argv[1], parse_result))
        return COMMAND_FAIL_PARSE;

    parse_result = SDL_max(SDL_min(parse_result, 32760), -32760);

    client->health = parse_result;
    client->update_health = 1;

    send_chat(sock, "Health: Setting health to %d", client->health);

    return COMMAND_OK;
}

MC_COMMAND(ent_status)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    std::vector<std::string> argv;
    if (!argv_from_str(argv, cmdline))
        return COMMAND_FAIL_PARSE;

    if (argv.size() < 2)
    {
        send_chat(sock, "ent_status: Requires one positional argument: status");
        return COMMAND_FAIL;
    }

    int parse_result;
    if (!int_from_str(argv[1], parse_result))
        return COMMAND_FAIL_PARSE;

    packet_ent_status_t stat;
    stat.eid = client->eid;
    stat.status = parse_result;

    send_buffer_to_players_if_coords(clients, stat.assemble(), client->player_x, client->player_z, client->dimension);

    send_chat(sock, "ent_status: Setting player status to %d", parse_result);

    return COMMAND_OK;
}

MC_COMMAND(food)
{
    MC_COMMAND_UNUSED();
    if (!client || !client->sock)
        return COMMAND_FAIL_INTERNAL;
    SDLNet_StreamSocket* sock = client->sock;

    std::vector<std::string> argv;
    if (!argv_from_str(argv, cmdline))
        return COMMAND_FAIL_PARSE;

    if (argv.size() < 2)
    {
        send_chat(sock, "Food: Food: %d", client->food);
        send_chat(sock, "Food: Saturation: %.1f", client->food_satur);
        return COMMAND_OK;
    }

    int parse_result;
    if (!int_from_str(argv[1], parse_result))
        return COMMAND_FAIL_PARSE;

    parse_result = SDL_max(SDL_min(parse_result, 32760), -32760);

    client->food = SDL_min(parse_result, 20);
    client->food_satur = SDL_max(parse_result - 20, 0);
    client->update_health = 1;

    send_chat(sock, "Food: Setting food to %d", client->food);
    send_chat(sock, "Food: Setting saturation to %.1f", client->food_satur);

    return COMMAND_OK;
}

static convar_string_t address_listen("address_listen", "127.0.0.1", "Address to listen for connections");

int main(int argc, const char** argv)
{
    /* KDevelop fully buffers the output and will not display anything */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    tetra::init("icrashstuff", "mcs_b181", argc, argv);

    LOG("Hello");

    if (!SDL_Init(0))
    {
        LOG("SDL_Init: %s", SDL_GetError());
        exit(1);
    }

    if (!SDLNet_Init())
    {
        LOG("SDLNet_Init: %s", SDL_GetError());
        exit(1);
    }

    if (!((convar_int_t*)convar_t::get_convar("dev"))->get())
        server_seed = cast_to_sint64((Uint64)SDL_rand_bits() << 32 | (Uint64)SDL_rand_bits());

    if (!((convar_int_t*)convar_t::get_convar("dev"))->get())
        server_time = cast_to_sint64(SDL_rand_bits());

    next_thunder_bolt = SDL_rand_bits() & 0x7fff;

    LOG("World seed: %ld", server_seed);

    LOG("Generating regions");
    Uint64 tick_region_start = SDL_GetTicks();
    dimension_t dimensions[2] = { dimension_t(0, server_seed, false), dimension_t(-1, server_seed, false) };

    SDL_Thread* dim_gen_threads[ARR_SIZE(dimensions)];
    for (size_t i = 1; i < ARR_SIZE(dim_gen_threads); i++)
        dim_gen_threads[i] = SDL_CreateThread(dim_gen_thread_func, "Dim gen thread", &dimensions[i]);
    dim_gen_thread_func(&dimensions[0]);
    for (size_t i = 1; i < ARR_SIZE(dim_gen_threads); i++)
        SDL_WaitThread(dim_gen_threads[i], NULL);

    Uint64 tick_region_time = SDL_GetTicks() - tick_region_start;
    LOG("Regions generated in %lu ms", tick_region_time);

    std::vector<mc_command_t> mc_commands;

    MC_COMMAND_REGB(smite, "", "Smite the player");
    if (((convar_int_t*)convar_t::get_convar("dev"))->get())
    {
        MC_COMMAND_REGB(strip_stone, "", "Strip all stone from chunk (dev)");
        MC_COMMAND_REGB(unload, "", "Forcebly unload the chunk (dev)");
    }
    MC_COMMAND_REGB(stats, "", "Show server stats");
    MC_COMMAND_REGB(kill, "", "Kill the player");
    MC_COMMAND_REGB(craft, "", "Show a crafting table");
    MC_COMMAND_REG("nether", dimension, "", "Transport player to nether");
    MC_COMMAND_REG("overworld", dimension, "", "Transport player to overworld");
    MC_COMMAND_REG("c", gamemode, "", "Change gamemode to creative");
    MC_COMMAND_REG("s", gamemode, "", "Change gamemode to survival");
    MC_COMMAND_REGB(health, "<level>", "Modify health [0-20]");
    MC_COMMAND_REGB(food, "<level>", "Modify food [0-25]");
    MC_COMMAND_REGB(weather, "<state>", "Modify weather [0: Off, 1: Rain, 2: Thunder, 3: ???]");
    MC_COMMAND_REGB(time, "<add:set> <time>", "Modify server time");
    MC_COMMAND_REGB(ent_status, "<status>", "Modify player entity status");

    LOG("Initializing server");

    bool done = false;

    LOG("Resolving host");
    SDLNet_Address* addr = SDLNet_ResolveHostname(address_listen.get().c_str());

    if (!addr)
    {
        LOG("SDLNet_ResolveHostname: %s", SDL_GetError());
        exit(1);
    }

    if (SDLNet_WaitUntilResolved(addr, 5000) != 1)
    {
        LOG("SDLNet_WaitUntilResolved: %s", SDL_GetError());
        exit(1);
    }

    LOG("Address: %s", SDLNet_GetAddressString(addr));

    LOG("Creating server");

    SDLNet_Server* server = SDLNet_CreateServer(addr, 25565);
    std::vector<client_t> clients;

    if (!server)
    {
        LOG("SDLNet_CreateServer: %s", SDL_GetError());
        exit(1);
    }

    SDLNet_UnrefAddress(addr);

    while (!done)
    {
        SDL_Delay(10);
        int done_client_searching = false;
        while (!done_client_searching)
        {
            client_t new_client;
            if (!SDLNet_AcceptClient(server, &new_client.sock))
            {
                LOG("SDLNet_AcceptClient: %s", SDL_GetError());
                exit(1);
            }
            if (new_client.sock == NULL)
            {
                done_client_searching = true;
                continue;
            }

            /* Set this to a value other than 0 to break things */
            SDLNet_SimulateStreamPacketLoss(new_client.sock, 0);

            SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(new_client.sock);
            LOG("New Socket: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(new_client.sock));
            SDLNet_UnrefAddress(client_addr);

            new_client.time_keep_alive_recv = SDL_GetTicks();

            add_to_inventory(BLOCK_ID_DIAMOND, 0, 1, new_client.inventory);
            add_to_inventory(BLOCK_ID_DIAMOND, 0, 1, new_client.inventory);
            add_to_inventory(ITEM_ID_SIGN, 0, 1, new_client.inventory);
            add_to_inventory(BLOCK_ID_TORCH, 0, 1, new_client.inventory);

            int wool_bits = SDL_rand_bits() & 0xFF;

            for (int i = 3; i < 9; i++)
            {
                add_to_inventory(BLOCK_ID_WOOL, ((i + wool_bits) * (1 + (wool_bits > 127))) % 16, -1, new_client.inventory);
            }

            new_client.inventory[5].id = ITEM_ID_CHAIN_CAP;
            new_client.inventory[6].quantity = 1;
            new_client.inventory[6].id = ITEM_ID_IRON_TUNIC;
            new_client.inventory[7].quantity = 1;
            new_client.inventory[7].id = ITEM_ID_DIAMOND_PANTS;
            new_client.inventory[7].quantity = 1;
            new_client.inventory[8].id = ITEM_ID_GOLD_BOOTS;
            new_client.inventory[8].quantity = 1;
            new_client.inventory[8].damage = 5000;

            clients.push_back(new_client);
        }

        cull_dying_sockets();

        std::vector<std::string> players_kicked;
        std::vector<jint> entities_kicked;

        for (auto it = clients.begin(); it != clients.end();)
        {
            if (!(*it).sock)
            {
                if ((*it).username.length())
                {
                    players_kicked.push_back((*it).username);
                    entities_kicked.push_back((*it).eid);
                    dimensions[(*it).dimension < 0].move_player((*it).eid, 0, 0, 0);
                }
                it = clients.erase(it);
            }
            else
                it = next(it);
        }

        for (int i = 0; i < ARR_SIZE_I(dimensions); i++)
            dimensions[i].update();

        for (size_t client_index = 0; client_index < clients.size(); client_index++)
        {
            client_t* client = &clients.data()[client_index];
            SDLNet_StreamSocket* sock = client->sock;

#define KICK(sock, msg)      \
    do                       \
    {                        \
        kick(sock, msg);     \
        client->sock = NULL; \
        goto loop_end;       \
    } while (0)

            update_server_time();
            Uint64 sdl_tick_last = client->packet.get_last_packet_time();
            Uint64 sdl_tick_cur = SDL_GetTicks();
            packet_t* pack = client->packet.get_next_packet(sock);

            if (sdl_tick_last < sdl_tick_cur && (sdl_tick_cur - sdl_tick_last) > 60000)
                KICK(sock, "Timed out!");

            if (client->time_keep_alive_recv < sdl_tick_cur && (sdl_tick_cur - client->time_keep_alive_recv) > 60000)
                KICK(sock, "Timed out! (No response to keep alive)");

            if (client->username.length())
            {
                for (size_t i = 0; i < players_kicked.size(); i++)
                {
                    packet_chat_message_t pack_leave_msg;
                    pack_leave_msg.msg = "§e";
                    pack_leave_msg.msg += players_kicked[i];
                    pack_leave_msg.msg += " left the game.";

                    send_buffer(sock, pack_leave_msg.assemble());

                    packet_play_list_item_t pack_player_list_leave;
                    pack_player_list_leave.username = players_kicked[i];
                    pack_player_list_leave.online = 0;
                    pack_player_list_leave.ping = 0;

                    send_buffer(sock, pack_player_list_leave.assemble());
                }
                for (size_t i = 0; i < entities_kicked.size(); i++)
                {
                    packet_ent_destroy_t pack_eid_no;
                    pack_eid_no.eid = entities_kicked[i];

                    send_buffer(sock, pack_eid_no.assemble());
                }

                if (sdl_tick_cur - client->time_keep_alive_sent > 200)
                {
                    packet_time_update_t pack_time;
                    pack_time.time = server_time;
                    send_buffer(sock, pack_time.assemble());

                    packet_keep_alive_t pack_keep_alive;
                    pack_keep_alive.keep_alive_id = sdl_tick_cur % (SDL_MAX_SINT32 - 2);
                    send_buffer(sock, pack_keep_alive.assemble());
                    client->time_keep_alive_sent = sdl_tick_cur;

                    {
                        packet_play_list_item_t pack_player_list;
                        pack_player_list.username = client->username;
                        pack_player_list.online = 1;
                        pack_player_list.ping = sdl_tick_cur - client->packet.get_last_packet_time();

                        send_buffer_to_players(clients, pack_player_list.assemble());
                    }

                    if (server_weather == WEATHER_THUNDER && next_thunder_bolt >= 0)
                        next_thunder_bolt -= 100;

                    if (server_weather == WEATHER_THUNDER_SUPER && next_thunder_bolt >= 0)
                        next_thunder_bolt -= 1000;

                    if (next_thunder_bolt < 0 && server_weather > WEATHER_RAIN)
                    {
                        packet_thunder_t pack_thunder;

                        int cx = (((int)client->player_x) >> 4) + (cast_to_sint16(SDL_rand_bits()) >> 11);
                        int cz = (((int)client->player_z) >> 4) + (cast_to_sint16(SDL_rand_bits()) >> 11);

                        chunk_t* c = dimensions[0].get_chunk(cx, cz);

                        double tx, ty, tz;
                        if (c && c->find_spawn_point(tx, ty, tz))
                        {
                            pack_thunder.x = (cx * CHUNK_SIZE_X + tx) * 32;
                            pack_thunder.y = (ty - 1) * 32;
                            if (pack_thunder.y < 0)
                                pack_thunder.y = 0;
                            pack_thunder.z = (cz * CHUNK_SIZE_Z + tz) * 32;
                            pack_thunder.unknown = 1;
                            pack_thunder.eid = eid_counter++;
                            send_buffer_to_players_if_dim(clients, pack_thunder.assemble(), 0);
                            next_thunder_bolt = SDL_rand_bits() & 0x7FFF;
                        }
                    }
                }

                if ((sdl_tick_cur - client->pos_update_time > 50 && client->pos_updated) || sdl_tick_cur - client->pos_update_time > 1000)
                {
                    client->pos_update_time = sdl_tick_cur;
                    client->pos_updated = 0;
                    packet_ent_create_t pack_ext_player_ent;
                    packet_ent_teleport_t pack_ext_player;

                    pack_ext_player_ent.eid = client->eid;
                    pack_ext_player.eid = client->eid;
                    pack_ext_player.x = client->player_x * 32;
                    pack_ext_player.y = client->player_y * 32;
                    pack_ext_player.z = client->player_z * 32;
                    pack_ext_player.rotation = ((int)client->player_yaw) * 255 / 360;
                    pack_ext_player.pitch = client->player_pitch * 64 / 90;

                    send_buffer_to_players_if_coords(clients, pack_ext_player_ent.assemble(), client->player_x, client->player_z, client->dimension, client);
                    send_buffer_to_players_if_coords(clients, pack_ext_player.assemble(), client->player_x, client->player_z, client->dimension, client);

                    if (client->old_dimension != client->dimension)
                    {
                        pack_ext_player.x = -1;
                        pack_ext_player.y = -CHUNK_SIZE_Y * CHUNK_SIZE_Y;
                        ;
                        pack_ext_player.z = -1;

                        send_buffer_to_players_if_coords(
                            clients, pack_ext_player.assemble(), client->player_x, client->player_z, client->dimension < 0 ? 0 : -1, client);
                        send_buffer_to_players_if_coords(
                            clients, pack_ext_player.assemble(), client->player_x, client->player_z, client->dimension < 0 ? 0 : -1, client);

                        client->old_dimension = client->dimension;
                    }

                    dimensions[client->dimension < 0].move_player(client->eid, client->player_x, client->player_y, client->player_z);

                    int diff_x = client->player_x - client->old_x;
                    int diff_z = client->player_z - client->old_z;

                    int diff_x_a = SDL_abs(diff_x);
                    int diff_z_a = SDL_abs(diff_z);

                    if (diff_x_a > 15 || diff_z_a > 15)
                    {
                        dimensions[client->dimension < 0].update();
                        if (diff_x_a > 15)
                            client->old_x = client->player_x;
                        if (diff_z_a > 15)
                            client->old_z = client->player_z;

                        send_square_chunks(client, dimensions, CHUNK_VIEW_DISTANCE);
                        chunk_remove_loaded(client);
                    }
                }

                if (client->is_raining != server_weather)
                {
                    client->is_raining = server_weather;
                    packet_new_state_t pack_rain;
                    if (client->is_raining)
                        pack_rain.reason = PACK_NEW_STATE_REASON_RAIN_START;
                    else
                        pack_rain.reason = PACK_NEW_STATE_REASON_RAIN_END;
                    pack_rain.mode = 0;

                    send_buffer(sock, pack_rain.assemble());
                }

                if (sdl_tick_cur - client->time_last_food_update > 100000 && client->player_mode == 0)
                {
                    client->time_last_food_update = sdl_tick_cur;
                    if (client->food_satur > 0.0)
                        client->food_satur--;
                    else if (client->food > 0)
                        client->food--;

                    client->update_health = 1;
                }

                if (client->player_y < -100)
                {
                    client->health = -100;
                    client->update_health = 1;
                }

                if (sdl_tick_cur - client->time_last_health_update > 1000 && client->player_mode == 0)
                {
                    if (client->health < 20 && client->food >= 20)
                        client->health++;
                    if (client->food < 1 && client->health > 0)
                        client->health--;

                    client->update_health = 1;
                }

                if (client->time_invulnerable_fall_damage)
                    client->time_invulnerable_fall_damage--;

                if (client->player_on_ground && client->player_mode == 0 && !client->time_invulnerable_fall_damage)
                {
                    if (client->y_last_ground - client->player_y > 4)
                    {
                        client->health -= (client->y_last_ground - client->player_y) / 2;
                        client->update_health = 1;
                    }
                }
                if (client->player_on_ground || client->player_mode == 1)
                {
                    client->y_last_ground = client->player_y;
                }

                if (client->update_health == 1)
                {
                    client->update_health = 0;
                    client->time_last_health_update = sdl_tick_cur;
                    packet_health_t pack_health;
                    pack_health.health = client->health;
                    pack_health.food = client->food;
                    pack_health.food_saturation = client->food_satur;

                    packet_ent_status_t pack_player_hurt;
                    pack_player_hurt.eid = client->eid;
                    pack_player_hurt.status = ENT_STATUS_HURT;

                    packet_ent_status_t pack_player_dead;
                    pack_player_dead.eid = client->eid;
                    pack_player_dead.status = ENT_STATUS_DEAD;

                    if (client->health < client->last_health)
                    {
                        if (client->health > 0)
                            send_buffer_to_players_if_coords(clients, pack_player_hurt.assemble(), client->player_x, client->player_z, client->dimension);
                        else
                            send_buffer_to_players_if_coords(clients, pack_player_dead.assemble(), client->player_x, client->player_z, client->dimension);
                    }
                    client->last_health = client->health;

                    send_buffer(sock, pack_health.assemble());
                }
                if (client->update_health)
                    client->update_health--;
            }

            if (!pack && client->packet.get_error().length() > 0)
                KICK(sock, "Server packet handler error: " + client->packet.get_error());

#define CAST_PACK_TO_P(type) type* p = (type*)pack
            if (pack)
            {
                switch (pack->id)
                {
                case PACKET_ID_KEEP_ALIVE:
                {
                    CAST_PACK_TO_P(packet_keep_alive_t);
                    if (p->keep_alive_id)
                        client->time_keep_alive_recv = sdl_tick_cur;
                    break;
                }
                case PACKET_ID_LOGIN_REQUEST:
                {
                    CAST_PACK_TO_P(packet_login_request_c2s_t);
                    LOG("Player \"%s\" has protocol version: %d", p->username.c_str(), p->protocol_ver);
                    if (p->protocol_ver != 17)
                        KICK(sock, "Nope");

                    client->eid = eid_counter++;

                    packet_login_request_s2c_t packet_login_s2c;
                    packet_login_s2c.player_eid = client->eid;
                    packet_login_s2c.seed = server_seed;
                    packet_login_s2c.mode = client->player_mode;
                    packet_login_s2c.dimension = client->dimension;
                    packet_login_s2c.difficulty = 0;
                    packet_login_s2c.world_height = WORLD_HEIGHT;
                    packet_login_s2c.max_players = MAX_PLAYERS;
                    send_buffer(sock, packet_login_s2c.assemble());

                    packet_time_update_t pack_time;
                    pack_time.time = server_time;
                    send_buffer(sock, pack_time.assemble());

                    /* We set the username here because at this point we are committed to having them */
                    client->username = p->username;

                    if (client->username.length() == 0)
                        break;

                    packet_chat_message_t pack_join_msg;
                    pack_join_msg.msg = "§e";
                    pack_join_msg.msg += client->username;
                    pack_join_msg.msg += " joined the game.";

                    spawn_player(clients, client, dimensions);

                    send_buffer_to_players(clients, pack_join_msg.assemble());

                    break;
                }
                case PACKET_ID_HANDSHAKE:
                {
                    CAST_PACK_TO_P(packet_handshake_c2s_t);

                    LOG("Player \"%s\" has initiated handshake", p->username.c_str());
                    packet_handshake_s2c_t packet;
                    packet.connection_hash = "-";
                    send_buffer(sock, packet.assemble());
                    break;
                }
                case PACKET_ID_CHAT_MSG:
                {
                    CAST_PACK_TO_P(packet_chat_message_t);

                    if (p->msg.length() > 100)
                        KICK(sock, "Message too long!");

                    if (p->msg.length() && p->msg[0] == '/')
                    {
                        LOG("Player \"%s\" issued: %s", client->username.c_str(), p->msg.c_str());
                        if (p->msg == "/stop")
                        {
                            done = true;
                            goto loop_end;
                        }
                        else if (p->msg.rfind("/help", 0) == 0)
                        {
                            int help_page = 1;

                            if (p->msg.length() > 6)
                                help_page = atoi(p->msg.c_str() + 6);

                            mc_command_t hard_cmd_help[] = {
                                { "stop", "", "Stop the server", NULL },
                                { "help", "<page>", "Display this help", NULL },
                            };
                            int num_cmd_pages = (7 + mc_commands.size() + ARR_SIZE(hard_cmd_help)) / 8;

                            if (help_page > num_cmd_pages)
                                help_page = num_cmd_pages;

                            if (help_page <= 0)
                                help_page = 1;

                            int printed = 0;
                            int tries = 0;
                            send_chat(sock, "§6============ Commands (Page %d/%d) ============", help_page, num_cmd_pages);

                            for (size_t i = 0; i < ARR_SIZE(hard_cmd_help) && printed < 8; i++, tries++)
                            {
                                if ((help_page - 1) * 8 <= tries && tries < (help_page * 8))
                                {
                                    printed++;
                                    if (hard_cmd_help[i].params != NULL && *(hard_cmd_help[i].params) != '\0')
                                        send_chat(sock, "§5/%s %s", hard_cmd_help[i].name, hard_cmd_help[i].params);
                                    else
                                        send_chat(sock, "§5/%s", hard_cmd_help[i].name);

                                    if (mc_commands[i].help != NULL && *(mc_commands[i].help) != '\0')
                                        send_chat(sock, "  §7%s", mc_commands[i].help);
                                }
                            }
                            for (size_t i = 0; i < mc_commands.size() && printed < 8; i++, tries++)
                            {

                                if ((help_page - 1) * 8 <= tries && tries < (help_page * 8))
                                {
                                    printed++;
                                    if (mc_commands[i].params != NULL && *(mc_commands[i].params) != '\0')
                                        send_chat(sock, "§5/%s %s", mc_commands[i].name, mc_commands[i].params);
                                    else
                                        send_chat(sock, "§5/%s", mc_commands[i].name);

                                    if (mc_commands[i].help != NULL && *(mc_commands[i].help) != '\0')
                                        send_chat(sock, "  §7%s", mc_commands[i].help);
                                }
                            }
                        }
                        else
                        {
                            std::string cmdline;
                            command_return_t cmd_ret = COMMAND_UNSET;
                            for (size_t i = 0; i < mc_commands.size() && cmd_ret == COMMAND_UNSET; i++)
                            {
                                if (p->msg.substr(1, p->msg.find(" ") - 1) == mc_commands[i].name)
                                {
                                    cmd_ret = mc_commands[i].cmd(p->msg.c_str() + 1, clients, client, dimensions);
                                }
                            }

                            if (cmd_ret == COMMAND_FAIL_INTERNAL)
                                send_chat(sock, "§c%s: An internal error occurred!", p->msg.substr(1, p->msg.find(" ")).c_str());

                            if (cmd_ret == COMMAND_FAIL_PARSE)
                                send_chat(sock, "§c%s: A parsing error occurred!", p->msg.substr(1, p->msg.find(" ")).c_str());

                            if (cmd_ret == COMMAND_FAIL_UNHANDLED)
                                send_chat(sock, "§c%s: An error occurred!", p->msg.substr(1, p->msg.find(" ")).c_str());
                        }
                    }
                    else
                    {
                        char buf2[128];
                        snprintf(buf2, ARR_SIZE(buf2), "<%s> %s", client->username.c_str(), p->msg.c_str());

                        LOG("%s", buf2);
                        packet_chat_message_t pack_msg;
                        pack_msg.msg = buf2;

                        send_buffer_to_players(clients, pack_msg.assemble());
                    }
                }
                case PACKET_ID_ENT_USE:
                {
                    break;
                }
                case PACKET_ID_RESPAWN:
                {
                    if (client->health > 0 || client->update_health)
                        break;

                    client->health = 20;
                    client->food = 20;
                    client->food_satur = 5;
                    client->update_health = 1;

                    packet_health_t pack_health;
                    pack_health.health = client->health;
                    pack_health.food = client->food;
                    pack_health.food_saturation = client->food_satur;
                    send_buffer(sock, pack_health.assemble());

                    packet_respawn_t pack_respawn;
                    pack_respawn.seed = server_seed;
                    pack_respawn.dimension = client->dimension;
                    pack_respawn.mode = client->player_mode;
                    pack_respawn.world_height = WORLD_HEIGHT;
                    send_buffer(sock, pack_respawn.assemble());

                    spawn_player(clients, client, dimensions);

                    packet_time_update_t pack_time;
                    pack_time.time = server_time;
                    send_buffer(sock, pack_time.assemble());

                    break;
                }
                case PACKET_ID_PLAYER_ON_GROUND:
                {
                    CAST_PACK_TO_P(packet_on_ground_t);

                    client->player_on_ground = p->on_ground;

                    break;
                }
                case PACKET_ID_PLAYER_POS:
                {
                    CAST_PACK_TO_P(packet_player_pos_t);

                    client->player_x = p->x;
                    client->player_y = p->y;
                    client->player_stance = p->stance;
                    client->player_z = p->z;
                    client->player_on_ground = p->on_ground;

                    client->pos_updated = 1;

                    break;
                }
                case PACKET_ID_PLAYER_LOOK:
                {
                    CAST_PACK_TO_P(packet_player_look_t);

                    client->player_yaw = p->yaw;
                    client->player_pitch = p->pitch;
                    client->player_on_ground = p->on_ground;

                    client->pos_updated = 1;

                    break;
                }
                case PACKET_ID_PLAYER_POS_LOOK:
                {
                    CAST_PACK_TO_P(packet_player_pos_look_c2s_t);

                    client->player_x = p->x;
                    client->player_y = p->y;
                    client->player_stance = p->stance;
                    client->player_z = p->z;
                    client->player_yaw = p->yaw;
                    client->player_pitch = p->pitch;
                    client->player_on_ground = p->on_ground;

                    client->pos_updated = 1;

                    break;
                }
                case PACKET_ID_PLAYER_DIG:
                {
                    CAST_PACK_TO_P(packet_player_dig_t);
                    double x_diff = p->x - client->player_x;
                    double y_diff = p->y - client->player_y;
                    double z_diff = p->z - client->player_z;
                    double radius = SDL_sqrt(x_diff * x_diff + y_diff * y_diff + z_diff * z_diff);

                    TRACE("Dig %d @ <%d, %d, %d>:%d (Radius: %.3f)", p->status, p->x, p->y, p->z, p->face, radius);

                    if (p->status == PLAYER_DIG_STATUS_FINISH_DIG || p->status == PLAYER_DIG_STATUS_START_DIG)
                    {
                        if (radius > 7.0)
                            LOG("Player \"%s\" sent dig with invalid radius of %.3f", client->username.c_str(), radius);
                        if (radius > 6.0)
                            goto loop_end;

                        int cx = p->x >> 4;
                        int cz = p->z >> 4;

                        chunk_t* c = dimensions[client->dimension < 0].get_chunk(cx, cz);
                        if (c)
                        {
                            if (p->status == PLAYER_DIG_STATUS_FINISH_DIG
                                || (client->player_mode > 0 && !mc_id::is_sword(client->inventory[client->cur_item_idx].id)))
                            {
                                TRACE("%d %d %d %d", p->x, p->y, p->z, c->get_type(p->x % 16, p->y, p->z % 16));
                                packet_sound_effect_t pack_break_sfx;
                                pack_break_sfx.effect_id = 2001;
                                pack_break_sfx.x = p->x;
                                pack_break_sfx.y = p->y;
                                pack_break_sfx.z = p->z;
                                Uint8 old_type = c->get_type(p->x % 16, p->y, p->z % 16);
                                Uint8 old_metadata = c->get_metadata(p->x % 16, p->y, p->z % 16);
                                pack_break_sfx.sound_data = old_type | (old_metadata << 8);

                                mc_id::block_return_t ret = mc_id::get_return_from_block(old_type, old_metadata);
                                int quant = ret.quantity_min;
                                if (quant != ret.quantity_max)
                                    quant = (SDL_rand_bits() % (ret.quantity_max - ret.quantity_min)) + ret.quantity_min;
                                if (client->player_mode == 0)
                                {
                                    add_to_inventory(ret.id, ret.damage, quant, client->inventory);
                                    send_inventory(client);
                                }

                                for (size_t i = 0; i < clients.size(); i++)
                                    if (clients[i].sock && clients[i].username.length() > 0)
                                        send_buffer_to_players_if_coords(
                                            clients, pack_break_sfx.assemble(), pack_break_sfx.x, pack_break_sfx.z, client->dimension);

                                packet_block_change_t pack_block_change;
                                pack_block_change.block_x = p->x;
                                pack_block_change.block_y = p->y;
                                pack_block_change.block_z = p->z;
                                pack_block_change.type = 0;
                                pack_block_change.metadata = 0;
                                c->set_type(p->x % 16, p->y, p->z % 16, 0);
                                c->set_metadata(p->x % 16, p->y, p->z % 16, 0);
                                c->set_light_sky(p->x % 16, p->y, p->z % 16, 15);
                                send_buffer_to_players_if_coords(clients, pack_block_change.assemble(), pack_break_sfx.x, pack_break_sfx.z, client->dimension);
                            }
                        }
                        else
                            LOG("Unable to place block");
                    }
                    else if (p->status == PLAYER_DIG_STATUS_DROP_ITEM)
                    {
                        client->inventory[client->cur_item_idx].quantity--;
                        if (client->inventory[client->cur_item_idx].quantity < 1)
                            client->inventory[client->cur_item_idx] = {};
                        send_inventory(client);
                    }
                    else
                    {
                        LOG("Player \"%s\" sent dig with unsupported status of %d", client->username.c_str(), p->status);
                        goto loop_end;
                    }

                    break;
                }
                case PACKET_ID_PLAYER_PLACE:
                {
                    CAST_PACK_TO_P(packet_player_place_t);
                    double x_diff = p->x - client->player_x;
                    double y_diff = p->y - client->player_y;
                    double z_diff = p->z - client->player_z;
                    double radius = SDL_sqrt(x_diff * x_diff + y_diff * y_diff + z_diff * z_diff);

                    (void)radius;

                    jshort type = p->block_item_id;

                    int place_x = p->x;
                    int place_y = p->y;
                    int place_z = p->z;
                    int center_x = p->x;
                    int center_y = p->y;
                    int center_z = p->z;
                    bool center_good = true;
                    int is_torch = type == BLOCK_ID_TORCH;
                    is_torch += type == BLOCK_ID_TORCH_REDSTONE_ON;
                    is_torch += type == BLOCK_ID_TORCH_REDSTONE_OFF;
                    is_torch += type == BLOCK_ID_LEVER;
                    int is_rail = type == BLOCK_ID_RAIL;
                    is_rail += type == BLOCK_ID_RAIL_POWERED;
                    is_rail += type == BLOCK_ID_RAIL_DETECTOR;
                    float theta = SDL_atan2f(place_x + 0.5f - client->player_x, place_z + 0.5f - client->player_z) * 180.0f / SDL_PI_F;
                    theta = client->player_yaw;
                    float phi = client->player_pitch;
                    theta = SDL_fmodf(SDL_fmodf(theta, 360.f) + 360.0f, 360.f);

                    int mc_face = 0;
                    if (45.0f < theta && theta < 135.0f)
                        mc_face = 1;
                    else if (135.0f < theta && theta < 225.0f)
                        mc_face = 2;
                    else if (225.0f < theta && theta < 315.0f)
                        mc_face = 3;
                    else
                        mc_face = 0;

                    int y_dir = 0;
                    if (phi > 50.0f)
                        y_dir = 1;
                    if (phi < -50.0f)
                        y_dir = -1;

                    bool x_dir = mc_face == 1 || mc_face == 3;

                    TRACE("Place %d @ <%d, %d, %d>:%d (%d) (Radius: %.3f)", type, p->x, p->y, p->z, p->direction, mc_face, radius);

                    Uint8 food_value = mc_id::get_food_value(type);
                    if (food_value && client->food < 20)
                    {
                        client->food += food_value;
                        if (client->food > 20)
                        {
                            client->food_satur = (float)(client->food - 20) * mc_id::get_food_staturation_ratio(type);
                            client->food = 20;
                        }

                        survival_mode_decrease_hand(client);
                        break;
                    }

                    if (p->direction != -1 && type >= 0 && type < 256)
                    {
                        switch (p->direction)
                        {
                        case 0:
                            place_y -= 1;
                            if (is_torch)
                                type = 0;
                            if (type == BLOCK_ID_LADDER)
                                type = 0;
                            break;
                        case 1:
                            place_y += 1;
                            if (is_torch)
                                p->damage = 5;
                            if (type == BLOCK_ID_LEVER && x_dir)
                                p->damage = 6;
                            if (type == BLOCK_ID_LADDER)
                                type = 0;
                            break;
                        case 2:
                            place_z -= 1;
                            if (is_torch)
                                p->damage = 4;
                            if (type == BLOCK_ID_LADDER)
                                p->damage = 2;
                            break;
                        case 3:
                            place_z += 1;
                            if (is_torch)
                                p->damage = 3;
                            if (type == BLOCK_ID_LADDER)
                                p->damage = 3;
                            break;
                        case 4:
                            place_x -= 1;
                            if (is_torch)
                                p->damage = 2;
                            if (type == BLOCK_ID_LADDER)
                                p->damage = 4;
                            break;
                        case 5:
                            place_x += 1;
                            if (is_torch)
                                p->damage = 1;
                            if (type == BLOCK_ID_LADDER)
                                p->damage = 5;
                            break;
                        default:
                            goto loop_end;
                            break;
                        }

                        if (is_rail && x_dir)
                            p->damage = 1;

                        if (type == BLOCK_ID_FURNACE_OFF || type == BLOCK_ID_CHEST || type == BLOCK_ID_DISPENSER || type == BLOCK_ID_PISTON
                            || type == BLOCK_ID_PISTON_STICKY)
                        {
                            switch (mc_face)
                            {
                            case 1:
                                p->damage = 5;
                                break;
                            case 2:
                                p->damage = 3;
                                break;
                            case 3:
                                p->damage = 4;
                                break;
                            default:
                                p->damage = 2;
                                break;
                            }
                        }

                        if (type == BLOCK_ID_PISTON || type == BLOCK_ID_PISTON_STICKY)
                        {
                            switch (y_dir)
                            {
                            case -1:
                                p->damage = 0;
                                break;
                            case 1:
                                p->damage = 1;
                                break;
                            default:
                                break;
                            }
                        }

                        if (type == BLOCK_ID_PUMPKIN || type == BLOCK_ID_PUMPKIN_GLOWING)
                        {
                            switch (mc_face)
                            {
                            case 1:
                                p->damage = 3;
                                break;
                            case 2:
                                p->damage = 0;
                                break;
                            case 3:
                                p->damage = 1;
                                break;
                            default:
                                p->damage = 2;
                                break;
                            }
                        }

                        Uint8 is_stairs = type == BLOCK_ID_STAIRS_BRICK;
                        is_stairs += type == BLOCK_ID_STAIRS_WOOD;
                        is_stairs += type == BLOCK_ID_STAIRS_BRICK_STONE;
                        is_stairs += type == BLOCK_ID_STAIRS_COBBLESTONE;

                        if (is_stairs)
                        {
                            switch (mc_face)
                            {
                            case 1:
                                p->damage = 1;
                                break;
                            case 2:
                                p->damage = 3;
                                break;
                            case 3:
                                p->damage = 0;
                                break;
                            default:
                                p->damage = 2;
                                break;
                            }
                        }

                        if (place_y < 0)
                            goto loop_end;

                        double diff_y = (double)place_y - client->player_y;

                        if (SDL_floor(client->player_x) == place_x && (-0.9 < diff_y && diff_y < 1.8) && SDL_floor(client->player_z) == place_z
                            && mc_id::block_has_collision(type))
                        {
                            type = 0;
                            p->damage = 0;
                        }

                        int cx = place_x >> 4;
                        int cz = place_z >> 4;

                        chunk_t* c = dimensions[client->dimension < 0].get_chunk(cx, cz);

                        if (c && (is_torch || type == BLOCK_ID_LADDER))
                        {
                            Uint8 center_type = c->get_type(center_x % 16, center_y, center_z % 16);
                            if (!mc_id::can_host_hanging(center_type))
                                center_good = false;
                        }

                        if (c && is_rail)
                        {
                            if (center_y > 0)
                            {
                                Uint8 lower_type = c->get_type(place_x % 16, place_y - 1, place_z % 16);
                                if (!mc_id::can_host_rail(lower_type))
                                    center_good = false;
                            }
                        }

                        if (c && type != 0 && c->get_type(place_x % 16, place_y, place_z % 16) == BLOCK_ID_AIR && center_good)
                        {
                            packet_block_change_t pack_block_change;
                            pack_block_change.block_x = place_x;
                            pack_block_change.block_y = place_y;
                            pack_block_change.block_z = place_z;
                            pack_block_change.type = type;
                            pack_block_change.metadata = p->damage;
                            c->set_type(place_x % 16, place_y, place_z % 16, type);
                            c->set_metadata(place_x % 16, place_y, place_z % 16, p->damage);
                            send_buffer_to_players_if_coords(clients, pack_block_change.assemble(), place_x, place_z, client->dimension);

                            survival_mode_decrease_hand(client);
                            break;
                        }
                        else
                        {
                            if (c)
                            {
                                packet_block_change_t pack_block_change;
                                pack_block_change.block_x = place_x;
                                pack_block_change.block_y = place_y;
                                pack_block_change.block_z = place_z;
                                pack_block_change.type = c->get_type(place_x % 16, place_y, place_z % 16);
                                pack_block_change.metadata = c->get_metadata(place_x % 16, place_y, place_z % 16);

                                send_buffer_to_players_if_coords(clients, pack_block_change.assemble(), place_x, place_z, client->dimension);
                            }
                            LOG("Unable to place block");
                        }
                    }

                    packet_window_set_slot_t pack_set_slot;
                    pack_set_slot.slot = client->cur_item_idx;
                    pack_set_slot.item = client->inventory[client->cur_item_idx];
                    send_buffer(sock, pack_set_slot.assemble());

                    break;
                }
                case PACKET_ID_HOLD_CHANGE:
                {
                    CAST_PACK_TO_P(packet_hold_change_t);

                    client->cur_item_idx = SDL_abs(p->slot_id) % 9 + 36;
                    break;
                }
                case PACKET_ID_ENT_ANIMATION:
                {
                    send_buffer_to_players_if_coords(clients, pack->assemble(), client->player_x, client->player_z, client->dimension, client);
                    break;
                }
                case PACKET_ID_ENT_ACTION:
                    break;
                case PACKET_ID_WINDOW_TRANSACTION:
                {
                    CAST_PACK_TO_P(packet_window_transaction_t);

                    LOG("Window %d transaction: %d %d", p->window_id, p->action_num, p->accepted);

                    break;
                }
                case PACKET_ID_WINDOW_CLICK:
                {
                    CAST_PACK_TO_P(packet_window_click_t);

                    LOG("Window %d click: %d %d %d %d", p->window_id, p->slot, p->right_click, p->action_num, p->shift);

                    break;
                }
                case PACKET_ID_WINDOW_CLOSE:
                {
                    CAST_PACK_TO_P(packet_window_close_t);

                    LOG("Window %d closed", p->window_id);

                    break;
                }
                case PACKET_ID_INV_CREATIVE_ACTION:
                {
                    CAST_PACK_TO_P(packet_inventory_action_creative_t);

                    if (p->slot < 0 || p->slot >= ARR_SIZE_S(client->inventory) || client->player_mode != 1)
                        goto loop_end;

                    const char* name = mc_id::get_name_from_item_id(p->item_id, p->damage);
                    LOG("%s %d %d (%s) %d %d", client->username.c_str(), p->slot, p->item_id, name, p->quantity, p->damage);

                    client->inventory[p->slot].id = p->item_id;
                    client->inventory[p->slot].quantity = p->quantity;
                    client->inventory[p->slot].damage = p->damage;

                    break;
                }
                case PACKET_ID_UPDATE_SIGN:
                {
                    CAST_PACK_TO_P(packet_update_sign_t);

                    if (p->text0.length())
                        LOG("%s Sign[0]: \"%s\"", client->username.c_str(), p->text0.c_str());
                    if (p->text1.length())
                        LOG("%s Sign[1]: \"%s\"", client->username.c_str(), p->text1.c_str());
                    if (p->text2.length())
                        LOG("%s Sign[2]: \"%s\"", client->username.c_str(), p->text2.c_str());
                    if (p->text3.length())
                        LOG("%s Sign[3]: \"%s\"", client->username.c_str(), p->text3.c_str());

                    break;
                }
                case PACKET_ID_SERVER_LIST_PING:
                {
                    std::string s = "A mcs_b181 server§";
                    Uint32 playercount = 0;
                    for (size_t i = 0; i < clients.size(); i++)
                        if (clients[i].username.length() > 0)
                            playercount++;
                    s.append(std::to_string(playercount));
                    s.append("§");
                    s.append(std::to_string(MAX_PLAYERS));
                    kick(sock, s.c_str(), 0);
                    client->sock = NULL;
                    goto loop_end;
                    break;
                }
                case PACKET_ID_KICK:
                {
                    CAST_PACK_TO_P(packet_kick_t);
                    client->sock = NULL;
                    if (p->reason != "Quitting")
                    {
                        LOG("Client kicked server with unknown message \"%s\"", p->reason.c_str());
                    }

                    if (client->username.length() == 0)
                        break;
                    break;
                }
                default:
                {
                    char buf2[32];
                    snprintf(buf2, ARR_SIZE(buf2), "Unknown packet: 0x%02x", pack->id);
                    LOG("Unknown packet: 0x%02x", pack->id);
                    KICK(sock, buf2);
                    break;
                }
                }
            }
#undef CAST_PACK_TO_P

        loop_end:;
            delete pack;
        }
    }

    LOG("Destroying server");
    for (size_t i = 0; i < clients.size(); i++)
    {
        if (clients[i].sock)
            kick(clients[i].sock, "Server stopping!");
    }

    SDL_Delay(50);

    cull_dying_sockets(true);

    SDLNet_DestroyServer(server);

    SDLNet_Quit();
    SDL_Quit();
}

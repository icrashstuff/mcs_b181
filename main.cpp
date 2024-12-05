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

#include <zlib.h>

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

#include "simplex_noise/SimplexNoise.h"

long world_seed = 0;

void kick(SDLNet_StreamSocket* sock, std::string reason, bool log = true)
{
    packet_kick_t packet;
    packet.reason = reason;
    send_buffer(sock, packet.assemble());

    SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(sock);
    if (log)
        LOG("Kicked: %s:%u, \"%s\"", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(sock), reason.c_str());
    SDLNet_UnrefAddress(client_addr);

    /* TODO: Move socket destruction to a separate place*/
    int remaining = SDLNet_WaitUntilStreamSocketDrained(sock, 100);
    if (remaining)
        LOG_WARN("Socket about to be destroyed has %d remaining bytes to be sent", remaining);
    SDLNet_DestroyStreamSocket(sock);
}

bool send_prechunk(SDLNet_StreamSocket* sock, int chunk_x, int chunk_z, bool mode)
{
    packet_prechunk_t packet;
    packet.chunk_x = chunk_x;
    packet.chunk_z = chunk_z;
    packet.mode = mode;
    if (!mode)
        send_buffer(sock, packet.assemble());
    return send_buffer(sock, packet.assemble());
}

/* A 16 * WORLD_HEIGHT * 16 chunk */
class chunk_t
{
public:
    chunk_t()
    {
        data.resize(CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 5 / 2, 0);
        assert(data.size());
        Uint8* ptr = data.data();
        r_state = *(Uint64*)&ptr;
    }

    void generate_from_seed_over(long seed, int cx, int cz)
    {
        SimplexNoise noise;

        /* Source: I made this up,*/
        int r = (seed % 0x012476) << 4 & seed % 0xF6F0AFF;

        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
            {
                double fx = x + cx * CHUNK_SIZE_X + r;
                double fz = z + cz * CHUNK_SIZE_X + r;
                int height_grass = (noise.fractal(2, fx / 100, fz / 100) + 1.0) + 2;
                double height = (noise.fractal(4, fx / 100, fz / 100) + 1.0 + noise.noise((fx + 10.0) / 100, (fz + 10.0) / 100) + 1.0) * 0.05 * CHUNK_SIZE_Y
                    + 56 - height_grass;
                double aggressive = noise.fractal(4, fx / 150, fz / 150) + 1.0;
                if (aggressive > 1.05)
                    height = height * (noise.fractal(3, fx / 150, fz / 150) + 1.0);
                if (aggressive > 1.5)
                    height = height * 1.5 / (noise.fractal(2, fx / 150, fz / 150) + 1.0);
                else
                    height = height + 1.5 / (noise.fractal(2, fx / 150, fz / 150) + 1.0);

                for (int i = 1; i < height && i < CHUNK_SIZE_Y; i++)
                    set_type(x, i, z, BLOCK_ID_STONE);
                for (int i = height; i < height + height_grass && i < CHUNK_SIZE_Y; i++)
                    set_type(x, i, z, BLOCK_ID_DIRT);
                set_type(x, height + height_grass, z, BLOCK_ID_GRASS);
                for (int i = height - 2; i < CHUNK_SIZE_Y; i++)
                    set_light_sky(x, i, z, 15);
                set_type(x, 0, z, BLOCK_ID_BEDROCK);
            }
        }
    }

    void generate_from_seed_nether(long seed, int cx, int cz)
    {
        SimplexNoise noise;

        int r = (seed % 0xF2FFF << 4 | seed) & seed % 0xF6F0AFF;

        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
            {
                double fx = x + cx * CHUNK_SIZE_X + r;
                double fz = z + cz * CHUNK_SIZE_X + r;
                int height = (noise.fractal(4, fx / 100, fz / 100) + 1.0) * 0.1 * CHUNK_SIZE_Y + 56;
                double noise2 = (noise.fractal(4, fx / 200, fz / 200) + 1.0) * 0.1 * CHUNK_SIZE_Y + 4;

                int height2 = CHUNK_SIZE_Y - noise2;
                for (int i = 1; i < height; i++)
                    set_type(x, i, z, BLOCK_ID_NETHERACK);
                for (int i = height - 2; i < height2; i++)
                {
                    if (i < 63)
                    {
                        set_type(x, i, z, BLOCK_ID_LAVA_FLOWING);
                        set_light_block(x, i, z, 15);
                    }
                    else
                        set_light_sky(x, i, z, 15);
                }
                for (int i = height2; i < CHUNK_SIZE_Y; i++)
                    set_type(x, i, z, BLOCK_ID_NETHERACK);
                set_type(x, 0, z, BLOCK_ID_BEDROCK);
                set_type(x, CHUNK_SIZE_Y - 1, z, BLOCK_ID_BEDROCK);
                set_light_sky(x, CHUNK_SIZE_Y - 1, z, 15);
            }
        }
    }

    void generate_special_ascending_type(int max_y)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
            {
                for (int z = 0; z < CHUNK_SIZE_Z; z++)
                {
                    if (y < BLOCK_ID_MAX && y < max_y)
                    {
                        set_type(x, y, z, y);
                    }
                    set_light_block(x, y, z, 15);
                    set_light_sky(x, y, z, 15);
                }
            }
        }
    }

    void generate_special_metadata()
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
            {
                for (int z = 0; z < CHUNK_SIZE_Z; z++)
                {
                    if (y < BLOCK_ID_MAX)
                    {
                        if (z == x)
                        {
                            set_type(x, y, z, y);
                            set_metadata(x, y, z, x);
                        }
                    }
                    set_light_block(x, y, z, 15);
                    set_light_sky(x, y, z, 15);
                }
            }
        }
    }

    /**
     * Attempts to find a suitable place to put a player in a chunk
     *
     * Returns true if a suitable location was found, false if a fallback location at world height was selected
     */
    inline bool find_spawn_point(double& x, double& y, double& z)
    {
        Uint32 pos = ((int)(x * 3) << 24) + ((int)(y * 3) << 12) + (int)(z * 3);
        pos += SDL_rand_bits_r(&r_state);

        int cx_s = (pos >> 16) % CHUNK_SIZE_X;
        int cz_s = pos % CHUNK_SIZE_Z;

        int found_air = 0;
        Uint8 last_type[2] = { 0, 0 };

        for (int ix = 0; ix < CHUNK_SIZE_X; ix++)
        {
            for (int iz = 0; iz < CHUNK_SIZE_Z; iz++)
            {
                int cx = (ix + cx_s) % CHUNK_SIZE_X;
                int cz = (iz + cz_s) % CHUNK_SIZE_Z;
                TRACE("checking %d %d", cx, cz);
                for (int i = CHUNK_SIZE_Y; i > 0; i--)
                {
                    int index = i - 1 + (cz * (CHUNK_SIZE_Y)) + (cx * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));
                    Uint8 type = data[index];
                    if (type == 0)
                        found_air++;

                    if (type > 0 && found_air > 2 && last_type[0] == 0 && last_type[1] == 0 && type != BLOCK_ID_LAVA_FLOWING && type != BLOCK_ID_LAVA_SOURCE)
                    {
                        x = cx + 0.5;
                        y = i + 1.8;
                        z = cz + 0.5;
                        return true;
                    }
                    last_type[1] = last_type[0];
                    last_type[0] = type;
                }
            }
        }

        x = cx_s + 0.5;
        y = CHUNK_SIZE_Y + 1.8;
        z = cz_s + 0.5;
        return false;
    }

    inline Uint8 get_type(int x, int y, int z)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));

        return data[index];
    }

    inline void set_type(int x, int y, int z, Uint8 type)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));

        if (type <= BLOCK_ID_MAX)
            data[index] = type;
        else
            data[index] = 0;
    }

    inline void set_metadata(int x, int y, int z, Uint8 metadata)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 2;

        if (index % 2 == 1)
            data[index / 2] = ((metadata & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (metadata & 0x0F) | (data[index / 2] & 0xF0);
    }

    inline void set_light_block(int x, int y, int z, Uint8 level)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 3;

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }

    inline void set_light_sky(int x, int y, int z, Uint8 level)
    {
        if (x < 0)
            x += 16;
        if (y < 0)
            y += 16;
        if (z < 0)
            z += 16;

        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 4;

        if (index % 2 == 1)
            data[index / 2] = ((level & 0x0F) << 4) | (data[index / 2] & 0x0F);
        else
            data[index / 2] = (level & 0x0F) | (data[index / 2] & 0xF0);
    }

    bool compress_to_buf(std::vector<Uint8>& out)
    {
        out.resize(compressBound(data.size()));

        uLongf compressed_len = out.size();
        int result = compress(out.data(), &compressed_len, data.data(), data.size());
        out.resize(compressed_len);

        return result == Z_OK;
    }

private:
    Uint64 r_state;
    std::vector<Uint8> data;
};

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

            threads[cx] = SDL_CreateThread(generate_chunk_thread_func, "Chunk Thread", &d[cx]);
        }
        for (size_t i = 0; i < ARR_SIZE(threads); i++)
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

class dimension_t
{
public:
    dimension_t(int terrain_generator, long seed_dim, bool update_on_init = true)
    {
        generator = terrain_generator;
        seed = seed_dim;

        r_state = seed + generator;

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
                if (!(*it).region)
                {
                    TRACE("Generating region %d %d", (*it).x, (*it).z);
                    (*it).region = new region_t();
                    if ((*it).region)
                        (*it).region->generate_from_seed(seed, generator, (*it).x, (*it).z);
                }
                else
                    TRACE("Skipping region %d %d", (*it).x, (*it).z);
                it = next(it);
            }
        }

        TRACE("Dimension Update done");
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
    packet_buffer_t packet;

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
    int player_mode = 1;

    short health = 20;
    short food = 20;
    float food_satur = 5.0;

    bool pos_updated = 1;

    Uint64 time_last_health_update = 0;

    Uint64 time_last_food_update = 0;
    Uint64 pos_update_time;

    int update_health = 1;

    bool is_raining = 0;

    std::vector<chunk_coords_t> loaded_chunks;
};

bool is_chunk_loaded(std::vector<chunk_coords_t> loaded_chunks, int chunk_x, int chunk_z)
{
    for (size_t i = 0; i < loaded_chunks.size(); i++)
        if (loaded_chunks[i].x == chunk_x && loaded_chunks[i].z == chunk_z)
            return true;
    return false;
}

void chunk_remove_loaded(client_t* client)
{
    int chunk_x_min = ((int)client->player_x >> 4) - VIEW_DISTANCE;
    int chunk_x_max = ((int)client->player_x >> 4) + VIEW_DISTANCE;
    int chunk_z_min = ((int)client->player_z >> 4) - VIEW_DISTANCE;
    int chunk_z_max = ((int)client->player_z >> 4) + VIEW_DISTANCE;

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

void send_buffer_to_players(std::vector<client_t> clients, std::vector<Uint8> buf)
{
    for (size_t i = 0; i < clients.size(); i++)
        if (clients[i].sock && clients[i].username.length() > 0)
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
            if (send_chunk(client->sock, dimensions[client->dimension < 0].get_chunk(coords.x, coords.z), coords.x, coords.z))
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
            if (send_chunk(client->sock, dimensions[client->dimension < 0].get_chunk(coords.x, coords.z), coords.x, coords.z))
                client->loaded_chunks.push_back(coords);
        }
    }
}

void spawn_player(std::vector<client_t> clients, client_t* client, dimension_t* dimensions)
{
    SDLNet_StreamSocket* sock = client->sock;

    dimensions[client->dimension < 0].find_spawn_point(client->player_x, client->player_y, client->player_z);
    client->y_last_ground = client->player_y;
    client->player_stance = client->player_y + 0.2;
    client->player_on_ground = 1;

    packet_ent_create_t pack_player_ent;
    packet_named_ent_spawn_t pack_player;

    pack_player_ent.eid = client->eid;
    pack_player.eid = client->eid;
    pack_player.name = client->username;
    pack_player.x = client->player_x * 32;
    pack_player.y = client->player_y * 32;
    pack_player.z = client->player_z * 32;
    pack_player.rotation = ((int)client->player_yaw) * 255 / 360;
    pack_player.pitch = client->player_pitch;

    for (size_t i = 0; i < clients.size(); i++)
    {
        if (clients[i].sock && clients[i].username.length() > 0 && clients[i].sock != sock)
        {
            packet_ent_create_t pack_ext_player_ent;
            packet_named_ent_spawn_t pack_ext_player;

            pack_ext_player_ent.eid = clients[i].eid;
            pack_ext_player.eid = clients[i].eid;
            pack_ext_player.name = clients[i].username;
            pack_ext_player.x = clients[i].player_x * 32;
            pack_ext_player.y = clients[i].player_y * 32;
            pack_ext_player.z = clients[i].player_z * 32;
            pack_ext_player.rotation = ((int)clients[i].player_yaw) * 255 / 360;
            pack_ext_player.pitch = clients[i].player_pitch;

            send_buffer(sock, pack_ext_player_ent.assemble());
            send_buffer(sock, pack_ext_player.assemble());

            send_buffer(clients[i].sock, pack_player_ent.assemble());
            send_buffer(clients[i].sock, pack_player.assemble());
        }
    }

    send_circle_chunks(client, dimensions, VIEW_DISTANCE / 2);

    packet_player_pos_look_s2c_t pack_player_pos;
    pack_player_pos.x = client->player_x;
    pack_player_pos.y = client->player_y;
    pack_player_pos.stance = client->player_stance;
    pack_player_pos.z = client->player_z;
    pack_player_pos.yaw = client->player_yaw;
    pack_player_pos.pitch = client->player_pitch;
    pack_player_pos.on_ground = client->player_on_ground;

    send_square_chunks(client, dimensions, VIEW_DISTANCE);

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

int main(int argc, char** argv)
{
    /* KDevelop fully buffers the output and will not display anything */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

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

    /* These seeds break terrain gen */

    world_seed = -1775145942054626728;
    world_seed = SDL_MIN_SINT64;
    world_seed = -1705145942054626728; /* More granular */

    world_seed = -((Uint64)7 << 60); /* Even more granular */

    world_seed = 5090434129841486344; /* Coarse: Nether only */

    /* Intentionally shift only 31 places so the number is negative */
    world_seed = (Sint64)((Uint64)SDL_rand_bits() << 31 | (Uint64)SDL_rand_bits());

    /* We limit ourselves to 16 bits because larger values tend to break the generators */
    world_seed = SDL_rand_bits() & 0xFFFF;

    LOG("World seed: %ld", world_seed);

    LOG("Generating regions");
    Uint64 tick_region_start = SDL_GetTicks();
    dimension_t dimensions[2] = { dimension_t(0, world_seed, false), dimension_t(-1, world_seed, false) };

    SDL_Thread* dim_gen_threads[ARR_SIZE(dimensions)];
    for (size_t i = 0; i < ARR_SIZE(dim_gen_threads); i++)
        dim_gen_threads[i] = SDL_CreateThread(dim_gen_thread_func, "Dim gen thread", &dimensions[i]);
    for (size_t i = 0; i < ARR_SIZE(dim_gen_threads); i++)
        SDL_WaitThread(dim_gen_threads[i], NULL);

    Uint64 tick_region_time = SDL_GetTicks() - tick_region_start;
    LOG("Regions generated in %lu ms", tick_region_time);

    LOG("Initializing server");

    bool done = false;

    LOG("Resolving host");
    SDLNet_Address* addr = SDLNet_ResolveHostname("127.0.0.1");

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

    bool is_raining = 0;

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

            clients.push_back(new_client);
        }

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
                }
                it = clients.erase(it);
            }
            else
                it = next(it);
        }

        for (size_t client_index = 0; client_index < clients.size(); client_index++)
        {
            client_t* client = &clients.data()[client_index];
            SDLNet_StreamSocket* sock = client->sock;

#define KICK(sock, msg)      \
    do                       \
    {                        \
        kick(sock, msg);     \
        client->sock = NULL; \
        delete pack;         \
        goto loop_end;       \
    } while (0)

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
                    pack_time.time = sdl_tick_cur / 50;
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
                    pack_ext_player.pitch = client->player_pitch;
                    for (size_t i = 0; i < clients.size(); i++)
                    {
                        if (clients[i].sock && clients[i].username.length() > 0 && clients[i].sock != sock)
                        {
                            send_buffer(clients[i].sock, pack_ext_player_ent.assemble());
                            send_buffer(clients[i].sock, pack_ext_player.assemble());
                        }
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

                        send_square_chunks(client, dimensions, VIEW_DISTANCE);
                        chunk_remove_loaded(client);
                    }
                }

                if (client->is_raining != is_raining)
                {
                    client->is_raining = is_raining;
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

                if (client->player_on_ground && client->player_mode == 0)
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
                case 0x00:
                {
                    CAST_PACK_TO_P(packet_keep_alive_t);
                    if (p->keep_alive_id)
                        client->time_keep_alive_recv = sdl_tick_cur;
                    break;
                }
                case 0x01:
                {
                    CAST_PACK_TO_P(packet_login_request_c2s_t);
                    LOG("Player \"%s\" has protocol version: %d", p->username.c_str(), p->protocol_ver);
                    if (p->protocol_ver != 17)
                        KICK(sock, "Nope");

                    client->eid = eid_counter++;

                    packet_login_request_s2c_t packet_login_s2c;
                    packet_login_s2c.player_eid = client->eid;
                    packet_login_s2c.seed = world_seed;
                    packet_login_s2c.mode = client->player_mode;
                    packet_login_s2c.dimension = client->dimension;
                    packet_login_s2c.difficulty = 0;
                    packet_login_s2c.world_height = WORLD_HEIGHT;
                    packet_login_s2c.max_players = MAX_PLAYERS;
                    send_buffer(sock, packet_login_s2c.assemble());

                    packet_time_update_t pack_time;
                    pack_time.time = sdl_tick_cur / 50;
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
                case 0x02:
                {
                    CAST_PACK_TO_P(packet_handshake_c2s_t);

                    LOG("Player \"%s\" has initiated handshake", p->username.c_str());
                    packet_handshake_s2c_t packet;
                    packet.connection_hash = "-";
                    send_buffer(sock, packet.assemble());
                    break;
                }
                case 0x03:
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
                            for (size_t i = 0; i < clients.size(); i++)
                            {
                                if (clients[i].sock && clients[i].username.length())
                                {
                                    kick(clients[i].sock, "Server stopping!");
                                    clients[i].sock = NULL;
                                }
                            }
                            goto loop_end;
                        }
                        else if (p->msg == "/kill")
                        {
                            client->health = -100;
                            client->update_health = 2;
                        }
                        else if (p->msg == "/smite")
                        {
                            packet_thunder_t pack_thunder;
                            pack_thunder.eid = 2;
                            pack_thunder.unknown = true;
                            pack_thunder.x = client->player_x;
                            pack_thunder.y = client->player_y;
                            pack_thunder.z = client->player_z;
                            send_buffer(sock, pack_thunder.assemble());
                        }
                        else if (p->msg == "/unload")
                        {
                            int x = (int)client->player_x >> 4;
                            int z = (int)client->player_z >> 4;
                            LOG("Unloading chunk %d %d", x, z);

                            /* The notchian client does not like chunks being unloaded near it */
                            for (int i = 0; i < 16; i++)
                                for (size_t j = 0; j < clients.size(); j++)
                                    if (clients[j].sock && clients[j].username.length() > 0)
                                        send_prechunk(clients[j].sock, x, z, 0);
                        }
                        else if (p->msg == "/nether" || p->msg == "/overworld")
                        {
                            packet_respawn_t pack_dim_change;
                            client->dimension = p->msg == "/overworld" ? 0 : -1;
                            pack_dim_change.seed = world_seed;
                            pack_dim_change.dimension = client->dimension;
                            pack_dim_change.mode = client->player_mode;
                            pack_dim_change.world_height = WORLD_HEIGHT;
                            send_buffer(sock, pack_dim_change.assemble());

                            packet_time_update_t pack_time;
                            pack_time.time = sdl_tick_cur / 50;
                            send_buffer(sock, pack_time.assemble());

                            spawn_player(clients, client, dimensions);
                        }
                        else if (p->msg == "/c" || p->msg == "/s")
                        {
                            client->player_mode = (p->msg == "/c") ? 1 : 0;

                            packet_new_state_t pack_mode;
                            pack_mode.reason = PACK_NEW_STATE_REASON_CHANGE_MODE;
                            pack_mode.mode = client->player_mode;

                            send_buffer(sock, pack_mode.assemble());
                        }
                        else if (p->msg == "/rain_on" || p->msg == "/rain_off")
                        {
                            is_raining = (p->msg == "/rain_on") ? 1 : 0;
                        }
                        else if (p->msg == "/help")
                        {
                            const char* commands[]
                                = { "/stop", "/smite", "/unload", "/overworld", "/nether", "/c", "/s", "/rain_on", "/rain_off", "/kill", NULL };
                            for (int i = 0; commands[i] != 0; i++)
                            {
                                packet_chat_message_t pack_msg;
                                pack_msg.msg = commands[i];
                                send_buffer(sock, pack_msg.assemble());
                            }
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
                case 0x07:
                {
                    break;
                }
                case 0x09:
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
                    pack_respawn.seed = world_seed;
                    pack_respawn.dimension = client->dimension;
                    pack_respawn.mode = client->player_mode;
                    pack_respawn.world_height = WORLD_HEIGHT;
                    send_buffer(sock, pack_respawn.assemble());

                    packet_time_update_t pack_time;
                    pack_time.time = sdl_tick_cur / 50;
                    send_buffer(sock, pack_time.assemble());

                    break;
                }
                case 0x0a:
                {
                    CAST_PACK_TO_P(packet_on_ground_t);

                    client->player_on_ground = p->on_ground;

                    break;
                }
                case 0x0b:
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
                case 0x0c:
                {
                    CAST_PACK_TO_P(packet_player_look_t);

                    client->player_yaw = p->yaw;
                    client->player_pitch = p->pitch;
                    client->player_on_ground = p->on_ground;

                    client->pos_updated = 1;

                    break;
                }
                case 0x0d:
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
                case 0x0e:
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
                            if (p->status == PLAYER_DIG_STATUS_FINISH_DIG || client->player_mode > 0)
                            {
                                TRACE("%d %d %d %d", p->x, p->y, p->z, c->get_type(p->x % 16, p->y, p->z % 16));
                                packet_sound_effect_t pack_break_sfx;
                                pack_break_sfx.effect_id = 2001;
                                pack_break_sfx.x = p->x;
                                pack_break_sfx.y = p->y;
                                pack_break_sfx.z = p->z;
                                pack_break_sfx.sound_data = c->get_type(p->x % 16, p->y, p->z % 16);
                                send_buffer_to_players(clients, pack_break_sfx.assemble());

                                packet_block_change_t pack_block_change;
                                pack_block_change.block_x = p->x;
                                pack_block_change.block_y = p->y;
                                pack_block_change.block_z = p->z;
                                pack_block_change.type = 0;
                                pack_block_change.metadata = 0;
                                c->set_type(p->x % 16, p->y, p->z % 16, 0);
                                c->set_metadata(p->x % 16, p->y, p->z % 16, 0);
                                c->set_light_sky(p->x % 16, p->y, p->z % 16, 15);
                                send_buffer_to_players(clients, pack_block_change.assemble());
                            }
                        }
                        else
                            LOG("Unable to place block");
                    }
                    else
                    {
                        LOG("Player \"%s\" sent dig with unsupported status of %d", client->username.c_str(), p->status);
                        goto loop_end;
                    }

                    break;
                }
                case 0x0f:
                {
                    CAST_PACK_TO_P(packet_player_place_t);
                    double x_diff = p->x - client->player_x;
                    double y_diff = p->y - client->player_y;
                    double z_diff = p->z - client->player_z;
                    double radius = SDL_sqrt(x_diff * x_diff + y_diff * y_diff + z_diff * z_diff);

                    TRACE("Place %d @ <%d, %d, %d>:%d (Radius: %.3f)", p->block_item_id, p->x, p->y, p->z, p->direction, radius);

                    int place_x = p->x;
                    int place_y = p->y;
                    int place_z = p->z;

                    if (p->direction != -1 && p->block_item_id >= 0 && p->block_item_id < 256)
                    {
                        switch (p->direction)
                        {
                        case 0:
                            place_y -= 1;
                            break;
                        case 1:
                            place_y += 1;
                            break;
                        case 2:
                            place_z -= 1;
                            break;
                        case 3:
                            place_z += 1;
                            break;
                        case 4:
                            place_x -= 1;
                            break;
                        case 5:
                            place_x += 1;
                            break;
                        default:
                            goto loop_end;
                            break;
                        }

                        if (place_y < 0)
                            goto loop_end;

                        double diff_y = (double)place_y - client->player_y;

                        if (SDL_floor(client->player_x) == place_x && (-0.9 < diff_y && diff_y < 1.8) && SDL_floor(client->player_z) == place_z)
                        {
                            p->block_item_id = 0;
                            p->damage = 0;
                        }

                        int cx = place_x >> 4;
                        int cz = place_z >> 4;

                        chunk_t* c = dimensions[client->dimension < 0].get_chunk(cx, cz);
                        if (c)
                        {
                            packet_block_change_t pack_block_change;
                            pack_block_change.block_x = place_x;
                            pack_block_change.block_y = place_y;
                            pack_block_change.block_z = place_z;
                            pack_block_change.type = p->block_item_id;
                            pack_block_change.metadata = p->damage;
                            c->set_type(place_x % 16, place_y, place_z % 16, p->block_item_id);
                            c->set_metadata(place_x % 16, place_y, place_z % 16, p->damage);
                            send_buffer_to_players(clients, pack_block_change.assemble());
                        }
                        else
                            LOG("Unable to place block");
                    }

                    break;
                }
                case 0x10:
                    break;
                case 0x12:
                {
                    for (size_t i = 0; i < clients.size(); i++)
                        if (clients[i].sock && clients[i].username.length() > 0 && clients[i].sock != sock)
                            send_buffer(clients[i].sock, pack->assemble());
                    break;
                }
                case 0x13:
                    break;
                case 0x65:
                    break;
                case 0x6b:
                    break;
                case 0xfe:
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
                case 0xff:
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
                delete pack;
            }
#undef CAST_PACK_TO_P

        loop_end:;
        }
    }

    LOG("Destroying server");
    for (size_t i = 0; i < clients.size(); i++)
    {
        if (clients[i].sock)
            SDLNet_DestroyStreamSocket(clients[i].sock);
    }
    SDLNet_DestroyServer(server);

    SDLNet_Quit();
    SDL_Quit();
}

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
#include "tetra/tetra.h"

#include <algorithm>

#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL3/SDL_opengl.h>

#include "tetra/gui/console.h"
#include "tetra/gui/gui_registrar.h"

#include "tetra/gui/imgui.h"

#include "tetra/util/convar.h"
#include "tetra/util/convar_file.h"
#include "tetra/util/misc.h"
#include "tetra/util/physfs/physfs.h"
#include "tetra/util/stbi.h"

#include "shared/chunk.h"
#include "shared/ids.h"
#include "shared/misc.h"
#include "shared/packet.h"

#include "sdl_net/include/SDL3_net/SDL_net.h"

#include "level.h"
#include "shaders.h"
#include "texture_terrain.h"

#include <zlib.h>

static convar_string_t cvr_username("username", "", "Username (duh)", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_assets("dir_assets", "", "Path to assets (ex: \"~/.minecraft/assets/\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_path_resource_pack(
    "path_base_resources", "", "File/Dir to use for base resources (ex: \"~/.minecraft/versions/1.6.4/1.6.4.jar\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_game("dir_game", "", "Path to store game files (Not mandatory)", CONVAR_FLAG_SAVE);

static convar_int_t cvr_autoconnect("dev_autoconnect", 0, 0, 1, "Auto connect to server", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static convar_string_t cvr_autoconnect_addr(
    "dev_server_addr", "localhost", "Address of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_autoconnect_port(
    "dev_server_port", 25565, 0, 65535, "Port of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);

static shader_t* shader = NULL;

static int ao_algorithm = 1;
static int use_texture = 1;
static bool take_screenshot = 0;

void compile_shaders()
{
    Uint64 tick_shader_start = SDL_GetTicksNS();
    shader->build();
    shader->set_uniform("ao_algorithm", ao_algorithm);
    shader->set_uniform("use_texture", use_texture);
    dc_log("Compiled shaders in %.2f ms", (SDL_GetTicksNS() - tick_shader_start) / 1000000.0);
}

static level_t* level;
static block_id_t block_hand = BLOCK_ID_STONE;
static Uint8 block_hand_meta = 0;

struct tentative_block_t
{
    Uint64 timestamp = 0;
    glm::ivec3 pos = { -1, -1, -1 };
    block_id_t old_type = BLOCK_ID_AIR;
    Uint8 old_meta = 0;
    bool fullfilled = 0;
};

std::vector<tentative_block_t> tentative_blocks;

struct connection_t
{
    enum connection_status_t
    {
        CONNECTION_UNSET,
        CONNECTION_RESOLVING,
        CONNECTION_RESOLVED,
        CONNECTION_CONNECTING,
        CONNECTION_ACTIVE,
        CONNECTION_DONE,
        CONNECTION_FAILED,
    } status
        = CONNECTION_UNSET;

    SDLNet_StreamSocket* socket = NULL;
    packet_handler_t pack_handler_client = packet_handler_t(false);
    std::string status_msg;
    std::string status_msg_sub;
    bool in_world = false;

    void set_status(std::string _status, std::string _sub_status = "")
    {
        status_msg = _status;
        status_msg_sub = _sub_status;
    }

    Uint16 port = 0;
    std::string addr;

    void init(std::string _addr, Uint16 _port)
    {
        addr = _addr;
        port = _port;

        set_status("Resolving address");
        addr_server = SDLNet_ResolveHostname(cvr_autoconnect_addr.get().c_str());
        status = CONNECTION_RESOLVING;
        if (!addr_server)
        {
            err_str = std::string("SDLNet_ResolveHostname: ") + SDL_GetError();
            status = CONNECTION_FAILED;
            return;
        }
    }

    SDLNet_Address* addr_server;
    std::string err_str;

    Uint64 start_time;

    /**
     * Steps (if possible) the state as fast a possible to CONNECTION_ACTIVE
     */
    void step_to_active()
    {
        if (status == CONNECTION_RESOLVING)
        {
            int address_status = SDLNet_GetAddressStatus(addr_server);

            if (address_status == 1)
                status = CONNECTION_RESOLVED;
            else if (address_status == -1)
            {
                err_str = std::string("SDLNet_WaitUntilResolved: ") + SDL_GetError();
                status = CONNECTION_FAILED;
            }
        }

        if (status == CONNECTION_RESOLVED)
        {
            socket = SDLNet_CreateClient(addr_server, port);
            SDLNet_UnrefAddress(addr_server);
            addr_server = NULL;

            status = CONNECTION_CONNECTING;

            if (!socket)
            {
                err_str = std::string("SDLNet_CreateClient: ") + SDL_GetError();
                status = CONNECTION_FAILED;
            }
        }

        if (status == CONNECTION_CONNECTING)
        {
            int connection_status = SDLNet_GetConnectionStatus(socket);

            if (connection_status == 1)
                status = CONNECTION_ACTIVE;
            else if (connection_status == -1)
            {
                err_str = std::string("SDLNet_GetConnectionStatus: ") + SDL_GetError();
                status = CONNECTION_FAILED;
            }
        }
    }

    ~connection_t()
    {
        SDLNet_UnrefAddress(addr_server);

        /* TODO: Store this in a vector of dying sockets to ensure things are properly closed down */
        SDLNet_DestroyStreamSocket(socket);
    }
};

static connection_t* connection = NULL;
static texture_terrain_t* texture_atlas = NULL;

static convar_int_t cvr_testworld("dev_world", 0, 0, 1, "Init to test world", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_world_size("dev_world_size", 6, 1, 32, "Side dimensions of the test world", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_world_y_off_pos("dev_world_y_off_pos", 0, 0, 32, "Positive Chunk Y offset of the test world", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_world_y_off_neg("dev_world_y_off_neg", 6, 0, 32, "Negative Chunk Y offset of the test world", CONVAR_FLAG_DEV_ONLY);
void create_testworld();
bool initialize_resources()
{
    /* In the future parsing of one of the indexes at /assets/indexes/ will need to happen here (For sound) */

    texture_atlas = new texture_terrain_t("/_resources/assets/minecraft/textures/");
    level = new level_t(texture_atlas);
    shader = new shader_t("/shaders/terrain.vert", "/shaders/terrain.frag");

    level->lightmap.set_world_time(1000);

    connection = new connection_t();

    if (cvr_autoconnect.get())
        connection->init(cvr_autoconnect_addr.get(), cvr_autoconnect_port.get());

    if (!cvr_autoconnect.get() || cvr_testworld.get())
        create_testworld();

    compile_shaders();

    return true;
}
void create_testworld()
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
            level->chunks.push_back(c);
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
        level->chunks.push_back(c);
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
        level->chunks.push_back(c);
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
}

bool deinitialize_resources()
{
    delete shader;
    delete level;
    delete texture_atlas;
    delete connection;
    texture_atlas = NULL;
    connection = NULL;
    shader = NULL;
    level = NULL;
    return true;
}

/**
 * Quick check to see if the game can be launched, intended for validating if the setup screen can be skipped
 */
bool can_launch_game()
{
    if (!cvr_username.get().length())
        return 0;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(cvr_dir_assets.get().c_str(), &info))
        return 0;

    if (info.type != SDL_PATHTYPE_DIRECTORY)
        return 0;

    SDL_GetPathInfo(cvr_dir_game.get().c_str(), &info);
    if (info.type != SDL_PATHTYPE_DIRECTORY && info.type != SDL_PATHTYPE_NONE)
        return 0;

    if (!SDL_GetPathInfo(cvr_path_resource_pack.get().c_str(), &info))
        return 0;

    return 1;
}

enum engine_state_t : int
{
    ENGINE_STATE_OFFLINE,
    ENGINE_STATE_CONFIGURE,
    ENGINE_STATE_INITIALIZE,
    ENGINE_STATE_RUNNING,
    ENGINE_STATE_SHUTDOWN,
    ENGINE_STATE_EXIT,
};

static engine_state_t engine_state_current = ENGINE_STATE_OFFLINE;
static engine_state_t engine_state_target = ENGINE_STATE_RUNNING;

#define ENUM_SWITCH_NAME(X) \
    case X:                 \
        return #X;
const char* engine_state_name(engine_state_t state)
{
    switch (state)
    {
        ENUM_SWITCH_NAME(ENGINE_STATE_OFFLINE)
        ENUM_SWITCH_NAME(ENGINE_STATE_CONFIGURE)
        ENUM_SWITCH_NAME(ENGINE_STATE_INITIALIZE)
        ENUM_SWITCH_NAME(ENGINE_STATE_RUNNING)
        ENUM_SWITCH_NAME(ENGINE_STATE_SHUTDOWN)
        ENUM_SWITCH_NAME(ENGINE_STATE_EXIT)
    }
    return "Unknown Engine State";
}

float yaw;
float pitch;

float delta_time;

bool held_w = 0;
bool held_a = 0;
bool held_s = 0;
bool held_d = 0;
bool held_space = 0;
bool held_shift = 0;
bool held_ctrl = 0;
bool mouse_grabbed = 0;
bool wireframe = 0;
#define AO_ALGO_MAX 5

#define FOV_MAX 77.0f
#define FOV_MIN 75.0f
float fov = FOV_MIN;

glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);

#pragma GCC push_options
#pragma GCC optimize("Ofast")
void decompress_chunk_packet(packet_chunk_t* p, std::vector<Uint8>& buffer)
{
    if (p->size_x < 0 || p->size_y < 0 || p->size_z < 0)
    {
        dc_log_error("Chunk packet with invalid size of <%d, %d, %d>", p->size_x, p->size_y, p->size_z);
        return;
    }

    const int min_block_x = p->block_x;
    const int min_block_y = p->block_y;
    const int min_block_z = p->block_z;
    const int max_block_x = min_block_x + p->size_x;
    const int max_block_y = min_block_y + p->size_y;
    const int max_block_z = min_block_z + p->size_z;

    const int min_chunk_x = min_block_x >> 4;
    const int min_chunk_y = min_block_y >> 4;
    const int min_chunk_z = min_block_z >> 4;
    const int max_chunk_x = max_block_x >> 4;
    const int max_chunk_y = max_block_y >> 4;
    const int max_chunk_z = max_block_z >> 4;

    const int real_size_x = p->size_x + 1;
    const int real_size_y = p->size_y + 1;
    const int real_size_z = p->size_z + 1;
    const int real_volume = (real_size_x * real_size_y * real_size_z);

    /* Oversize the buffer a small amount in case weirdness occurs  */
    size_t uncompressed_size = real_size_x * real_size_y * real_size_z * 41 / 16;
    if (uncompressed_size > buffer.size())
    {
        dc_log("Resizing decompression buffer to %zu", uncompressed_size);
        buffer.resize(uncompressed_size, 0);
    }
    Uint8* uncompressed = buffer.data();
    memset(uncompressed + uncompressed_size, 0, buffer.size() - uncompressed_size);

    int decompress_result = uncompress(uncompressed, &uncompressed_size, p->compressed_data.data(), p->compressed_data.size());

    dc_log("%d %d %d | %d %d %d", min_chunk_x, min_chunk_y, min_chunk_z, max_chunk_x, max_chunk_y, max_chunk_z);

    if (decompress_result != Z_OK)
    {
        const char* err_str = "Unknown error";
        switch (decompress_result)
        {
            ENUM_SWITCH_CASE(err_str, Z_OK);
            ENUM_SWITCH_CASE(err_str, Z_STREAM_END);
            ENUM_SWITCH_CASE(err_str, Z_NEED_DICT);
            ENUM_SWITCH_CASE(err_str, Z_ERRNO);
            ENUM_SWITCH_CASE(err_str, Z_STREAM_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_DATA_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_MEM_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_BUF_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_VERSION_ERROR);
        default:
            break;
        }
        dc_log_error("Error %d (%s) decompressing chunk data!", decompress_result, err_str);
    }

    // TODO: Create nonexistent chunks
    for (chunk_cubic_t* c : level->chunks)
    {
        if (!BETWEEN_INCL(c->pos.x, min_chunk_x, max_chunk_x))
            continue;
        if (!BETWEEN_INCL(c->pos.y, min_chunk_y, max_chunk_y))
            continue;
        if (!BETWEEN_INCL(c->pos.z, min_chunk_z, max_chunk_z))
            continue;

        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
        {
            const int block_x = x + ((c->pos.x) << 4);
            if (!BETWEEN_INCL(block_x, min_block_x, max_block_x))
                continue;
            const int uncompressed_x = block_x - min_block_x;

            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int block_z = z + ((c->pos.z) << 4);
                if (!BETWEEN_INCL(block_z, min_block_z, max_block_z))
                    continue;
                const int uncompressed_z = block_z - min_block_z;

                for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
                {
                    const int block_y = y + ((c->pos.y) << 4);
                    if (!BETWEEN_INCL(block_y, min_block_y, max_block_y))
                        continue;
                    const int uncompressed_y = block_y - min_block_y;

                    size_t index = uncompressed_y + (uncompressed_z * (real_size_y)) + (uncompressed_x * (real_size_y) * (real_size_z));
                    Uint8 type = uncompressed[index];

                    index += real_volume * 2;
                    Uint8 meta;
                    if (index % 2 == 1)
                        meta = (uncompressed[index / 2] >> 4) & 0x0F;
                    else
                        meta = uncompressed[index / 2] & 0x0F;

                    index += real_volume;
                    Uint8 light_block;
                    if (index % 2 == 1)
                        light_block = (uncompressed[index / 2] >> 4) & 0x0F;
                    else
                        light_block = uncompressed[index / 2] & 0x0F;

                    index += real_volume;
                    Uint8 light_sky;
                    if (index % 2 == 1)
                        light_sky = (uncompressed[index / 2] >> 4) & 0x0F;
                    else
                        light_sky = uncompressed[index / 2] & 0x0F;

                    c->set_type(x, y, z, type);
                    c->set_metadata(x, y, z, meta);
                    c->set_light_block(x, y, z, light_block);
                    c->set_light_sky(x, y, z, light_sky);
                }
            }
        }

        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
    }
}
#pragma GCC pop_options

void normal_loop()
{

    if (connection->status == connection_t::CONNECTION_ACTIVE)
    {
        static bool sent_init = false;
        static Uint64 last_update_tick_build = 0;
        static Uint64 last_update_tick_camera = 0;

        if (!sent_init)
        {
            packet_handshake_c2s_t pack_handshake;

            pack_handshake.username = cvr_username.get();

            sent_init = send_buffer(connection->socket, pack_handshake.assemble());
            connection->set_status("connect.connecting");
        }
        Uint64 sdl_start_tick = SDL_GetTicks();
        packet_t* pack_from_server = NULL;
        while (SDL_GetTicks() - sdl_start_tick < (connection->in_world ? 25 : 150) && connection->status == connection_t::CONNECTION_ACTIVE
            && (pack_from_server = connection->pack_handler_client.get_next_packet(connection->socket)))
        {
#define CAST_PACK_TO_P(type) type* p = (type*)pack_from_server
            Uint8 pack_type = pack_from_server->id;
            switch (pack_type)
            {
            case PACKET_ID_KEEP_ALIVE:
            {
                send_buffer(connection->socket, pack_from_server->assemble());
                break;
            }
            case PACKET_ID_HANDSHAKE:
            {
                packet_login_request_c2s_t login_request;

                /* The idea is to maybe implement spectator mode and maybe 1.3+ style plugin channels */
                const char ext_magic[] = "B181_EXT";
                const int ext_ver = 0;
                static_assert(sizeof(ext_magic) == 9, "Client id must be 8 characters (excluding terminator)");

                login_request.unused0 = SDL_Swap64LE(*(jlong*)ext_magic);
                login_request.unused1 = ext_ver;
                login_request.protocol_ver = 17;
                login_request.username = cvr_username.get();

                send_buffer(connection->socket, login_request.assemble());
                connection->set_status("connect.authorizing");
                break;
            }
            case PACKET_ID_LOGIN_REQUEST:
            {
                CAST_PACK_TO_P(packet_login_request_s2c_t);

                level->world_height = p->world_height;
                if (p->dimension == -1)
                    level->lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_NETHER);
                else
                    level->lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_OVERWORLD);

                connection->set_status("multiplayer.downloadingTerrain");

                break;
            }
            case PACKET_ID_UPDATE_TIME:
            {
                CAST_PACK_TO_P(packet_time_update_t);
                level->lightmap.set_world_time(p->time);
                break;
            }
            case PACKET_ID_PLAYER_LOOK:
            {
                CAST_PACK_TO_P(packet_player_look_t);

                pitch = SDL_clamp(-p->pitch, -89.0f, 89.0f);
                yaw = p->yaw + 90.0f;
                last_update_tick_camera = 0;

                break;
            }
            case PACKET_ID_PLAYER_POS:
            {
                CAST_PACK_TO_P(packet_player_pos_t);

                level->camera_pos = { p->x, p->y, p->z };
                last_update_tick_camera = 0;

                break;
            }
            case PACKET_ID_PLAYER_POS_LOOK:
            {
                CAST_PACK_TO_P(packet_player_pos_look_s2c_t);

                level->camera_pos = { p->x, p->y, p->z };
                pitch = SDL_clamp(-p->pitch, -89.0f, 89.0f);
                yaw = p->yaw + 90.0f;

                connection->in_world = true;
                connection->set_status("");
                last_update_tick_camera = 0;

                break;
            }
            case PACKET_ID_CHUNK_CACHE:
            {
                CAST_PACK_TO_P(packet_chunk_cache_t);

                int max_cy = (level->world_height + SUBCHUNK_SIZE_Y - 1) / SUBCHUNK_SIZE_Y;

                if (!p->mode)
                {

                    for (std::vector<chunk_cubic_t*>::iterator it = level->chunks.begin(); it != level->chunks.end();)
                    {
                        chunk_cubic_t* c = *it;
                        if (c->pos.x == p->chunk_x && c->pos.z == p->chunk_z && 0 <= c->pos.y && c->pos.y < max_cy)
                            it = level->chunks.erase(it);
                        else
                            it = std::next(it);
                    }
                }
                else
                {
                    std::vector<int> exists;
                    exists.resize(max_cy);
                    for (chunk_cubic_t* c : level->chunks)
                        if (c->pos.x == p->chunk_x && c->pos.z == p->chunk_z && 0 <= c->pos.y && c->pos.y < max_cy)
                            exists[c->pos.y] = 1;

                    for (int i = 0; i < max_cy; i++)
                    {
                        if (exists[i])
                            continue;
                        chunk_cubic_t* c = new chunk_cubic_t();
                        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
                        c->pos.x = p->chunk_x;
                        c->pos.y = i;
                        c->pos.z = p->chunk_z;
                        level->chunks.push_back(c);
                    }
                }
                break;
            }
            case PACKET_ID_BLOCK_CHANGE:
            {
                CAST_PACK_TO_P(packet_block_change_t);

                glm::ivec3 block_pos = { p->block_x, p->block_y, p->block_z };
                level->set_block(block_pos, p->type, p->metadata);

                /* Mark as fulfilled to delay erasing until after packet handling is finished */
                for (tentative_block_t& it : tentative_blocks)
                {
                    if (it.fullfilled || it.pos != block_pos)
                        continue;
                    it.fullfilled = 1;
                    break;
                }
                break;
            }
            case PACKET_ID_BLOCK_CHANGE_MULTI:
            {
                CAST_PACK_TO_P(packet_block_change_multi_t);

                for (block_change_dat_t b : p->payload)
                {
                    glm::ivec3 block_pos = { p->chunk_x * CHUNK_SIZE_X + b.x, b.y, p->chunk_z * CHUNK_SIZE_Z + b.z };
                    level->set_block(block_pos, b.type, b.metadata);

                    /* Mark as fulfilled to delay erasing until after packet handling is finished */
                    for (tentative_block_t& it : tentative_blocks)
                    {
                        if (it.fullfilled || it.pos != block_pos)
                            continue;
                        it.fullfilled = 1;
                    }
                }
                break;
            }
            case PACKET_ID_CHUNK_MAP:
            {
                CAST_PACK_TO_P(packet_chunk_t);
                static std::vector<Uint8> buf;
                decompress_chunk_packet(p, buf);

                /* Mark as fulfilled to delay erasing until after packet handling is finished */
                for (tentative_block_t& it : tentative_blocks)
                {
                    if (it.fullfilled)
                        continue;
                    if (!BETWEEN_INCL(it.pos.x, p->block_x, p->block_x + p->size_x))
                        continue;
                    if (!BETWEEN_INCL(it.pos.y, p->block_y, p->block_y + p->size_y))
                        continue;
                    if (!BETWEEN_INCL(it.pos.z, p->block_z, p->block_z + p->size_z))
                        continue;
                    it.fullfilled = 1;
                }

                break;
            }

            case PACKET_ID_CHAT_MSG:
            {
                CAST_PACK_TO_P(packet_chat_message_t);
                dc_log("[CHAT]: %s", p->msg.c_str());
                break;
            }

            case PACKET_ID_KICK:
            {
                CAST_PACK_TO_P(packet_kick_t);
                connection->set_status("disconnect.lost", p->reason);
                break;
            }
            case PACKET_ID_PLAYER_LIST_ITEM:
            case PACKET_ID_ENT_VELOCITY:
            case PACKET_ID_ENT_DESTROY:
            case PACKET_ID_ENT_ENSURE_SPAWN:
            case PACKET_ID_ENT_MOVE_REL:
            case PACKET_ID_ENT_LOOK:
            case PACKET_ID_ENT_LOOK_MOVE_REL:
            case PACKET_ID_ENT_MOVE_TELEPORT:
                break;
            default:
                dc_log_error("Unknown packet from server with id: 0x%02x", pack_type);
                break;
            }
            connection->pack_handler_client.free_packet(pack_from_server);
        }

        if (connection->in_world)
        {
            if (SDL_GetTicks() - last_update_tick_camera > 50)
            {
                packet_player_pos_look_c2s_t location_response;
                location_response.x = level->camera_pos.x;
                location_response.y = level->camera_pos.y;
                location_response.stance = level->camera_pos.y + 1.0f;
                location_response.z = level->camera_pos.z;

                location_response.pitch = -pitch;
                location_response.yaw = yaw - 90.0f;

                send_buffer(connection->socket, location_response.assemble());
                last_update_tick_camera = SDL_GetTicks();
            }
            if (SDL_GetTicks() - last_update_tick_build > 50)
            {
                level->build_dirty_meshes();
                last_update_tick_build = SDL_GetTicks();
            }
        }
    }

    level->lightmap.update();

    level->get_terrain()->update();

    const Uint64 time_tentative = SDL_GetTicks();
    for (std::vector<tentative_block_t>::iterator it = tentative_blocks.begin(); it != tentative_blocks.end();)
    {
        if (time_tentative - it->timestamp < 5000)
            it = next(it);
        else
        {
            if (!it->fullfilled)
                level->set_block(it->pos, it->old_type, it->old_meta);
            it = tentative_blocks.erase(it);
        }
    }

    if (SDL_GetWindowMouseGrab(tetra::window) != mouse_grabbed)
        SDL_SetWindowMouseGrab(tetra::window, mouse_grabbed);

    if (SDL_GetWindowRelativeMouseMode(tetra::window) != mouse_grabbed)
        SDL_SetWindowRelativeMouseMode(tetra::window, mouse_grabbed);

    if ((ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) && tetra::imgui_ctx_main_wants_input())
        mouse_grabbed = 0;

    if (!mouse_grabbed)
    {
        held_w = 0;
        held_a = 0;
        held_s = 0;
        held_d = 0;
    }

    float camera_speed = 3.5f * delta_time * (held_ctrl ? 4.0f : 1.0f);

    if (held_w)
        level->camera_pos += camera_speed * glm::vec3(SDL_cosf(glm::radians(yaw)), 0, SDL_sinf(glm::radians(yaw)));
    if (held_s)
        level->camera_pos -= camera_speed * glm::vec3(SDL_cosf(glm::radians(yaw)), 0, SDL_sinf(glm::radians(yaw)));
    if (held_a)
        level->camera_pos -= camera_speed * glm::vec3(-SDL_sinf(glm::radians(yaw)), 0, SDL_cosf(glm::radians(yaw)));
    if (held_d)
        level->camera_pos += camera_speed * glm::vec3(-SDL_sinf(glm::radians(yaw)), 0, SDL_cosf(glm::radians(yaw)));
    if (held_space)
        level->camera_pos.y += camera_speed;
    if (held_shift)
        level->camera_pos.y -= camera_speed;

    if (held_ctrl)
        fov += delta_time * 30.0f;
    else
        fov -= delta_time * 30.0f;

    if (fov > FOV_MAX)
        fov = FOV_MAX;
    else if (fov < FOV_MIN)
        fov = FOV_MIN;

    glUseProgram(shader->id);

    int win_width, win_height;
    SDL_GetWindowSize(tetra::window, &win_width, &win_height);
    glm::mat4 mat_proj = glm::perspective(glm::radians(fov), (float)win_width / (float)win_height, 0.03125f, 256.0f);
    shader->set_projection(mat_proj);

    glm::vec3 direction;
    direction.x = SDL_cosf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
    direction.y = SDL_sinf(glm::radians(pitch));
    direction.z = SDL_sinf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
    camera_front = glm::normalize(direction);

    glm::mat4 mat_cam = glm::lookAt(level->camera_pos, level->camera_pos + camera_front, camera_up);
    shader->set_camera(mat_cam);

    shader->set_model(glm::mat4(1.0f));
    shader->set_uniform("tex_atlas", 0);
    shader->set_uniform("tex_lightmap", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, level->get_terrain()->tex_id_main);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, level->lightmap.tex_id_linear);
    glActiveTexture(GL_TEXTURE0);

    glm::dvec3 c2pos(glm::ivec3(level->camera_pos) >> 3);
    std::sort(level->chunks.begin(), level->chunks.end(), [=](chunk_cubic_t* a, chunk_cubic_t* b) {
        float adist = glm::distance(glm::dvec3(a->pos * 2 + glm::ivec3(1, 1, 1)), c2pos);
        float bdist = glm::distance(glm::dvec3(b->pos * 2 + glm::ivec3(1, 1, 1)), c2pos);
        return adist > bdist;
    });

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, level->get_terrain()->tex_id_main);
    for (chunk_cubic_t* c : level->chunks)
    {
        if (c->index_type == GL_NONE || c->index_count == 0)
            continue;
        glBindVertexArray(c->vao);
        glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
        shader->set_model(glm::translate(glm::mat4(1.0f), translate));
        glDrawElements(GL_TRIANGLES, c->index_count, c->index_type, 0);
    }

    for (chunk_cubic_t* c : level->chunks)
    {
        if (c->index_type == GL_NONE || c->index_count_translucent == 0)
            continue;
        assert(c->index_type == GL_UNSIGNED_INT);
        glBindVertexArray(c->vao);
        glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
        shader->set_model(glm::translate(glm::mat4(1.0f), translate));
        glDrawElements(GL_TRIANGLES, c->index_count_translucent, c->index_type, (void*)(c->index_count * 4));
    }
}

void process_event(SDL_Event& event, bool* done)
{
    if (tetra::process_event(event))
        *done = true;

    if (event.type == SDL_EVENT_QUIT)
        *done = true;

    if (event.window.windowID != SDL_GetWindowID(tetra::window))
        return;

    switch (event.window.type)
    {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        *done = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        mouse_grabbed = false;
        break;
    default:
        break;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.key.scancode == SDL_SCANCODE_F2 && !event.key.repeat)
        take_screenshot = 1;

    if (tetra::imgui_ctx_main_wants_input())
        return;

    static Uint64 last_button_down = -1;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        last_button_down = event.common.timestamp;

        if (mouse_grabbed)
        {
            // TODO: Add place block function
            glm::vec3 cam_dir;
            cam_dir.x = SDL_cosf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
            cam_dir.y = SDL_sinf(glm::radians(pitch));
            cam_dir.z = SDL_sinf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
            glm::ivec3 cam_pos = level->camera_pos + glm::normalize(cam_dir) * 2.5f;

            if (event.button.button == 1)
            {
                tentative_block_t t;
                t.timestamp = SDL_GetTicks();
                t.pos = cam_pos;
                if (level->get_block(t.pos, t.old_type, t.old_meta) && t.old_type != BLOCK_ID_AIR)
                {
                    tentative_blocks.push_back(t);
                    level->set_block(t.pos, BLOCK_ID_AIR, 0);

                    packet_player_dig_t p;
                    p.status = PLAYER_DIG_STATUS_FINISH_DIG;
                    p.x = t.pos.x;
                    p.y = t.pos.y - 1;
                    p.z = t.pos.z;
                    p.face = 1;
                    send_buffer(connection->socket, p.assemble());
                }
            }
            if (event.button.button == 2)
            {
                // TODO: Pick block
            }
            else if (event.button.button == 3)
            {
                tentative_block_t t;
                t.timestamp = SDL_GetTicks();
                t.pos = cam_pos;
                if (level->get_block(t.pos, t.old_type, t.old_meta) && (t.old_type != block_hand || t.old_meta != block_hand_meta))
                {
                    tentative_blocks.push_back(t);
                    level->set_block(t.pos, block_hand, block_hand_meta);

                    packet_player_place_t p;
                    p.x = t.pos.x;
                    p.y = t.pos.y - 1;
                    p.z = t.pos.z;
                    p.direction = 1;
                    p.block_item_id = block_hand;
                    p.amount = 0;
                    p.damage = block_hand_meta;
                    send_buffer(connection->socket, p.assemble());
                }
            }
        }
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        if (event.common.timestamp > last_button_down && event.common.timestamp - last_button_down < (250 * 1000 * 1000))
            mouse_grabbed = true;

    if (mouse_grabbed && event.type == SDL_EVENT_MOUSE_MOTION)
    {
        const float sensitivity = 0.1f;

        if (event.motion.yrel != 0.0f)
        {
            pitch -= event.motion.yrel * sensitivity;
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }

        if (event.motion.xrel != 0.0f)
            yaw = SDL_fmodf(yaw + event.motion.xrel * sensitivity, 360.0f);
    }

    if (event.type == SDL_EVENT_KEY_DOWN)
        switch (event.key.scancode)
        {
        case SDL_SCANCODE_END:
            *done = 1;
            break;
        case SDL_SCANCODE_W:
            held_w = 1;
            break;
        case SDL_SCANCODE_S:
            held_s = 1;
            break;
        case SDL_SCANCODE_A:
            held_a = 1;
            break;
        case SDL_SCANCODE_D:
            held_d = 1;
            break;
        case SDL_SCANCODE_SPACE:
            held_space = 1;
            break;
        case SDL_SCANCODE_LSHIFT:
            held_shift = 1;
            break;
        case SDL_SCANCODE_LCTRL:
            held_ctrl = 1;
            break;
        case SDL_SCANCODE_B:
        {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            break;
        }
        case SDL_SCANCODE_P:
        {
            glm::ivec3 chunk_coords = glm::ivec3(level->camera_pos) >> 4;
            for (chunk_cubic_t* c : level->chunks)
            {
                if (chunk_coords != c->pos)
                    continue;
                c->free_gl();
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
            }
            break;
        }
        case SDL_SCANCODE_N:
        {
            for (chunk_cubic_t* c : level->chunks)
            {
                glm::ivec3 chunk_coords = glm::ivec3(level->camera_pos) >> 4;
                if (chunk_coords != c->pos)
                    continue;
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
            }
            break;
        }
        case SDL_SCANCODE_1:
        {
            block_hand = BLOCK_ID_STONE;
            break;
        }
        case SDL_SCANCODE_2:
        {
            block_hand = BLOCK_ID_COBBLESTONE;
            break;
        }
        case SDL_SCANCODE_3:
        {
            block_hand = BLOCK_ID_TORCH;
            break;
        }
        case SDL_SCANCODE_M:
        {
            glm::ivec3 chunk_coords = glm::ivec3(level->camera_pos) >> 4;
            for (chunk_cubic_t* c : level->chunks)
            {
                if (SDL_abs(chunk_coords.x - c->pos.x) > 1 || SDL_abs(chunk_coords.y - c->pos.y) > 1 || SDL_abs(chunk_coords.z - c->pos.z) > 1)
                    continue;
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
            }
            break;
        }
        case SDL_SCANCODE_C:
        {
            ao_algorithm = (ao_algorithm + 1) % (AO_ALGO_MAX + 1);
            dc_log("Setting ao_algorithm to %d", ao_algorithm);
            shader->set_uniform("ao_algorithm", ao_algorithm);
            break;
        }
        case SDL_SCANCODE_X:
        {
            use_texture = !use_texture;
            dc_log("Setting use_texture to %d", use_texture);
            shader->set_uniform("use_texture", use_texture);
            break;
        }
        case SDL_SCANCODE_R:
        {
            compile_shaders();
            break;
        }
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_GRAVE:
            mouse_grabbed = false;
            break;
        default:
            break;
        }

    if (event.type == SDL_EVENT_KEY_UP)
        switch (event.key.scancode)
        {
        case SDL_SCANCODE_W:
            held_w = 0;
            break;
        case SDL_SCANCODE_S:
            held_s = 0;
            break;
        case SDL_SCANCODE_A:
            held_a = 0;
            break;
        case SDL_SCANCODE_D:
            held_d = 0;
            break;
        case SDL_SCANCODE_SPACE:
            held_space = 0;
            break;
        case SDL_SCANCODE_LSHIFT:
            held_shift = 0;
            break;
        case SDL_SCANCODE_LCTRL:
            held_ctrl = 0;
            break;
        default:
            break;
        }
}

/* This isn't the best way to do this, but it will do for now */
static bool render_water_overlay()
{
    if (!level)
        return false;

    glm::ivec3 cam_pos = level->camera_pos;
    glm::ivec3 chunk_coords = cam_pos >> 4;

    bool found_water = false;
    for (chunk_cubic_t* c : level->chunks)
    {
        if (!c || chunk_coords != c->pos)
            continue;

        block_id_t type = c->get_type(cam_pos.x & 0x0F, cam_pos.y & 0x0F, cam_pos.z & 0x0F);
        found_water = (type == BLOCK_ID_WATER_FLOWING) || (type == BLOCK_ID_WATER_SOURCE);
    }
    if (!found_water)
        return false;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Water", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground))
    {
        mc_id::terrain_face_t face = level->get_terrain()->get_face(mc_id::FACE_WATER_STILL);
        ImVec2 uv0(face.corners[0].x, face.corners[0].y);
        ImVec2 uv1(face.corners[3].x, face.corners[3].y);
        ImGui::Image((void*)(size_t)level->get_terrain()->tex_id_main, ImGui::GetMainViewport()->WorkSize, uv0, uv1);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    return true;
}

static gui_register_overlay reg_render_water_overlay(render_water_overlay);

static bool render_status_msg()
{
    if (!connection)
        return false;

    if (!connection->status_msg.length())
        return false;

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->GetWorkCenter().x, 0), ImGuiCond_Always, ImVec2(0.5f, 0));

    ImVec2 size0 = ImGui::CalcTextSize(connection->status_msg.c_str());
    ImVec2 size1 = ImGui::CalcTextSize(connection->status_msg_sub.c_str());

    if (connection->status_msg_sub.length())
        size1.y += ImGui::GetStyle().ItemSpacing.y * 2.0f;

    ImVec2 win_size = ImVec2(SDL_max(size0.x, size1.x) + 10.0f, size0.y + size1.y) + ImGui::GetStyle().WindowPadding * 1.05f;

    ImGui::SetNextWindowSize(win_size, ImGuiCond_Always);

    if (ImGui::Begin("Status MSG", NULL, ImGuiWindowFlags_NoDecoration))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(0, 0));
        ImGui::SeparatorText(connection->status_msg.c_str());
        if (connection->status_msg_sub.length())
            ImGui::SeparatorText(connection->status_msg_sub.c_str());
        ImGui::PopStyleVar(3);
    }

    ImGui::End();

    return true;
}

static gui_register_overlay reg_render_status_msg(render_status_msg);

static convar_int_t cvr_gui_renderer("gui_renderer", 0, 0, 1, "Show renderer internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_lightmap("gui_lightmap", 0, 0, 1, "Show lightmap internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_engine_state("gui_engine_state", 0, 0, 1, "Show engine state menu", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

bool engine_state_step()
{
    /* Ensure engine only steps forwards */
    SDL_assert(engine_state_current <= engine_state_target);
    if (!(engine_state_current < engine_state_target))
        return 0;

    switch (engine_state_current)
    {
    case ENGINE_STATE_OFFLINE:
    {
        if (can_launch_game() && engine_state_target > ENGINE_STATE_CONFIGURE)
        {
            engine_state_current = ENGINE_STATE_INITIALIZE;
            dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
            return engine_state_step();
        }
        else
        {
            engine_state_current = ENGINE_STATE_CONFIGURE;
            dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
            return 0;
        }
    }
    case ENGINE_STATE_CONFIGURE:
    {
        if (engine_state_target == ENGINE_STATE_EXIT)
            engine_state_current = ENGINE_STATE_EXIT;
        return 0;
    }
    case ENGINE_STATE_INITIALIZE:
    {
        PHYSFS_mkdir("/game");
        if (cvr_dir_game.get().length())
            PHYSFS_mount(cvr_dir_game.get().c_str(), "/game", 0);
        if (!PHYSFS_mount(cvr_dir_assets.get().c_str(), "/assets", 0))
            util::die("Unable to mount assets");
        if (!PHYSFS_mount(cvr_path_resource_pack.get().c_str(), "/_resources/", 0))
            util::die("Unable to mount base resource pack");

        initialize_resources();
        engine_state_current = ENGINE_STATE_RUNNING;
        dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
        return engine_state_step();
    }
    case ENGINE_STATE_RUNNING:
        engine_state_current = ENGINE_STATE_SHUTDOWN;
        return engine_state_step();
    case ENGINE_STATE_SHUTDOWN:
        deinitialize_resources();
        engine_state_current = ENGINE_STATE_EXIT;
        dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
        return engine_state_step();
    case ENGINE_STATE_EXIT:
        return 0;
    }

    SDL_assert(0);
    return 0;
}

static bool engine_state_menu()
{
    if (!cvr_gui_engine_state.get())
        return false;

    ImGui::BeginCVR("Engine State Viewer/Manipulator", &cvr_gui_engine_state);

    if (ImGui::BeginTable("Engine State Table", 3))
    {
        float char_width = ImGui::CalcTextSize("ABCDEF").x / 6.0f;
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, char_width * 15.0f);
        ImGui::TableSetupColumn("State Name", ImGuiTableColumnFlags_WidthFixed, char_width * 25.0f);
        ImGui::TableSetupColumn("Manipulate", ImGuiTableColumnFlags_WidthFixed, char_width * 18.0f);

        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Current state:");
        ImGui::TableNextColumn();
        ImGui::Text("%s", engine_state_name(engine_state_current));
        ImGui::TableNextColumn();
        ImGui::InputInt("##Current state", (int*)&engine_state_current);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Target state:");
        ImGui::TableNextColumn();
        ImGui::Text("%s", engine_state_name(engine_state_target));
        ImGui::TableNextColumn();
        ImGui::InputInt("##Target state", (int*)&engine_state_target);

        ImGui::EndTable();
    }
    ImGui::End();

    return cvr_gui_engine_state.get();
}

static gui_register_menu register_engine_state_menu(engine_state_menu);

static int win_width, win_height;
void screenshot_callback()
{
    if (!take_screenshot)
        return;
    take_screenshot = 0;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    Uint8* buf = (Uint8*)malloc(win_width * win_height * 3);
    glReadPixels(0, 0, win_width, win_height, GL_RGB, GL_UNSIGNED_BYTE, buf);

    SDL_Time cur_time;
    SDL_GetCurrentTime(&cur_time);

    SDL_DateTime dt;
    SDL_TimeToDateTime(cur_time, &dt, 1);

    char buf_path[256];
    snprintf(buf_path, SDL_arraysize(buf_path), "screenshots/Screenshot_%04d-%02d-%02d_%02d.%02d.%02d.%02d.png", dt.year, dt.month, dt.day, dt.hour, dt.minute,
        dt.second, dt.nanosecond / 10000000);

    stbi_flip_vertically_on_write(true);

    if (PHYSFS_mkdir("screenshots") && !PHYSFS_exists(buf_path))
        if (stbi_physfs_write_png(buf_path, win_width, win_height, 3, buf, win_width * 3))
            dc_log("Saved screenshot to %s", buf_path);

    free(buf);
}

int main(const int argc, const char** argv)
{
    tetra::init("icrashstuff", "mcs_b181", "mcs_b181_client", argc, argv);

    if (!cvr_username.get().length())
    {
        cvr_username.set_default(std::string("Player") + std::to_string(SDL_rand_bits() % 65536));
        cvr_username.set(cvr_username.get_default());
    }

    if (!SDLNet_Init())
        util::die("SDLNet_Init: %s", SDL_GetError());

    tetra::set_render_api(tetra::RENDER_API_GL_CORE, 3, 3);

    tetra::init_gui("mcs_b181_client");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int done = 0;
    Uint64 last_loop_time = 0;
    delta_time = 0;
    while (!done)
    {
        SDL_Event event;
        bool should_cleanup = false;
        while (SDL_PollEvent(&event))
            process_event(event, &should_cleanup);
        if (should_cleanup)
            engine_state_target = ENGINE_STATE_EXIT;
        tetra::start_frame(false);
        Uint64 loop_start_time = SDL_GetTicksNS();
        delta_time = (double)last_loop_time / 1000000000.0;
        SDL_GetWindowSize(tetra::window, &win_width, &win_height);
        glViewport(0, 0, win_width, win_height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        engine_state_step();

        tetra::show_imgui_ctx_main(engine_state_current != ENGINE_STATE_RUNNING);

        if (tetra::imgui_ctx_main_wants_input())
            mouse_grabbed = 0;

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        switch (engine_state_current)
        {
        case ENGINE_STATE_OFFLINE:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Offline", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Hmm...\nYou should not be here");
            ImGui::End();
            break;
        }
        case ENGINE_STATE_CONFIGURE:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Configuration Required!", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

            float width = viewport->WorkSize.x * 0.75f;

            float width_new = ImGui::CalcTextSize(cvr_username.get().c_str()).x;
            if (width < width_new)
                width = width_new;
            width_new = ImGui::CalcTextSize(cvr_dir_assets.get().c_str()).x;
            if (width < width_new)
                width = width_new;
            width_new = ImGui::CalcTextSize(cvr_path_resource_pack.get().c_str()).x;
            if (width < width_new)
                width = width_new;
            width_new = ImGui::CalcTextSize(cvr_dir_game.get().c_str()).x;
            if (width < width_new)
                width = width_new;

            if (width > viewport->WorkSize.x)
                width = viewport->WorkSize.x;

            width *= 0.6f;

            ImGui::SetNextItemWidth(width);
            cvr_username.imgui_edit();
            ImGui::SetNextItemWidth(width);
            cvr_dir_assets.imgui_edit();
            ImGui::SetNextItemWidth(width);
            cvr_path_resource_pack.imgui_edit();
            ImGui::SetNextItemWidth(width);
            cvr_dir_game.imgui_edit();

            if (ImGui::Button("ENGAGE!"))
            {
                convar_file_parser::write();
                engine_state_current = ENGINE_STATE_OFFLINE;
            }
            ImGui::End();
            break;
        }
        case ENGINE_STATE_INITIALIZE:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Initializing", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Initializing! (Hello)");
            ImGui::End();
            break;
        }
        case ENGINE_STATE_RUNNING:
        {
            if (cvr_gui_renderer.get())
            {
                ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5, 0.5));
                ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Running", &cvr_gui_renderer);

                ImGui::Text("Camera: <%.1f, %.1f, %.1f>", level->camera_pos.x, level->camera_pos.y, level->camera_pos.z);

                ImGui::SliderFloat("Camera Pitch", &pitch, -89.0f, 89.0f);
                ImGui::SliderFloat("Camera Yaw", &yaw, 0.0f, 360.0f);
                ImGui::InputFloat("Camera X", &level->camera_pos.x, 1.0f);
                ImGui::InputFloat("Camera Y", &level->camera_pos.y, 1.0f);
                ImGui::InputFloat("Camera Z", &level->camera_pos.z, 1.0f);

                if (ImGui::Button("Rebuild atlas & meshes"))
                {
                    delete texture_atlas;
                    texture_atlas = new texture_terrain_t("/_resources/assets/minecraft/textures/");
                    level->set_terrain(texture_atlas);
                }
                ImGui::SameLine();
                if (ImGui::Button("Rebuild meshes"))
                    level->rebuild_meshes();
                ImGui::SameLine();
                if (ImGui::Button("Clear meshes"))
                    level->clear_mesh(false);

                if (ImGui::Button("Rebuild shaders"))
                    compile_shaders();

                level->get_terrain()->imgui_view();

                ImGui::End();
            }

            if (cvr_gui_lightmap.get())
            {
                ImGui::SetNextWindowPos(viewport->Size * ImVec2(0.0075f, 0.1875f), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.0));
                ImGui::SetNextWindowSize(viewport->Size * ImVec2(0.425f, 0.8f), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Lightmap", &cvr_gui_lightmap);
                level->lightmap.imgui_contents();
                ImGui::End();
            }

            connection->step_to_active();

            normal_loop();

            break;
        }
        case ENGINE_STATE_SHUTDOWN:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Shutdown", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Shutting down! (Goodbye)");
            ImGui::End();
            break;
        }
        case ENGINE_STATE_EXIT:
            done = 1;
            break;
        default:
            dc_log_fatal("engine_state_current set to %d (%s)", engine_state_current, engine_state_name(engine_state_current));
            done = 1;
            break;
        }

        tetra::end_frame(0, screenshot_callback);
        last_loop_time = SDL_GetTicksNS() - loop_start_time;
    }

    tetra::deinit();
    SDLNet_Quit();
    SDL_Quit();
}

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

#include "java_strings.h"
#include "misc.h"
#include "packet.h"

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
    return send_buffer(sock, packet.assemble());
}

/* A 16 * WORLD_HEIGHT * 16 chunk */
class chunk_t
{
public:
    chunk_t() { data.resize(CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 5 / 2, 0); }

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

    inline void set_type(int x, int y, int z, Uint8 type)
    {
        int index = y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z));

        if (type <= BLOCK_ID_MAX)
            data[index] = type;
        else
            data[index] = 0;
    }

    inline void set_metadata(int x, int y, int z, Uint8 metadata)
    {
        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 2;

        if (index % 2 == 0)
            data[index / 2] |= (metadata & 0x0F) << 4;
        else
            data[index / 2] |= metadata & 0x0F;
    }

    inline void set_light_block(int x, int y, int z, Uint8 level)
    {
        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 3;

        if (index % 2 == 0)
            data[index / 2] |= (level & 0x0F) << 4;
        else
            data[index / 2] |= level & 0x0F;
    }

    inline void set_light_sky(int x, int y, int z, Uint8 level)
    {
        int index = (y + (z * (CHUNK_SIZE_Y)) + (x * (CHUNK_SIZE_Y) * (CHUNK_SIZE_Z))) + CHUNK_SIZE_Y * CHUNK_SIZE_Z * CHUNK_SIZE_X * 4;

        if (index % 2 == 0)
            data[index / 2] = (level & 0x0F) << 4;
        else
            data[index / 2] = level & 0x0F;
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
    std::vector<Uint8> data;
};

bool send_chunk(SDLNet_StreamSocket* sock, int chunk_x, int chunk_z, int max_y)
{
    packet_chunk_t packet;
    packet.block_x = chunk_x * CHUNK_SIZE_X;
    packet.block_y = 0;
    packet.block_z = chunk_z * CHUNK_SIZE_Y;
    packet.size_x = CHUNK_SIZE_X - 1;
    packet.size_y = CHUNK_SIZE_Y - 1;
    packet.size_z = CHUNK_SIZE_Z - 1;

    int h = packet.size_y + 1;
    int sx = packet.size_x + 1;
    int sz = packet.size_z + 1;

    chunk_t c;

    if (max_y >= BLOCK_ID_MAX)
        c.generate_special_metadata();
    else
        c.generate_special_ascending_type(max_y);

    c.compress_to_buf(packet.compressed_data);

    send_prechunk(sock, chunk_x, chunk_z, 1);
    send_prechunk(sock, chunk_x, chunk_z, 1);

    return send_buffer(sock, packet.assemble());
}

struct client_t
{
    SDLNet_StreamSocket* sock = NULL;
    packet_buffer_t packet;

    Uint64 time_keep_alive_sent = 0;
    Uint64 time_keep_alive_recv = 0;

    std::string username;
    int counter = 0;
    double player_x = 0.0;
    double player_y = 0.0;
    double player_stance = 0.0;
    double player_z = 0.0;
    float player_yaw = 0.0;
    float player_pitch = 0.0;
    Uint8 player_on_ground = 0;
    int dimension = 0;
    int player_mode = 1;

    bool is_raining = 0;
};

void send_buffer_to_players(std::vector<client_t> clients, std::vector<Uint8> buf)
{
    for (size_t i = 0; i < clients.size(); i++)
        if (clients[i].sock && clients[i].username.length() > 0)
            send_buffer(clients[i].sock, buf);
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

    bool done = false;

    LOG("Resolving host");
    SDLNet_Address* addr = SDLNet_ResolveHostname("localhost");

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

        for (auto it = clients.begin(); it != clients.end();)
        {
            if (!(*it).sock)
            {
                if ((*it).username.length())
                    players_kicked.push_back((*it).username);
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

                    packet_login_request_s2c_t packet_login_s2c;
                    packet_login_s2c.player_eid = 0;
                    packet_login_s2c.seed = 0;
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

                    for (int x = -2; x < 2; x++)
                    {
                        for (int z = -2; z < 2; z++)
                        {
                            send_chunk(sock, x, z, 32);
                        }
                    }
                    send_chunk(sock, 4, 1, 0);
                    send_chunk(sock, 4, 0, 0);
                    send_chunk(sock, 4, -1, 0);
                    send_chunk(sock, 3, 1, 0);
                    send_chunk(sock, 3, 0, 128);
                    send_chunk(sock, 3, -1, 0);
                    send_chunk(sock, 2, 0, 0);
                    send_chunk(sock, 2, -1, 0);
                    send_chunk(sock, 2, 1, 0);

                    client->player_y = 64.0;
                    client->player_stance = client->player_y + 0.2;
                    client->player_on_ground = 1;

                    packet_player_pos_look_s2c_t pack_player_pos;
                    pack_player_pos.x = client->player_x;
                    pack_player_pos.y = client->player_y;
                    pack_player_pos.stance = client->player_stance;
                    pack_player_pos.z = client->player_z;
                    pack_player_pos.yaw = client->player_yaw;
                    pack_player_pos.pitch = client->player_pitch;
                    pack_player_pos.on_ground = client->player_on_ground;

                    send_buffer(sock, pack_player_pos.assemble());

                    for (int x = -4; x < -2; x++)
                    {
                        for (int z = -2; z < 2; z++)
                        {
                            send_chunk(sock, x, z, 28);
                        }
                    }

                    if (client->username.length() == 0)
                        break;

                    packet_chat_message_t pack_join_msg;
                    pack_join_msg.msg = "§e";
                    pack_join_msg.msg += client->username;
                    pack_join_msg.msg += " joined the game.";

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
                            packet_respawn pack_dim_change;
                            client->dimension = p->msg == "/overworld" ? 0 : -1;
                            pack_dim_change.dimension = client->dimension;
                            pack_dim_change.mode = client->player_mode;
                            pack_dim_change.world_height = WORLD_HEIGHT;
                            send_buffer(sock, pack_dim_change.assemble());

                            packet_time_update_t pack_time;
                            pack_time.time = sdl_tick_cur / 50;
                            send_buffer(sock, pack_time.assemble());

                            for (int x = -2; x < 2; x++)
                            {
                                for (int z = -2; z < 2; z++)
                                {
                                    send_chunk(sock, x, z, 32);
                                }
                            }

                            client->player_y = 64.0;
                            client->player_stance = client->player_y + 0.2;
                            client->player_on_ground = 1;

                            packet_player_pos_look_s2c_t pack_player_pos;
                            pack_player_pos.x = client->player_x;
                            pack_player_pos.y = client->player_y;
                            pack_player_pos.stance = client->player_stance;
                            pack_player_pos.z = client->player_z;
                            pack_player_pos.yaw = client->player_yaw;
                            pack_player_pos.pitch = client->player_pitch;
                            pack_player_pos.on_ground = client->player_on_ground;

                            send_buffer(sock, pack_player_pos.assemble());

                            for (int x = -4; x < -2; x++)
                            {
                                for (int z = -2; z < 2; z++)
                                {
                                    send_chunk(sock, x, z, 28);
                                }
                            }
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
                            const char* commands[] = { "/stop", "/smite", "/unload", "/overworld", "/nether", "/c", "/s", "/rain_on", "/rain_off", NULL };
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

                    break;
                }
                case 0x0c:
                {
                    CAST_PACK_TO_P(packet_player_look_t);

                    client->player_yaw = p->yaw;
                    client->player_pitch = p->pitch;
                    client->player_on_ground = p->on_ground;

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

                    break;
                }
                case 0x0e:
                {
                    CAST_PACK_TO_P(packet_player_dig_t);
                    LOG("Unloading chunk %d %d", p->x >> 4, p->z >> 4);

                    /* The notchian client does not like chunks being unloaded near it */
                    for (int i = 0; i < 16; i++)
                        for (size_t j = 0; j < clients.size(); j++)
                            if (clients[j].sock && clients[j].username.length() > 0)
                                send_prechunk(clients[j].sock, p->x >> 4, p->z >> 4, 0);
                    break;
                }
                case 0x0f:
                {
                    int x = client->player_x;
                    int z = client->player_z;

                    LOG("Loading chunk %d %d", x >> 4, z >> 4);
                    for (size_t j = 0; j < clients.size(); j++)
                        if (clients[j].sock && clients[j].username.length() > 0)
                            send_chunk(clients[j].sock, x >> 4, z >> 4, 0);
                    break;
                }
                case 0x10:
                case 0x12:
                case 0x13:
                    break;
                case 0x65:
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

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

#define STR_PACKET_TOO_SHORT "Packet too short"

#define MAX_PLAYERS 20

#define WORLD_HEIGHT 128

#define ARR_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define LOG(fmt, ...) printf("%s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)

void assemble_string16(std::vector<Uint8>& dat, std::string str)
{
    /* TODO: Find somewhere else to test the utf8 <-> ucs2 code */
    std::u16string str_ucs2 = UTF8_to_UCS2(UCS2_to_UTF8(UTF8_to_UCS2(str.c_str()).c_str()).c_str());
    Uint16 reason_len = SDL_Swap16BE(((Uint16)str_ucs2.size()));

    dat.push_back(reason_len & 0xFF);
    dat.push_back((reason_len >> 8) & 0xFF);

    size_t loc = dat.size();
    dat.resize(dat.size() + str_ucs2.size() * 2);
    memcpy(dat.data() + loc, str_ucs2.data(), str_ucs2.size() * 2);
}

void assemble_bytes(std::vector<Uint8>& dat, Uint8* in, size_t len)
{
    size_t loc = dat.size();
    dat.resize(dat.size() + len);
    memcpy(dat.data() + loc, in, len);
}

void assemble_ubyte(std::vector<Uint8>& dat, Uint8 in) { dat.push_back(in); }

void assemble_byte(std::vector<Uint8>& dat, Sint8 in) { dat.push_back(in); }

void assemble_short(std::vector<Uint8>& dat, Sint16 in)
{
    Sint16 temp = SDL_Swap16BE(in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_int(std::vector<Uint8>& dat, Sint32 in)
{
    Sint32 temp = SDL_Swap32BE(in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_long(std::vector<Uint8>& dat, Sint64 in)
{
    Sint64 temp = SDL_Swap64BE(in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_float(std::vector<Uint8>& dat, float in)
{
    Uint32 temp = SDL_Swap32BE(*(Uint32*)&in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_double(std::vector<Uint8>& dat, double in)
{
    Uint64 temp = SDL_Swap64BE(*(Uint64*)&in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

bool send_buffer(SDLNet_StreamSocket* sock, std::vector<Uint8>& dat) { return SDLNet_WriteToStreamSocket(sock, dat.data(), dat.size()); }

void kick(SDLNet_StreamSocket* sock, std::string reason, bool log = true)
{
    std::vector<Uint8> dat;
    dat.push_back(0xFF);

    assemble_string16(dat, reason);

    send_buffer(sock, dat);

    SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(sock);
    if (log)
        LOG("Kicked: %s:%u, \"%s\"", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(sock), reason.c_str());
    SDLNet_UnrefAddress(client_addr);

    SDLNet_WaitUntilStreamSocketDrained(sock, 100);
    SDLNet_DestroyStreamSocket(sock);
}

bool consume_bytes(SDLNet_StreamSocket* sock, int len)
{
    Uint8 buf_fixed[1];
    for (int i = 0; i < len; i++)
        if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
            return 0;
    return 1;
}

bool read_ubyte(SDLNet_StreamSocket* sock, Uint8* out)
{
    Uint8 buf_fixed[sizeof(*out)];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    if (out)
        *out = *buf_fixed;

    return 1;
}

bool read_byte(SDLNet_StreamSocket* sock, Sint8* out)
{
    Uint8 buf_fixed[sizeof(*out)];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    if (out)
        *out = *buf_fixed;

    return 1;
}

bool read_short(SDLNet_StreamSocket* sock, Sint16* out)
{
    Uint8 buf_fixed[sizeof(*out)];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    if (out)
        *out = SDL_Swap32BE(*((Sint16*)buf_fixed));

    return 1;
}

bool read_int(SDLNet_StreamSocket* sock, Sint32* out)
{
    Uint8 buf_fixed[sizeof(*out)];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    if (out)
        *out = SDL_Swap32BE(*((Sint32*)buf_fixed));

    return 1;
}

bool read_long(SDLNet_StreamSocket* sock, Sint64* out)
{
    Uint8 buf_fixed[sizeof(*out)];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    if (out)
        *out = SDL_Swap64BE(*((Sint64*)buf_fixed));

    return 1;
}

bool read_float(SDLNet_StreamSocket* sock, float* out)
{
    Uint8 buf_fixed[4];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    Uint32 temp = SDL_Swap32BE(*((Uint32*)buf_fixed));
    ;

    if (out)
        *out = *(float*)&temp;

    return 1;
}

bool read_double(SDLNet_StreamSocket* sock, double* out)
{
    Uint8 buf_fixed[8];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, ARR_SIZE(buf_fixed)) != ARR_SIZE(buf_fixed))
        return 0;

    Uint64 temp = SDL_Swap64BE(*((Uint64*)buf_fixed));
    ;

    if (out)
        *out = *(double*)&temp;

    return 1;
}

bool read_string16(SDLNet_StreamSocket* sock, std::string& out)
{
    Uint8 buf_fixed[2];
    if (SDLNet_ReadFromStreamSocket(sock, buf_fixed, 2) != 2)
        return 0;

    Uint16 len = SDL_Swap16BE(*((Uint16*)buf_fixed)) * 2;

    std::vector<Uint8> buf_var;
    buf_var.resize(len + 2, 0);
    if (SDLNet_ReadFromStreamSocket(sock, buf_var.data(), len) != len)
        return 0;

    out = UCS2_to_UTF8((Uint16*)buf_var.data());
    return 1;
}

bool send_prechunk(SDLNet_StreamSocket* sock, int chunk_x, int chunk_z, bool mode)
{
    std::vector<Uint8> dat;
    dat.push_back(0x32);
    assemble_int(dat, chunk_x);
    assemble_int(dat, chunk_z);
    assemble_ubyte(dat, mode);
    return send_buffer(sock, dat);
}

bool send_chunk(SDLNet_StreamSocket* sock, int chunk_x, int chunk_z, int max_y)
{
    std::vector<Uint8> compressed;
    int h = WORLD_HEIGHT;
    uLongf compressed_len;
    {
        std::vector<Uint8> precompress;
        precompress.resize(h * 16 * 16 * 5 / 2);

        for (int x = 0; x < 16; x++)
        {
            for (int y = 0; y < h; y++)
            {
                for (int z = 0; z < 16; z++)
                {
                    int index_type = y + (z * (h)) + (x * (h) * (16));
                    int index_light_block = (y + (z * (h)) + (x * (h) * (16))) / 2 + h * 16 * 16 * 3 / 2;
                    int index_light_sky = (y + (z * (h)) + (x * (h) * (16))) / 2 + h * 16 * 16 * 4 / 2;
                    if (y < max_y)
                        precompress[index_type] = y;
                    precompress[index_light_block] = 255;
                    precompress[index_light_sky] = 255;
                }
            }
        }
        compressed.resize(compressBound(precompress.size()));
        LOG("precompress_len: %lu", precompress.size());
        LOG("precompress_bound: %lu", compressed.size());

        compressed_len = compressed.size();
        compress(compressed.data(), &compressed_len, precompress.data(), precompress.size());
    }

    LOG("compressed_len: %lu", compressed_len);

    send_prechunk(sock, chunk_x, chunk_z, 1);
    send_prechunk(sock, chunk_x, chunk_z, 1);

    std::vector<Uint8> dat;
    dat.push_back(0x33);
    assemble_int(dat, chunk_x * 16);
    assemble_short(dat, 0);
    assemble_int(dat, chunk_z * 16);
    assemble_byte(dat, 15);
    assemble_byte(dat, h - 1);
    assemble_byte(dat, 15);

    if (compressed_len >= SDL_MAX_SINT32)
    {
        LOG("Error: compressed_len too big!");
        return 0;
    }

    assemble_int(dat, compressed_len);
    assemble_bytes(dat, compressed.data(), compressed_len);
    return send_buffer(sock, dat);
}

struct client_t
{
    SDLNet_StreamSocket* sock = NULL;
    bool recv = true;
    size_t no_recv_counter = 0;
    std::string username;
    int counter = 0;
    double player_x = 0.0;
    double player_y = 0.0;
    double player_stance = 0.0;
    double player_z = 0.0;
    float player_yaw = 0.0;
    float player_pitch = 0.0;
    Uint8 player_on_ground = 0;
};

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

    while (!done)
    {
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

            /* Setting this to values other than 0 breaks everything, TODO: fix */
            SDLNet_SimulateStreamPacketLoss(new_client.sock, 0);

            SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(new_client.sock);
            LOG("New Socket: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(new_client.sock));
            SDLNet_UnrefAddress(client_addr);

            clients.push_back(new_client);
        }

        for (auto it = clients.begin(); it != clients.end();)
        {
            if (!(*it).sock)
                it = clients.erase(it);
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
        goto loop_end;       \
    } while (0)

            if (client->no_recv_counter > 500 || !client->sock)
                continue;

            Uint8 buf[1024];
            buf[0] = 0;

            client->recv = false;
            client->no_recv_counter++;
            for (int i = 0; i < 10 && !client->recv; i++)
            {
                if (SDLNet_ReadFromStreamSocket(sock, buf, 1))
                    client->recv = 1;
                if (SDLNet_GetConnectionStatus(sock) != 1)
                    client->recv = 0;
                SDL_Delay(1);
            }
            if (client->recv)
                client->no_recv_counter = 0;

            /* TODO: Create assemble_packet_* functions */

            if (client->no_recv_counter > 500 || client->counter > 1200)
            {
                KICK(sock, "Timed out!");
            }

            if (!client->recv)
                continue;

            client->counter++;
            if (client->counter % 5 == 0 && client->counter > 4)
            {
                std::vector<Uint8> dat;
                dat.push_back(0x00);
                assemble_int(dat, client->counter);
                send_buffer(sock, dat);

                dat.clear();
                dat.push_back(0xC9);
                assemble_string16(dat, client->username);
                assemble_ubyte(dat, 1);
                assemble_short(dat, client->counter);
                for (size_t i = 0; i < clients.size(); i++)
                    if (clients[i].sock && clients[i].username.length() > 0)
                        send_buffer(clients[i].sock, dat);
            }

            switch (buf[0])
            {
            case 0x00:
            {
                Sint32 id;
                read_int(sock, &id);
                client->counter = 0;
                break;
            }
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
                break;
            }
            case 0x01:
            {
                Sint32 protocol_ver;
                int status = !read_int(sock, &protocol_ver);
                status += !read_string16(sock, client->username);
                status += !read_long(sock, 0);
                status += !read_int(sock, 0);
                status += !read_byte(sock, 0);
                status += !read_byte(sock, 0);
                status += !read_ubyte(sock, 0);
                status += !read_ubyte(sock, 0);
                if (status)
                    KICK(sock, STR_PACKET_TOO_SHORT);

                LOG("Player \"%s\" has protocol version: %d", client->username.c_str(), protocol_ver);
                if (protocol_ver != 17)
                    KICK(sock, "Nope");

                std::vector<Uint8> dat;
                dat.push_back(0x01);
                assemble_int(dat, 0);
                assemble_string16(dat, "");
                assemble_long(dat, 0);
                assemble_int(dat, 1);
                assemble_byte(dat, 0);
                assemble_byte(dat, 0);
                assemble_ubyte(dat, WORLD_HEIGHT);
                assemble_ubyte(dat, MAX_PLAYERS);
                send_buffer(sock, dat);

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

                dat.clear();
                dat.push_back(0x0d);
                assemble_double(dat, client->player_x);
                assemble_double(dat, client->player_stance);
                assemble_double(dat, client->player_y);
                assemble_double(dat, client->player_z);
                assemble_float(dat, client->player_yaw);
                assemble_float(dat, client->player_pitch);
                assemble_ubyte(dat, client->player_on_ground);

                send_buffer(sock, dat);

                for (int x = -4; x < -2; x++)
                {
                    for (int z = -2; z < 2; z++)
                    {
                        send_chunk(sock, x, z, 28);
                    }
                }

                if (client->username.length() == 0)
                    break;

                std::string msg = "§e";
                msg += client->username;
                msg += " joined the game.";

                dat.clear();
                dat.push_back(0x03);
                assemble_string16(dat, msg);

                for (size_t j = 0; j < clients.size(); j++)
                    if (clients[j].sock && clients[j].username.length() > 0)
                        send_buffer(clients[j].sock, dat);

                break;
            }
            case 0x02:
            {
                if (!read_string16(sock, client->username))
                    KICK(sock, STR_PACKET_TOO_SHORT);

                LOG("Player \"%s\" has initiated handshake", client->username.c_str());
                std::vector<Uint8> dat;
                dat.push_back(0x02);
                assemble_string16(dat, "-");
                send_buffer(sock, dat);
                break;
            }
            case 0x03:
            {
                std::string msg;
                read_string16(sock, msg);
                if (msg.length() > 100)
                    KICK(sock, "Message too long!");

                if (msg.length() > 0 && msg[0] == '/')
                {
                    LOG("Player \"%s\" issued: %s", client->username.c_str(), msg.c_str());
                    if (msg == "/stop")
                    {
                        done = true;
                        KICK(sock, "Server stopping!");
                    }
                    else if (msg == "/smite")
                    {
                        std::vector<Uint8> dat;
                        dat.push_back(0x47);
                        assemble_int(dat, client->counter);
                        assemble_ubyte(dat, 1);
                        assemble_int(dat, (int)client->player_x);
                        assemble_int(dat, (int)client->player_y);
                        assemble_int(dat, (int)client->player_z);
                        send_buffer(sock, dat);
                    }
                    else if (msg == "/unload")
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
                    else if (msg == "/help")
                    {
                        const char* commands[] = { "/stop", "/smite", "/unload", NULL };
                        for (int i = 0; commands[i] != 0; i++)
                        {
                            std::vector<Uint8> dat;
                            dat.push_back(0x03);
                            assemble_string16(dat, commands[i]);
                            send_buffer(sock, dat);
                        }
                    }
                }
                else
                {
                    char buf2[128];
                    snprintf(buf2, ARR_SIZE(buf2), "<%s> %s", client->username.c_str(), msg.c_str());

                    LOG("%s", buf2);

                    std::vector<Uint8> dat;
                    dat.push_back(0x03);
                    assemble_string16(dat, buf2);
                    for (size_t i = 0; i < clients.size(); i++)
                        if (clients[i].sock && clients[i].username.length() > 0)
                            send_buffer(clients[i].sock, dat);
                }
            }
            case 0x0a:
                read_ubyte(sock, &client->player_on_ground);
                break;
            case 0x0b:
            {
                read_double(sock, &client->player_x);
                read_double(sock, &client->player_y);
                read_double(sock, &client->player_stance);
                read_double(sock, &client->player_z);
                read_ubyte(sock, &client->player_on_ground);
                // LOG("Player pos: <%.2f, %.2f(%.2f), %.2f> On Ground: %d", player_x, player_y, player_stance, player_z, player_on_ground != 0);
                break;
            }
            case 0x0c:
            {
                read_float(sock, &client->player_yaw);
                read_float(sock, &client->player_pitch);
                read_ubyte(sock, &client->player_on_ground);
                // LOG("Player look: <%.2f, %.2f> On Ground: %d", player_yaw, player_pitch, player_on_ground != 0);
                break;
            }
            case 0x0d:
            {
                read_double(sock, &client->player_x);
                read_double(sock, &client->player_y);
                read_double(sock, &client->player_stance);
                read_double(sock, &client->player_z);
                read_float(sock, &client->player_yaw);
                read_float(sock, &client->player_pitch);
                read_ubyte(sock, &client->player_on_ground);
                // LOG("Player pos: <%.2f, %.2f(%.2f), %.2f> On Ground: %d", player_x, player_y, player_stance, player_z, player_on_ground != 0);
                // LOG("Player look: <%.2f, %.2f> On Ground: %d", player_yaw, player_pitch, player_on_ground != 0);
                break;
            }
            case 0x0f:
            {
                Sint32 x;
                Sint8 y;
                Sint32 z;
                read_int(sock, &x);
                read_byte(sock, &y);
                read_int(sock, &z);
                read_byte(sock, 0);
                Sint16 id;
                read_short(sock, &id);
                LOG("id: %d", id);
                if (id >= 0)
                    consume_bytes(sock, 3);

                x = client->player_x;
                z = client->player_z;

                LOG("Loading chunk %d %d", x >> 4, z >> 4);
                for (size_t j = 0; j < clients.size(); j++)
                    if (clients[j].sock && clients[j].username.length() > 0)
                        send_chunk(clients[j].sock, x >> 4, z >> 4, 0);

                break;
            }
            case 0x0e:
            {
                Sint8 status;
                Sint32 x;
                Sint8 y;
                Sint32 z;
                Sint8 face;
                read_byte(sock, &status);
                read_int(sock, &x);
                read_byte(sock, &y);
                read_int(sock, &z);
                read_byte(sock, &face);

                LOG("Unloading chunk %d %d", x >> 4, z >> 4);

                /* The notchian client does not like chunks being unloaded near it */
                for (int i = 0; i < 16; i++)
                    for (size_t j = 0; j < clients.size(); j++)
                        if (clients[j].sock && clients[j].username.length() > 0)
                            send_prechunk(clients[j].sock, x >> 4, z >> 4, 0);
                break;
            }
            case 0x10:
                consume_bytes(sock, 2);
                break;
            case 0x12:
            case 0x13:
                consume_bytes(sock, 5);
                break;
            case 0x65:
                consume_bytes(sock, 1);
                break;
            case 0x6b:
                consume_bytes(sock, 8);
                break;
            case 0xff:
            {
                std::string str;
                read_string16(sock, str);
                client->sock = NULL;
                if (str != "Quitting")
                {
                    LOG("Client kicked server with unknown message \"%s\"", str.c_str());
                    break;
                }
                if (client->username.length() == 0)
                    break;

                std::string msg = "§e";
                msg += client->username;
                msg += " left the game.";

                std::vector<Uint8> dat;
                dat.push_back(0x03);
                assemble_string16(dat, msg);

                for (size_t j = 0; j < clients.size(); j++)
                    if (clients[j].sock && clients[j].username.length() > 0)
                        send_buffer(clients[j].sock, dat);
                break;
            }
            default:
            {
                char buf2[32];
                snprintf(buf2, ARR_SIZE(buf2), "Unknown packet: 0x%02x", buf[0]);
                LOG("Unknown packet: 0x%02x", buf[0]);
                KICK(sock, buf2);
                break;
            }
            }

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

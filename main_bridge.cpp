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
 *
 * A bridge for Minecraft Beta 1.8.* client server communication
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
}

struct client_t
{
    /**
     * Socket obtained from the server component of the bridge
     */
    SDLNet_StreamSocket* sock_server = NULL;

    /**
     * Socket created to connect to the real server
     */
    SDLNet_StreamSocket* sock_client = NULL;

    packet_handler_t pack_handler_server = packet_handler_t(true);

    packet_handler_t pack_handler_client = packet_handler_t(false);

    Uint64 time_last_read = 0;

    bool skip = 0;

    int change_happened = 0;
};

SDLNet_Address* resolve_addr(const char* addr)
{
    SDLNet_Address* t = SDLNet_ResolveHostname(addr);

    if (!t)
    {
        LOG("SDLNet_ResolveHostname: %s", SDL_GetError());
        exit(1);
    }

    return t;
}

int main()
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

    LOG("Initializing server");

    bool done = false;

    LOG("Resolving hosts");
    SDLNet_Address* addr = resolve_addr("127.0.0.3");
    SDLNet_Address* addr_real_server = resolve_addr("127.0.0.1");

    if (SDLNet_WaitUntilResolved(addr, 5000) != 1)
    {
        LOG("SDLNet_WaitUntilResolved: %s", SDL_GetError());
        exit(1);
    }

    if (SDLNet_WaitUntilResolved(addr_real_server, 5000) != 1)
    {
        LOG("SDLNet_WaitUntilResolved: %s", SDL_GetError());
        exit(1);
    }

    LOG("Bridging: %s -> %s", SDLNet_GetAddressString(addr), SDLNet_GetAddressString(addr_real_server));

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
        SDL_Delay(5);
        int done_client_searching = false;
        while (!done_client_searching)
        {
            client_t new_client;
            if (!SDLNet_AcceptClient(server, &new_client.sock_server))
            {
                LOG("SDLNet_AcceptClient: %s", SDL_GetError());
                exit(1);
            }
            if (new_client.sock_server == NULL)
            {
                done_client_searching = true;
                continue;
            }

            /* Set this to a value other than 0 to break things */
            SDLNet_SimulateStreamPacketLoss(new_client.sock_server, 0);

            SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(new_client.sock_server);
            LOG("New Socket: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(new_client.sock_server));
            SDLNet_UnrefAddress(client_addr);

            new_client.sock_client = SDLNet_CreateClient(addr_real_server, 25565);

            new_client.time_last_read = SDL_GetTicks();

            clients.push_back(new_client);
        }

        for (size_t i = 0; i < clients.size() * 2; i++)
        {
            int sdl_tick_cur = SDL_GetTicks();
            client_t* c = &clients.data()[i / 2];
            if (c->skip)
                continue;
            c->change_happened = true;
            for (size_t j = 0; j < 20; j++)
            {
                if (c->skip || !c->change_happened)
                    continue;

                if (sdl_tick_cur - c->time_last_read > 60000)
                {
                    c->skip = true;
                    continue;
                }

                c->change_happened = false;

                packet_t* pack_server = c->pack_handler_server.get_next_packet(c->sock_server);
                packet_t* pack_client = c->pack_handler_client.get_next_packet(c->sock_client);

                if (pack_server)
                {
                    c->change_happened++;
                    c->time_last_read = sdl_tick_cur;
                    TRACE("Got packet from client[%zu]: 0x%02x", i, pack_server->id);
                    send_buffer(c->sock_client, pack_server->assemble());
                    delete pack_server;
                }
                else if (c->pack_handler_server.get_error().length())
                {
                    c->skip = true;
                    char buf[100];
                    snprintf(buf, ARR_SIZE(buf), "Error parsing packet from client[%zu]: %s", i, c->pack_handler_server.get_error().c_str());
                    kick(c->sock_server, buf);
                    kick(c->sock_client, buf);
                }

                if (pack_client)
                {
                    c->change_happened++;
                    c->time_last_read = sdl_tick_cur;
                    TRACE("Got packet from server: 0x%02x", pack_client->id);
                    send_buffer(c->sock_server, pack_client->assemble());
                    delete pack_client;
                }
                else if (c->pack_handler_client.get_error().length())
                {
                    c->skip = true;
                    char buf[100];
                    snprintf(buf, ARR_SIZE(buf), "Error parsing packet from server: %s", c->pack_handler_client.get_error().c_str());
                    kick(c->sock_server, buf);
                    kick(c->sock_client, buf);
                }

                /* In the future when the bridge understands packets, kick sockets here */
            }
        }
    }

    LOG("Destroying server");

    for (size_t i = 0; i < clients.size(); i++)
    {
        if (clients[i].sock_server)
            SDLNet_DestroyStreamSocket(clients[i].sock_server);
        if (clients[i].sock_client)
            SDLNet_DestroyStreamSocket(clients[i].sock_client);
    }

    SDLNet_DestroyServer(server);

    SDLNet_Quit();
    SDL_Quit();
}

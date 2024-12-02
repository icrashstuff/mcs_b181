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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "java_strings.h"

#define ARR_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define LOG(fmt, ...) printf("%s:%d %s():" fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)

void kick(SDLNet_StreamSocket* sock, std::string reason)
{
    std::vector<Uint8> dat;
    dat.push_back(0xFF);

    /* TODO: Find somewhere else to test the utf8 <-> ucs2 code */
    std::u16string reason_ucs2 = UTF8_to_UCS2(UCS2_to_UTF8(UTF8_to_UCS2(reason.c_str()).c_str()).c_str());
    Uint16 reason_len = SDL_Swap16BE(((Uint16)reason_ucs2.size()));

    dat.push_back(reason_len & 0xFF);
    dat.push_back((reason_len >> 8) & 0xFF);

    size_t loc = dat.size();
    dat.resize(dat.size() + reason_ucs2.size() * 2);
    memcpy(dat.data() + loc, reason_ucs2.data(), reason_ucs2.size() * 2);

    SDLNet_WriteToStreamSocket(sock, dat.data(), dat.size());

    SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(sock);
    LOG("Kicked: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(sock));
    SDLNet_UnrefAddress(client_addr);

    SDLNet_WaitUntilStreamSocketDrained(sock, 100);
    SDLNet_DestroyStreamSocket(sock);
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
    SDLNet_StreamSocket* client;

    if (!server)
    {
        LOG("SDLNet_CreateServer: %s", SDL_GetError());
        exit(1);
    }

    SDLNet_UnrefAddress(addr);

    while (!done)
    {

        if (!SDLNet_AcceptClient(server, &client))
        {
            LOG("SDLNet_AcceptClient: %s", SDL_GetError());
            exit(1);
        }
        if (client == NULL)
        {
            // LOG("No clients available");
            SDL_Delay(50);
            continue;
        }

        SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(client);
        LOG("New Client: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(client));
        SDLNet_UnrefAddress(client_addr);

        bool desc = false;

        Uint8 buf[1];
        buf[0] = 0;

        for (int i = 0; i < 50 && !desc; i++)
        {
            if (SDLNet_ReadFromStreamSocket(client, buf, sizeof(buf) / sizeof(buf[0])))
                desc = 1;
            SDL_Delay(1);
        }

        if (buf[0] == 0xfe)
        {
            std::string s = "A mcs_b181 server§";
            Uint32 r = SDL_rand_bits();
            s.append(std::to_string(r >> 16 & 0xFFFF));
            s.append("§");
            s.append(std::to_string(r & 0xFFFF));
            kick(client, s.c_str());
        }
        else
            kick(client, "§6Lol, no");

        client = NULL;
    }

    LOG("Destroying server");
    if (client)
        SDLNet_DestroyStreamSocket(client);
    SDLNet_DestroyServer(server);

    SDLNet_Quit();
    SDL_Quit();
}

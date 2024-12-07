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

#include "tetra/gui/console.h"
#include "tetra/tetra.h"

#include "misc.h"
#include "packet.h"

void kick_sock(SDLNet_StreamSocket* sock, std::string reason, bool log = true)
{
    packet_kick_t packet;
    packet.reason = reason;
    send_buffer(sock, packet.assemble());

    SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(sock);
    if (log)
        LOG("Kicked: %s:%u, \"%s\"", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(sock), reason.c_str());
    SDLNet_UnrefAddress(client_addr);
}

struct packet_viewer_dat_t
{
    packet_viewer_dat_t()
        : force_scroll(true)
    {
        default_filters();
    }
    bool force_scroll;
    bool filters[256];

    void default_filters()
    {
        memset(filters, 1, ARR_SIZE(filters));
        filters[PACKET_ID_KEEP_ALIVE] = 0;
    }
};

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

    Uint64 time_init = 0;

    Uint64 time_last_read = 0;

    bool skip = 0;

    int change_happened = 0;

    /* TODO-OPT: Store timestamps? */
    std::vector<packet_t*> packets_server;

    /* TODO-OPT: Store timestamps? */
    std::vector<packet_t*> packets_client;

    /* TODO-OPT: Store timestamps? */
    /* TODO: Store where the packet came from, or at least which handler was used */
    std::vector<packet_t*> packets;

    packet_viewer_dat_t packet_viewer_dat_server;
    packet_viewer_dat_t packet_viewer_dat_client;
    packet_viewer_dat_t packet_viewer_dat;

    std::string kick_reason;

    void kick(std::string reason)
    {
        kick_reason = reason;
        kick_sock(sock_server, reason);
        kick_sock(sock_client, reason);
    }

    /* TODO: Add option to filter out packets (eg. Keep Alive) */
    /* TODO: Add option to force scrolling */
    /* TODO: Put packets in child window */
    void draw_packets(const char* label, std::vector<packet_t*>& packs, packet_viewer_dat_t& dat)
    {
        if (!ImGui::TreeNode(label))
            return;

        ImGui::Checkbox("Force Scroll", &dat.force_scroll);

        ImGui::SameLine();

        if (ImGui::Button("Clear Filters"))
            memset(dat.filters, 0, ARR_SIZE(dat.filters));

        ImGui::SameLine();

        if (ImGui::Button("Enable all Filters"))
            memset(dat.filters, 1, ARR_SIZE(dat.filters));

        ImGui::SameLine();

        if (ImGui::Button("Default filters"))
            dat.default_filters();

        char filter_buf[64];
        size_t filters_enabled = 0;
        size_t filters_total = 0;
        for (size_t i = 0; i < ARR_SIZE(dat.filters); i++)
        {
            filters_enabled += (dat.filters[i] && packet_t::is_valid_id(i));
            filters_total += packet_t::is_valid_id(i);
        }
        snprintf(filter_buf, ARR_SIZE(filter_buf), "%zu/%zu packets enabled", filters_enabled, filters_total);

        if (ImGui::BeginCombo("Filters", filter_buf))
        {
            for (size_t i = 0; i < ARR_SIZE(dat.filters); i++)
            {
                char buf[64];
                snprintf(buf, ARR_SIZE(buf), "0x%02x (%s)", (Uint8)i, packet_t::get_name_for_id(i));
                if (packet_t::is_valid_id(i))
                    ImGui::Checkbox(buf, &dat.filters[i]);
            }
            ImGui::EndCombo();
        }

        if (!packs.size())
        {
            ImGui::Text("No packets");
            ImGui::TreePop();
            return;
        }

        if (!ImGui::BeginChild(
                "Packet List", ImVec2(-1, ImGui::GetMainViewport()->WorkSize.y / 3), ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::EndChild();
            ImGui::TreePop();
            return;
        }

        for (size_t i = 0; i < packs.size(); i++)
        {
            if (!dat.filters[packs[i]->id])
                continue;
            ImGui::PushID(i);
            if (ImGui::TreeNode("Packet", "Packet[%zu]: 0x%02x (%s)", i, packs[i]->id, packs[i]->get_name()))
            {
                ImGui::Text("Information coming soon");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (dat.force_scroll)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::TreePop();
    }

#define TABLE_FIELD(field_text, fmt, ...) \
    do                                    \
    {                                     \
        ImGui::TableNextRow();            \
        ImGui::TableNextColumn();         \
        ImGui::Text(field_text);          \
        ImGui::TableNextColumn();         \
        ImGui::Text(fmt, ##__VA_ARGS__);  \
    } while (0)

    bool draw_imgui()
    {
        if (!ImGui::BeginTable("client_info_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            return false;
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Seconds since first read: ").x);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        TABLE_FIELD("Seconds since first read: ", "%.2f", ((float)((SDL_GetTicks() - time_init) / 10)) / 100.0f);
        TABLE_FIELD("Seconds since last read: ", "%.2f", ((float)((SDL_GetTicks() - time_last_read) / 10)) / 100.0f);

        TABLE_FIELD("Num packets from client: ", "%zu", packets_server.size());
        TABLE_FIELD("Num packets from server: ", "%zu", packets_client.size());
        TABLE_FIELD("Num packets: ", "%zu", packets_client.size() + packets_server.size());

        if (time_last_read - time_init > 0)
        {
            TABLE_FIELD("Client packets/s: ", "%zu", packets_server.size() * 1000 / (time_last_read - time_init));
            TABLE_FIELD("Server packets/s: ", "%zu", packets_client.size() * 1000 / (time_last_read - time_init));
            TABLE_FIELD("Total packets/s: ", "%zu", (packets_client.size() + packets_server.size()) * 1000 / (time_last_read - time_init));
        }

        if (kick_reason.length())
        {
            TABLE_FIELD("Kick Reason: ", "%s", kick_reason.c_str());
        }

        ImGui::EndTable();

        /* TODO: Put world information here (eg. entities, loaded chunks, time, health, player list, inventory, ...)*/
        if (ImGui::TreeNode("World"))
        {
            ImGui::Text("Information coming soon");
            ImGui::TreePop();
        }

        draw_packets("Packets from Client", packets_server, packet_viewer_dat_server);
        draw_packets("Packets from Server", packets_client, packet_viewer_dat_client);
        draw_packets("Packets", packets, packet_viewer_dat);

        return true;
    }
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

int main(int argc, const char** argv)
{
    /* KDevelop fully buffers the output and will not display anything */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    tetra::init(argc, argv);
    tetra::init_gui("mcs_b181_bridge");

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
    SDLNet_Address* addr_real_server = resolve_addr(argc > 1 ? argv[1] : "127.0.0.1");

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
        if (tetra::start_frame() == 0)
            done = true;
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

            SDLNet_SimulateStreamPacketLoss(new_client.sock_server, 0);

            SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(new_client.sock_server);
            LOG("New Socket: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(new_client.sock_server));
            SDLNet_UnrefAddress(client_addr);

            new_client.sock_client = SDLNet_CreateClient(addr_real_server, 25565);

            new_client.time_last_read = SDL_GetTicks();
            new_client.time_init = new_client.time_last_read;

            clients.push_back(new_client);
        }

        for (size_t i = 0; i < clients.size() * 3; i++)
        {
            int sdl_tick_cur = SDL_GetTicks();
            client_t* c = &clients.data()[i % clients.size()];
            if (c->skip)
                continue;
            c->change_happened = true;
            for (size_t j = 0; j < 25; j++)
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
                    if (pack_server->id == PACKET_ID_CHAT_MSG)
                    {
                        packet_chat_message_t* p = (packet_chat_message_t*)pack_server;

                        if (p->msg == "/stop_bridge")
                            done = true;
                        else
                            send_buffer(c->sock_client, pack_server->assemble());
                    }
                    else
                        send_buffer(c->sock_client, pack_server->assemble());

                    c->packets_server.push_back(pack_server);
                    c->packets.push_back(pack_server);
                }
                else if (c->pack_handler_server.get_error().length())
                {
                    c->skip = true;
                    char buf[100];
                    snprintf(buf, ARR_SIZE(buf), "Error parsing packet from client[%zu]: %s", i, c->pack_handler_server.get_error().c_str());
                    c->kick(buf);
                }

                if (pack_client)
                {
                    c->change_happened++;
                    c->time_last_read = sdl_tick_cur;
                    TRACE("Got packet from server: 0x%02x", pack_client->id);
                    send_buffer(c->sock_server, pack_client->assemble());
                    c->packets_client.push_back(pack_client);
                    c->packets.push_back(pack_client);
                }
                else if (c->pack_handler_client.get_error().length())
                {
                    c->skip = true;
                    char buf[100];
                    snprintf(buf, ARR_SIZE(buf), "Error parsing packet from server: %s", c->pack_handler_client.get_error().c_str());
                    c->kick(buf);
                }
            }
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
        Uint32 window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
        if (ImGui::Begin("Client Inspector Window", NULL, window_flags))
        {

            if (ImGui::TreeNode("All clients", "All clients (%zu)", clients.size()))
            {
                for (size_t i = 0; i < clients.size(); i++)
                {
                    ImGui::PushID(i);
                    if (ImGui::TreeNode("client", "Clients[%zu]", i))
                    {
                        clients[i].draw_imgui();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }

            size_t active_clients_num = 0;
            for (size_t i = 0; i < clients.size(); i++)
                active_clients_num += !clients[i].skip;

            if (ImGui::TreeNode("Active clients", "Active clients (%zu)", active_clients_num))
            {
                for (size_t i = 0; i < clients.size(); i++)
                {
                    if (clients[i].skip)
                        continue;
                    ImGui::PushID(i);
                    if (ImGui::TreeNode("client", "Clients[%zu]", i))
                    {
                        clients[i].draw_imgui();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }
        ImGui::End();
        tetra::end_frame();
    }

    LOG("Destroying server");

    for (size_t i = 0; i < clients.size(); i++)
    {
        if (clients[i].sock_server)
            SDLNet_DestroyStreamSocket(clients[i].sock_server);
        if (clients[i].sock_client)
            SDLNet_DestroyStreamSocket(clients[i].sock_client);
        for (size_t j = 0; j < clients[i].packets_client.size(); j++)
            if (clients[i].packets_client[j])
                delete clients[i].packets_client[j];
        for (size_t j = 0; j < clients[i].packets_server.size(); j++)
            if (clients[i].packets_server[j])
                delete clients[i].packets_server[j];
    }

    SDLNet_DestroyServer(server);

    SDLNet_Quit();
    tetra::deinit();
    SDL_Quit();
}

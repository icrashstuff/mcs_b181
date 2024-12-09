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
#include "tetra/util/convar.h"

#include "misc.h"
#include "packet.h"

/**
 * We use this in timestamp_from_tick() to ensure it's output is stable
 */
static struct timestamp_dat_t
{
    SDL_Time time = 0;
    Uint64 tick = 0;
    bool initialized = 0;
} timestamp_dat;

std::string timestamp_from_tick(Uint64 sdl_tick)
{
    if (!timestamp_dat.initialized && !SDL_GetCurrentTime(&timestamp_dat.time))
        return "Error creating timestamp! (SDL_GetCurrentTime)";
    if (!timestamp_dat.initialized)
    {
        timestamp_dat.tick = SDL_GetTicks();
        timestamp_dat.initialized = 1;
    }

    SDL_Time timestamp_tick = timestamp_dat.time + ((sdl_tick - timestamp_dat.tick) * 1000000);

    SDL_DateTime dt;

    if (!SDL_TimeToDateTime(timestamp_tick, &dt, 1))
        return "Error creating timestamp! (SDL_TimeToDateTime)";

    char buf[128];

    char sign = '+';
    int off_hour = dt.utc_offset / 3600;
    if (off_hour < 0)
    {
        sign = '-';
        off_hour = SDL_abs(off_hour);
    }

    int off_min = dt.utc_offset % 60;
    if (off_min < 0)
        off_min += 60;

    snprintf(buf, ARR_SIZE(buf), "%04d-%02d-%02d %02d:%02d:%02d.%02d (%c%02d:%02d)", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
        dt.nanosecond / 10000000, sign, off_hour, off_min);

    return buf;
}

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
    packet_viewer_dat_t() { default_filters(); }
    size_t sel = -1;
    bool filters[256];
    bool force_scroll = true;
    bool select_recent = false;

    void default_filters()
    {
        memset(filters, 1, ARR_SIZE(filters));
        filters[PACKET_ID_KEEP_ALIVE] = 0;
    }
};

#define TABLE_VALUE(fmt, ...)            \
    do                                   \
    {                                    \
        ImGui::TableNextColumn();        \
        ImGui::Text(fmt, ##__VA_ARGS__); \
    } while (0)

#define TABLE_FIELD(field_text, fmt, ...)   \
    do                                      \
    {                                       \
        ImGui::TableNextRow();              \
        ImGui::TableNextColumn();           \
        ImGui::TextUnformatted(field_text); \
        TABLE_VALUE(fmt, ##__VA_ARGS__);    \
    } while (0)

#define TABLE_FIELD_BOOL(x) TABLE_FIELD(#x ": ", "%s", (x) ? "true" : "false")
#define TABLE_FIELD_INT(x) TABLE_FIELD(#x ": ", "%d", x)
#define TABLE_FIELD_UINT(x) TABLE_FIELD(#x ": ", "%u", x)
#define TABLE_FIELD_LONG(x) TABLE_FIELD(#x ": ", "%ld", x)
#define TABLE_FIELD_ULONG(x) TABLE_FIELD(#x ": ", "%lu", x)
#define TABLE_FIELD_FLOAT(x) TABLE_FIELD(#x ": ", "%.3f", x)
#define TABLE_FIELD_STRING(x) TABLE_FIELD(#x ": ", "\"%s\"", x.c_str())
#define TABLE_FIELD_CSTRING(x) TABLE_FIELD(#x ": ", "\"%s\"", x)

struct chat_t
{
    std::string* msg;
    bool sent_by_client;
};

struct entity_info_t
{
    int eid = 0;

    int pos_x = 0;
    int pos_y = 0;
    int pos_z = 0;

    int vel_x = 0;
    int vel_y = 0;
    int vel_z = 0;

    jbyte yaw = 0;
    jbyte pitch = 0;
    jbyte roll = 0;

    packet_t* pack_creation = NULL;
    packet_t* pack_destruction = NULL;
    std::string name;

    void draw_imgui()
    {
        if (ImGui::BeginTable("Current Players Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("x").x * 18);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            if (pack_creation)
                TABLE_FIELD("Created: ", "%s", timestamp_from_tick(pack_creation->assemble_tick).c_str());

            if (pack_destruction)
                TABLE_FIELD("Destroyed: ", "%s", timestamp_from_tick(pack_destruction->assemble_tick).c_str());

            if (name.length())
                TABLE_FIELD("Name", "%s", name.c_str());
            TABLE_FIELD("pos: ", "<%.2f, %.2f, %.2f>", (float)pos_x / 32, (float)pos_y / 32, (float)pos_z / 32);
            TABLE_FIELD("vel: ", "<%.2f, %.2f, %.2f>", (float)vel_x / (32000 / 5), (float)vel_y / (32000 / 5), (float)vel_z / (32000 / 5));

            ImGui::EndTable();
        }

        if (pack_creation)
        {
            ImGui::SeparatorText("Packet Creation");
            pack_creation->draw_imgui();
        }

        if (pack_destruction)
        {
            ImGui::SeparatorText("Packet Destruction");
            pack_destruction->draw_imgui();
        }
    }
};

struct world_diag_t
{
public:
    bool handshake_occured = false;
    Uint64 last_mc_tick = 0;

    std::string connection_hash;
    std::string username;

    jint protocol_ver = 0;

    jint player_eid = 0;
    jlong seed = 0;
    jint gamemode = 0;
    jbyte dimension = 0;
    jbyte difficulty = 0;
    jubyte world_height = 0;
    jubyte max_players = 0;

    jdouble player_x = 0.0;
    jdouble player_y = 0.0;
    jdouble player_stance = 0.0;
    jdouble player_z = 0.0;
    jfloat yaw = 0.0;
    jfloat pitch = 0.0;
    jbool on_ground = 0;

    jbyte xp_current = 0;
    jbyte xp_level = 0;
    jshort xp_total = 0;

    jlong time = 0;

    jshort health = 0;
    jshort food = 0;
    jfloat food_saturation = 0.0;

    jint spawn_x = 0;
    jint spawn_y = 0;
    jint spawn_z = 0;

    Uint64 keep_alive_time_from_client = 0;
    Uint64 keep_alive_time_from_server = 0;

    std::vector<chat_t> chat_history;
    char chat_buf[101] = "";
    bool send_chat = false;

    std::vector<packet_play_list_item_t*> player_list;

    std::vector<entity_info_t> entities;

#define CAST_PACK_TO_P(type) type* p = (type*)pack
    void feed_packet_from_server(packet_t* pack)
    {
        switch (pack->id)
        {
        case PACKET_ID_KEEP_ALIVE:
        {
            keep_alive_time_from_server = pack->assemble_tick;
            break;
        }
        case PACKET_ID_HANDSHAKE:
        {
            CAST_PACK_TO_P(packet_handshake_s2c_t);
            connection_hash = p->connection_hash;
            handshake_occured = true;
            break;
        }
        case PACKET_ID_LOGIN_REQUEST:
        {
            CAST_PACK_TO_P(packet_login_request_s2c_t);
            player_eid = p->player_eid;
            seed = p->seed;
            gamemode = p->mode;
            dimension = p->dimension;
            difficulty = p->difficulty;
            world_height = p->world_height;
            max_players = p->max_players;
            break;
        }
        case PACKET_ID_CHAT_MSG:
        {
            CAST_PACK_TO_P(packet_chat_message_t);
            chat_history.push_back({ &p->msg, 0 });
            break;
        }
        case PACKET_ID_PLAYER_POS_LOOK:
        {
            CAST_PACK_TO_P(packet_player_pos_look_s2c_t);
            player_x = p->x;
            player_y = p->y;
            player_stance = p->stance;
            player_z = p->z;
            yaw = p->yaw;
            pitch = p->pitch;
            on_ground = p->on_ground;
            break;
        }
        default:
            feed_packet_common(pack);
            break;
        }
        tick();
    }

    void feed_packet_from_client(packet_t* pack)
    {
        switch (pack->id)
        {
        case PACKET_ID_KEEP_ALIVE:
        {
            keep_alive_time_from_client = pack->assemble_tick;
            break;
        }
        case PACKET_ID_HANDSHAKE:
        {
            CAST_PACK_TO_P(packet_handshake_c2s_t);
            username = p->username;
            break;
        }
        case PACKET_ID_LOGIN_REQUEST:
        {
            CAST_PACK_TO_P(packet_login_request_c2s_t);
            username = p->username;
            protocol_ver = p->protocol_ver;
            break;
        }
        case PACKET_ID_CHAT_MSG:
        {
            CAST_PACK_TO_P(packet_chat_message_t);
            chat_history.push_back({ &p->msg, 1 });
            break;
        }
        case PACKET_ID_PLAYER_POS_LOOK:
        {
            CAST_PACK_TO_P(packet_player_pos_look_c2s_t);
            player_x = p->x;
            player_y = p->y;
            player_stance = p->stance;
            player_z = p->z;
            yaw = p->yaw;
            pitch = p->pitch;
            on_ground = p->on_ground;
            break;
        }
        default:
            feed_packet_common(pack);
            break;
        }
        tick();
    }

    void draw_imgui_basic()
    {
        if (!ImGui::BeginTable("client_info_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            return;
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("AVG Est. Memory footprint rate: ").x);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        TABLE_FIELD_BOOL(handshake_occured);

        if (!handshake_occured)
        {
            ImGui::EndTable();
            return;
        }

        TABLE_FIELD_STRING(connection_hash);

        TABLE_FIELD_STRING(username);

        TABLE_FIELD_INT(protocol_ver);

        TABLE_FIELD_LONG(seed);
        TABLE_FIELD_INT(gamemode);
        TABLE_FIELD_INT(dimension);
        TABLE_FIELD_INT(difficulty);
        TABLE_FIELD_UINT(world_height);
        TABLE_FIELD_UINT(max_players);

        TABLE_FIELD_INT(player_eid);
        TABLE_FIELD_FLOAT(player_x);
        TABLE_FIELD("player_y: ", "%.3f (%.3f)", player_y, player_stance - player_y);
        TABLE_FIELD_FLOAT(player_z);
        TABLE_FIELD_FLOAT(yaw);
        TABLE_FIELD_FLOAT(pitch);
        TABLE_FIELD_BOOL(on_ground);

        TABLE_FIELD("XP: ", "Level: %d, Progress: %d/%d", xp_level, xp_current, xp_level * 10 + 10);

        const char* time_states[4] = { "Sunrise", "Noon", "Sunset", "Midnight" };
        long tod = time % 24000;
        TABLE_FIELD("Time: ", "%ld (%ld) (%s) (Day: %ld)", time, tod, time_states[tod / 6000], time / 24000);

        TABLE_FIELD("Keep alive server: ", "%s", timestamp_from_tick(keep_alive_time_from_server).c_str());
        TABLE_FIELD("Keep alive client: ", "%s", timestamp_from_tick(keep_alive_time_from_client).c_str());

        TABLE_FIELD_INT(health);
        TABLE_FIELD_INT(food);
        TABLE_FIELD_FLOAT(food_saturation);

        ImGui::EndTable();
    }

    bool chat_auto_scroll = true;
    size_t last_chat_history_size = 0;

    void draw_imgui_chat()
    {
        ImGui::Checkbox("Auto Scroll", &chat_auto_scroll);

        if (ImGui::BeginListBox("Chat History", ImVec2(0, ImGui::GetMainViewport()->WorkSize.y / 3)))
        {
            if (!ImGui::BeginTable("Chat Table", 2, ImGuiTableFlags_BordersInnerV))
                goto list_box_end;
            ImGui::TableSetupColumn("Sender", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Client: ").x);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

            for (size_t i = 0; i < chat_history.size(); i++)
            {
                chat_t c = chat_history[i];
                TABLE_FIELD(c.sent_by_client ? "Client: " : "Server: ", "%s", c.msg->c_str());
            }

            if (chat_auto_scroll && last_chat_history_size != chat_history.size())
                ImGui::SetScrollHereY(0.0f);

            last_chat_history_size = chat_history.size();

            ImGui::EndTable();
        list_box_end:
            ImGui::EndListBox();
        }

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
        flags |= send_chat ? ImGuiInputTextFlags_ReadOnly : 0;
        if (ImGui::InputText("##chat input", chat_buf, ARR_SIZE(chat_buf), flags))
            send_chat = true;

        ImGui::SameLine();
        ImGui::Text("%zu/%zu", strlen(chat_buf), ARR_SIZE(chat_buf) - 1);
    }

    bool ent_viewer_force_scroll = false;
    bool ent_viewer_no_destroyed = true;
    size_t ent_viewer_sel = -1;

    void draw_imgui_entities()
    {
        ImGui::Checkbox("Force Scroll", &ent_viewer_force_scroll);
        ImGui::SameLine();
        ImGui::Checkbox("No Destroyed", &ent_viewer_no_destroyed);

        float child_height = ImGui::GetMainViewport()->WorkSize.y / 3;

        float list_width = ImGui::CalcTextSize("x").x * 90 + ImGui::GetStyle().ScrollbarSize;

        if (list_width < ImGui::GetContentRegionAvail().x / 2)
            list_width = ImGui::GetContentRegionAvail().x / 2;

        if (ImGui::BeginListBox("##Packet Listbox", ImVec2(list_width, child_height)))
        {
            float text_spacing = ImGui::GetTextLineHeightWithSpacing();

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (ent_viewer_no_destroyed && entities[i].pack_destruction)
                    continue;
                ImGui::PushID(i);
                char buf[56] = "";
                char buf2[88] = "";
                if (!ImGui::IsRectVisible(ImVec2(20, text_spacing)))
                    ImGui::Spacing();
                else
                {
                    if (entities[i].pack_creation)
                    {
                        packet_t* pack = entities[i].pack_creation;
                        const char* name = NULL;
                        switch (pack->id)
                        {
                        case PACKET_ID_ENT_SPAWN_MOB:
                        {
                            CAST_PACK_TO_P(packet_ent_spawn_mob_t);
                            name = mc_id::get_name_mob(p->mob_type);
                            break;
                        }
                        case PACKET_ID_ADD_OBJ:
                        {
                            CAST_PACK_TO_P(packet_add_obj_t);
                            name = mc_id::get_name_vehicle(p->obj_type);
                            break;
                        }
                        default:
                            name = entities[i].pack_creation->get_name();
                            break;
                        }
                        snprintf(buf, ARR_SIZE(buf), "(%s)", name);
                    }
                    snprintf(buf2, ARR_SIZE(buf2), "eid[%d]: %s", entities[i].eid, buf);

                    if (ImGui::Selectable(buf2, ent_viewer_sel == i))
                        ent_viewer_sel = i;
                }

                ImGui::PopID();
            }

            if (ent_viewer_force_scroll)
                ImGui::SetScrollHereY(0.0f);

            ImGui::EndListBox();
        }

        ImGui::SameLine();

        if (!ImGui::BeginChild("Packet Table", ImVec2(-1, child_height), 0, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::EndChild();
            return;
        }

        if (ent_viewer_sel < entities.size())
            entities[ent_viewer_sel].draw_imgui();
        else
        {
            PACKET_NEW_TABLE_CHOICE_IF("blank_table", goto skip_end_table;);
            ImGui::EndTable();
        skip_end_table:;
        }

        ImGui::EndChild();
    }

    void draw_imgui_players()
    {
        ImGui::SeparatorText("Current players");

        if (ImGui::BeginTable("Current Players Table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("x").x * 18);
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Online ", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Last Seen", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < player_list.size(); i++)
            {
                packet_play_list_item_t* p = player_list[i];
                if (!p->online)
                    continue;
                ImGui::TableNextRow();
                TABLE_VALUE("%s", p->username.c_str());
                TABLE_VALUE("%d ms", p->ping);
                TABLE_VALUE("%s", p->online ? "Online" : "Offline");
                TABLE_VALUE("%s", timestamp_from_tick(p->assemble_tick).c_str());
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();

        ImGui::SeparatorText("Previous players");

        if (ImGui::BeginTable("Prior Players Table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("x").x * 18);
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Online ", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Last Seen", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < player_list.size(); i++)
            {
                packet_play_list_item_t* p = player_list[i];
                if (p->online)
                    continue;
                ImGui::TableNextRow();
                TABLE_VALUE("%s", p->username.c_str());
                TABLE_VALUE("%d ms", p->ping);
                TABLE_VALUE("%s", p->online ? "Online" : "Offline");
                TABLE_VALUE("%s", timestamp_from_tick(p->assemble_tick).c_str());
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
    }

private:
    void tick()
    {
        Uint64 sdl_tick_cur = SDL_GetTicks();
        if (last_mc_tick == 0)
        {
            last_mc_tick = sdl_tick_cur;
            return;
        }
        if (sdl_tick_cur - last_mc_tick < 50)
            return;

        Uint64 ticks = (sdl_tick_cur - last_mc_tick) / 50;

        if (ticks < 50)
        {
            for (Uint64 i = 0; i < ticks; i++)
            {
                tick_real();
                last_mc_tick += 50;
            }
        }
        else
        {
            LOG("Skipping %lu ticks", ticks);
            tick_real();
            last_mc_tick = sdl_tick_cur;
        }
    }

    void tick_real() { time++; }

    void feed_packet_common(packet_t* pack)
    {
        switch (pack->id)
        {
        case PACKET_ID_HANDSHAKE:
        case PACKET_ID_LOGIN_REQUEST:
        case PACKET_ID_PLAYER_POS_LOOK:
        case PACKET_ID_CHAT_MSG:
        case PACKET_ID_KEEP_ALIVE:
        case PACKET_ID_KICK:
            LOG_WARN("Unhandled packet 0x%02x(%s)", pack->id, pack->get_name());
            break;
        case PACKET_ID_UPDATE_TIME:
        {
            CAST_PACK_TO_P(packet_time_update_t);
            time = p->time;
            break;
        }
        case PACKET_ID_UPDATE_HEALTH:
        {
            CAST_PACK_TO_P(packet_health_t);
            health = p->health;
            food = p->food;
            food_saturation = p->food_saturation;
            break;
        }
        case PACKET_ID_RESPAWN:
        {
            CAST_PACK_TO_P(packet_respawn_t);
            seed = p->seed;
            gamemode = p->mode;
            dimension = p->dimension;
            difficulty = p->difficulty;
            world_height = p->world_height;
            break;
        }
        case PACKET_ID_PLAYER_ON_GROUND:
        {
            CAST_PACK_TO_P(packet_on_ground_t);
            on_ground = p->on_ground;
            break;
        }
        case PACKET_ID_PLAYER_POS:
        {
            CAST_PACK_TO_P(packet_player_pos_t);
            player_x = p->x;
            player_y = p->y;
            player_stance = p->stance;
            player_z = p->z;
            on_ground = p->on_ground;
            break;
        }
        case PACKET_ID_PLAYER_LOOK:
        {
            CAST_PACK_TO_P(packet_player_look_t);
            yaw = p->yaw;
            pitch = p->pitch;
            on_ground = p->on_ground;
            break;
        }
        case PACKET_ID_XP_SET:
        {
            CAST_PACK_TO_P(packet_xp_set_t);

            xp_current = p->current_xp;
            xp_level = p->level;
            xp_total = p->total;
            break;
        }
        case PACKET_ID_PLAYER_LIST_ITEM:
        {
            CAST_PACK_TO_P(packet_play_list_item_t);

            size_t i = 0;
            for (i = 0; i < player_list.size(); i++)
            {
                if (player_list[i]->username == p->username)
                {
                    player_list[i] = p;
                    break;
                }
            }

            player_list.push_back(p);

            break;
        }
        case PACKET_ID_ENT_SPAWN_NAMED:
        {
            CAST_PACK_TO_P(packet_ent_spawn_named_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->x;
            t.pos_y = p->y;
            t.pos_z = p->z;
            t.yaw = p->rotation;
            t.pitch = p->pitch;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ENT_SPAWN_PICKUP:
        {
            CAST_PACK_TO_P(packet_ent_spawn_pickup_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->x;
            t.pos_y = p->y;
            t.pos_z = p->z;
            t.yaw = p->rotation;
            t.pitch = p->pitch;
            t.roll = p->roll;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ADD_OBJ:
        {
            CAST_PACK_TO_P(packet_add_obj_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->x;
            t.pos_y = p->y;
            t.pos_z = p->z;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ENT_ENSURE_SPAWN:
        {
            CAST_PACK_TO_P(packet_ent_create_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ENT_SPAWN_MOB:
        {
            CAST_PACK_TO_P(packet_ent_spawn_mob_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->x;
            t.pos_y = p->y;
            t.pos_z = p->z;
            t.yaw = p->yaw;
            t.pitch = p->pitch;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ENT_SPAWN_PAINTING:
        {
            CAST_PACK_TO_P(packet_ent_spawn_painting_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->center_x;
            t.pos_y = p->center_y;
            t.pos_z = p->center_z;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ENT_SPAWN_XP:
        {
            CAST_PACK_TO_P(packet_ent_spawn_xp_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->x;
            t.pos_y = p->y;
            t.pos_z = p->z;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_THUNDERBOLT:
        {
            CAST_PACK_TO_P(packet_thunder_t);

            entity_info_t t;
            t.eid = p->eid;
            t.pos_x = p->x;
            t.pos_y = p->y;
            t.pos_z = p->z;
            t.pack_creation = p;

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i] = t;
                    break;
                }
            }

            entities.push_back(t);

            break;
        }
        case PACKET_ID_ENT_VELOCITY:
        {
            CAST_PACK_TO_P(packet_ent_velocity_t);

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i].vel_x = p->vel_x;
                    entities[i].vel_y = p->vel_y;
                    entities[i].vel_z = p->vel_z;
                    break;
                }
            }
            break;
        }
        case PACKET_ID_ENT_MOVE_REL:
        {
            CAST_PACK_TO_P(packet_ent_move_rel_t);

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i].pos_x += p->delta_x;
                    entities[i].pos_y += p->delta_y;
                    entities[i].pos_z += p->delta_z;
                    break;
                }
            }
            break;
        }
        case PACKET_ID_ENT_LOOK:
        {
            CAST_PACK_TO_P(packet_ent_look_t);

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i].yaw = p->yaw;
                    entities[i].pitch = p->pitch;
                    break;
                }
            }
            break;
        }
        case PACKET_ID_ENT_LOOK_MOVE_REL:
        {
            CAST_PACK_TO_P(packet_ent_look_move_rel_t);

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i].pos_x += p->delta_x;
                    entities[i].pos_y += p->delta_y;
                    entities[i].pos_z += p->delta_z;
                    entities[i].yaw = p->yaw;
                    entities[i].pitch = p->pitch;
                    break;
                }
            }
            break;
        }
        case PACKET_ID_ENT_MOVE_TELEPORT:
        {
            CAST_PACK_TO_P(packet_ent_teleport_t);

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i].pos_x = p->x;
                    entities[i].pos_y = p->y;
                    entities[i].pos_z = p->z;
                    entities[i].yaw = p->rotation;
                    entities[i].pitch = p->pitch;
                    break;
                }
            }
            break;
        }
        case PACKET_ID_ENT_DESTROY:
        {
            CAST_PACK_TO_P(packet_ent_destroy_t);

            for (size_t i = 0; i < entities.size(); i++)
            {
                if (entities[i].eid == p->eid)
                {
                    entities[i].pack_destruction = p;
                    break;
                }
            }

            entity_info_t t;
            t.eid = p->eid;
            t.pack_destruction = p;

            entities.push_back(t);

            break;
        }
        }
    }
};

struct client_t
{
    /**
     * Socket obtained from the server component of the bridge
     */
    SDLNet_StreamSocket* sock_to_client = NULL;

    /**
     * Socket created to connect to the real server
     */
    SDLNet_StreamSocket* sock_to_server = NULL;

    packet_handler_t pack_handler_client = packet_handler_t(true);

    packet_handler_t pack_handler_server = packet_handler_t(false);

    Uint64 time_init = 0;

    Uint64 time_last_read = 0;

    bool skip = 0;

    int change_happened = 0;

    std::vector<packet_t*> packs_from_client;

    std::vector<packet_t*> packs_from_server;

    /* TODO: Store where the packet came from, or at least which handler was used */
    std::vector<packet_t*> packets;

    size_t packets_mem_footprint = 0;

    packet_viewer_dat_t packet_viewer_dat_server;
    packet_viewer_dat_t packet_viewer_dat_client;
    packet_viewer_dat_t packet_viewer_dat;

    world_diag_t world_diag;

    std::string kick_reason;

    void destroy()
    {
        for (size_t j = 0; j < packs_from_server.size(); j++)
            if (packs_from_server[j])
                delete packs_from_server[j];
        packs_from_server.resize(0);

        for (size_t j = 0; j < packs_from_client.size(); j++)
            if (packs_from_client[j])
                delete packs_from_client[j];
        packs_from_client.resize(0);

        if (sock_to_client)
        {
            SDLNet_DestroyStreamSocket(sock_to_client);
            sock_to_client = NULL;
        }

        if (sock_to_server)
        {
            SDLNet_DestroyStreamSocket(sock_to_server);
            sock_to_server = NULL;
        }
    }

    void kick(std::string reason)
    {
        kick_reason = reason;
        kick_sock(sock_to_client, reason);
        kick_sock(sock_to_server, reason);
    }

    void draw_packets(const char* label, std::vector<packet_t*>& packs, packet_viewer_dat_t& dat)
    {
        if (!ImGui::TreeNode(label))
            return;

        ImGui::Checkbox("Force Scroll", &dat.force_scroll);

        ImGui::SameLine();

        ImGui::Checkbox("Select Most Recent Packet", &dat.select_recent);

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

        float child_height = ImGui::GetMainViewport()->WorkSize.y / 3;

        float list_width = ImGui::CalcTextSize("x").x * 90 + ImGui::GetStyle().ScrollbarSize;

        if (list_width < ImGui::GetContentRegionAvail().x / 2)
            list_width = ImGui::GetContentRegionAvail().x / 2;

        if (ImGui::BeginListBox("##Packet Listbox", ImVec2(list_width, child_height)))
        {
            if (dat.select_recent)
            {
                for (size_t i = 0; i < packs.size(); i++)
                {
                    if (!dat.filters[packs[i]->id])
                        continue;
                    dat.sel = i;
                }
            }

            float text_spacing = ImGui::GetTextLineHeightWithSpacing();

            for (size_t i = 0; i < packs.size(); i++)
            {
                if (!dat.filters[packs[i]->id])
                    continue;
                ImGui::PushID(i);
                char buf[56];
                char buf2[88];
                if (!ImGui::IsRectVisible(ImVec2(20, text_spacing)))
                {
                    buf2[0] = 0;
                    ImGui::Spacing();
                }
                else
                {
                    snprintf(buf, ARR_SIZE(buf), "Packet[%zu]: 0x%02x (%s)", i, packs[i]->id, packs[i]->get_name());
                    int buf_len = strlen(buf);
                    memset(buf + buf_len, ' ', ARR_SIZE(buf) - buf_len);
                    buf[ARR_SIZE(buf) - 1] = '\0';

                    snprintf(buf2, ARR_SIZE(buf2), "%s %s", buf, timestamp_from_tick(packs[i]->assemble_tick).c_str());

                    if (ImGui::Selectable(buf2, dat.sel == i))
                        dat.sel = i;
                }

                ImGui::PopID();
            }

            if (dat.force_scroll)
                ImGui::SetScrollHereY(0.0f);

            ImGui::EndListBox();
        }

        ImGui::SameLine();

        if (!ImGui::BeginChild("Packet Table", ImVec2(-1, child_height), 0, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::EndChild();
            ImGui::TreePop();
            return;
        }

        if (dat.sel < packs.size())
            packs[dat.sel]->draw_imgui();
        else
        {
            PACKET_NEW_TABLE_CHOICE_IF("blank_table", goto skip_end_table;);
            ImGui::EndTable();
        skip_end_table:;
        }

        ImGui::EndChild();
        ImGui::TreePop();
    }

    void draw_memory_field(const char* name, size_t size, bool rate)
    {
        if (size < 1000u)
            TABLE_FIELD(name, "%zu bytes%s", size, rate ? "/s" : "");
        else if (size < (1000u * 1000u))
            TABLE_FIELD(name, "%.1f KB%s", (float)size / 1000.0f, rate ? "/s" : "");
        else if (size < (1000u * 1000u * 1000u))
            TABLE_FIELD(name, "%.2f MB%s", (float)(size / 1000u) / 1000.0f, rate ? "/s" : "");
        else if (size < (1000u * 1000u * 1000u * 1000u))
            TABLE_FIELD(name, "%.2f GB%s", (float)(size / (1000u * 1000u)) / 1000.0f, rate ? "/s" : "");
        else
            TABLE_FIELD(name, "%.2f TB%s", (float)(size / (1000u * 1000u * 1000u)) / 1000.0f, rate ? "/s" : "");
    }

    void calc_packet_data(std::vector<packet_t*>& packs, size_t& num, Uint64& tick_diff, size_t& mem_foot, Uint64 max_diff)
    {
        int misses = 0;
        num = 0;
        tick_diff = 0;
        mem_foot = 0;
        for (size_t i = packs.size() - 1; i < packs.size(); i--)
        {
            if (time_last_read - packs[i]->assemble_tick < max_diff)
            {
                tick_diff = time_last_read - packs[i]->assemble_tick;
                num++;
                mem_foot += packs[i]->mem_size() + sizeof(packet_t*);
            }
            else
            {
                misses++;
                if (misses > 1000)
                    return;
            }
        }
    }

    void draw_imgui()
    {
        float field_size = ImGui::CalcTextSize("Num packets from client (read - 10sec): ").x;
        ImGui::SeparatorText("Connection Info Table");
        if (ImGui::BeginTable("client_info_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, field_size);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            TABLE_FIELD("Connection init timestamp: ", "%s", timestamp_from_tick(time_init).c_str());
            TABLE_FIELD("Last read timestamp: ", "%s", timestamp_from_tick(time_last_read).c_str());
            TABLE_FIELD("Duration of connection: ", "%.2fs", ((float)((time_last_read - time_init) / 10)) / 100.0f);

            TABLE_FIELD("Num packets from client: ", "%zu", packs_from_client.size());
            TABLE_FIELD("Num packets from server: ", "%zu", packs_from_server.size());
            TABLE_FIELD("Num packets: ", "%zu", packets.size());

            size_t mem_foot = packets_mem_footprint + (packets.capacity() + packs_from_client.capacity() + packs_from_server.capacity()) * sizeof(packets[0]);

            draw_memory_field("Est. Packet memory footprint: ", mem_foot, false);
            draw_memory_field("Client data transfer: ", pack_handler_client.get_bytes_received(), false);
            draw_memory_field("Server data transfer: ", pack_handler_server.get_bytes_received(), false);

            size_t tdiff = time_last_read - time_init;
            if (tdiff != 0)
            {
                TABLE_FIELD("AVG Client packets/s: ", "%zu", packs_from_client.size() * 1000 / tdiff);
                TABLE_FIELD("AVG Server packets/s: ", "%zu", packs_from_server.size() * 1000 / tdiff);
                TABLE_FIELD("AVG Packets/s: ", "%zu", (packets.size()) * 1000 / tdiff);

                draw_memory_field("AVG Est. Memory footprint rate: ", mem_foot * 1000 / tdiff, true);
                draw_memory_field("AVG Client data rate: ", pack_handler_client.get_bytes_received() * 1000 / tdiff, true);
                draw_memory_field("AVG Server data rate: ", pack_handler_server.get_bytes_received() * 1000 / tdiff, true);
            }

            if (kick_reason.length())
            {
                TABLE_FIELD("Kick Reason: ", "%s", kick_reason.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::SeparatorText("Connection Info Table (last 10 seconds)");
        if (ImGui::BeginTable("client_info_table_10s", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, field_size);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            size_t packets_recent[3] = { 0, 0, 0 };
            Uint64 packets_recent_ticks[3] = { time_last_read, time_last_read, time_last_read };
            size_t packets_recent_foot[3] = { 0, 0, 0 };

            calc_packet_data(packs_from_client, packets_recent[0], packets_recent_ticks[0], packets_recent_foot[0], 10000);
            calc_packet_data(packs_from_server, packets_recent[1], packets_recent_ticks[1], packets_recent_foot[1], 10000);
            calc_packet_data(packets, packets_recent[2], packets_recent_ticks[2], packets_recent_foot[2], 10000);

            TABLE_FIELD("Num packets from client: ", "%zu", packets_recent[0]);
            TABLE_FIELD("Num packets from server: ", "%zu", packets_recent[1]);
            TABLE_FIELD("Num packets: ", "%zu", packets_recent[2]);

            draw_memory_field("Est. Packet mem footprint (client): ", packets_recent_foot[0], false);
            draw_memory_field("Est. Packet mem footprint (server): ", packets_recent_foot[1], false);
            draw_memory_field("Est. Packet mem footprint (total): ", packets_recent_foot[2], false);

            if (packets_recent_ticks[0] != 0)
                draw_memory_field("AVG Est. Memory footprint rate (client): ", packets_recent_foot[0] * 1000 / packets_recent_ticks[0], true);

            if (packets_recent_ticks[1] != 0)
                draw_memory_field("AVG Est. Memory footprint rate (server): ", packets_recent_foot[1] * 1000 / packets_recent_ticks[1], true);

            if (packets_recent_ticks[2] != 0)
                draw_memory_field("AVG Est. Memory footprint rate (total): ", packets_recent_foot[2] * 1000 / packets_recent_ticks[2], true);

            if (packets_recent_ticks[0] != 0)
                TABLE_FIELD("AVG Client packets/s: ", "%zu", packets_recent[0] * 1000 / packets_recent_ticks[0]);

            if (packets_recent_ticks[1] != 0)
                TABLE_FIELD("AVG Server packets/s: ", "%zu", packets_recent[1] * 1000 / packets_recent_ticks[1]);

            if (packets_recent_ticks[2] != 0)
                TABLE_FIELD("AVG Packets/s: ", "%zu", packets_recent[2] * 1000 / packets_recent_ticks[2]);

            ImGui::EndTable();
        }

        /* TODO: Put world information here (eg. entities, loaded chunks, time, health, player list, inventory, ...)*/
        if (ImGui::TreeNode("Basic world info"))
        {
            world_diag.draw_imgui_basic();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Chat"))
        {
            world_diag.draw_imgui_chat();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Chunks"))
        {
            /* TODO: A history of chunk loading and unloading + a list or visual of loaded chunks */
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Inventories"))
        {
            /* TODO: Listing of every window and the values sent for it */
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Player list"))
        {
            world_diag.draw_imgui_players();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Entities"))
        {
            /* TODO: Basic list of entities, their types (if declared), their positions, and maybe a history */
            /* TODO-OPT: More advanced list of entities */
            world_diag.draw_imgui_entities();
            ImGui::TreePop();
        }

        draw_packets("Packets from Client", packs_from_client, packet_viewer_dat_server);
        draw_packets("Packets from Server", packs_from_server, packet_viewer_dat_client);
        draw_packets("Packets", packets, packet_viewer_dat);
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

static convar_string_t address_listen("address_listen", "127.0.0.3", "Address to listen for connections");
static convar_string_t address_server("address_server", "127.0.0.1", "Address of the server to bridge to");

int main(int argc, const char** argv)
{
    /* KDevelop fully buffers the output and will not display anything */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    tetra::init("icrashstuff", "mcs_b181", argc, argv);
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
    SDLNet_Address* addr = resolve_addr(address_listen.get().c_str());
    SDLNet_Address* addr_real_server = resolve_addr(address_server.get().c_str());

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

    char imgui_win_title[128];

    snprintf(imgui_win_title, 128, "Client Inspector Window (\"%s\" -> \"%s\")", address_listen.get().c_str(), address_server.get().c_str());

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
            if (!SDLNet_AcceptClient(server, &new_client.sock_to_client))
            {
                LOG("SDLNet_AcceptClient: %s", SDL_GetError());
                exit(1);
            }
            if (new_client.sock_to_client == NULL)
            {
                done_client_searching = true;
                continue;
            }

            SDLNet_SimulateStreamPacketLoss(new_client.sock_to_client, 0);

            SDLNet_Address* client_addr = SDLNet_GetStreamSocketAddress(new_client.sock_to_client);
            LOG("New Socket: %s:%u", SDLNet_GetAddressString(client_addr), SDLNet_GetStreamSocketPort(new_client.sock_to_client));
            SDLNet_UnrefAddress(client_addr);

            new_client.sock_to_server = SDLNet_CreateClient(addr_real_server, 25565);

            new_client.time_last_read = SDL_GetTicks();
            new_client.time_init = new_client.time_last_read;

            if (new_client.sock_to_server)
                clients.push_back(new_client);
            else
            {
                LOG("Failed to connect to server!");
                if (new_client.sock_to_client)
                    SDLNet_DestroyStreamSocket(new_client.sock_to_client);
            }
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

                if (c->world_diag.send_chat)
                {
                    packet_chat_message_t cmsg;
                    cmsg.msg = c->world_diag.chat_buf;
                    send_buffer(c->sock_to_server, cmsg.assemble());
                    c->world_diag.chat_buf[0] = 0;
                    c->world_diag.send_chat = false;
                }

                packet_t* pack_from_client = c->pack_handler_client.get_next_packet(c->sock_to_client);
                packet_t* pack_from_server = c->pack_handler_server.get_next_packet(c->sock_to_server);

                if (pack_from_client)
                {
                    c->change_happened++;
                    c->time_last_read = sdl_tick_cur;
                    TRACE("Got packet from client[%zu]: 0x%02x", i, pack_server->id);
                    if (pack_from_client->id == PACKET_ID_CHAT_MSG)
                    {
                        packet_chat_message_t* p = (packet_chat_message_t*)pack_from_client;

                        if (p->msg == "/stop_bridge")
                            done = true;
                        else
                            send_buffer(c->sock_to_server, pack_from_client->assemble());
                    }
                    else
                        send_buffer(c->sock_to_server, pack_from_client->assemble());

                    c->world_diag.feed_packet_from_client(pack_from_client);
                    c->packets_mem_footprint += pack_from_client->mem_size();
                    c->packs_from_client.push_back(pack_from_client);
                    c->packets.push_back(pack_from_client);
                }
                else if (c->pack_handler_client.get_error().length())
                {
                    c->skip = true;
                    char buf[100];
                    snprintf(buf, ARR_SIZE(buf), "Error parsing packet from client[%zu]: %s", i, c->pack_handler_client.get_error().c_str());
                    c->kick(buf);
                }

                if (pack_from_server)
                {
                    c->change_happened++;
                    c->time_last_read = sdl_tick_cur;
                    TRACE("Got packet from server: 0x%02x", pack_client->id);
                    send_buffer(c->sock_to_client, pack_from_server->assemble());

                    c->world_diag.feed_packet_from_server(pack_from_server);
                    c->packets_mem_footprint += pack_from_server->mem_size();
                    c->packs_from_server.push_back(pack_from_server);
                    c->packets.push_back(pack_from_server);
                }
                else if (c->pack_handler_server.get_error().length() && !c->skip)
                {
                    c->skip = true;
                    char buf[100];
                    snprintf(buf, ARR_SIZE(buf), "Error parsing packet from server: %s", c->pack_handler_server.get_error().c_str());
                    c->kick(buf);
                }
            }
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
        Uint32 window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
        if (ImGui::Begin(imgui_win_title, NULL, window_flags))
        {
            for (size_t i = 0; i < clients.size(); i++)
            {
                ImGui::PushID(i);
                const char* txt_active = clients[i].skip ? "" : "(Active)";
                bool open = false;
                if (clients[i].world_diag.username.length())
                    open = ImGui::TreeNode("client", "Clients[%zu] (%s) %s", i, clients[i].world_diag.username.c_str(), txt_active);
                else
                    open = ImGui::TreeNode("client", "Clients[%zu] %s", i, txt_active);

                if (open)
                {
                    clients[i].draw_imgui();
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
        tetra::end_frame();
    }

    LOG("Destroying server");

    for (size_t i = 0; i < clients.size(); i++)
        clients[i].destroy();

    SDLNet_DestroyServer(server);

    SDLNet_Quit();
    tetra::deinit();
    SDL_Quit();
}

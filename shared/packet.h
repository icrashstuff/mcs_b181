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
#ifndef MCS_B181_PACKET_H
#define MCS_B181_PACKET_H

#include "sdl_net/include/SDL3_net/SDL_net.h"
#include <SDL3/SDL_stdinc.h>
#include <assert.h>
#include <string>
#include <vector>

#include "ids.h"
#include "misc.h"
#include "tetra/gui/imgui.h"

void assemble_string16(std::vector<Uint8>& dat, std::string str);

void assemble_bool(std::vector<Uint8>& dat, bool in);

void assemble_bytes(std::vector<Uint8>& dat, Uint8* in, size_t len);

void assemble_ubyte(std::vector<Uint8>& dat, Uint8 in);

void assemble_byte(std::vector<Uint8>& dat, Sint8 in);

void assemble_short(std::vector<Uint8>& dat, Sint16 in);

void assemble_int(std::vector<Uint8>& dat, Sint32 in);

void assemble_long(std::vector<Uint8>& dat, Sint64 in);

void assemble_float(std::vector<Uint8>& dat, float in);

void assemble_double(std::vector<Uint8>& dat, double in);

bool send_buffer(SDLNet_StreamSocket* sock, std::vector<Uint8> dat);

bool send_chat(SDLNet_StreamSocket* sock, const char* fmt, ...);

bool consume_bytes(SDLNet_StreamSocket* sock, int len);

bool read_ubyte(SDLNet_StreamSocket* sock, Uint8* out);

bool read_byte(SDLNet_StreamSocket* sock, Sint8* out);

bool read_short(SDLNet_StreamSocket* sock, Sint16* out);

bool read_int(SDLNet_StreamSocket* sock, Sint32* out);

bool read_long(SDLNet_StreamSocket* sock, Sint64* out);

bool read_float(SDLNet_StreamSocket* sock, float* out);

bool read_double(SDLNet_StreamSocket* sock, double* out);

bool read_string16(SDLNet_StreamSocket* sock, std::string& out);

enum packet_id_t : jubyte
{
    PACKET_ID_KEEP_ALIVE = 0x00,
    PACKET_ID_LOGIN_REQUEST = 0x01,
    PACKET_ID_HANDSHAKE = 0x02,
    PACKET_ID_CHAT_MSG = 0x03,
    PACKET_ID_UPDATE_TIME = 0x04,
    PACKET_ID_ENT_EQUIPMENT = 0x05,
    PACKET_ID_SPAWN_POS = 0x06,
    PACKET_ID_ENT_USE = 0x07,
    PACKET_ID_UPDATE_HEALTH = 0x08,
    PACKET_ID_RESPAWN = 0x09,
    PACKET_ID_PLAYER_ON_GROUND = 0x0a,
    PACKET_ID_PLAYER_POS = 0x0b,
    PACKET_ID_PLAYER_LOOK = 0x0c,
    PACKET_ID_PLAYER_POS_LOOK = 0x0d,
    PACKET_ID_PLAYER_DIG = 0x0e,
    PACKET_ID_PLAYER_PLACE = 0x0f,
    PACKET_ID_HOLD_CHANGE = 0x10,
    PACKET_ID_USE_BED = 0x11,
    PACKET_ID_ENT_ANIMATION = 0x12,
    PACKET_ID_ENT_ACTION = 0x13,
    PACKET_ID_ENT_SPAWN_NAMED = 0x14,
    PACKET_ID_ENT_SPAWN_PICKUP = 0x15,
    PACKET_ID_COLLECT_ITEM = 0x16,
    PACKET_ID_ADD_OBJ = 0x17,
    PACKET_ID_ENT_SPAWN_MOB = 0x18,
    PACKET_ID_ENT_SPAWN_PAINTING = 0x19,
    PACKET_ID_ENT_SPAWN_XP = 0x1a,

    /**
     * Wiki.vg notes this is as unused, and all field names are ???, so...
     */
    PACKET_ID_STANCE_UPDATE = 0x1b,

    /**/
    PACKET_ID_ENT_VELOCITY = 0x1c,
    PACKET_ID_ENT_DESTROY = 0x1d,
    PACKET_ID_ENT_ENSURE_SPAWN = 0x1e,
    PACKET_ID_ENT_MOVE_REL = 0x1f,
    PACKET_ID_ENT_LOOK = 0x20,
    PACKET_ID_ENT_LOOK_MOVE_REL = 0x21,
    PACKET_ID_ENT_MOVE_TELEPORT = 0x22,
    PACKET_ID_ENT_STATUS = 0x26,
    PACKET_ID_ENT_ATTACH = 0x27,
    PACKET_ID_ENT_METADATA = 0x28,
    PACKET_ID_ENT_EFFECT = 0x29,
    PACKET_ID_ENT_EFFECT_REMOVE = 0x2A,
    PACKET_ID_XP_SET = 0x2B,
    PACKET_ID_CHUNK_CACHE = 0x32,
    PACKET_ID_CHUNK_MAP = 0x33,
    PACKET_ID_BLOCK_CHANGE_MULTI = 0x34,
    PACKET_ID_BLOCK_CHANGE = 0x35,
    PACKET_ID_BLOCK_ACTION = 0x36,
    PACKET_ID_EXPLOSION = 0x3C,
    PACKET_ID_SFX = 0x3D,
    PACKET_ID_NEW_STATE = 0x46,
    PACKET_ID_THUNDERBOLT = 0x47,
    PACKET_ID_WINDOW_OPEN = 0x64,
    PACKET_ID_WINDOW_CLOSE = 0x65,
    PACKET_ID_WINDOW_CLICK = 0x66,
    PACKET_ID_WINDOW_SET_SLOT = 0x67,
    PACKET_ID_WINDOW_SET_ITEMS = 0x68,
    PACKET_ID_WINDOW_UPDATE_PROGRESS = 0x69,
    PACKET_ID_WINDOW_TRANSACTION = 0x6A,
    PACKET_ID_INV_CREATIVE_ACTION = 0x6B,
    PACKET_ID_UPDATE_SIGN = 0x82,
    PACKET_ID_ITEM_DATA = 0x83,
    PACKET_ID_INCREMENT_STATISTIC = 0xC8,
    PACKET_ID_PLAYER_LIST_ITEM = 0xC9,
    PACKET_ID_INVALID = 0xF0,
    PACKET_ID_SERVER_LIST_PING = 0xFE,
    PACKET_ID_KICK = 0xFF,

};

#define PACKET_NEW_TABLE_CHOICE_IF(NAME, ACTION)                                                        \
    do                                                                                                  \
    {                                                                                                   \
        if (!ImGui::BeginTable(NAME " info table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) \
            ACTION;                                                                                     \
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 16);  \
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 10);   \
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);                           \
        ImGui::TableHeadersRow();                                                                       \
    } while (0)

#define PACKET_NEW_TABLE(NAME) PACKET_NEW_TABLE_CHOICE_IF(NAME, return;)

#define PACKET_TABLE_FIELD_TYPE(field_type, field_text, fmt, ...) \
    do                                                            \
    {                                                             \
        ImGui::TableNextRow();                                    \
        ImGui::TableNextColumn();                                 \
        ImGui::TextUnformatted(field_text);                       \
        ImGui::TableNextColumn();                                 \
        ImGui::TextUnformatted(field_type);                       \
        ImGui::TableNextColumn();                                 \
        ImGui::Text(fmt, ##__VA_ARGS__);                          \
    } while (0)

#define PACKET_DEFINE_MEM_SIZE(additional) \
    size_t mem_size() { return sizeof(*this) + (additional); }

#define PACKET_TABLE_FIELD(field_text, fmt, ...) PACKET_TABLE_FIELD_TYPE("", field_text, fmt, ##__VA_ARGS__)

#define PACKET_TABLE_FIELD_ID() PACKET_TABLE_FIELD_TYPE("ubyte", "Packet ID: ", "0x%02x (%s)", id, get_name())
#define PACKET_TABLE_FIELD_JBOOL(var) PACKET_TABLE_FIELD_TYPE("bool", #var ": ", "%u", var)
#define PACKET_TABLE_FIELD_JUBYTE(var) PACKET_TABLE_FIELD_TYPE("ubyte", #var ": ", "%u", var)
#define PACKET_TABLE_FIELD_JBYTE(var) PACKET_TABLE_FIELD_TYPE("byte", #var ": ", "%d", var)
#define PACKET_TABLE_FIELD_JSHORT(var) PACKET_TABLE_FIELD_TYPE("short", #var ": ", "%d", var)
#define PACKET_TABLE_FIELD_JINT(var) PACKET_TABLE_FIELD_TYPE("int", #var ": ", "%d", var)
#define PACKET_TABLE_FIELD_JLONG(var) PACKET_TABLE_FIELD_TYPE("long", #var ": ", "%ld", var)
#define PACKET_TABLE_FIELD_JFLOAT(var) PACKET_TABLE_FIELD_TYPE("float", #var ": ", "%.3f", var)
#define PACKET_TABLE_FIELD_JDOUBLE(var) PACKET_TABLE_FIELD_TYPE("double", #var ": ", "%.3f", var)
#define PACKET_TABLE_FIELD_JSTRING16(var) PACKET_TABLE_FIELD_TYPE("string16", #var ": ", "\"%s\"", var.c_str())
#define PACKET_TABLE_FIELD_METADATA(var) PACKET_TABLE_FIELD_TYPE("metadata", #var " length: ", "%zu", var.size())
#define PACKET_TABLE_FIELD_SIZE_T(var) PACKET_TABLE_FIELD_TYPE("size_t", #var ": ", "%zu", var)

struct packet_t
{
    packet_id_t id = PACKET_ID_INVALID;

    /**
     * If non-zero, then this is the SDL tick (0.001s) when the packet was full assembled
     */
    Uint64 assemble_tick = 0;

    /**
     * Gets the name for the packet
     *
     * Wrapper around packet_t::get_name_for_id()
     */
    const char* get_name();

    /**
     * Get the name for the corresponding packet id
     */
    static const char* get_name_for_id(Uint8 pack_id);

    /**
     * Returns true if the id is a valid, false otherwise
     */
    static bool is_valid_id(Uint8 pack_id);

    /**
     * Returns the size of the packet_t struct, not the actual packet when sent
     */
    virtual PACKET_DEFINE_MEM_SIZE(0);

    virtual void draw_imgui()
    {
        if (!ImGui::BeginTable("Default Packet Info Table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            return;

        PACKET_TABLE_FIELD("Packet ID: ", "0x%02x (%s)", id, get_name());
        PACKET_TABLE_FIELD("Note: ", "draw_imgui() was not overridden for this packet");

        ImGui::EndTable();
    }

    virtual ~packet_t() {};

    virtual std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        return dat;
    };
};

#define PLAYER_DIG_STATUS_START_DIG 0
#define PLAYER_DIG_STATUS_FINISH_DIG 2
#define PLAYER_DIG_STATUS_DROP_ITEM 4
#define PLAYER_DIG_STATUS_SHOOT_ARROW 5

struct packet_player_place_t : packet_t
{
    packet_player_place_t() { id = PACKET_ID_PLAYER_PLACE; }

    jint x = 0;
    jbyte y = 0;
    jint z = 0;
    jbyte direction = 0;
    jshort block_item_id = 0;
    jbyte amount = 0;
    jshort damage = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_PLACE);
        dat.push_back(id);
        assemble_int(dat, x);
        assemble_byte(dat, y);
        assemble_int(dat, z);
        assemble_byte(dat, direction);
        assemble_short(dat, block_item_id);
        if (block_item_id >= 0)
        {
            assemble_byte(dat, amount);
            assemble_short(dat, damage);
        }
        assert(dat.size() == 13 || dat.size() == 16);
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(0);

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_player_place_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JINT(x);
        PACKET_TABLE_FIELD_JBYTE(y);
        PACKET_TABLE_FIELD_JINT(z);
        PACKET_TABLE_FIELD_JBYTE(direction);
        PACKET_TABLE_FIELD_JSHORT(block_item_id);
        PACKET_TABLE_FIELD_JBYTE(amount);
        PACKET_TABLE_FIELD_JSHORT(damage);

        ImGui::EndTable();
    }
};

#define ENT_ACTION_ID_CROUCH 1
#define ENT_ACTION_ID_UNCROUCH 2
#define ENT_ACTION_ID_LEAVE_BED 3
#define ENT_ACTION_ID_SPRINT_START 4
#define ENT_ACTION_ID_SPRINT_STOP 5

#define ENT_STATUS_HURT 2
#define ENT_STATUS_DEAD 3

/**
 * Server -> Client
 */
struct packet_add_obj_t : packet_t
{
    packet_add_obj_t() { id = PACKET_ID_ADD_OBJ; }

    jint eid = 0;
    union
    {
        jbyte type = 0;
        vehicle_type_t obj_type;
    };
    jint x = 0;
    jint y = 0;
    jint z = 0;
    jint fire_ball_thrower_id = 0;
    jshort unknown0 = 0;
    jshort unknown1 = 0;
    jshort unknown2 = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ADD_OBJ);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_byte(dat, type);
        assemble_int(dat, x);
        assemble_int(dat, y);
        assemble_int(dat, z);
        assemble_int(dat, fire_ball_thrower_id);
        if (fire_ball_thrower_id > 0)
        {
            assemble_short(dat, unknown0);
            assemble_short(dat, unknown1);
            assemble_short(dat, unknown2);
        }
        assert(dat.size() == (fire_ball_thrower_id > 0 ? 28 : 22));
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(0);

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_add_obj_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JINT(eid);
        PACKET_TABLE_FIELD_JBYTE(type);
        PACKET_TABLE_FIELD_JINT(x);
        PACKET_TABLE_FIELD_JINT(y);
        PACKET_TABLE_FIELD_JINT(z);
        PACKET_TABLE_FIELD_JINT(fire_ball_thrower_id);
        PACKET_TABLE_FIELD_JSHORT(unknown0);
        PACKET_TABLE_FIELD_JSHORT(unknown1);
        PACKET_TABLE_FIELD_JSHORT(unknown2);

        ImGui::EndTable();
    }
};

/**
 * Server -> Client
 */
struct packet_ent_spawn_mob_t : packet_t
{
    packet_ent_spawn_mob_t() { id = PACKET_ID_ENT_SPAWN_MOB; }

    jint eid = 0;
    union
    {
        jbyte type = 0;
        mob_type_t mob_type;
    };
    jint x = 0;
    jint y = 0;
    jint z = 0;
    jbyte yaw = 0;
    jbyte pitch = 0;

    std::vector<Uint8> metadata;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_SPAWN_MOB);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_byte(dat, type);
        assemble_int(dat, x);
        assemble_int(dat, y);
        assemble_int(dat, z);
        assemble_byte(dat, yaw);
        assemble_byte(dat, pitch);
        assemble_bytes(dat, metadata.data(), metadata.size());

        assert(dat.size() == 20 + metadata.size());
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(metadata.capacity());

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_ent_spawn_mob_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JINT(eid);
        PACKET_TABLE_FIELD_JBYTE(type);
        PACKET_TABLE_FIELD_JINT(x);
        PACKET_TABLE_FIELD_JINT(y);
        PACKET_TABLE_FIELD_JINT(z);
        PACKET_TABLE_FIELD_JBYTE(yaw);
        PACKET_TABLE_FIELD_JBYTE(pitch);

        PACKET_TABLE_FIELD_METADATA(metadata);

        ImGui::EndTable();
    }
};

struct packet_ent_metadata_t : packet_t
{
    packet_ent_metadata_t() { id = PACKET_ID_ENT_METADATA; }

    jint eid = 0;

    std::vector<Uint8> metadata;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_METADATA);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_bytes(dat, metadata.data(), metadata.size());
        assert(dat.size() == 5 + metadata.size());
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(metadata.capacity());

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_ent_metadata_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JINT(eid);
        PACKET_TABLE_FIELD_METADATA(metadata);

        ImGui::EndTable();
    }
};

struct packet_chunk_t : packet_t
{
    packet_chunk_t() { id = PACKET_ID_CHUNK_MAP; }

    jint block_x;
    jshort block_y;
    jint block_z;
    jbyte size_x;
    jbyte size_y;
    jbyte size_z;

    std::vector<Uint8> compressed_data;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        if (compressed_data.size() >= SDL_MAX_SINT32)
        {
            LOG_ERROR("Compressed_data too big!");
            return dat;
        }

        assert(id == PACKET_ID_CHUNK_MAP);
        dat.push_back(id);
        assemble_int(dat, block_x);
        assemble_short(dat, block_y);
        assemble_int(dat, block_z);
        assemble_byte(dat, size_x);
        assemble_byte(dat, size_y);
        assemble_byte(dat, size_z);

        assemble_int(dat, compressed_data.size());
        assemble_bytes(dat, compressed_data.data(), compressed_data.size());
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(compressed_data.capacity());

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_chunk_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JINT(block_x);
        PACKET_TABLE_FIELD_JSHORT(block_y);
        PACKET_TABLE_FIELD_JINT(block_z);
        PACKET_TABLE_FIELD_JBYTE(size_x);
        PACKET_TABLE_FIELD_JBYTE(size_y);
        PACKET_TABLE_FIELD_JBYTE(size_z);
        PACKET_TABLE_FIELD_SIZE_T(compressed_data.size());

        ImGui::EndTable();
    }
};

struct block_change_dat_t
{
    jbyte x;
    jbyte y;
    jbyte z;
    jbyte type;
    jbyte metadata;
};

struct packet_block_change_multi_t : packet_t
{
    packet_block_change_multi_t() { id = PACKET_ID_BLOCK_CHANGE_MULTI; }

    jint chunk_x;
    jint chunk_z;

    std::vector<block_change_dat_t> payload;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_BLOCK_CHANGE_MULTI);
        dat.push_back(id);
        assemble_int(dat, chunk_x);
        assemble_int(dat, chunk_z);
        assemble_short(dat, payload.size());

        for (size_t i = 0; i < payload.size(); i++)
        {
            jshort type = payload[i].x & 0x08 << 12;
            type += payload[i].z & 0x08 << 8;
            type += payload[i].y & 0x0F;
            assemble_short(dat, type);
        }

        for (size_t i = 0; i < payload.size(); i++)
        {
            assemble_byte(dat, payload[i].type);
        }

        for (size_t i = 0; i < payload.size(); i++)
        {
            assemble_byte(dat, payload[i].metadata);
        }
        assert(dat.size() == 11 + payload.size() * 4);
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(payload.capacity() * sizeof(payload[0]));

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_block_change_multi_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JINT(chunk_x);
        PACKET_TABLE_FIELD_JINT(chunk_z);
        PACKET_TABLE_FIELD_SIZE_T(payload.size());

        ImGui::EndTable();
    }
};

struct explosion_record_t
{
    jbyte off_x;
    jbyte off_y;
    jbyte off_z;
};

struct packet_explosion_t : packet_t
{
    packet_explosion_t() { id = PACKET_ID_EXPLOSION; }

    jdouble x;
    jdouble y;
    jdouble z;

    jfloat radius;

    std::vector<explosion_record_t> records;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_EXPLOSION);
        dat.push_back(id);
        assemble_double(dat, x);
        assemble_double(dat, y);
        assemble_double(dat, z);
        assemble_float(dat, radius);
        assemble_int(dat, records.size());

        for (size_t i = 0; i < records.size(); i++)
        {
            assemble_byte(dat, records[i].off_x);
            assemble_byte(dat, records[i].off_y);
            assemble_byte(dat, records[i].off_z);
        }
        assert(dat.size() == 33 + records.size() * 3);
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(records.capacity() * sizeof(records[0]));

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_explosion_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JDOUBLE(x);
        PACKET_TABLE_FIELD_JDOUBLE(y);
        PACKET_TABLE_FIELD_JDOUBLE(z);

        PACKET_TABLE_FIELD_JFLOAT(radius);

        PACKET_TABLE_FIELD_SIZE_T(records.size());

        ImGui::EndTable();
    }
};

#define PACK_NEW_STATE_REASON_INVALID_BED 0
#define PACK_NEW_STATE_REASON_RAIN_START 1
#define PACK_NEW_STATE_REASON_RAIN_END 2
#define PACK_NEW_STATE_REASON_CHANGE_MODE 3

struct inventory_item_t
{
    short id = -1;
    short damage = 0;
    jbyte quantity = 0;
};

struct packet_window_items_t : packet_t
{
    packet_window_items_t() { id = PACKET_ID_WINDOW_SET_ITEMS; }

    jbyte window_id = 0;

    std::vector<inventory_item_t> payload;

    void payload_from_array(inventory_item_t* arr, Uint32 len)
    {
        payload.resize(len);
        memcpy(payload.data(), arr, len * sizeof(inventory_item_t));
    }

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_WINDOW_SET_ITEMS);
        dat.push_back(id);
        assemble_byte(dat, window_id);
        assemble_short(dat, payload.size());
        for (size_t i = 0; i < payload.size(); i++)
        {
            assemble_short(dat, payload[i].id);
            if (payload[i].id > -1)
            {
                assemble_byte(dat, payload[i].quantity);
                assemble_short(dat, payload[i].damage);
            }
        }
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(payload.capacity() * sizeof(payload[0]));

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_window_items_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JBYTE(window_id);

        PACKET_TABLE_FIELD_SIZE_T(payload.size());

        ImGui::EndTable();
    }
};

struct packet_window_click_t : packet_t
{
    packet_window_click_t() { id = PACKET_ID_WINDOW_CLICK; }

    jbyte window_id = 0;
    jshort slot = 0;
    jbool right_click = 0;
    jshort action_num = 0;
    jbool shift = 0;

    inventory_item_t item;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_WINDOW_CLICK);
        dat.push_back(id);
        assemble_byte(dat, window_id);
        assemble_short(dat, slot);
        assemble_bool(dat, right_click);
        assemble_short(dat, action_num);
        assemble_bool(dat, shift);
        assemble_short(dat, item.id);
        if (item.id != -1)
        {
            assemble_byte(dat, item.quantity);
            assemble_short(dat, item.damage);
        }
        assert(dat.size() == (item.id != -1 ? 13 : 10));
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(0);

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_window_click_t");

        PACKET_TABLE_FIELD_ID();

        PACKET_TABLE_FIELD_JBYTE(window_id);
        PACKET_TABLE_FIELD_JSHORT(slot);
        PACKET_TABLE_FIELD_JBOOL(right_click);
        PACKET_TABLE_FIELD_JSHORT(action_num);
        PACKET_TABLE_FIELD_JBOOL(shift);
        PACKET_TABLE_FIELD_JSHORT(item.id);
        PACKET_TABLE_FIELD_JBYTE(item.quantity);
        PACKET_TABLE_FIELD_JSHORT(item.damage);

        ImGui::EndTable();
    }
};

struct packet_window_set_slot_t : packet_t
{
    packet_window_set_slot_t() { id = PACKET_ID_WINDOW_SET_SLOT; }

    jbyte window_id = 0;
    jshort slot = 0;
    inventory_item_t item;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_WINDOW_SET_SLOT);
        dat.push_back(id);
        assemble_byte(dat, window_id);
        assemble_short(dat, slot);
        assemble_short(dat, item.id);
        if (item.id != -1)
        {
            assemble_byte(dat, item.quantity);
            assemble_short(dat, item.damage);
        }
        assert(dat.size() == (item.id != -1 ? 9 : 6));
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(0);

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_window_set_slot_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JBYTE(window_id);
        PACKET_TABLE_FIELD_JSHORT(slot);
        PACKET_TABLE_FIELD_JSHORT(item.id);
        PACKET_TABLE_FIELD_JBYTE(item.quantity);
        PACKET_TABLE_FIELD_JSHORT(item.damage);

        ImGui::EndTable();
    }
};

struct packet_item_data_t : packet_t
{
    packet_item_data_t() { id = PACKET_ID_ITEM_DATA; }

    jshort item_type = 0;
    jshort item_id = 0;

    std::vector<Uint8> text;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ITEM_DATA);

        if (text.size() > 255)
        {
            LOG_ERROR("Text data too big!");
            return dat;
        }

        dat.push_back(id);
        assemble_short(dat, item_type);
        assemble_short(dat, item_id);

        assemble_ubyte(dat, text.size());
        assemble_bytes(dat, text.data(), text.size());

        assert(dat.size() == 6 + text.size());
        return dat;
    }

    PACKET_DEFINE_MEM_SIZE(text.capacity() * sizeof(text[0]));

    void draw_imgui()
    {
        PACKET_NEW_TABLE("packet_item_data_t");

        PACKET_TABLE_FIELD_ID();
        PACKET_TABLE_FIELD_JSHORT(item_type);
        PACKET_TABLE_FIELD_JSHORT(item_id);
        PACKET_TABLE_FIELD_SIZE_T(text.size());

        ImGui::EndTable();
    }
};

#include "packet_gen_def.h"

#undef PACKET_DEFINE_MEM_SIZE
#undef PACKET_NEW_TABLE
#undef PACKET_TABLE_FIELD
#undef PACKET_TABLE_FIELD_TYPE
#undef PACKET_TABLE_FIELD_ID
#undef PACKET_TABLE_FIELD_JBOOL
#undef PACKET_TABLE_FIELD_JUBYTE
#undef PACKET_TABLE_FIELD_JBYTE
#undef PACKET_TABLE_FIELD_JSHORT
#undef PACKET_TABLE_FIELD_JINT
#undef PACKET_TABLE_FIELD_JLONG
#undef PACKET_TABLE_FIELD_JFLOAT
#undef PACKET_TABLE_FIELD_JDOUBLE
#undef PACKET_TABLE_FIELD_JSTRING16
#undef PACKET_TABLE_FIELD_METADATA
#undef PACKET_TABLE_FIELD_SIZE_T

class packet_handler_t
{
public:
    /**
     * Initializes the packet handler
     *
     * @param is_server Determines how some packets are parsed (server vs. client)
     */
    packet_handler_t(bool is_server = true);

    /**
     * Returns NULL if no packet is available or on error
     */
    packet_t* get_next_packet(SDLNet_StreamSocket* sock);

    /**
     * Returns a non zero length error string when an error occurred
     */
    inline std::string get_error() { return err_str; }

    /**
     * Returns the SDL tick when the last complete packet was received
     */
    inline Uint64 get_last_packet_time() { return last_packet_time; }

    /**
     * Returns how many bytes the packet handler has read from the socket
     */
    inline size_t get_bytes_received() { return bytes_received; }

private:
    Uint64 last_packet_time;

    size_t bytes_received;

    std::vector<Uint8> buf;

    size_t buf_size;
    Uint16 packet_type;
    size_t len;
    size_t var_len_pos;
    int var_len;

    Uint16 last_metadata_cmd;

    bool is_server;

    std::string err_str;
};
#endif

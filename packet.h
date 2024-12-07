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

#include <SDL3/SDL_stdinc.h>
#include <assert.h>
#include <string>
#include <vector>

#include "misc.h"

typedef Uint8 jubyte;
typedef Uint8 jbool;
typedef Sint8 jbyte;
typedef Sint16 jshort;
typedef Sint32 jint;
typedef Sint64 jlong;
typedef float jfloat;
typedef double jdouble;

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
     * Wiki.vg notes this is unused, and all field names are ???, so...
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
    PACKET_ID_SERVER_LIST_PING = 0xFE,
    PACKET_ID_KICK = 0xFF,

};

struct packet_t
{
    packet_id_t id = PACKET_ID_KICK;

    const char* get_name();

    static const char* get_name_for_id(Uint8 pack_id);

    virtual ~packet_t() {};

    virtual std::vector<Uint8> assemble() = 0;
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
};

#define ENT_ACTION_ID_CROUCH 1
#define ENT_ACTION_ID_UNCROUCH 2
#define ENT_ACTION_ID_LEAVE_BED 3
#define ENT_ACTION_ID_SPRINT_START 4
#define ENT_ACTION_ID_SPRINT_STOP 5

#define PACK_ADD_OBJ_TYPE_BOAT 1
#define PACK_ADD_OBJ_TYPE_CART 10
#define PACK_ADD_OBJ_TYPE_CART_CHEST 11
#define PACK_ADD_OBJ_TYPE_CART_POWERED 12
#define PACK_ADD_OBJ_TYPE_TNT 50
#define PACK_ADD_OBJ_TYPE_ARROW 60
#define PACK_ADD_OBJ_TYPE_SNOWBALL 61
#define PACK_ADD_OBJ_TYPE_EGG 62
#define PACK_ADD_OBJ_TYPE_SAND 70
#define PACK_ADD_OBJ_TYPE_GRAVEL 71
#define PACK_ADD_OBJ_TYPE_FISH_FLOAT 90

/**
 * Server -> Client
 */
struct packet_add_obj_t : packet_t
{
    packet_add_obj_t() { id = PACKET_ID_ADD_OBJ; }

    jint eid = 0;
    jbyte type = 0;
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
};

/**
 * Server -> Client
 */
struct packet_ent_spawn_mob_t : packet_t
{
    packet_ent_spawn_mob_t() { id = PACKET_ID_ENT_SPAWN_MOB; }

    jint eid = 0;
    jbyte type = 0;
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
};

#include "packet_gen_def.h"

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

private:
    Uint64 last_packet_time;

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

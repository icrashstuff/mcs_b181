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

/**
 * TODO List
 * PACKET_ID_ENT_VELOCITY = 0x1c,
 * PACKET_ID_ENT_MOVE_REL = 0x1f,
 * PACKET_ID_ENT_LOOK = 0x20,
 * PACKET_ID_ENT_LOOK_MOVE_REL = 0x21,
 * PACKET_ID_ENT_STATUS = 0x26,
 * PACKET_ID_ENT_ATTACH = 0x27,
 * PACKET_ID_ENT_METADATA = 0x28,
 * PACKET_ID_ENT_EFFECT = 0x29,
 * PACKET_ID_ENT_EFFECT_REMOVE = 0x2A,
 * PACKET_ID_XP_SET = 0x2B,
 * PACKET_ID_BLOCK_CHANGE_MULTI = 0x34,
 * PACKET_ID_BLOCK_ACTION = 0x36,
 * PACKET_ID_EXPLOSION = 0x3C,
 * PACKET_ID_WINDOW_OPEN = 0x64,
 * PACKET_ID_WINDOW_CLICK = 0x66,
 * PACKET_ID_WINDOW_SET_SLOT = 0x67,
 * PACKET_ID_WINDOW_UPDATE_PROGRESS = 0x69,
 * PACKET_ID_WINDOW_TRANSACTION = 0x6A,
 * PACKET_ID_UPDATE_SIGN = 0x82,
 * PACKET_ID_ITEM_DATA = 0x83,
 * PACKET_ID_INCREMENT_STATISTIC = 0xC8,
 *
 * PACKET_ID_ENT_EQUIPMENT = 0x05,
 * PACKET_ID_SPAWN_POS = 0x06,
 * PACKET_ID_USE_BED = 0x11,
 * PACKET_ID_ENT_SPAWN_PICKUP = 0x15,
 * PACKET_ID_COLLECT_ITEM = 0x16,
 * PACKET_ID_ADD_OBJ = 0x17,
 * PACKET_ID_ENT_SPAWN_MOB = 0x18,
 * PACKET_ID_ENT_SPAWN_PAINTING = 0x19,
 * PACKET_ID_ENT_SPAWN_XP = 0x1a,
 */
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

    virtual ~packet_t() {};

    virtual std::vector<Uint8> assemble() = 0;
};

struct packet_keep_alive_t : packet_t
{
    packet_keep_alive_t() { id = PACKET_ID_KEEP_ALIVE; }

    jint keep_alive_id = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_KEEP_ALIVE);
        dat.push_back(id);
        assemble_int(dat, keep_alive_id);
        return dat;
    }
};

struct packet_login_request_c2s_t : packet_t
{
    packet_login_request_c2s_t() { id = PACKET_ID_LOGIN_REQUEST; }

    jint protocol_ver = 0;
    std::string username;
    jlong unused0 = 0;
    jint unused1 = 0;
    jbyte unused2 = 0;
    jbyte unused3 = 0;
    jubyte unused4 = 0;
    jubyte unused5 = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_LOGIN_REQUEST);
        dat.push_back(id);
        assemble_int(dat, protocol_ver);
        assemble_string16(dat, username);
        assemble_long(dat, unused0);
        assemble_int(dat, unused1);
        assemble_byte(dat, unused2);
        assemble_byte(dat, unused3);
        assemble_ubyte(dat, unused4);
        assemble_ubyte(dat, unused5);
        return dat;
    }
};

struct packet_login_request_s2c_t : packet_t
{
    packet_login_request_s2c_t() { id = PACKET_ID_LOGIN_REQUEST; }

    jint player_eid = 0;
    std::string unused;
    jlong seed = 0;
    jint mode = 0;
    jbyte dimension = 0;
    jbyte difficulty = 0;
    jubyte world_height = 0;
    jubyte max_players = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_LOGIN_REQUEST);
        dat.push_back(id);
        assemble_int(dat, player_eid);
        assemble_string16(dat, unused);
        assemble_long(dat, seed);
        assemble_int(dat, mode);
        /* Prevent sending invalid dimension values which crash the notchian client */
        assemble_byte(dat, dimension >= 0 ? 0 : -1);
        assemble_byte(dat, difficulty);
        assemble_ubyte(dat, world_height);
        assemble_ubyte(dat, max_players);
        return dat;
    }
};

struct packet_handshake_c2s_t : packet_t
{
    packet_handshake_c2s_t() { id = PACKET_ID_HANDSHAKE; }

    std::string username;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_HANDSHAKE);
        dat.push_back(id);
        assemble_string16(dat, username);
        return dat;
    }
};

struct packet_handshake_s2c_t : packet_t
{
    packet_handshake_s2c_t() { id = PACKET_ID_HANDSHAKE; }

    std::string connection_hash;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_HANDSHAKE);
        dat.push_back(id);
        assemble_string16(dat, connection_hash);
        return dat;
    }
};

struct packet_chat_message_t : packet_t
{
    packet_chat_message_t() { id = PACKET_ID_CHAT_MSG; }

    std::string msg;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_CHAT_MSG);
        dat.push_back(id);
        assemble_string16(dat, msg);
        return dat;
    }
};

struct packet_time_update_t : packet_t
{
    packet_time_update_t() { id = PACKET_ID_UPDATE_TIME; }

    jlong time = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_UPDATE_TIME);
        dat.push_back(id);
        assemble_long(dat, time);
        return dat;
    }
};

struct packet_ent_use_t : packet_t
{
    packet_ent_use_t() { id = PACKET_ID_ENT_USE; }

    jint user;
    jint target;
    jbool left_click;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_USE);
        dat.push_back(id);
        assemble_int(dat, user);
        assemble_int(dat, target);
        assemble_bool(dat, left_click);
        return dat;
    }
};

struct packet_health_t : packet_t
{
    packet_health_t() { id = PACKET_ID_UPDATE_HEALTH; }

    jshort health = 0;
    jshort food = 0;
    jfloat food_saturation = 0.0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_UPDATE_HEALTH);
        dat.push_back(id);
        assemble_short(dat, health);
        assemble_short(dat, food);
        assemble_float(dat, food_saturation);
        return dat;
    }
};

/**
 * Sent by client after hitting respawn
 * Sent by server to change dimension or as a response to the client
 */
struct packet_respawn_t : packet_t
{
    packet_respawn_t() { id = PACKET_ID_UPDATE_HEALTH; }

    jbyte dimension = 0;
    jbyte difficulty = 0;
    jbyte mode = 0;
    jshort world_height = 0;
    jlong seed = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_UPDATE_HEALTH);
        dat.push_back(id);
        assemble_byte(dat, dimension >= 0 ? 0 : -1);
        assemble_byte(dat, difficulty);
        assemble_byte(dat, mode);
        assemble_short(dat, world_height);
        assemble_long(dat, seed);
        return dat;
    }
};

/**
 * Client -> Server
 */
struct packet_on_ground_t : packet_t
{
    packet_on_ground_t() { id = PACKET_ID_PLAYER_ON_GROUND; }

    jbool on_ground = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_ON_GROUND);
        dat.push_back(id);
        assemble_bool(dat, on_ground);
        return dat;
    }
};

/**
 * Client -> Server
 */
struct packet_player_pos_t : packet_t
{
    packet_player_pos_t() { id = PACKET_ID_PLAYER_POS; }

    jdouble x = 0;
    jdouble y = 0;
    jdouble stance = 0;
    jdouble z = 0;
    jbool on_ground = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_POS);
        dat.push_back(id);
        assemble_double(dat, x);
        assemble_double(dat, y);
        assemble_double(dat, stance);
        assemble_double(dat, z);
        assemble_bool(dat, on_ground);
        return dat;
    }
};

/**
 * Client -> Server
 */
struct packet_player_look_t : packet_t
{
    packet_player_look_t() { id = PACKET_ID_PLAYER_LOOK; }

    jfloat yaw = 0;
    jfloat pitch = 0;
    jbool on_ground = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_LOOK);
        dat.push_back(id);
        assemble_float(dat, yaw);
        assemble_float(dat, pitch);
        assemble_bool(dat, on_ground);
        return dat;
    }
};

struct packet_player_pos_look_c2s_t : packet_t
{
    packet_player_pos_look_c2s_t() { id = PACKET_ID_PLAYER_POS_LOOK; }

    jdouble x = 0;
    jdouble y = 0;
    jdouble stance = 0;
    jdouble z = 0;
    jfloat yaw = 0;
    jfloat pitch = 0;
    jbool on_ground = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_POS_LOOK);
        dat.push_back(id);
        dat.push_back(id);
        assemble_double(dat, x);
        assemble_double(dat, y);
        assemble_double(dat, stance);
        assemble_double(dat, z);
        assemble_float(dat, yaw);
        assemble_float(dat, pitch);
        assemble_bool(dat, on_ground);
        return dat;
    }
};

struct packet_player_pos_look_s2c_t : packet_t
{
    packet_player_pos_look_s2c_t() { id = PACKET_ID_PLAYER_POS_LOOK; }

    jdouble x = 0;
    jdouble stance = 0;
    jdouble y = 0;
    jdouble z = 0;
    jfloat yaw = 0;
    jfloat pitch = 0;
    jbool on_ground = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_POS_LOOK);
        dat.push_back(id);
        assemble_double(dat, x);
        assemble_double(dat, stance);
        assemble_double(dat, y);
        assemble_double(dat, z);
        assemble_float(dat, yaw);
        assemble_float(dat, pitch);
        assemble_bool(dat, on_ground);
        return dat;
    }
};

#define PLAYER_DIG_STATUS_START_DIG 0
#define PLAYER_DIG_STATUS_FINISH_DIG 2
#define PLAYER_DIG_STATUS_DROP_ITEM 4
#define PLAYER_DIG_STATUS_SHOOT_ARROW 5

struct packet_player_dig_t : packet_t
{
    packet_player_dig_t() { id = PACKET_ID_PLAYER_DIG; }

    jbyte status = 0;
    jint x = 0;
    jbyte y = 0;
    jint z = 0;
    jbyte face = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_DIG);
        dat.push_back(id);
        assemble_byte(dat, status);
        assemble_int(dat, x);
        assemble_byte(dat, y);
        assemble_int(dat, z);
        assemble_byte(dat, face);
        return dat;
    }
};

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
        return dat;
    }
};

struct packet_hold_change_t : packet_t
{
    packet_hold_change_t() { id = PACKET_ID_HOLD_CHANGE; }

    jshort slot_id;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_HOLD_CHANGE);
        dat.push_back(id);
        assemble_short(dat, slot_id);
        return dat;
    }
};

struct packet_ent_animation_t : packet_t
{
    packet_ent_animation_t() { id = PACKET_ID_ENT_ANIMATION; }

    jint eid;
    jbyte animate;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_ANIMATION);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_byte(dat, animate);
        return dat;
    }
};

#define ENT_ACTION_ID_CROUCH 1
#define ENT_ACTION_ID_UNCROUCH 2
#define ENT_ACTION_ID_LEAVE_BED 3
#define ENT_ACTION_ID_SPRINT_START 4
#define ENT_ACTION_ID_SPRINT_STOP 5

struct packet_ent_action_t : packet_t
{
    packet_ent_action_t() { id = PACKET_ID_ENT_ACTION; }

    jint eid;
    jbyte action_id;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_ACTION);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_byte(dat, action_id);
        return dat;
    }
};

struct packet_named_ent_spawn_t : packet_t
{
    packet_named_ent_spawn_t() { id = PACKET_ID_ENT_SPAWN_NAMED; }

    jint eid = 0;
    std::string name;
    jint x = 0;
    jint y = 0;
    jint z = 0;
    jbyte rotation = 0;
    jbyte pitch = 0;
    jshort cur_item = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_SPAWN_NAMED);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_string16(dat, name);
        assemble_int(dat, x);
        assemble_int(dat, y);
        assemble_int(dat, z);
        assemble_byte(dat, rotation);
        assemble_byte(dat, pitch);
        assemble_short(dat, cur_item);
        return dat;
    }
};

struct packet_ent_create_t : packet_t
{
    packet_ent_create_t() { id = PACKET_ID_ENT_ENSURE_SPAWN; }

    jint eid;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_ENSURE_SPAWN);
        dat.push_back(id);
        assemble_int(dat, eid);
        return dat;
    }
};
struct packet_ent_destroy_t : packet_t
{
    packet_ent_destroy_t() { id = PACKET_ID_ENT_DESTROY; }

    jint eid;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_DESTROY);
        dat.push_back(id);
        assemble_int(dat, eid);
        return dat;
    }
};

struct packet_ent_teleport_t : packet_t
{
    packet_ent_teleport_t() { id = PACKET_ID_ENT_MOVE_TELEPORT; }

    jint eid = 0;
    jint x = 0;
    jint y = 0;
    jint z = 0;
    jbyte rotation = 0;
    jbyte pitch = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_ENT_MOVE_TELEPORT);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_int(dat, x);
        assemble_int(dat, y);
        assemble_int(dat, z);
        assemble_byte(dat, rotation);
        assemble_byte(dat, pitch);
        return dat;
    }
};

struct packet_chunk_cache_t : packet_t
{
    packet_chunk_cache_t() { id = PACKET_ID_CHUNK_CACHE; }

    jint chunk_x;
    jint chunk_z;
    jbool mode;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_CHUNK_CACHE);
        dat.push_back(id);
        assemble_int(dat, chunk_x);
        assemble_int(dat, chunk_z);
        assemble_bool(dat, mode);
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
            LOG_ERROR("Wompressed_data too big!");
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

struct packet_block_change_t : packet_t
{
    packet_block_change_t() { id = PACKET_ID_BLOCK_CHANGE; }

    jint block_x;
    jbyte block_y;
    jint block_z;
    jbyte type;
    jbyte metadata;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_BLOCK_CHANGE);
        dat.push_back(id);
        assemble_int(dat, block_x);
        assemble_byte(dat, block_y);
        assemble_int(dat, block_z);
        assemble_byte(dat, type);
        assemble_byte(dat, metadata & 0x0F);
        return dat;
    }
};

struct packet_sound_effect_t : packet_t
{
    packet_sound_effect_t() { id = PACKET_ID_SFX; }

    jint effect_id;
    jint x;
    jbyte y;
    jint z;
    jint sound_data;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_SFX);
        dat.push_back(id);
        assemble_int(dat, effect_id);
        assemble_int(dat, x);
        assemble_byte(dat, y);
        assemble_int(dat, z);
        assemble_int(dat, sound_data);
        return dat;
    }
};

#define PACK_NEW_STATE_REASON_INVALID_BED 0
#define PACK_NEW_STATE_REASON_RAIN_START 1
#define PACK_NEW_STATE_REASON_RAIN_END 2
#define PACK_NEW_STATE_REASON_CHANGE_MODE 3

struct packet_new_state_t : packet_t
{
    packet_new_state_t() { id = PACKET_ID_NEW_STATE; }

    jbyte reason = 0;
    jbyte mode = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_NEW_STATE);
        dat.push_back(id);
        assemble_byte(dat, reason);
        assemble_byte(dat, mode);
        return dat;
    }
};

struct packet_thunder_t : packet_t
{
    packet_thunder_t() { id = PACKET_ID_THUNDERBOLT; }

    jint eid = 0;
    jbool unknown = 0;
    jint x = 0;
    jint y = 0;
    jint z = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_THUNDERBOLT);
        dat.push_back(id);
        assemble_int(dat, eid);
        assemble_bool(dat, unknown);
        assemble_int(dat, x);
        assemble_int(dat, y);
        assemble_int(dat, z);
        return dat;
    }
};

struct packet_window_close_t : packet_t
{
    packet_window_close_t() { id = PACKET_ID_WINDOW_CLOSE; }

    jbyte window_id;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_WINDOW_CLOSE);
        dat.push_back(id);
        assemble_byte(dat, window_id);
        return dat;
    }
};

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

struct packet_inventory_action_creative_t : packet_t
{
    packet_inventory_action_creative_t() { id = PACKET_ID_INV_CREATIVE_ACTION; }

    jshort slot;
    jshort item_id;
    jshort quantity;
    jshort damage;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_INV_CREATIVE_ACTION);
        dat.push_back(id);
        assemble_short(dat, slot);
        assemble_short(dat, item_id);
        assemble_short(dat, quantity);
        assemble_short(dat, damage);
        return dat;
    }
};

struct packet_play_list_item_t : packet_t
{
    packet_play_list_item_t() { id = PACKET_ID_PLAYER_LIST_ITEM; }

    std::string username;
    jbool online;
    jshort ping;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_PLAYER_LIST_ITEM);
        dat.push_back(id);
        assemble_string16(dat, username);
        assemble_bool(dat, online);
        assemble_short(dat, ping);
        return dat;
    }
};

struct packet_server_list_ping_t : packet_t
{
    packet_server_list_ping_t() { id = PACKET_ID_SERVER_LIST_PING; }

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_SERVER_LIST_PING);
        dat.push_back(id);
        return dat;
    }
};

struct packet_kick_t : packet_t
{
    packet_kick_t() { id = PACKET_ID_KICK; }

    std::string reason;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == PACKET_ID_KICK);
        dat.push_back(id);
        assemble_string16(dat, reason);
        return dat;
    }
};

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
    Uint8 packet_type;
    size_t len;
    int var_len;

    bool is_server;

    std::string err_str;
};

#endif

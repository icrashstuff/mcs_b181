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

#define NO_ASSEMBLE()             \
    std::vector<Uint8> assemble() \
    {                             \
        std::vector<Uint8> dat;   \
        return dat;               \
    }

struct packet_t
{
    jubyte id = 0;

    virtual ~packet_t() {};

    virtual std::vector<Uint8> assemble() = 0;
};

struct packet_keep_alive_t : packet_t
{
    packet_keep_alive_t() { id = 0x00; }

    jint keep_alive_id = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x00);
        dat.push_back(id);
        assemble_int(dat, keep_alive_id);
        return dat;
    }
};

struct packet_login_request_c2s_t : packet_t
{
    packet_login_request_c2s_t() { id = 0x01; }

    jint protocol_ver = 0;
    std::string username;
    jlong unused0 = 0;
    jint unused1 = 0;
    jbyte unused2 = 0;
    jbyte unused3 = 0;
    jubyte unused4 = 0;
    jubyte unused5 = 0;

    NO_ASSEMBLE();
};

struct packet_login_request_s2c_t : packet_t
{
    packet_login_request_s2c_t() { id = 0x01; }

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
        assert(id == 0x01);
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
    packet_handshake_c2s_t() { id = 0x02; }

    std::string username;

    NO_ASSEMBLE();
};

struct packet_handshake_s2c_t : packet_t
{
    packet_handshake_s2c_t() { id = 0x02; }

    std::string connection_hash;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x02);
        dat.push_back(id);
        assemble_string16(dat, connection_hash);
        return dat;
    }
};

struct packet_chat_message_t : packet_t
{
    packet_chat_message_t() { id = 0x03; }

    std::string msg;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x03);
        dat.push_back(id);
        assemble_string16(dat, msg);
        return dat;
    }
};

struct packet_time_update_t : packet_t
{
    packet_time_update_t() { id = 0x04; }

    jlong time = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x04);
        dat.push_back(id);
        assemble_long(dat, time);
        return dat;
    }
};

struct packet_health_t : packet_t
{
    packet_health_t() { id = 0x08; }

    jshort health = 0;
    jshort food = 0;
    jfloat food_saturation = 0.0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x08);
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
    packet_respawn_t() { id = 0x09; }

    jbyte dimension = 0;
    jbyte difficulty = 0;
    jbyte mode = 0;
    jshort world_height = 0;
    jlong seed = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x09);
        dat.push_back(id);
        assemble_byte(dat, dimension >= 0 ? 0 : -1);
        assemble_byte(dat, difficulty);
        assemble_byte(dat, mode);
        assemble_short(dat, world_height);
        assemble_long(dat, seed);
        return dat;
    }
};

struct packet_on_ground_t : packet_t
{
    packet_on_ground_t() { id = 0x0a; }

    jbool on_ground = 0;

    NO_ASSEMBLE();
};

struct packet_player_pos_t : packet_t
{
    packet_player_pos_t() { id = 0x0b; }

    jdouble x = 0;
    jdouble y = 0;
    jdouble stance = 0;
    jdouble z = 0;
    jbool on_ground = 0;

    NO_ASSEMBLE();
};

struct packet_player_look_t : packet_t
{
    packet_player_look_t() { id = 0x0c; }

    jfloat yaw = 0;
    jfloat pitch = 0;
    jbool on_ground = 0;

    NO_ASSEMBLE();
};

struct packet_player_pos_look_c2s_t : packet_t
{
    packet_player_pos_look_c2s_t() { id = 0x0d; }

    jdouble x = 0;
    jdouble y = 0;
    jdouble stance = 0;
    jdouble z = 0;
    jfloat yaw = 0;
    jfloat pitch = 0;
    jbool on_ground = 0;

    NO_ASSEMBLE();
};

struct packet_player_pos_look_s2c_t : packet_t
{
    packet_player_pos_look_s2c_t() { id = 0x0d; }

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
        assert(id == 0x0d);
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
    packet_player_dig_t() { id = 0x0e; }

    jbyte status = 0;
    jint x = 0;
    jbyte y = 0;
    jint z = 0;
    jbyte face = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x0e);
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
    packet_player_place_t() { id = 0x0f; }

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
        assert(id == 0x0f);
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
    packet_hold_change_t() { id = 0x10; }

    jshort slot_id;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x10);
        dat.push_back(id);
        assemble_short(dat, slot_id);
        return dat;
    }
};

struct packet_ent_animation_t : packet_t
{
    packet_ent_animation_t() { id = 0x12; }

    jint eid;
    jbyte animate;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x12);
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
    packet_ent_action_t() { id = 0x13; }

    jint eid;
    jbyte action_id;

    NO_ASSEMBLE();
};

struct packet_prechunk_t : packet_t
{
    packet_prechunk_t() { id = 0x32; }

    jint chunk_x;
    jint chunk_z;
    jbool mode;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x32);
        dat.push_back(id);
        assemble_int(dat, chunk_x);
        assemble_int(dat, chunk_z);
        assemble_bool(dat, mode);
        return dat;
    }
};

struct packet_chunk_t : packet_t
{
    packet_chunk_t() { id = 0x33; }

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
            LOG("Error: compressed_data too big!");
            return dat;
        }

        assert(id == 0x33);
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
    packet_block_change_t() { id = 0x35; }

    jint block_x;
    jbyte block_y;
    jint block_z;
    jbyte type;
    jbyte metadata;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x35);
        dat.push_back(id);
        assemble_int(dat, block_x);
        assemble_byte(dat, block_y);
        assemble_int(dat, block_z);
        assemble_byte(dat, type);
        assemble_byte(dat, metadata & 0x0F);
        return dat;
    }
};

#define PACK_NEW_STATE_REASON_INVALID_BED 0
#define PACK_NEW_STATE_REASON_RAIN_START 1
#define PACK_NEW_STATE_REASON_RAIN_END 2
#define PACK_NEW_STATE_REASON_CHANGE_MODE 3

struct packet_new_state_t : packet_t
{
    packet_new_state_t() { id = 0x46; }

    jbyte reason = 0;
    jbyte mode = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x46);
        dat.push_back(id);
        assemble_byte(dat, reason);
        assemble_byte(dat, mode);
        return dat;
    }
};

struct packet_thunder_t : packet_t
{
    packet_thunder_t() { id = 0x47; }

    jint eid = 0;
    jbool unknown = 0;
    jint x = 0;
    jint y = 0;
    jint z = 0;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x47);
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
    packet_window_close_t() { id = 0x65; }

    jbyte window_id;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x65);
        dat.push_back(id);
        assemble_byte(dat, window_id);
        return dat;
    }
};

struct packet_inventory_action_creative_t : packet_t
{
    packet_inventory_action_creative_t() { id = 0x6b; }

    jshort slot;
    jshort item_id;
    jshort quantity;
    jshort damage;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0x6b);
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
    packet_play_list_item_t() { id = 0xc9; }

    std::string username;
    jbool online;
    jshort ping;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0xc9);
        dat.push_back(id);
        assemble_string16(dat, username);
        assemble_bool(dat, online);
        assemble_short(dat, ping);
        return dat;
    }
};

struct packet_server_list_ping_t : packet_t
{
    packet_server_list_ping_t() { id = 0xfe; }

    NO_ASSEMBLE();
};

struct packet_kick_t : packet_t
{
    packet_kick_t() { id = 0xff; }

    std::string reason;

    std::vector<Uint8> assemble()
    {
        std::vector<Uint8> dat;
        assert(id == 0xff);
        dat.push_back(id);
        assemble_string16(dat, reason);
        return dat;
    }
};

class packet_buffer_t
{
public:
    packet_buffer_t();

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

    std::string err_str;
};

#endif

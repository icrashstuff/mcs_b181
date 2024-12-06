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

#include <SDL3/SDL_endian.h>
#include <SDL3_net/SDL_net.h>

#include "java_strings.h"
#include "misc.h"
#include "packet.h"

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

void assemble_bool(std::vector<Uint8>& dat, bool in) { dat.push_back(in ? 1 : 0); }

void assemble_ubyte(std::vector<Uint8>& dat, Uint8 in) { dat.push_back(*(Uint8*)&in); }

void assemble_byte(std::vector<Uint8>& dat, Sint8 in) { dat.push_back(*(Uint8*)&in); }

void assemble_short(std::vector<Uint8>& dat, Sint16 in)
{
    Uint16 temp = SDL_Swap16BE(*(Uint16*)&in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_int(std::vector<Uint8>& dat, Sint32 in)
{
    Uint32 temp = SDL_Swap32BE(*(Uint32*)&in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_long(std::vector<Uint8>& dat, Sint64 in)
{
    Uint64 temp = SDL_Swap64BE(*(Uint64*)&in);
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

bool send_buffer(SDLNet_StreamSocket* sock, std::vector<Uint8> dat)
{
    if (dat.size())
        TRACE("Packet 0x%02x", dat[0]);
    return SDLNet_WriteToStreamSocket(sock, dat.data(), dat.size());
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
        *out = SDL_Swap16BE(*((Sint16*)buf_fixed));

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

#define BAIL_READ(width)                                                            \
    do                                                                              \
    {                                                                               \
        if (dat.size() < (width) + pos)                                             \
        {                                                                           \
            LOG_TRACE("dat.size(): %zu, expected: %zu", dat.size(), (width) + pos); \
            return 0;                                                               \
        }                                                                           \
    } while (0)

SDL_FORCE_INLINE bool read_ubyte(std::vector<Uint8>& dat, size_t& pos, Uint8* out)
{
    BAIL_READ(1);

    if (out)
        *out = *(dat.data() + pos);

    pos = pos + 1;
    return 1;
}

SDL_FORCE_INLINE bool read_byte(std::vector<Uint8>& dat, size_t& pos, Sint8* out)
{
    BAIL_READ(1);

    if (out)
        *out = *(dat.data() + pos);

    pos = pos + 1;
    return 1;
}

SDL_FORCE_INLINE bool read_short(std::vector<Uint8>& dat, size_t& pos, Sint16* out)
{
    BAIL_READ(2);

    if (out)
        *out = SDL_Swap16BE(*((Sint16*)(dat.data() + pos)));

    pos = pos + 2;
    return 1;
}

SDL_FORCE_INLINE bool read_int(std::vector<Uint8>& dat, size_t& pos, Sint32* out)
{
    BAIL_READ(4);

    if (out)
        *out = SDL_Swap32BE(*((Sint32*)(dat.data() + pos)));

    pos = pos + 4;
    return 1;
}

SDL_FORCE_INLINE bool read_long(std::vector<Uint8>& dat, size_t& pos, Sint64* out)
{
    BAIL_READ(8);

    if (out)
        *out = SDL_Swap64BE(*((Sint64*)(dat.data() + pos)));

    pos = pos + 8;
    return 1;
}

SDL_FORCE_INLINE bool read_float(std::vector<Uint8>& dat, size_t& pos, float* out)
{
    BAIL_READ(4);

    Uint32 temp = SDL_Swap32BE(*((Uint32*)(dat.data() + pos)));

    if (out)
        *out = *(float*)&temp;

    pos = pos + 4;
    return 1;
}

SDL_FORCE_INLINE bool read_double(std::vector<Uint8>& dat, size_t& pos, double* out)
{
    BAIL_READ(8);

    Uint64 temp = SDL_Swap64BE(*((Uint64*)(dat.data() + pos)));

    if (out)
        *out = *(double*)&temp;

    pos = pos + 8;
    return 1;
}

SDL_FORCE_INLINE bool read_string16(std::vector<Uint8>& dat, size_t& pos, std::string& out)
{
    BAIL_READ(2);

    Uint16 len = SDL_Swap16BE(*((Uint16*)(dat.data() + pos))) * 2;

    BAIL_READ(2 + len);

    out = UCS2_to_UTF8((Uint16*)(dat.data() + pos + 2), len / 2);
    pos = pos + 2 + len;
    return 1;
}

#undef BAIL_READ

#define MCS_B181_PACKET_GEN_IMPL
#include "packet_gen_def.h"

#define PACK_NAME(x)    \
    case PACKET_ID_##x: \
        return #x
const char* packet_t::get_name_for_id(Uint8 _id)
{
    switch (_id)
    {
        PACK_NAME(KEEP_ALIVE);
        PACK_NAME(LOGIN_REQUEST);
        PACK_NAME(HANDSHAKE);
        PACK_NAME(CHAT_MSG);
        PACK_NAME(UPDATE_TIME);
        PACK_NAME(ENT_EQUIPMENT);
        PACK_NAME(SPAWN_POS);
        PACK_NAME(ENT_USE);
        PACK_NAME(UPDATE_HEALTH);
        PACK_NAME(RESPAWN);
        PACK_NAME(PLAYER_ON_GROUND);
        PACK_NAME(PLAYER_POS);
        PACK_NAME(PLAYER_LOOK);
        PACK_NAME(PLAYER_POS_LOOK);
        PACK_NAME(PLAYER_DIG);
        PACK_NAME(PLAYER_PLACE);
        PACK_NAME(HOLD_CHANGE);
        PACK_NAME(USE_BED);
        PACK_NAME(ENT_ANIMATION);
        PACK_NAME(ENT_ACTION);
        PACK_NAME(ENT_SPAWN_NAMED);
        PACK_NAME(ENT_SPAWN_PICKUP);
        PACK_NAME(COLLECT_ITEM);
        PACK_NAME(ADD_OBJ);
        PACK_NAME(ENT_SPAWN_MOB);
        PACK_NAME(ENT_SPAWN_PAINTING);
        PACK_NAME(ENT_SPAWN_XP);
        PACK_NAME(STANCE_UPDATE);
        PACK_NAME(ENT_VELOCITY);
        PACK_NAME(ENT_DESTROY);
        PACK_NAME(ENT_ENSURE_SPAWN);
        PACK_NAME(ENT_MOVE_REL);
        PACK_NAME(ENT_LOOK);
        PACK_NAME(ENT_LOOK_MOVE_REL);
        PACK_NAME(ENT_MOVE_TELEPORT);
        PACK_NAME(ENT_STATUS);
        PACK_NAME(ENT_ATTACH);
        PACK_NAME(ENT_METADATA);
        PACK_NAME(ENT_EFFECT);
        PACK_NAME(ENT_EFFECT_REMOVE);
        PACK_NAME(XP_SET);
        PACK_NAME(CHUNK_CACHE);
        PACK_NAME(CHUNK_MAP);
        PACK_NAME(BLOCK_CHANGE_MULTI);
        PACK_NAME(BLOCK_CHANGE);
        PACK_NAME(BLOCK_ACTION);
        PACK_NAME(EXPLOSION);
        PACK_NAME(SFX);
        PACK_NAME(NEW_STATE);
        PACK_NAME(THUNDERBOLT);
        PACK_NAME(WINDOW_OPEN);
        PACK_NAME(WINDOW_CLOSE);
        PACK_NAME(WINDOW_CLICK);
        PACK_NAME(WINDOW_SET_SLOT);
        PACK_NAME(WINDOW_SET_ITEMS);
        PACK_NAME(WINDOW_UPDATE_PROGRESS);
        PACK_NAME(WINDOW_TRANSACTION);
        PACK_NAME(INV_CREATIVE_ACTION);
        PACK_NAME(UPDATE_SIGN);
        PACK_NAME(ITEM_DATA);
        PACK_NAME(INCREMENT_STATISTIC);
        PACK_NAME(PLAYER_LIST_ITEM);
        PACK_NAME(SERVER_LIST_PING);
        PACK_NAME(KICK);
    default:
        return "Unknown";
    }
}
#undef PACK_NAME

const char* packet_t::get_name() { return get_name_for_id(id); }

packet_handler_t::packet_handler_t(bool is_server_)
{
    is_server = is_server_;
    buf.reserve(1024);
    last_packet_time = SDL_GetTicks();
    buf_size = 0;
}

packet_t* packet_handler_t::get_next_packet(SDLNet_StreamSocket* sock)
{
    if (err_str.length() > 0)
        return NULL;

    /* This section handles starting a new packet */
    if (buf_size == 0)
    {
        buf.resize(128);
        if (SDLNet_GetConnectionStatus(sock) != 1)
        {
            err_str = "SDLNet_GetConnectionStatus failed!";
            return NULL;
        }

        int buf_inc = SDLNet_ReadFromStreamSocket(sock, buf.data(), 1);
        if (buf_inc < 0)
        {
            err_str = "Socket is dead!";
            return NULL;
        }

        buf_size += buf_inc;

        if (buf_size == 0)
            return NULL;

        packet_type = buf[0];
        len = 0;
        var_len = 0;

#define PACK_LENV(ID, LEN, VLEN) \
    case ID:                     \
    {                            \
        len = LEN;               \
        var_len = VLEN;          \
        break;                   \
    }
#define PACK_LEN(ID, LEN) PACK_LENV(ID, LEN, 0)
        switch (packet_type)
        {
            PACK_LEN(PACKET_ID_KEEP_ALIVE, 5);
            /* wiki.vg says that the login packet is 0x01 is >22 bytes long, but someone made a math error */
            PACK_LENV(PACKET_ID_LOGIN_REQUEST, 23, 1);
            PACK_LENV(PACKET_ID_HANDSHAKE, 3, 1);
            PACK_LENV(PACKET_ID_CHAT_MSG, 3, 1);
            PACK_LEN(PACKET_ID_ENT_USE, 10);
            PACK_LEN(PACKET_ID_RESPAWN, 14);
            PACK_LEN(PACKET_ID_PLAYER_ON_GROUND, 2);
            PACK_LEN(PACKET_ID_PLAYER_POS, 34);
            PACK_LEN(PACKET_ID_PLAYER_LOOK, 10);
            PACK_LEN(PACKET_ID_PLAYER_POS_LOOK, 42);
            PACK_LEN(PACKET_ID_PLAYER_DIG, 12);
            PACK_LENV(PACKET_ID_PLAYER_PLACE, 13, 1);
            PACK_LEN(PACKET_ID_HOLD_CHANGE, 3);
            PACK_LEN(PACKET_ID_ENT_ANIMATION, 6);
            PACK_LEN(PACKET_ID_ENT_ACTION, 6);
            PACK_LEN(PACKET_ID_WINDOW_CLOSE, 2);
            PACK_LEN(PACKET_ID_INV_CREATIVE_ACTION, 9);
            PACK_LEN(PACKET_ID_SERVER_LIST_PING, 1);
            PACK_LENV(PACKET_ID_KICK, 3, 1);

            PACK_LEN(PACKET_ID_ENT_EQUIPMENT, 11);
            PACK_LEN(PACKET_ID_SPAWN_POS, 13);
            PACK_LEN(PACKET_ID_USE_BED, 15);
            PACK_LEN(PACKET_ID_ENT_SPAWN_PICKUP, 25);
            PACK_LEN(PACKET_ID_COLLECT_ITEM, 9);
            PACK_LENV(PACKET_ID_ADD_OBJ, 22, 1);
            PACK_LENV(PACKET_ID_ENT_SPAWN_MOB, 21, 1);
            PACK_LENV(PACKET_ID_ENT_SPAWN_PAINTING, 22, 1);
            PACK_LEN(PACKET_ID_ENT_SPAWN_XP, 19)

            PACK_LEN(PACKET_ID_STANCE_UPDATE, 18);
            PACK_LEN(PACKET_ID_ENT_VELOCITY, 11);
            PACK_LEN(PACKET_ID_ENT_MOVE_REL, 8);
            PACK_LEN(PACKET_ID_ENT_LOOK, 7);
            PACK_LEN(PACKET_ID_ENT_LOOK_MOVE_REL, 10);
            PACK_LEN(PACKET_ID_ENT_STATUS, 6);
            PACK_LEN(PACKET_ID_ENT_ATTACH, 9);
            PACK_LENV(PACKET_ID_ENT_METADATA, 6, 1);
            PACK_LEN(PACKET_ID_ENT_EFFECT, 9);
            PACK_LEN(PACKET_ID_ENT_EFFECT_REMOVE, 6);
            PACK_LEN(PACKET_ID_XP_SET, 5);
            PACK_LENV(PACKET_ID_BLOCK_CHANGE_MULTI, 11, 1);
            PACK_LEN(PACKET_ID_BLOCK_ACTION, 13);
            PACK_LENV(PACKET_ID_EXPLOSION, 33, 1);
            PACK_LENV(PACKET_ID_WINDOW_OPEN, 6, 1);
            PACK_LENV(PACKET_ID_WINDOW_CLICK, 2, 1);
            PACK_LENV(PACKET_ID_WINDOW_SET_SLOT, 6, 1);
            PACK_LEN(PACKET_ID_WINDOW_UPDATE_PROGRESS, 6);
            PACK_LEN(PACKET_ID_WINDOW_TRANSACTION, 5);
            PACK_LENV(PACKET_ID_UPDATE_SIGN, 13, 4);
            PACK_LENV(PACKET_ID_ITEM_DATA, 6, 1);
            PACK_LEN(PACKET_ID_INCREMENT_STATISTIC, 6);

        default:
        {
            char buffer[64];
            snprintf(buffer, ARR_SIZE(buffer), "Unknown Packet ID: 0x%02x(%s)", packet_type, packet_t::get_name_for_id(packet_type));
            err_str = buffer;
            break;
        }
        }
#undef PACK_LEN
#undef PACK_LENV
    }

    /* This section reads the data and determines any variable length stuff as well */

    int change_happened = false;

    do
    {
        change_happened = false;

        if (len >= buf.size())
            buf.resize(len);

        int buf_inc = SDLNet_ReadFromStreamSocket(sock, buf.data() + buf_size, len - buf_size);

        if (buf_inc < 0)
        {
            err_str = "Socket is dead!";
            return NULL;
        }

        if (buf_inc)
            change_happened++;

        buf_size += buf_inc;

        // LOG("0x%02x %zu %zu %zu %d", packet_type, buf_inc, len, buf_size, var_len);

#define GET_STR_LEN(var_len_val, off, add)                                   \
    do                                                                       \
    {                                                                        \
        if (var_len == var_len_val && buf_size >= ((off) + 2))               \
        {                                                                    \
            len += (SDL_Swap16BE(*(Uint16*)(buf.data() + (off))) * 2) + add; \
            var_len--;                                                       \
            change_happened++;                                               \
        }                                                                    \
    } while (0)

        if (var_len > 0)
        {
            bool vlen_gen_result = 0;

            if (is_server)
                vlen_gen_result = vlen_gen_server(packet_type, buf, buf_size, change_happened, var_len, len);
            else
                vlen_gen_result = vlen_gen_client(packet_type, buf, buf_size, change_happened, var_len, len);

            if (!vlen_gen_result)
            {
                switch (packet_type)
                {
                case PACKET_ID_UPDATE_SIGN:
                {
                    GET_STR_LEN(1, len - 2, 0);
                    GET_STR_LEN(2, len - 2, 2);
                    GET_STR_LEN(3, len - 2, 2);
                    GET_STR_LEN(4, len - 2, 2);
                    break;
                }
                case PACKET_ID_PLAYER_PLACE:
                {
                    if (var_len == 1 && buf_size >= 13)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + 11));
                        len += ((*(Sint16*)&temp) >= 0) ? 3 : 0;
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                default:
                    char buffer[128];
                    snprintf(buffer, ARR_SIZE(buffer), "Packet ID: 0x%02x(%s): var_len set but no handler found", packet_type,
                        packet_t::get_name_for_id(packet_type));
                    err_str = buffer;
                    break;
                }
            }
        }
    } while (change_happened && (var_len || buf_size != len));

    if (buf_size != len || var_len)
        return NULL;

    TRACE("Packet 0x%02x(%s) has size: %zu(%zu) bytes", packet_type, len, buf_size, packet_t::get_name_for_id(packet_type));

    /* This section handles the actual packet parsing*/
    buf.resize(buf_size);
    last_packet_time = SDL_GetTicks();
    buf_size = 0;

    packet_t* packet = NULL;

#define P(type)          \
    packet = new type(); \
    type* p = (type*)packet;

    size_t pos = 1;
    int err = 0;

    bool parse_gen_result = 0;

    if (is_server)
        parse_gen_result = parse_gen_packets_server(packet_type, buf, err, pos, packet);
    else
        parse_gen_result = parse_gen_packets_client(packet_type, buf, err, pos, packet);

    if (!parse_gen_result)
    {
        switch (packet_type)
        {
        case PACKET_ID_PLAYER_PLACE:
        {
            P(packet_player_place_t);

            err += !read_int(buf, pos, &p->x);
            err += !read_byte(buf, pos, &p->y);
            err += !read_int(buf, pos, &p->z);
            err += !read_byte(buf, pos, &p->direction);
            err += !read_short(buf, pos, &p->block_item_id);
            if (p->block_item_id >= 0)
            {
                err += !read_byte(buf, pos, &p->amount);
                err += !read_short(buf, pos, &p->damage);
            }

            break;
        }

        default:
            char buffer[128];
            snprintf(buffer, ARR_SIZE(buffer), "Packet ID: 0x%02x(%s): missing final parse", packet_type, packet_t::get_name_for_id(packet_type));
            err_str = buffer;
            return NULL;
            break;
        }
    }

    TRACE("Packet buffer read: %zu/%zu", pos, buf.size());
    TRACE("Packet type(actual): 0x%02x(0x%02x)", packet->id, packet_type);
    assert(pos == buf.size());
    assert(packet->id == packet_type);
    packet->id = (packet_id_t)packet_type;

    if (err)
    {
        delete packet;
        packet = NULL;
        char buffer[128];
        snprintf(buffer, ARR_SIZE(buffer), "Error parsing packet with ID: 0x%02x(%s)", packet_type, packet_t::get_name_for_id(packet_type));
        err_str = buffer;
    }

    return packet;
}

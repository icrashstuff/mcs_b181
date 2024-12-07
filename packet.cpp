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
            assert(true);                                                           \
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

SDL_FORCE_INLINE bool read_bytes(std::vector<Uint8>& dat, size_t& pos, size_t len, Uint8* out)
{
    BAIL_READ(len);

    memcpy(out, dat.data() + pos, len);

    pos = pos + len;
    return 1;
}

static void dump_buffer(std::vector<Uint8>& buf)
{
    printf("\n\n\n=== %zu ===\n", buf.size());
    for (size_t i = 0; i < buf.size(); i++)
    {
        if (i % 8 == 0)
            printf("\n");
        printf("0x%02x ", buf.data()[i]);
    }
    printf("\n");
    for (size_t i = 0; i < buf.size(); i++)
    {
        if (i % 8 == 0)
            printf("\n");
        printf("%03d ", buf.data()[i]);
    }
    printf("\n");
}

SDL_FORCE_INLINE bool read_metadata(std::vector<Uint8>& dat, size_t& pos, std::vector<Uint8>& out)
{
    BAIL_READ(1);

    jubyte x;
    if (!read_ubyte(dat, pos, &x))
        return 0;

    assemble_ubyte(out, x);

    while (x != 127)
    {
        int err = 0;
        switch (x >> 5)
        {
        case 0:
        {
            jbyte r;
            err += !read_byte(dat, pos, &r);
            assemble_byte(out, r);
            break;
        }
        case 1:
        {
            jshort r;
            err += !read_short(dat, pos, &r);
            assemble_short(out, r);
            break;
        }
        case 2:
        {
            jint r;
            err += !read_int(dat, pos, &r);
            assemble_int(out, r);
            break;
        }
        case 3:
        {
            jfloat r;
            err += !read_float(dat, pos, &r);
            assemble_float(out, r);
            break;
        }
        case 4:
        {
            std::string r;
            err += !read_string16(dat, pos, r);
            assemble_string16(out, r);
            break;
        }
        case 5:
        {
            jshort r0;
            jbyte r1;
            jshort r2;
            err += !read_short(dat, pos, &r0);
            err += !read_byte(dat, pos, &r1);
            err += !read_short(dat, pos, &r2);
            if (err)
                return 0;
            assemble_short(out, r0);
            assemble_byte(out, r1);
            assemble_short(out, r2);
            break;
        }
        case 6:
        {
            jint r0;
            jint r1;
            jint r2;
            err += !read_int(dat, pos, &r0);
            err += !read_int(dat, pos, &r1);
            err += !read_int(dat, pos, &r2);
            if (err)
                return 0;
            assemble_int(out, r0);
            assemble_int(out, r1);
            assemble_int(out, r2);
            break;
        }
        default:
            return 0;
        }

        if (err)
            return 0;

        if (read_ubyte(dat, pos, &x))
            assemble_ubyte(out, x);
        else
            return 0;
    }

    assert(dat[dat.size() - 1] == 0x7f);

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

#define PACK_VALID(x)   \
    case PACKET_ID_##x: \
        return true
bool packet_t::is_valid_id(Uint8 _id)
{
    switch (_id)
    {
        PACK_VALID(KEEP_ALIVE);
        PACK_VALID(LOGIN_REQUEST);
        PACK_VALID(HANDSHAKE);
        PACK_VALID(CHAT_MSG);
        PACK_VALID(UPDATE_TIME);
        PACK_VALID(ENT_EQUIPMENT);
        PACK_VALID(SPAWN_POS);
        PACK_VALID(ENT_USE);
        PACK_VALID(UPDATE_HEALTH);
        PACK_VALID(RESPAWN);
        PACK_VALID(PLAYER_ON_GROUND);
        PACK_VALID(PLAYER_POS);
        PACK_VALID(PLAYER_LOOK);
        PACK_VALID(PLAYER_POS_LOOK);
        PACK_VALID(PLAYER_DIG);
        PACK_VALID(PLAYER_PLACE);
        PACK_VALID(HOLD_CHANGE);
        PACK_VALID(USE_BED);
        PACK_VALID(ENT_ANIMATION);
        PACK_VALID(ENT_ACTION);
        PACK_VALID(ENT_SPAWN_NAMED);
        PACK_VALID(ENT_SPAWN_PICKUP);
        PACK_VALID(COLLECT_ITEM);
        PACK_VALID(ADD_OBJ);
        PACK_VALID(ENT_SPAWN_MOB);
        PACK_VALID(ENT_SPAWN_PAINTING);
        PACK_VALID(ENT_SPAWN_XP);
        PACK_VALID(STANCE_UPDATE);
        PACK_VALID(ENT_VELOCITY);
        PACK_VALID(ENT_DESTROY);
        PACK_VALID(ENT_ENSURE_SPAWN);
        PACK_VALID(ENT_MOVE_REL);
        PACK_VALID(ENT_LOOK);
        PACK_VALID(ENT_LOOK_MOVE_REL);
        PACK_VALID(ENT_MOVE_TELEPORT);
        PACK_VALID(ENT_STATUS);
        PACK_VALID(ENT_ATTACH);
        PACK_VALID(ENT_METADATA);
        PACK_VALID(ENT_EFFECT);
        PACK_VALID(ENT_EFFECT_REMOVE);
        PACK_VALID(XP_SET);
        PACK_VALID(CHUNK_CACHE);
        PACK_VALID(CHUNK_MAP);
        PACK_VALID(BLOCK_CHANGE_MULTI);
        PACK_VALID(BLOCK_CHANGE);
        PACK_VALID(BLOCK_ACTION);
        PACK_VALID(EXPLOSION);
        PACK_VALID(SFX);
        PACK_VALID(NEW_STATE);
        PACK_VALID(THUNDERBOLT);
        PACK_VALID(WINDOW_OPEN);
        PACK_VALID(WINDOW_CLOSE);
        PACK_VALID(WINDOW_CLICK);
        PACK_VALID(WINDOW_SET_SLOT);
        PACK_VALID(WINDOW_SET_ITEMS);
        PACK_VALID(WINDOW_UPDATE_PROGRESS);
        PACK_VALID(WINDOW_TRANSACTION);
        PACK_VALID(INV_CREATIVE_ACTION);
        PACK_VALID(UPDATE_SIGN);
        PACK_VALID(ITEM_DATA);
        PACK_VALID(INCREMENT_STATISTIC);
        PACK_VALID(PLAYER_LIST_ITEM);
        PACK_VALID(SERVER_LIST_PING);
        PACK_VALID(KICK);
    default:
        return false;
    }
}
#undef PACK_VALID

const char* packet_t::get_name() { return get_name_for_id(id); }

packet_handler_t::packet_handler_t(bool is_server_)
{
    is_server = is_server_;
    buf.reserve(1024);
    last_packet_time = SDL_GetTicks();
    buf_size = 0;
    packet_type = 16384;
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
        var_len_pos = 0;
        last_metadata_cmd = 2048;

#define PACK_LENV(ID, LEN, VLEN) \
    case ID:                     \
    {                            \
        len = LEN;               \
        var_len = VLEN;          \
        break;                   \
    }
#define PACK_LEN(ID, LEN) PACK_LENV(ID, LEN, 0)
        bool length_gen_result = 0;

        if (is_server)
            length_gen_result = gen_lengths_server(packet_type, len, var_len);
        else
            length_gen_result = gen_lengths_client(packet_type, len, var_len);

        if (!length_gen_result)
        {
            switch (packet_type)
            {
                PACK_LENV(PACKET_ID_ADD_OBJ, 22, 1);
                PACK_LENV(PACKET_ID_BLOCK_CHANGE_MULTI, 11, 1);
                PACK_LENV(PACKET_ID_EXPLOSION, 33, 1);
                PACK_LENV(PACKET_ID_ENT_METADATA, 6, 1);
                PACK_LENV(PACKET_ID_ENT_SPAWN_MOB, 21, 1);
                PACK_LENV(PACKET_ID_PLAYER_PLACE, 13, 1);
                PACK_LENV(PACKET_ID_CHUNK_MAP, 18, 1);
                PACK_LENV(PACKET_ID_WINDOW_CLICK, 10, 1);
                PACK_LENV(PACKET_ID_WINDOW_SET_ITEMS, 4, (1 << 18));
                PACK_LENV(PACKET_ID_WINDOW_SET_SLOT, 6, 1);
                PACK_LENV(PACKET_ID_UPDATE_SIGN, 13, 4);
                PACK_LENV(PACKET_ID_ITEM_DATA, 6, 1);

            default:
            {
                char buffer[64];
                snprintf(buffer, ARR_SIZE(buffer), "Unknown Packet ID: 0x%02x(%s)", packet_type, packet_t::get_name_for_id(packet_type));
                err_str = buffer;
                break;
            }
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
                case PACKET_ID_ENT_METADATA:
                case PACKET_ID_ENT_SPAWN_MOB:
                {
                    if (!var_len_pos && packet_type == PACKET_ID_ENT_SPAWN_MOB)
                        var_len_pos = 20;
                    if (!var_len_pos && packet_type == PACKET_ID_ENT_METADATA)
                        var_len_pos = 5;
                    if (var_len == 1 && buf_size >= var_len_pos)
                    {
                        if (last_metadata_cmd == 2048)
                        {
                            last_metadata_cmd = buf[var_len_pos];
                        }
                        if (last_metadata_cmd == 127)
                        {
                            var_len--;
                            last_metadata_cmd = 1024;
                            change_happened++;
                        }
                        else if (last_metadata_cmd == 512)
                        {
                            if (buf_size >= var_len_pos)
                            {
                                Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + var_len_pos - 2));
                                LOG("Read stream %d", (*(Sint16*)&temp));
                                len += (*(Sint16*)&temp) * 2;
                                var_len_pos += (*(Sint16*)&temp) * 2;
                                last_metadata_cmd = 2048;
                                len++;
                                var_len_pos++;
                                change_happened++;
                            }
                        }
                        else
                        {
                            switch (last_metadata_cmd >> 5)
                            {
                            case 0:
                                len++;
                                var_len_pos++;
                                last_metadata_cmd = 2048;
                                change_happened++;
                                break;
                            case 1:
                                len += 2;
                                var_len_pos += 2;
                                last_metadata_cmd = 2048;
                                change_happened++;
                                break;
                            case 2:
                            case 3:
                                len += 4;
                                var_len_pos += 4;
                                last_metadata_cmd = 2048;
                                change_happened++;
                                break;
                            case 4:
                            {
                                LOG_WARN("String in metadata, things may break!");
                                len += 2;
                                var_len_pos += 2;
                                last_metadata_cmd = 512;
                                change_happened++;
                                break;
                            }
                            case 5:
                                len += 5;
                                var_len_pos += 5;
                                last_metadata_cmd = 2048;
                                change_happened++;
                                break;
                            case 6:
                                len += 12;
                                var_len_pos += 12;
                                last_metadata_cmd = 2048;
                                change_happened++;
                                break;
                            default:
                                char buffer[128];
                                snprintf(buffer, ARR_SIZE(buffer), "Unknown command %d(%d) in metadata stream", last_metadata_cmd, last_metadata_cmd >> 5);
                                err_str = buffer;
                                break;
                            }
                            len++;
                            var_len_pos++;
                        }
                    }
                    break;
                }
                case PACKET_ID_CHUNK_MAP:
                {
                    if (var_len == 1 && buf_size >= 18)
                    {
                        Uint32 temp = SDL_Swap32BE(*(Uint32*)(buf.data() + 14));
                        len += (*(Sint32*)&temp);
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                case PACKET_ID_WINDOW_SET_ITEMS:
                {
                    if (var_len == (1 << 18) && buf_size >= 4)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + 2));
                        len += (*(Sint16*)&temp) * 2;
                        var_len = (*(Sint16*)&temp);
                        var_len_pos = 4;
                        change_happened++;
                    }
                    if (var_len > 0 && var_len < (1 << 18) && buf_size >= var_len_pos + 2)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + var_len_pos));
                        var_len_pos += 2;
                        if ((*(Sint16*)&temp) != -1)
                        {
                            len += 3;
                            var_len_pos += 3;
                        }
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                case PACKET_ID_UPDATE_SIGN:
                {
                    GET_STR_LEN(1, len - 2, 0);
                    GET_STR_LEN(2, len - 2, 2);
                    GET_STR_LEN(3, len - 2, 2);
                    GET_STR_LEN(4, len - 2, 2);
                    break;
                }
                case PACKET_ID_BLOCK_CHANGE_MULTI:
                {
                    if (var_len == 1 && buf_size >= 11)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + 9));
                        len += (*(Sint16*)&temp) * 4;
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                case PACKET_ID_EXPLOSION:
                {
                    if (var_len == 1 && buf_size >= 33)
                    {
                        Uint32 temp = SDL_Swap32BE(*(Uint32*)(buf.data() + 29));
                        len += (*(Sint32*)&temp) * 3;
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                case PACKET_ID_WINDOW_CLICK:
                {
                    if (var_len == 1 && buf_size >= 10)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + 8));
                        len += ((*(Sint16*)&temp) >= 0) ? 3 : 0;
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                case PACKET_ID_ADD_OBJ:
                {
                    if (var_len == 1 && buf_size >= 22)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + 20));
                        len += ((*(Sint16*)&temp) > 0) ? 6 : 0;
                        var_len--;
                        change_happened++;
                    }
                    break;
                }
                case PACKET_ID_WINDOW_SET_SLOT:
                {
                    if (var_len == 1 && buf_size >= 6)
                    {
                        Uint16 temp = SDL_Swap16BE(*(Uint16*)(buf.data() + 4));
                        len += ((*(Sint16*)&temp) >= 0) ? 3 : 0;
                        var_len--;
                        change_happened++;
                    }
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
                case PACKET_ID_ITEM_DATA:
                {
                    if (var_len == 1 && buf_size >= 6)
                    {
                        Uint8 temp = *(buf.data() + 5);
                        len += temp;
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
    } while (change_happened && (var_len > 0 || buf_size != len));

    if (buf_size != len || var_len > 0)
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
        case PACKET_ID_ENT_METADATA:
        {
            P(packet_ent_metadata_t);

            err += !read_int(buf, pos, &p->eid);
            err += !read_metadata(buf, pos, p->metadata);

            break;
        }
        case PACKET_ID_ENT_SPAWN_MOB:
        {
            P(packet_ent_spawn_mob_t);

            err += !read_int(buf, pos, &p->eid);
            err += !read_byte(buf, pos, &p->type);
            err += !read_int(buf, pos, &p->x);
            err += !read_int(buf, pos, &p->y);
            err += !read_int(buf, pos, &p->z);
            err += !read_byte(buf, pos, &p->yaw);
            err += !read_byte(buf, pos, &p->pitch);
            err += !read_metadata(buf, pos, p->metadata);

            break;
        }
        case PACKET_ID_ADD_OBJ:
        {
            P(packet_add_obj_t);

            err += !read_int(buf, pos, &p->eid);
            err += !read_byte(buf, pos, &p->type);
            err += !read_int(buf, pos, &p->x);
            err += !read_int(buf, pos, &p->y);
            err += !read_int(buf, pos, &p->z);
            err += !read_int(buf, pos, &p->fire_ball_thrower_id);

            if (p->fire_ball_thrower_id > 0)
            {
                err += !read_short(buf, pos, &p->unknown0);
                err += !read_short(buf, pos, &p->unknown1);
                err += !read_short(buf, pos, &p->unknown2);
            }

            break;
        }
        case PACKET_ID_CHUNK_MAP:
        {
            P(packet_chunk_t);

            err += !read_int(buf, pos, &p->block_x);
            err += !read_short(buf, pos, &p->block_y);
            err += !read_int(buf, pos, &p->block_z);

            err += !read_byte(buf, pos, &p->size_x);
            err += !read_byte(buf, pos, &p->size_y);
            err += !read_byte(buf, pos, &p->size_z);

            int compressed_size;

            err += !read_int(buf, pos, &compressed_size);

            if (compressed_size < 0)
                err++;
            else
            {
                p->compressed_data.resize(compressed_size);
                err += !read_bytes(buf, pos, compressed_size, p->compressed_data.data());
            }

            break;
        }
        case PACKET_ID_BLOCK_CHANGE_MULTI:
        {
            P(packet_block_change_multi_t);

            err += !read_int(buf, pos, &p->chunk_x);
            err += !read_int(buf, pos, &p->chunk_z);

            short payload_size;

            err += !read_short(buf, pos, &payload_size);

            if (payload_size < 0)
                err++;

            if (!err)
            {
                p->payload.resize(payload_size);

                for (short i = 0; i < payload_size; i++)
                {
                    short coord;
                    err += !read_short(buf, pos, &coord);

                    p->payload[i].x = (coord >> 12) & 0x08;
                    p->payload[i].z = (coord >> 8) & 0x08;
                    p->payload[i].y = coord & 0x0F;
                }

                for (short i = 0; i < payload_size; i++)
                {
                    err += !read_byte(buf, pos, &p->payload[i].type);
                }

                for (short i = 0; i < payload_size; i++)
                {
                    err += !read_byte(buf, pos, &p->payload[i].metadata);
                }
            }

            break;
        }
        case PACKET_ID_EXPLOSION:
        {
            P(packet_explosion_t);

            err += !read_double(buf, pos, &p->x);
            err += !read_double(buf, pos, &p->y);
            err += !read_double(buf, pos, &p->z);
            err += !read_float(buf, pos, &p->radius);

            int record_count;

            err += !read_int(buf, pos, &record_count);

            if (record_count < 0)
                err++;

            if (!err)
            {
                p->records.resize(record_count);

                for (int i = 0; i < record_count; i++)
                {
                    err += !read_byte(buf, pos, &p->records[i].off_x);
                    err += !read_byte(buf, pos, &p->records[i].off_y);
                    err += !read_byte(buf, pos, &p->records[i].off_z);
                }
            }

            break;
        }
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
        case PACKET_ID_WINDOW_CLICK:
        {
            P(packet_window_click_t);

            err += !read_byte(buf, pos, &p->window_id);
            err += !read_short(buf, pos, &p->slot);
            err += !read_ubyte(buf, pos, &p->right_click);
            err += !read_short(buf, pos, &p->action_num);
            err += !read_ubyte(buf, pos, &p->shift);

            err += !read_short(buf, pos, &p->item.id);
            if (p->item.id != -1)
            {
                err += !read_byte(buf, pos, &p->item.quantity);
                err += !read_short(buf, pos, &p->item.damage);
            }

            break;
        }
        case PACKET_ID_WINDOW_SET_SLOT:
        {
            P(packet_window_set_slot_t);

            err += !read_byte(buf, pos, &p->window_id);
            err += !read_short(buf, pos, &p->slot);

            err += !read_short(buf, pos, &p->item.id);
            if (p->item.id != -1)
            {
                err += !read_byte(buf, pos, &p->item.quantity);
                err += !read_short(buf, pos, &p->item.damage);
            }

            break;
        }
        case PACKET_ID_WINDOW_SET_ITEMS:
        {
            P(packet_window_items_t);

            err += !read_byte(buf, pos, &p->window_id);

            short payload_size;

            err += !read_short(buf, pos, &payload_size);

            if (payload_size < 0)
                err++;

            if (!err)
            {
                p->payload.reserve(payload_size);
                LOG("Payload size: %d", payload_size);
                for (short i = 0; i < payload_size; i++)
                {
                    inventory_item_t t;
                    err += !read_short(buf, pos, &t.id);
                    if (t.id != -1)
                    {
                        err += !read_byte(buf, pos, &t.quantity);
                        err += !read_short(buf, pos, &t.damage);
                    }
                    p->payload.push_back(t);
                }
            }
            break;
        }
        case PACKET_ID_ITEM_DATA:
        {
            P(packet_item_data_t);

            err += !read_short(buf, pos, &p->item_type);
            err += !read_short(buf, pos, &p->item_id);

            jubyte text_len;

            err += !read_ubyte(buf, pos, &text_len);

            if (!err)
            {
                p->text.resize(text_len);

                err += !read_bytes(buf, pos, text_len, p->text.data());
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
    helpful_assert(pos == buf.size(), "Packet 0x%02x buffer read: %zu/%zu%s", packet_type, pos, buf.size(), err ? " (err)" : " (err not set)");
    helpful_assert(packet->id == packet_type, "Packet type(actual): 0x%02x(0x%02x)%s", packet->id, packet_type, err ? " (err)" : " (err not set)");
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

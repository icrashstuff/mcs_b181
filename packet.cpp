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

void assemble_ubyte(std::vector<Uint8>& dat, Uint8 in) { dat.push_back(in); }

void assemble_byte(std::vector<Uint8>& dat, Sint8 in) { dat.push_back(in); }

void assemble_short(std::vector<Uint8>& dat, Sint16 in)
{
    Sint16 temp = SDL_Swap16BE(in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_int(std::vector<Uint8>& dat, Sint32 in)
{
    Sint32 temp = SDL_Swap32BE(in);
    size_t loc = dat.size();
    dat.resize(dat.size() + sizeof(temp));
    memcpy(dat.data() + loc, &temp, sizeof(temp));
}

void assemble_long(std::vector<Uint8>& dat, Sint64 in)
{
    Sint64 temp = SDL_Swap64BE(in);
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

bool send_buffer(SDLNet_StreamSocket* sock, std::vector<Uint8> dat) { return SDLNet_WriteToStreamSocket(sock, dat.data(), dat.size()); }

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

#define BAIL_READ(width)                                     \
    do                                                       \
    {                                                        \
        if (dat.size() < (width) + pos)                      \
        {                                                    \
            LOG_TRACE("%zu %zu", dat.size(), (width) + pos); \
            return 0;                                        \
        }                                                    \
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

packet_buffer_t::packet_buffer_t()
{
    buf.reserve(1024);
    last_packet_time = SDL_GetTicks();
    buf_size = 0;
}

packet_t* packet_buffer_t::get_next_packet(SDLNet_StreamSocket* sock)
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

        switch (packet_type)
        {
        case 0x00:
        {
            len = 5;
            break;
        }
        case 0x01:
        {
            /* wiki.vg says that the login packet is 0x01 is >22 bytes long, but someone made a math error */
            len = 23;
            var_len = 1;
            break;
        }
        case 0x02:
        case 0x03:
        case 0xff:
        {
            len = 3;
            var_len = 1;
            break;
        }
        case 0x07:
        {
            len = 10;
            break;
        }
        case 0x09:
        {
            len = 14;
            break;
        }
        case 0x10:
        {
            len = 3;
            break;
        }
        case 0x12:
        case 0x13:
        {
            len = 6;
            break;
        }
        case 0x0a:
        case 0x65:
        {
            len = 2;
            break;
        }
        case 0x0b:
        {
            len = 34;
            break;
        }
        case 0x0c:
        {
            len = 10;
            break;
        }
        case 0x0d:
        {
            len = 42;
            break;
        }
        case 0x0f:
        {
            len = 13;
            var_len = 1;
            break;
        }
        case 0x0e:
        {
            len = 12;
            break;
        }
        case 0x6b:
        {
            len = 9;
            break;
        }
        case 0xfe:
        {
            len = 1;
            break;
        }
        default:
        {
            char buffer[32];
            snprintf(buffer, ARR_SIZE(buffer), "Unknown Packet ID: 0x%02x", packet_type);
            err_str = buffer;
            break;
        }
        }
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

#define GET_STR_LEN(var_len_val, off)                                  \
    do                                                                 \
    {                                                                  \
        if (var_len == var_len_val && buf_size >= ((off) + 2))         \
        {                                                              \
            len += (SDL_Swap16BE(*(Uint16*)(buf.data() + (off))) * 2); \
            var_len--;                                                 \
            change_happened++;                                         \
        }                                                              \
    } while (0)

        if (var_len > 0)
        {
            switch (packet_type)
            {
            case 0x01:
            {
                GET_STR_LEN(1, 5);
                break;
            }
            case 0x02:
            case 0x03:
            case 0xff:
            {
                GET_STR_LEN(1, 1);
                break;
            }
            case 0x0f:
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
                err_str = "var_len set when packet does not support var_len";
                break;
            }
        }
    } while (change_happened && (var_len || buf_size != len));

    if (buf_size != len || var_len)
        return NULL;

    TRACE("Packet 0x%02x has size: %zu(%zu) bytes", packet_type, len, buf_size);

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
    switch (packet_type)
    {
    case 0x00:
    {
        P(packet_keep_alive_t);

        err += !read_int(buf, pos, &p->keep_alive_id);

        break;
    }
    case 0x01:
    {
        P(packet_login_request_c2s_t);

        err += !read_int(buf, pos, &p->protocol_ver);
        err += !read_string16(buf, pos, p->username);
        err += !read_long(buf, pos, &p->unused0);
        err += !read_int(buf, pos, &p->unused1);
        err += !read_byte(buf, pos, &p->unused2);
        err += !read_byte(buf, pos, &p->unused3);
        err += !read_ubyte(buf, pos, &p->unused4);
        err += !read_ubyte(buf, pos, &p->unused5);

        break;
    }
    case 0x02:
    {
        P(packet_handshake_c2s_t);

        err += !read_string16(buf, pos, p->username);

        break;
    }
    case 0x03:
    {
        P(packet_chat_message_t);

        err += !read_string16(buf, pos, p->msg);

        break;
    }
    case 0x07:
    {
        P(packet_ent_use_t);

        err += !read_int(buf, pos, &p->user);
        err += !read_int(buf, pos, &p->target);
        err += !read_ubyte(buf, pos, &p->left_click);

        break;
    }
    case 0x09:
    {
        P(packet_respawn_t);

        err += !read_byte(buf, pos, &p->dimension);
        err += !read_byte(buf, pos, &p->difficulty);
        err += !read_byte(buf, pos, &p->mode);
        err += !read_short(buf, pos, &p->world_height);
        err += !read_long(buf, pos, &p->seed);

        break;
    }
    case 0x0a:
    {
        P(packet_on_ground_t);

        err += !read_ubyte(buf, pos, &p->on_ground);

        break;
    }
    case 0x0b:
    {
        P(packet_player_pos_t);

        err += !read_double(buf, pos, &p->x);
        err += !read_double(buf, pos, &p->y);
        err += !read_double(buf, pos, &p->stance);
        err += !read_double(buf, pos, &p->z);
        err += !read_ubyte(buf, pos, &p->on_ground);

        break;
    }
    case 0x0c:
    {
        P(packet_player_look_t);

        err += !read_float(buf, pos, &p->yaw);
        err += !read_float(buf, pos, &p->pitch);
        err += !read_ubyte(buf, pos, &p->on_ground);

        break;
    }
    case 0x0d:
    {
        P(packet_player_pos_look_c2s_t);

        err += !read_double(buf, pos, &p->x);
        err += !read_double(buf, pos, &p->y);
        err += !read_double(buf, pos, &p->stance);
        err += !read_double(buf, pos, &p->z);
        err += !read_float(buf, pos, &p->yaw);
        err += !read_float(buf, pos, &p->pitch);
        err += !read_ubyte(buf, pos, &p->on_ground);

        break;
    }
    case 0x0e:
    {
        P(packet_player_dig_t);

        err += !read_byte(buf, pos, &p->status);
        err += !read_int(buf, pos, &p->x);
        err += !read_byte(buf, pos, &p->y);
        err += !read_int(buf, pos, &p->z);
        err += !read_byte(buf, pos, &p->face);

        break;
    }
    case 0x0f:
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
    case 0x10:
    {
        P(packet_hold_change_t);

        err += !read_short(buf, pos, &p->slot_id);

        break;
    }
    case 0x12:
    {
        P(packet_ent_animation_t);

        err += !read_int(buf, pos, &p->eid);
        err += !read_byte(buf, pos, &p->animate);

        break;
    }
    case 0x13:
    {
        P(packet_ent_action_t);

        err += !read_int(buf, pos, &p->eid);
        err += !read_byte(buf, pos, &p->action_id);

        break;
    }
    case 0x65:
    {
        P(packet_window_close_t);

        err += !read_byte(buf, pos, &p->window_id);

        break;
    }
    case 0x6b:
    {
        P(packet_inventory_action_creative_t);

        err += !read_short(buf, pos, &p->slot);
        err += !read_short(buf, pos, &p->item_id);
        err += !read_short(buf, pos, &p->quantity);
        err += !read_short(buf, pos, &p->damage);

        break;
    }
    case 0xfe:
    {
        packet = new packet_server_list_ping_t();
        break;
    }
    case 0xff:
    {
        P(packet_kick_t);

        err += !read_string16(buf, pos, p->reason);

        break;
    }

    default:
        err_str = "Packet missing final parse";
        return NULL;
        break;
    }

    TRACE("Packet buffer read: %zu/%zu", pos, buf.size());
    TRACE("Packet type(actual): 0x%02x(0x%02x)", packet->id, packet_type);
    assert(pos == buf.size());
    assert(packet->id == packet_type);
    packet->id = packet_type;

    if (err)
    {
        delete packet;
        packet = NULL;
        char buffer[64];
        snprintf(buffer, ARR_SIZE(buffer), "Error parsing packet with ID: 0x%02x", packet_type);
        err_str = buffer;
    }

    return packet;
}

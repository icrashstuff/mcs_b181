/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
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

#include "connection.h"

#include <zlib.h>

#pragma GCC push_options
#pragma GCC optimize("Ofast")
/**
 * Parse/Decompress a chunk packet and write the changes to the level
 *
 * @param level Level to modify
 * @param p Chunk packet to parse and decompress
 * @param buffer Buffer used for temporarily storing data, the intent is to reduce allocations \n
 *               by allowing the same buffer to be reused over the life of the connection
 */
static void decompress_chunk_packet(level_t* const level, const packet_chunk_t* const p, std::vector<Uint8>& buffer)
{
    if (p->size_x < 0 || p->size_y < 0 || p->size_z < 0)
    {
        dc_log_error("Chunk packet with invalid size of <%d, %d, %d>", p->size_x, p->size_y, p->size_z);
        return;
    }

    const int min_block_x = p->block_x;
    const int min_block_y = p->block_y;
    const int min_block_z = p->block_z;
    const int max_block_x = min_block_x + p->size_x;
    const int max_block_y = min_block_y + p->size_y;
    const int max_block_z = min_block_z + p->size_z;

    const int min_chunk_x = min_block_x >> 4;
    const int min_chunk_y = min_block_y >> 4;
    const int min_chunk_z = min_block_z >> 4;
    const int max_chunk_x = max_block_x >> 4;
    const int max_chunk_y = max_block_y >> 4;
    const int max_chunk_z = max_block_z >> 4;

    const int real_size_x = p->size_x + 1;
    const int real_size_y = p->size_y + 1;
    const int real_size_z = p->size_z + 1;
    const int real_volume = (real_size_x * real_size_y * real_size_z);

    /* Oversize the buffer a small amount in case weirdness occurs  */
    size_t uncompressed_size = real_size_x * real_size_y * real_size_z * 41 / 16;
    if (uncompressed_size > buffer.size())
    {
        dc_log("Resizing decompression buffer to %zu", uncompressed_size);
        buffer.resize(uncompressed_size, 0);
    }
    Uint8* uncompressed = buffer.data();
    memset(uncompressed + uncompressed_size, 0, buffer.size() - uncompressed_size);

    int decompress_result = uncompress(uncompressed, &uncompressed_size, p->compressed_data.data(), p->compressed_data.size());

    TRACE("%d %d %d | %d %d %d", min_chunk_x, min_chunk_y, min_chunk_z, max_chunk_x, max_chunk_y, max_chunk_z);

    if (decompress_result != Z_OK)
    {
        const char* err_str = "Unknown error";
        switch (decompress_result)
        {
            ENUM_SWITCH_CASE(err_str, Z_OK);
            ENUM_SWITCH_CASE(err_str, Z_STREAM_END);
            ENUM_SWITCH_CASE(err_str, Z_NEED_DICT);
            ENUM_SWITCH_CASE(err_str, Z_ERRNO);
            ENUM_SWITCH_CASE(err_str, Z_STREAM_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_DATA_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_MEM_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_BUF_ERROR);
            ENUM_SWITCH_CASE(err_str, Z_VERSION_ERROR);
        default:
            break;
        }
        dc_log_error("Error %d (%s) decompressing chunk data!", decompress_result, err_str);
    }

    // TODO: Create nonexistent chunks
    for (chunk_cubic_t* c : level->get_chunk_vec())
    {
        if (!BETWEEN_INCL(c->pos.x, min_chunk_x, max_chunk_x))
            continue;
        if (!BETWEEN_INCL(c->pos.y, min_chunk_y, max_chunk_y))
            continue;
        if (!BETWEEN_INCL(c->pos.z, min_chunk_z, max_chunk_z))
            continue;

        for (int x = 0; x < SUBCHUNK_SIZE_X; x++)
        {
            const int block_x = x + ((c->pos.x) << 4);
            if (!BETWEEN_INCL(block_x, min_block_x, max_block_x))
                continue;
            const int uncompressed_x = block_x - min_block_x;

            for (int z = 0; z < SUBCHUNK_SIZE_Z; z++)
            {
                const int block_z = z + ((c->pos.z) << 4);
                if (!BETWEEN_INCL(block_z, min_block_z, max_block_z))
                    continue;
                const int uncompressed_z = block_z - min_block_z;

                for (int y = 0; y < SUBCHUNK_SIZE_Y; y++)
                {
                    const int block_y = y + ((c->pos.y) << 4);
                    if (!BETWEEN_INCL(block_y, min_block_y, max_block_y))
                        continue;
                    const int uncompressed_y = block_y - min_block_y;

                    size_t index = uncompressed_y + (uncompressed_z * (real_size_y)) + (uncompressed_x * (real_size_y) * (real_size_z));
                    Uint8 type = uncompressed[index];

                    index += real_volume * 2;
                    Uint8 meta;
                    if (index % 2 == 1)
                        meta = (uncompressed[index / 2] >> 4) & 0x0F;
                    else
                        meta = uncompressed[index / 2] & 0x0F;

                    index += real_volume;
                    Uint8 light_block;
                    if (index % 2 == 1)
                        light_block = (uncompressed[index / 2] >> 4) & 0x0F;
                    else
                        light_block = uncompressed[index / 2] & 0x0F;

                    index += real_volume;
                    Uint8 light_sky;
                    if (index % 2 == 1)
                        light_sky = (uncompressed[index / 2] >> 4) & 0x0F;
                    else
                        light_sky = uncompressed[index / 2] & 0x0F;

                    c->set_type(x, y, z, type);
                    c->set_metadata(x, y, z, meta);
                    c->set_light_block(x, y, z, light_block);
                    c->set_light_sky(x, y, z, light_sky);
                }
            }
        }

        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
    }
}
#pragma GCC pop_options

void connection_t::run(level_t* const level)
{
    if (status == connection_status_t::CONNECTION_UNINITIALIZED)
    {
        dc_log_error("Connection must be initialized before running");
        return;
    }

    if (!level)
    {
        dc_log_error("A level is required for the connection to run!");
        return;
    }

    step_to_active();

    if (status == connection_t::CONNECTION_ACTIVE)
    {
        if (!sent_init)
        {
            packet_handshake_c2s_t pack_handshake;

            pack_handshake.username = username;

            sent_init = send_buffer(socket, pack_handshake.assemble());
            set_status_msg("connect.connecting");
        }
        Uint64 sdl_start_tick = SDL_GetTicks();
        packet_t* pack_from_server = NULL;
        while (SDL_GetTicks() - sdl_start_tick < (in_world ? 25 : 150) && status == connection_t::CONNECTION_ACTIVE
            && (pack_from_server = pack_handler_client.get_next_packet(socket)))
        {

#define CAST_PACK_TO_P(type) type* p = (type*)pack_from_server
#define CAST_ENT_TO_E(type) type* e = (type*)level->get_or_create_ent(p->eid);
            Uint8 pack_type = pack_from_server->id;
            switch ((packet_id_t)pack_type)
            {
            case PACKET_ID_KEEP_ALIVE:
            {
                send_buffer(socket, pack_from_server->assemble());
                break;
            }
            case PACKET_ID_HANDSHAKE:
            {
                packet_login_request_c2s_t login_request;

                /* The idea is to maybe implement spectator mode and maybe 1.3+ style plugin channels */
                const char ext_magic[] = "B181_EXT";
                const int ext_ver = 0;
                static_assert(sizeof(ext_magic) == 9, "Client id must be 8 characters (excluding terminator)");

                login_request.unused0 = SDL_Swap64LE(*(jlong*)ext_magic);
                login_request.unused1 = ext_ver;
                login_request.protocol_ver = 17;
                login_request.username = username;

                send_buffer(socket, login_request.assemble());
                set_status_msg("connect.authorizing");
                break;
            }
            case PACKET_ID_LOGIN_REQUEST:
            {
                CAST_PACK_TO_P(packet_login_request_s2c_t);

                level->gamemode_set(p->mode);
                level->world_height = p->world_height;
                if (p->dimension == -1)
                    level->lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_NETHER);
                else
                    level->lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_OVERWORLD);

                set_status_msg("multiplayer.downloadingTerrain");

                break;
            }
            case PACKET_ID_RESPAWN:
            {
                CAST_PACK_TO_P(packet_respawn_t);

                level->gamemode_set(p->mode);
                level->world_height = p->world_height;

                break;
            }
            case PACKET_ID_NEW_STATE:
            {
                CAST_PACK_TO_P(packet_new_state_t);

                switch (p->reason)
                {
                case PACK_NEW_STATE_REASON_INVALID_BED:
                    dc_log("Invalid bed");
                    break;
                case PACK_NEW_STATE_REASON_RAIN_START:
                    dc_log("Rain start");
                    break;
                case PACK_NEW_STATE_REASON_RAIN_END:
                    dc_log("Rain end");
                    break;
                case PACK_NEW_STATE_REASON_CHANGE_MODE:
                    if (level->gamemode_set(p->mode))
                        dc_log("Gamemode updated to %d (%s)", p->mode, mc_id::gamemode_get_trans_id(level->gamemode_get()));
                    break;
                default:
                    dc_log_error("Unknown reason %d (%d) in " SDL_STRINGIFY_ARG(PACKET_ID_NEW_STATE) " (0x%02x)", p->reason, p->mode, p->id);
                    break;
                }

                break;
            }
            case PACKET_ID_UPDATE_TIME:
            {
                CAST_PACK_TO_P(packet_time_update_t);
                level->lightmap.set_world_time(p->time);
                break;
            }
            case PACKET_ID_PLAYER_LOOK:
            {
                CAST_PACK_TO_P(packet_player_look_t);

                level->pitch = SDL_clamp(-p->pitch, -89.0f, 89.0f);
                level->yaw = p->yaw + 90.0f;
                last_update_tick_camera = 0;

                break;
            }
            case PACKET_ID_PLAYER_POS:
            {
                CAST_PACK_TO_P(packet_player_pos_t);

                level->camera_pos = { p->x, p->y, p->z };
                last_update_tick_camera = 0;

                break;
            }
            case PACKET_ID_PLAYER_POS_LOOK:
            {
                CAST_PACK_TO_P(packet_player_pos_look_s2c_t);

                level->camera_pos = { p->x, p->y, p->z };
                level->pitch = SDL_clamp(-p->pitch, -89.0f, 89.0f);
                level->yaw = p->yaw + 90.0f;

                in_world = true;
                set_status_msg("");
                last_update_tick_camera = 0;

                break;
            }
            case PACKET_ID_CHUNK_CACHE:
            {
                CAST_PACK_TO_P(packet_chunk_cache_t);

                const int max_cy = (level->world_height + SUBCHUNK_SIZE_Y - 1) / SUBCHUNK_SIZE_Y;

                if (!p->mode)
                {
                    for (int i = 0; i < max_cy; i++)
                        level->remove_chunk(glm::ivec3(p->chunk_x, i, p->chunk_z));
                }
                else
                {
                    std::vector<int> exists;
                    exists.resize(max_cy);
                    for (chunk_cubic_t* c : level->get_chunk_vec())
                        if (c->pos.x == p->chunk_x && c->pos.z == p->chunk_z && 0 <= c->pos.y && c->pos.y < max_cy)
                            exists[c->pos.y] = 1;

                    for (int i = 0; i < max_cy; i++)
                    {
                        if (exists[i])
                            continue;
                        chunk_cubic_t* c = new chunk_cubic_t();
                        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
                        c->pos.x = p->chunk_x;
                        c->pos.y = i;
                        c->pos.z = p->chunk_z;
                        level->add_chunk(c);
                    }
                }
                break;
            }
            case PACKET_ID_BLOCK_CHANGE:
            {
                CAST_PACK_TO_P(packet_block_change_t);

                glm::ivec3 block_pos = { p->block_x, p->block_y, p->block_z };
                level->set_block(block_pos, p->type, p->metadata);

                /* Mark as fulfilled to delay erasing until after packet handling is finished */
                for (tentative_block_t& it : tentative_blocks)
                {
                    if (it.fullfilled || it.pos != block_pos)
                        continue;
                    it.fullfilled = 1;
                    break;
                }
                break;
            }
            case PACKET_ID_BLOCK_CHANGE_MULTI:
            {
                CAST_PACK_TO_P(packet_block_change_multi_t);

                for (block_change_dat_t b : p->payload)
                {
                    glm::ivec3 block_pos = { p->chunk_x * CHUNK_SIZE_X + b.x, b.y, p->chunk_z * CHUNK_SIZE_Z + b.z };
                    level->set_block(block_pos, b.type, b.metadata);

                    /* Mark as fulfilled to delay erasing until after packet handling is finished */
                    for (tentative_block_t& it : tentative_blocks)
                    {
                        if (it.fullfilled || it.pos != block_pos)
                            continue;
                        it.fullfilled = 1;
                    }
                }
                break;
            }
            case PACKET_ID_CHUNK_MAP:
            {
                CAST_PACK_TO_P(packet_chunk_t);
                static std::vector<Uint8> buf;
                decompress_chunk_packet(level, p, buf);

                /* Mark as fulfilled to delay erasing until after packet handling is finished */
                for (tentative_block_t& it : tentative_blocks)
                {
                    if (it.fullfilled)
                        continue;
                    if (!BETWEEN_INCL(it.pos.x, p->block_x, p->block_x + p->size_x))
                        continue;
                    if (!BETWEEN_INCL(it.pos.y, p->block_y, p->block_y + p->size_y))
                        continue;
                    if (!BETWEEN_INCL(it.pos.z, p->block_z, p->block_z + p->size_z))
                        continue;
                    it.fullfilled = 1;
                }

                break;
            }

            case PACKET_ID_CHAT_MSG:
            {
                CAST_PACK_TO_P(packet_chat_message_t);
                dc_log("[CHAT]: %s", p->msg.c_str());
                break;
            }

            case PACKET_ID_KICK:
            {
                CAST_PACK_TO_P(packet_kick_t);
                set_status_msg("disconnect.lost", p->reason);
                break;
            }
            case PACKET_ID_WINDOW_SET_ITEMS:
            {
                CAST_PACK_TO_P(packet_window_items_t);

                switch (p->window_id)
                {
                case WINDOW_ID_INVENTORY:
                    p->payload.resize(SDL_min(p->payload.size(), SDL_arraysize(level->inventory.items)));
                    for (size_t i = 0; i < p->payload.size(); i++)
                        level->inventory.items[i].id = i;

                    break;
                default:
                    dc_log_error("Unknown window id %d", p->window_id);
                    break;
                }

                break;
            }
            case PACKET_ID_WINDOW_SET_SLOT:
            {
                CAST_PACK_TO_P(packet_window_set_slot_t);

                switch (p->window_id)
                {
                case WINDOW_ID_INVENTORY:
                    if (p->slot < 0 || IM_ARRAYSIZE(level->inventory.items) < p->slot)
                        break;
                    level->inventory.items[p->slot] = p->item;
                    break;
                default:
                    dc_log_error("Unknown window id %d", p->window_id);
                    break;
                }

                break;
            }
            case PACKET_ID_ENT_DESTROY:
            {
                CAST_PACK_TO_P(packet_ent_destroy_t);
                auto it = level->entities.find(p->eid);
                if (it != level->entities.end())
                    level->entities.erase(it);
                break;
            }
            case PACKET_ID_ENT_VELOCITY:
            {
                CAST_PACK_TO_P(packet_ent_velocity_t);
                CAST_ENT_TO_E(entity_base_t);
                e->vel = { p->vel_x, p->vel_y, p->vel_z };
                break;
            }
            case PACKET_ID_ENT_ENSURE_SPAWN:
            {
                CAST_PACK_TO_P(packet_ent_create_t);
                level->get_or_create_ent(p->eid);
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_THUNDERBOLT:
            {
                CAST_PACK_TO_P(packet_thunder_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = ENT_ID_THUNDERBOLT;
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_ADD_OBJ:
            {
                CAST_PACK_TO_P(packet_add_obj_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = entity_base_t::mc_id_to_id(p->obj_type, 1);
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_ENT_SPAWN_MOB:
            {
                CAST_PACK_TO_P(packet_ent_spawn_mob_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = entity_base_t::mc_id_to_id(p->type, 0);
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                e->yaw = p->yaw * 360.0f / 256.0f;
                e->pitch = p->pitch * 360.0f / 256.0f;
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_ENT_SPAWN_XP:
            {
                CAST_PACK_TO_P(packet_ent_spawn_xp_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = ENT_ID_XP;
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_ENT_SPAWN_PICKUP:
            {
                CAST_PACK_TO_P(packet_ent_spawn_pickup_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = ENT_ID_ITEM;
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                e->yaw = p->rotation * 360.0f / 256.0f;
                e->pitch = p->pitch * 360.0f / 256.0f;
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_ENT_SPAWN_PAINTING:
            {
                CAST_PACK_TO_P(packet_ent_spawn_painting_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = ENT_ID_PAINTING;
                e->pos = (glm::ivec3(p->center_x, p->center_y, p->center_z) * 32 + glm::ivec3(16)) << 10;
                e->yaw = p->direction * 90.0f;
                break;
            }
            /* TODO: Handle beyond missing */
            case PACKET_ID_ENT_SPAWN_NAMED:
            {
                CAST_PACK_TO_P(packet_ent_spawn_named_t);
                CAST_ENT_TO_E(entity_base_t);
                e->id = ENT_ID_ITEM;
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                e->yaw = p->rotation * 360.0f / 256.0f;
                e->pitch = p->pitch * 360.0f / 256.0f;
                break;
            }
            case PACKET_ID_ENT_MOVE_REL:
            {
                CAST_PACK_TO_P(packet_ent_move_rel_t);
                CAST_ENT_TO_E(entity_base_t);
                e->pos += glm::ivec3(p->delta_x, p->delta_y, p->delta_z) << 10;
                break;
            }
            case PACKET_ID_ENT_LOOK:
            {
                CAST_PACK_TO_P(packet_ent_look_t);
                CAST_ENT_TO_E(entity_base_t);
                e->yaw = p->yaw * 360.0f / 256.0f;
                e->pitch = p->pitch * 360.0f / 256.0f;
                break;
            }
            case PACKET_ID_ENT_LOOK_MOVE_REL:
            {
                CAST_PACK_TO_P(packet_ent_look_move_rel_t);
                CAST_ENT_TO_E(entity_base_t);
                e->pos += glm::ivec3(p->delta_x, p->delta_y, p->delta_z) << 10;
                e->yaw = p->yaw * 360.0f / 256.0f;
                e->pitch = p->pitch * 360.0f / 256.0f;
                break;
            }
            case PACKET_ID_ENT_MOVE_TELEPORT:
            {
                CAST_PACK_TO_P(packet_ent_teleport_t);
                CAST_ENT_TO_E(entity_base_t);
                e->pos = glm::ivec3(p->x, p->y, p->z) << 10;
                e->yaw = p->rotation * 360.0f / 256.0f;
                e->pitch = p->pitch * 360.0f / 256.0f;
                break;
            }
            case PACKET_ID_PLAYER_LIST_ITEM:
                break;
            default:
                dc_log_error("Unknown packet from server with id: 0x%02x", pack_type);
                break;
            }
            pack_handler_client.free_packet(pack_from_server);
        }

        if (in_world)
        {
            if (SDL_GetTicks() - last_update_tick_camera > 50)
            {
                packet_player_pos_look_c2s_t location_response;
                location_response.x = level->camera_pos.x;
                location_response.y = level->camera_pos.y;
                location_response.stance = level->camera_pos.y + 1.0f;
                location_response.z = level->camera_pos.z;

                location_response.pitch = -level->pitch;
                location_response.yaw = level->yaw - 90.0f;

                send_buffer(socket, location_response.assemble());
                last_update_tick_camera = SDL_GetTicks();
            }
        }
    }

    /* Remove tentative blocks if they have been fulfilled or their timeout has expired */
    const Uint64 time_tentative = SDL_GetTicks();
    for (std::vector<tentative_block_t>::iterator it = tentative_blocks.begin(); it != tentative_blocks.end();)
    {
        if (time_tentative - it->timestamp < 5000)
            it = next(it);
        else
        {
            if (!it->fullfilled)
                level->set_block(it->pos, it->old.id, it->old.damage);
            it = tentative_blocks.erase(it);
        }
    }
}

void connection_t::set_status_msg(const std::string _status, const std::string _sub_status)
{
    status_msg = _status;
    status_msg_sub = _sub_status;
}

bool connection_t::init(const std::string _addr, const Uint16 _port, const std::string _username)
{
    if (status != CONNECTION_UNINITIALIZED)
    {
        dc_log_error("A connection may only be initialized once!");
        return false;
    }

    addr = _addr;
    port = _port;
    username = _username;

    set_status_msg("Resolving address");
    addr_server = SDLNet_ResolveHostname(addr.c_str());
    status = CONNECTION_ADDR_RESOLVING;
    if (!addr_server)
    {
        err_str = std::string("SDLNet_ResolveHostname: ") + SDL_GetError();
        status = CONNECTION_FAILED;
        return false;
    }
    return true;
}

/**
 * Steps (if possible) the state as fast a possible to CONNECTION_ACTIVE
 */
void connection_t::step_to_active()
{
    if (status == CONNECTION_ADDR_RESOLVING)
    {
        int address_status = SDLNet_GetAddressStatus(addr_server);

        if (address_status == 1)
        {
            set_status_msg("Address resolved");
            status = CONNECTION_ADDR_RESOLVED;
        }
        else if (address_status == -1)
        {
            err_str = std::string("SDLNet_WaitUntilResolved: ") + SDL_GetError();
            status = CONNECTION_FAILED;
        }
    }

    if (status == CONNECTION_ADDR_RESOLVED)
    {
        socket = SDLNet_CreateClient(addr_server, port);
        SDLNet_UnrefAddress(addr_server);
        addr_server = NULL;

        set_status_msg("Connecting");
        status = CONNECTION_CONNECTING;

        if (!socket)
        {
            err_str = std::string("SDLNet_CreateClient: ") + SDL_GetError();
            status = CONNECTION_FAILED;
        }
    }

    if (status == CONNECTION_CONNECTING)
    {
        int connection_status = SDLNet_GetConnectionStatus(socket);

        if (connection_status == 1)
        {
            set_status_msg("Connected");
            status = CONNECTION_ACTIVE;
        }
        else if (connection_status == -1)
        {
            err_str = std::string("SDLNet_GetConnectionStatus: ") + SDL_GetError();
            status = CONNECTION_FAILED;
        }
    }
}

bool connection_t::send_packet(packet_t& pack)
{
    if (status == CONNECTION_ACTIVE && socket)
        return send_buffer(socket, pack.assemble());
    return false;
}

bool connection_t::send_packet(packet_t* pack)
{
    if (pack && status == CONNECTION_ACTIVE && socket)
        return send_buffer(socket, pack->assemble());
    return false;
}

connection_t::~connection_t()
{
    SDLNet_UnrefAddress(addr_server);

    /* TODO: Store this in a vector of dying sockets to ensure things are properly closed down */
    SDLNet_DestroyStreamSocket(socket);
}

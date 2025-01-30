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
#ifndef MCS_B181_CLIENT_ENTITY_H
#define MCS_B181_CLIENT_ENTITY_H

#include <SDL3/SDL_stdinc.h>
#include <glm/glm.hpp>

#include "shared/misc.h"

/**
 * This is seperate from minecraft's actual entity id system
 */
enum entity_id_t : Uint8
{
    OBJ_ID_NONE = 0,
    ENT_ID_NONE = OBJ_ID_NONE,

    OBJ_ID_BOAT,

    OBJ_ID_MINECART,
    OBJ_ID_MINECART_CHEST,
    OBJ_ID_MINECART_FURNACE,

    OBJ_ID_TNT,

    OBJ_ID_ARROW,
    OBJ_ID_SNOWBALL,
    OBJ_ID_EGG,

    OBJ_ID_FALLING_SAND,
    OBJ_ID_FALLING_GRAVEL,

    OBJ_ID_FISHING_FLOAT,

    OBJ_ID_MAX,

    ENT_ID_PIG = OBJ_ID_MAX,
    ENT_ID_SHEEP,
    ENT_ID_COW,
    ENT_ID_CHICKEN,
    ENT_ID_SQUID,
    ENT_ID_WOLF,

    ENT_ID_CREEPER,
    ENT_ID_SKELETON,
    ENT_ID_SPIDER,
    ENT_ID_SPIDER_CAVE,

    ENT_ID_ZOMBIE,
    ENT_ID_ZOMBIE_PIG,
    ENT_ID_ZOMBIE_GIANT,

    ENT_ID_SLIME,
    ENT_ID_GHAST,
    ENT_ID_ENDERMAN,
    ENT_ID_SILVER_FISH,

    ENT_ID_PAINTING,
    ENT_ID_THUNDERBOLT,
    ENT_ID_ITEM,
    ENT_ID_XP,

    ENT_ID_PLAYER,
    ENT_ID_SELF,

    ENT_ID_MAX,
};

typedef jint eid_t;

struct entity_base_t
{
    Uint8 id = 0;

    bool gravity : 1;
    bool attached : 1;
    bool flying : 1;
    bool swimming : 1;
    bool on_ground : 1;

    eid_t parent_eid = 0;

    /**
     * Translates a minecraft mob or object ID to an entity_base_t ID
     */
    static Uint8 mc_id_to_id(Uint8 id, bool is_object);

    /**
     * Entity velocity (1/6400) blocks/second
     */
    glm::i16vec3 vel;

    /**
     * Entity coordinates (fixed 1/32768 points)
     */
    glm::i64vec3 pos;
    float yaw = 0;
    float pitch = 0;

    inline entity_base_t()
        : gravity(1)
        , attached(0)
        , flying(0)
        , swimming(0)
        , on_ground(0)
    {
    }
};

#endif

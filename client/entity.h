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
#include <glm/ext/matrix_transform.hpp>
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

/** Minecraft Tick (50ms)*/
typedef int mc_tick_t;

/**
 * Fixed precision number (1/1048576)
 *
 * Ranges from -2^43 to 2^43
 */
typedef Sint64 ecoord_t;

/**
 * Fixed precision number (1/32) number used in entity packets
 *
 * Ranges from -2^26 to 2^26
 */
typedef Sint32 ecoord_abs_t;

#define ABSCOORD_ECOORD_SHIFT (20 - 5)

#define ABSCOORD_TO_ECOORD(X) (ecoord_t(X) << ABSCOORD_ECOORD_SHIFT)
#define ECOORD_TO_ABSCOORD(X) (ecoord_abs_t(ecoord_t((X)) >> ABSCOORD_ECOORD_SHIFT))

#define BLOCKCOORD_TO_ECOORD(X) (ecoord_t(X) << ABSCOORD_ECOORD_SHIFT << 5)
#define ECOORD_TO_BLOCKCOORD(X) (ecoord_t(X) >> ABSCOORD_ECOORD_SHIFT >> 5)

#define CHUNKCOORD_TO_ECOORD(X) (ecoord_t(X) << ABSCOORD_ECOORD_SHIFT << 9)
#define ECOORD_TO_CHUNKCOORD(X) (ecoord_t(X) >> ABSCOORD_ECOORD_SHIFT >> 9)

#define ABSCOORD_TO_ECOORD_GLM(X) ((X) << ABSCOORD_ECOORD_SHIFT)
#define ECOORD_TO_ABSCOORD_GLM(X) ((X) >> ABSCOORD_ECOORD_SHIFT)

#define ABSCOORD_TO_ECOORD_GLM_LONG(X) ((X) << long(ABSCOORD_ECOORD_SHIFT))
#define ECOORD_TO_ABSCOORD_GLM_LONG(X) ((X) >> long(ABSCOORD_ECOORD_SHIFT))

#define ABSCOORD_ONE_BLOCK (ecoord_abs_t(32))
#define ECOORD_ONE_BLOCK (ABSCOORD_TO_ECOORD(ABSCOORD_ONE_BLOCK))

struct entity_base_t
{
    /**
     * Translates a minecraft mob or object ID to an entity_base_t ID
     */
    static Uint8 mc_id_to_id(Uint8 id, bool is_object);
};

struct entity_food_t
{
    /**
     * Decremented to zero by level_t::tick()
     *
     * When it reaches zero the last fields are set to the current values
     */
    mc_tick_t update_effect_counter;

    int cur;
    int max;
    int last;

    float satur_cur;
    float satur_last;
};

struct entity_experience_t
{
    int level;
    int progress;
    int total;
};

struct entity_health_t
{
    /**
     * Decremented to zero by level_t::tick()
     *
     * When it reaches zero the last field is set to current value
     */
    mc_tick_t update_effect_counter;

    int cur;
    int max;
    int last;
};

struct entity_transform_t
{
    ecoord_t x;
    ecoord_t y;
    ecoord_t z;

    float pitch;
    float yaw;
    float roll;

    inline glm::mat4 get_mat() const
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(ECOORD_TO_ABSCOORD(x), ECOORD_TO_ABSCOORD(y), ECOORD_TO_ABSCOORD(z)) / 32.0f);
        model = glm::scale(model, glm::vec3(1.0f) / 24.0f);
        model = glm::rotate(model, glm::radians(-pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

        return model;
    }
};

/**
 * This is necessary for thunderbolts, because the notchian server doesn't delete them, *sigh*
 */
struct entity_timed_destroy_t
{
    /**
     * This counter is decremented by level_t::tick(), if less than 0 than the entity is destroyed
     */
    mc_tick_t counter;
    /**
     * Whether or not the corresponding entity is owned by the server
     *
     * This controls who ultimately deletes the entity
     * - If true then connection_t::run() destroys the entity
     * - If false then level_t::tick() destroys the entity
     */
    bool server_entity;
};

struct entity_velocity_t
{
    /**
     * Unit: 1/(2^20) block/second
     */
    ecoord_t vel_x;
    /**
     * Unit: 1/(2^20) block/second
     */
    ecoord_t vel_y;
    /**
     * Unit: 1/(2^20) block/second
     */
    ecoord_t vel_z;
};

#endif

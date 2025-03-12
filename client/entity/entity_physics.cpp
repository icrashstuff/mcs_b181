/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"));
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense);
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY);
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "entity_physics.h"

void entity_physics_t::reset_to_entity_defaults(const entity_id_t type)
{
    vel = { 0.0, 0.0, 0.0 };
    acceleration = 0.0f;
    drag_horizontal = 1.0f;
    drag_vertical = 1.0f;
    drag_horizontal_on_ground = -90000.0f;
    flags.can_collide_with_world = 0;
    flags.can_collide_with_entities = 0;
    flags.on_ground = 0;
    flags.apply_gravity = 0;
    flags.apply_drag_after_accel = 0;
    flags.apply_accel_to_position = 0;
    flags.update_velocity_before_position = 0;

    /* Most values pulled from https://minecraft.wiki/w/Entity#Motion_of_entities */
    switch (type)
    {
    case ENT_ID_SELF:
        flags.can_collide_with_entities = 1;
        acceleration = 0.08;
        drag_vertical = 0.02;
        drag_horizontal = 0.09;
        drag_horizontal_on_ground = 0.454;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        break;

    case ENT_ID_PIG:
    case ENT_ID_SHEEP:
    case ENT_ID_COW:
    case ENT_ID_CHICKEN:
    case ENT_ID_SQUID:
    case ENT_ID_WOLF:
    case ENT_ID_CREEPER:
    case ENT_ID_SKELETON:
    case ENT_ID_SPIDER:
    case ENT_ID_SPIDER_CAVE:
    case ENT_ID_ZOMBIE:
    case ENT_ID_ZOMBIE_PIG:
    case ENT_ID_ZOMBIE_GIANT:
    case ENT_ID_SLIME:
    case ENT_ID_GHAST:
    case ENT_ID_ENDERMAN:
    case ENT_ID_SILVER_FISH:
    case ENT_ID_PAINTING:
        acceleration = 0.08;
        drag_vertical = 0.02;
        drag_horizontal = 0.09;
        drag_horizontal_on_ground = 0.454;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        break;

    case ENT_ID_ITEM:
    case OBJ_ID_TNT:
    case OBJ_ID_FALLING_SAND:
    case OBJ_ID_FALLING_GRAVEL:
        acceleration = 0.04;
        drag_vertical = drag_horizontal = 0.02;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        flags.apply_accel_to_position = 1;
        break;

    case OBJ_ID_MINECART_CHEST:
    case OBJ_ID_MINECART_FURNACE:
    case OBJ_ID_MINECART:
        acceleration = 0.04;
        drag_vertical = drag_horizontal = 0.05;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        flags.apply_accel_to_position = 1;
        break;

    case OBJ_ID_BOAT:
        acceleration = 0.04;
        drag_vertical = 0.0;
        drag_horizontal = 0.10;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        flags.apply_accel_to_position = 1;
        flags.update_velocity_before_position = 1;
        break;

    case OBJ_ID_EGG:
    case OBJ_ID_SNOWBALL:
        acceleration = 0.03;
        drag_vertical = drag_horizontal = 0.01;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 0;
        break;

    case ENT_ID_XP:
        acceleration = 0.03;
        drag_vertical = drag_horizontal = 0.02;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        flags.apply_accel_to_position = 1;
        break;

    case OBJ_ID_FISHING_FLOAT:
        acceleration = 0.03;
        drag_vertical = drag_horizontal = 0.08;
        flags.can_collide_with_entities = 1;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 1;
        flags.apply_accel_to_position = 1;
        break;

    case OBJ_ID_ARROW:
        acceleration = 0.05;
        drag_vertical = drag_horizontal = 0.01;
        flags.can_collide_with_world = 1;
        flags.apply_gravity = 1;
        flags.apply_drag_after_accel = 0;
        break;

    case ENT_ID_PLAYER:
        break;

    default:
        dc_log_warn("Unknown entity of internal type: %02x. Setting fields to make entity motion unlikely!", type);
        break;
    }

    if (drag_horizontal_on_ground < -10000.0f)
        drag_horizontal_on_ground = drag_horizontal;

    float bb_xz = 0.0f, bb_y = 0.0f;
#define ADD_BB(CASE, XZ, Y)       \
    case CASE:                    \
        bb_xz = (XZ), bb_y = (Y); \
        break

    /* Values pulled from https://web.archive.org/web/20150208030456/http://wiki.vg/Entities */
    switch (type)
    {
        ADD_BB(ENT_ID_SILVER_FISH, 0.4, 0.3);

        ADD_BB(ENT_ID_CHICKEN, 0.4, 0.7);

        ADD_BB(ENT_ID_WOLF, 0.6, 0.8);

        ADD_BB(ENT_ID_CREEPER, 0.6, 1.7);

        /* Sneaking player has height of 1.5 */
    case ENT_ID_PLAYER:
    case ENT_ID_SELF:
    case ENT_ID_ZOMBIE:
        ADD_BB(ENT_ID_ZOMBIE_PIG, 0.6, 1.8);

        ADD_BB(ENT_ID_ZOMBIE_GIANT, 0.6 * 6.0, 1.8 * 6.0);

        ADD_BB(ENT_ID_SKELETON, 0.6, 1.95);

        /* Angry enderman have a height of 3.25 */
        ADD_BB(ENT_ID_ENDERMAN, 0.6, 2.9);

        ADD_BB(ENT_ID_SPIDER_CAVE, 0.7, 0.5);

        ADD_BB(ENT_ID_PIG, 0.9, 0.9);

    case ENT_ID_SHEEP:
        ADD_BB(ENT_ID_COW, 0.9, 1.3);

        ADD_BB(ENT_ID_SQUID, 0.95, 0.95);

        ADD_BB(ENT_ID_SPIDER, 1.4, 0.9);

        ADD_BB(ENT_ID_GHAST, 4.0, 4.0);

        /* TODO: The actual BB is ADD_BB(ENT_ID_SLIME, 0.5 * (2^slime_size), 0.5 * (2^slime_size)); */
        ADD_BB(ENT_ID_SLIME, 0.5, 0.5);

        ADD_BB(OBJ_ID_BOAT, 1.5, 0.6);

    case OBJ_ID_FISHING_FLOAT:
    case OBJ_ID_SNOWBALL:
    case OBJ_ID_EGG:
    case ENT_ID_XP:
        ADD_BB(ENT_ID_ITEM, 0.25, 0.25);

    case OBJ_ID_MINECART_CHEST:
    case OBJ_ID_MINECART_FURNACE:
        ADD_BB(OBJ_ID_MINECART, 0.98, 0.7);

        ADD_BB(OBJ_ID_TNT, 0.98, 0.98);

        ADD_BB(OBJ_ID_ARROW, 0.5, 0.5);

    case OBJ_ID_FALLING_SAND:
        ADD_BB(OBJ_ID_FALLING_GRAVEL, 0.98, 0.98);

    default:
    case ENT_ID_THUNDERBOLT:
    case ENT_ID_PAINTING:
        ADD_BB(OBJ_ID_NONE, 0.0, 0.0);
    }

    bb_size = { bb_xz, bb_y, bb_xz };
}

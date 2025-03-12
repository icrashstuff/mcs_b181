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
#ifndef MCS_B181__CLIENT__ENTITY__ENTITY_PHYSICS_H
#define MCS_B181__CLIENT__ENTITY__ENTITY_PHYSICS_H

#include "entity.h"

struct entity_physics_t
{
    /** Bounding box size */
    glm::vec3 bb_size;

    /** Unit: blocks/mc_tick */
    glm::f64vec3 vel;

    float acceleration;

    float drag_vertical;

    float drag_horizontal;

    /* If flags.on_ground, then use this value instead of drag_horizontal */
    float drag_horizontal_on_ground;

    struct
    {
        /** Entity can collide with blocks */
        bool can_collide_with_world : 1;

        /** This entity can be pushed by other entities, (Unused) */
        bool can_collide_with_entities : 1;

        bool apply_gravity : 1;

        bool apply_drag_after_accel : 1;

        /** Subtract acceleration from position (equivalent to the k coefficient from https://github.com/OrHy3/MinecraftMotionTools) */
        bool apply_accel_to_position : 1;

        /** Update velocity before updating position */
        bool update_velocity_before_position : 1;

        /** True if the foot plane is less than 0.001 blocks away from the ground (Source for 0.001 threshold: https://www.youtube.com/watch?v=ei58gGM9Z8k) */
        bool on_ground : 1;
    } flags;

    /**
     * Reset all fields to the default for an entity
     *
     * NOTE: This function does not reset velocity
     *
     * @param type Internal type of entity
     */
    void reset_to_entity_defaults(const entity_id_t type);
};

#endif

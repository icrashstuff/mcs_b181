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
#include "entity.h"
#include "shared/ids.h"

#define ADD_NAME(id, name) \
    case id:               \
        return name

Uint8 entity_base_t::mc_id_to_id(Uint8 id, bool is_object)
{

    if (is_object)
    {
        switch ((vehicle_type_t)id)
        {
            ADD_NAME(VEHICLE_TYPE_BOAT, OBJ_ID_BOAT);

            ADD_NAME(VEHICLE_TYPE_CART, OBJ_ID_MINECART);
            ADD_NAME(VEHICLE_TYPE_CART_CHEST, OBJ_ID_MINECART_CHEST);
            ADD_NAME(VEHICLE_TYPE_CART_POWERED, OBJ_ID_MINECART_FURNACE);

            ADD_NAME(VEHICLE_TYPE_TNT, OBJ_ID_TNT);

            ADD_NAME(VEHICLE_TYPE_ARROW, OBJ_ID_ARROW);
            ADD_NAME(VEHICLE_TYPE_SNOWBALL, OBJ_ID_SNOWBALL);
            ADD_NAME(VEHICLE_TYPE_EGG, OBJ_ID_EGG);

            ADD_NAME(VEHICLE_TYPE_SAND, OBJ_ID_FALLING_SAND);
            ADD_NAME(VEHICLE_TYPE_GRAVEL, OBJ_ID_FALLING_GRAVEL);

            ADD_NAME(VEHICLE_TYPE_FISH_FLOAT, OBJ_ID_FISHING_FLOAT);
        default:
            return OBJ_ID_NONE;
        }
    }
    else
    {
        switch ((mob_type_t)id)
        {
            ADD_NAME(MOB_TYPE_PIG, ENT_ID_PIG);
            ADD_NAME(MOB_TYPE_SHEEP, ENT_ID_SHEEP);
            ADD_NAME(MOB_TYPE_COW, ENT_ID_COW);
            ADD_NAME(MOB_TYPE_CHICKEN, ENT_ID_CHICKEN);
            ADD_NAME(MOB_TYPE_SQUID, ENT_ID_SQUID);
            ADD_NAME(MOB_TYPE_WOLF, ENT_ID_WOLF);

            ADD_NAME(MOB_TYPE_CREEPER, ENT_ID_CREEPER);
            ADD_NAME(MOB_TYPE_SKELETON, ENT_ID_SKELETON);
            ADD_NAME(MOB_TYPE_SPIDER, ENT_ID_SPIDER);
            ADD_NAME(MOB_TYPE_SPIDER_CAVE, ENT_ID_SPIDER_CAVE);

            ADD_NAME(MOB_TYPE_ZOMBIE, ENT_ID_ZOMBIE);
            ADD_NAME(MOB_TYPE_ZOMBIE_PIG, ENT_ID_ZOMBIE_PIG);
            ADD_NAME(MOB_TYPE_ZOMBIE_GIANT, ENT_ID_ZOMBIE_GIANT);

            ADD_NAME(MOB_TYPE_SLIME, ENT_ID_SLIME);
            ADD_NAME(MOB_TYPE_GHAST, ENT_ID_GHAST);
            ADD_NAME(MOB_TYPE_ENDERMAN, ENT_ID_ENDERMAN);
            ADD_NAME(MOB_TYPE_SILVERFISH, ENT_ID_SILVER_FISH);
        default:
            return ENT_ID_NONE;
        }
    }
}

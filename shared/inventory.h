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
#ifndef MCS_B181_SHARED_INVENTORY_H_INCLUDED
#define MCS_B181_SHARED_INVENTORY_H_INCLUDED

#include "misc.h"

struct itemstack_t
{
    short id = -1;
    short damage = 0;
    jbyte quantity = 0;

    /**
     * Draws a simple text only imgui widget
     */
    void imgui();

    /**
     * Moves as much as possible from stack B to stack A if both are of the same id/damage
     *
     * If stack B is empty, it will be set to ITEM_ID_NONE
     *
     * @returns True if items were moved, False otherwise
     */
    static bool add_stacks(itemstack_t& a, itemstack_t& b);

    /**
     * Sorts and consolidates item stacks in the specified range
     *
     * @returns True if items were moved, False otherwise
     */
    static bool sort_stacks(itemstack_t* items, int start, int end, bool sort_descending = true);
};

inline bool operator<(itemstack_t& lhs, itemstack_t& rhs)
{
    if (lhs.id < rhs.id)
        return true;
    if (lhs.id > rhs.id)
        return false;

    if (lhs.damage < rhs.damage)
        return true;
    if (lhs.damage > rhs.damage)
        return false;

    return lhs.quantity < rhs.quantity;
}

inline bool operator>(itemstack_t& lhs, itemstack_t& rhs)
{
    if (lhs.id > rhs.id)
        return true;
    if (lhs.id < rhs.id)
        return false;

    if (lhs.damage > rhs.damage)
        return true;
    if (lhs.damage < rhs.damage)
        return false;

    return lhs.quantity > rhs.quantity;
}

inline bool operator==(itemstack_t& lhs, itemstack_t& rhs) { return lhs.id == rhs.id && lhs.damage == rhs.damage; }
inline bool operator!=(itemstack_t& lhs, itemstack_t& rhs) { return lhs.id != rhs.id || lhs.damage != rhs.damage; }

struct inventory_mob_t
{
    itemstack_t items[6];

    const int armor_min = 0;
    const int armor_max = 4;

    const int hand_right = 5;
    const int hand_left = 6;
};

struct inventory_player_t
{
    /**
     * Inventory layout
     *
     * +---+-------+ +-----+    +---+
     * | 5 |   o   | | 1 2 | -> | 0 |
     * | 6 |  ---  | | 3 4 | -> | 0 |
     * | 7 |   |   | +-----+    +---+
     * | 8 |  / \  +----------------+
     * +---+-------+ 45 (1.9+ only) |
     * +-----------+----------------+
     * |  9 10 11 12 13 14 15 16 17 |
     * | 18 19 20 21 22 23 24 25 26 |
     * | 27 28 29 30 31 32 33 34 35 |
     * +----------------------------+
     * | 36 37 38 39 40 41 42 43 44 |
     * +----------------------------+
     */
    itemstack_t items[46];

    const int crafting_out = 0;
    const int crafting_min = 1;
    const int crafting_max = 4;

    const int armor_min = 5;
    const int armor_max = 8;

    const int main_min = 9;
    const int main_max = 35;

    const int hotbar_min = 36;
    const int hotbar_max = 44;
    const int hotbar_offhand = 45;
    int hotbar_sel = 36;

    /**
     * Draws a table view of the inventory
     */
    void imgui();

    void sort(bool sort_descending = true) { itemstack_t::sort_stacks(items, main_min, main_max, sort_descending); }
};

#endif

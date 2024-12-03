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
#ifndef MCS_B181_IDS_H
#define MCS_B181_IDS_H

enum block_id_t
{
    BLOCK_ID_AIR = 0,
    BLOCK_ID_STONE,
    BLOCK_ID_GRASS,
    BLOCK_ID_DIRT,
    BLOCK_ID_COBBLESTONE,
    BLOCK_ID_WOOD_PLANKS,
    BLOCK_ID_SAPLING,
    BLOCK_ID_BEDROCK,

    /** Not sure if I got this correct */
    BLOCK_ID_WATER_FLOWING,
    /** Not sure if I got this correct */
    BLOCK_ID_WATER_SOURCE,
    /** Not sure if I got this correct */
    BLOCK_ID_LAVA_FLOWING,
    /** Not sure if I got this correct */
    BLOCK_ID_LAVA_SOURCE,

    BLOCK_ID_SAND,
    BLOCK_ID_GRAVEL,
    BLOCK_ID_ORE_GOLD,
    BLOCK_ID_ORE_IRON,
    BLOCK_ID_ORE_COAL,
    BLOCK_ID_WOOD,
    BLOCK_ID_LEAVES,
    BLOCK_ID_SPONGE,
    BLOCK_ID_GLASS,
    BLOCK_ID_ORE_LAPIS,

    BLOCK_ID_LAPIS,
    BLOCK_ID_DISPENSER,
    BLOCK_ID_SANDSTONE,
    BLOCK_ID_NOTE_BLOCK,
    BLOCK_ID_BED,
    BLOCK_ID_RAIL_POWERED,
    BLOCK_ID_RAIL_DETECTOR,
    BLOCK_ID_PISTON_STICKY,
    BLOCK_ID_COBWEB,
    BLOCK_ID_FOLIAGE,
    BLOCK_ID_DEAD_BUSH,
    BLOCK_ID_PISTON,
    BLOCK_ID_PISTON_HEAD,
    BLOCK_ID_WOOL,

    BLOCK_ID_UNKNOWN,
    BLOCK_ID_FLOWER_YELLOW,
    BLOCK_ID_FLOWER_RED,
    BLOCK_ID_MUSHROOM_BLAND,
    BLOCK_ID_MUSHROOM_RED,

    BLOCK_ID_GOLD,
    BLOCK_ID_IRON,

    BLOCK_ID_SLAB_DOUBLE,
    BLOCK_ID_SLAB_SINGLE,
    BLOCK_ID_BRICKS,
    BLOCK_ID_TNT,
    BLOCK_ID_BOOKSHELF,
    BLOCK_ID_COBBLESTONE_MOSSY,
    BLOCK_ID_OBSIDIAN,

    BLOCK_ID_TORCH,
    BLOCK_ID_UNKNOWN_MAYBE_FIRE,
    BLOCK_ID_SPAWNER,

    BLOCK_ID_STAIRS_WOOD,
    BLOCK_ID_CHEST,
    BLOCK_ID_REDSTONE,
    BLOCK_ID_ORE_DIAMOND,
    BLOCK_ID_DIAMOND,

    BLOCK_ID_CRAFTING_TABLE,
    BLOCK_ID_PLANT_FOOD,
    BLOCK_ID_DIRT_TILLED,
    BLOCK_ID_FURNACE_OFF,
    BLOCK_ID_FURNACE_ON,
    BLOCK_ID_SIGN_STANDING,
    BLOCK_ID_DOOR_WOOD,
    BLOCK_ID_LADDER,
    BLOCK_ID_RAIL,
    BLOCK_ID_STAIRS_COBBLESTONE,
    BLOCK_ID_SIGN_WALL,
    BLOCK_ID_LEVER,
    BLOCK_ID_PRESSURE_PLATE_STONE,
    BLOCK_ID_DOOR_IRON,
    BLOCK_ID_PRESSURE_PLATE_WOOD,
    BLOCK_ID_ORE_REDSTONE_OFF,
    BLOCK_ID_ORE_REDSTONE_ON,
    BLOCK_ID_TORCH_REDSTONE_OFF,
    BLOCK_ID_TORCH_REDSTONE_ON,
    BLOCK_ID_BUTTON_STONE,
    BLOCK_ID_SNOW,
    BLOCK_ID_ICE,
    BLOCK_ID_SNOW_BLOCK,
    BLOCK_ID_CACTUS,
    BLOCK_ID_CLAY,
    BLOCK_ID_SUGAR_CANE,
    BLOCK_ID_JUKEBOX,
    BLOCK_ID_FENCE_WOOD,
    BLOCK_ID_PUMPKIN,
    BLOCK_ID_NETHERACK,
    BLOCK_ID_SOULSTONE,
    BLOCK_ID_GLOWSTONE,
    BLOCK_ID_NETHER_PORTAL,
    BLOCK_ID_PUMPKIN_GLOWING,
    BLOCK_ID_CAKE,
    BLOCK_ID_REPEATER_OFF,
    BLOCK_ID_REPEATER_ON,
    BLOCK_ID_CHEST_FAT,
    BLOCK_ID_TRAPDOOR,
    BLOCK_ID_UNKNOWN_STONE,
    BLOCK_ID_BRICKS_STONE,
    BLOCK_ID_MUSHROOM_BLOCK_BLAND,
    BLOCK_ID_MUSHROOM_BLOCK_RED,
    BLOCK_ID_IRON_BARS,
    BLOCK_ID_GLASS_PANE,
    BLOCK_ID_MELON,
    BLOCK_ID_STEM_UNKNOWN0,
    BLOCK_ID_STEM_UNKNOWN1,
    BLOCK_ID_MOSS,
    BLOCK_ID_GATE,
    BLOCK_ID_STAIRS_BRICK,
    BLOCK_ID_STAIRS_BRICK_STONE,

};
#endif

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

#include "SDL3/SDL_stdinc.h"
#include "misc.h"
#include <glm/glm.hpp>

#ifndef MC_ID_CONST
#if defined(__GNUC__) || (defined(__has_attribute) && __has_attribute(const))
#define MC_ID_CONST __attribute__((__const__))
#else
#define MC_ID_CONST
#warning No MC_ID_CONST
#endif
#endif

#ifndef MC_ID_PURE
#if defined(__GNUC__) || (defined(__has_attribute) && __has_attribute(pure))
#define MC_ID_PURE __attribute__((__pure__))
#else
#define MC_ID_PURE
#warning No MC_ID_PURE
#endif
#endif

enum damage_id_t : Uint8
{
    WOOL_ID_WHITE = 0,
    WOOL_ID_ORANGE,
    WOOL_ID_MAGENTA,
    WOOL_ID_LIGHT_BLUE,
    WOOL_ID_YELLOW,
    WOOL_ID_LIME,
    WOOL_ID_PINK,
    WOOL_ID_GRAY,
    WOOL_ID_LIGHT_GRAY,
    WOOL_ID_CYAN,
    WOOL_ID_PURPLE,
    WOOL_ID_BLUE,
    WOOL_ID_BROWN,
    WOOL_ID_GREEN,
    WOOL_ID_RED,
    WOOL_ID_BLACK,

    DYE_ID_BLACK = 0,
    DYE_ID_RED,
    DYE_ID_GREEN,
    DYE_ID_BROWN,
    DYE_ID_BLUE,
    DYE_ID_PURPLE,
    DYE_ID_CYAN,
    DYE_ID_LIGHT_GRAY,
    DYE_ID_GRAY,
    DYE_ID_PINK,
    DYE_ID_LIME,
    DYE_ID_YELLOW,
    DYE_ID_LIGHT_BLUE,
    DYE_ID_MAGENTA,
    DYE_ID_ORANGE,
    DYE_ID_WHITE,

    DYE_ID_BONEMEAL = DYE_ID_WHITE,
    DYE_ID_CACTUS = DYE_ID_GREEN,
    DYE_ID_COCOA = DYE_ID_BROWN,
    DYE_ID_LAPIS = DYE_ID_BLUE,

    WOOD_ID_OAK = 0,
    WOOD_ID_SPRUCE,
    WOOD_ID_BIRCH,

    SLAB_ID_SMOOTH = 0,
    SLAB_ID_SANDSTONE,
    SLAB_ID_WOOD,
    SLAB_ID_COBBLESTONE,
    SLAB_ID_BRICKS,
    SLAB_ID_BRICKS_STONE,
    SLAB_ID_SMOOTH_SIDE,

    STONE_BRICK_ID_REGULAR = 0,
    STONE_BRICK_ID_MOSSY,
    STONE_BRICK_ID_CRACKED,
};

typedef jshort item_id_t;
typedef jshort block_id_t;

enum item_id_t_ : int
{
    BLOCK_ID_NONE = -1,
    ITEM_ID_NONE = BLOCK_ID_NONE,

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

    /**/
    BLOCK_ID_SAND,
    BLOCK_ID_GRAVEL,
    BLOCK_ID_ORE_GOLD,
    BLOCK_ID_ORE_IRON,
    BLOCK_ID_ORE_COAL,
    BLOCK_ID_LOG,
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
    BLOCK_ID_FIRE,
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
    BLOCK_ID_NETHERRACK,
    BLOCK_ID_SOULSAND,
    BLOCK_ID_GLOWSTONE,
    BLOCK_ID_NETHER_PORTAL,
    BLOCK_ID_PUMPKIN_GLOWING,
    BLOCK_ID_CAKE,
    BLOCK_ID_REPEATER_OFF,
    BLOCK_ID_REPEATER_ON,
    BLOCK_ID_CHEST_LOCKED,
    BLOCK_ID_TRAPDOOR,
    BLOCK_ID_UNKNOWN_STONE,
    BLOCK_ID_BRICKS_STONE,
    BLOCK_ID_MUSHROOM_BLOCK_BLAND,
    BLOCK_ID_MUSHROOM_BLOCK_RED,
    BLOCK_ID_IRON_BARS,
    BLOCK_ID_GLASS_PANE,
    BLOCK_ID_MELON,
    BLOCK_ID_STEM_PUMPKIN,
    BLOCK_ID_STEM_MELON,
    BLOCK_ID_MOSS,
    BLOCK_ID_GATE,
    BLOCK_ID_STAIRS_BRICK,
    BLOCK_ID_STAIRS_BRICK_STONE,

    /** Maximum number of used ids */
    BLOCK_ID_NUM_USED,

    /** Technical limit of block ids */
    BLOCK_ID_LIMIT = 255,

    /** Size of an array that can hold any block id within the range [0, BLOCK_ID_LIMIT] */
    BLOCK_ID_ARRAY_SIZE = 256,

    /***/
    ITEM_ID_IRON_SHOVEL = 256,
    ITEM_ID_IRON_PICK,
    ITEM_ID_IRON_AXE,

    ITEM_ID_FLINT_STEEL,
    ITEM_ID_APPLE,
    ITEM_ID_BOW,
    ITEM_ID_ARROW,
    ITEM_ID_COAL,
    ITEM_ID_DIAMOND,
    ITEM_ID_INGOT_IRON,
    ITEM_ID_INGOT_GOLD,

    ITEM_ID_IRON_SWORD,

    ITEM_ID_WOOD_SWORD,
    ITEM_ID_WOOD_SHOVEL,
    ITEM_ID_WOOD_PICK,
    ITEM_ID_WOOD_AXE,

    ITEM_ID_STONE_SWORD,
    ITEM_ID_STONE_SHOVEL,
    ITEM_ID_STONE_PICK,
    ITEM_ID_STONE_AXE,

    ITEM_ID_DIAMOND_SWORD,
    ITEM_ID_DIAMOND_SHOVEL,
    ITEM_ID_DIAMOND_PICK,
    ITEM_ID_DIAMOND_AXE,

    ITEM_ID_STICK,
    ITEM_ID_BOWL,
    ITEM_ID_BOWL_STEW_MUSHROOM,

    ITEM_ID_GOLD_SWORD,
    ITEM_ID_GOLD_SHOVEL,
    ITEM_ID_GOLD_PICK,
    ITEM_ID_GOLD_AXE,

    ITEM_ID_STRING,
    ITEM_ID_FEATHER,
    ITEM_ID_GUNPOWDER,

    ITEM_ID_WOOD_HOE,
    ITEM_ID_STONE_HOE,
    ITEM_ID_IRON_HOE,
    ITEM_ID_DIAMOND_HOE,
    ITEM_ID_GOLD_HOE,

    ITEM_ID_SEEDS,
    ITEM_ID_WHEAT,
    ITEM_ID_BREAD,

    ITEM_ID_LEATHER_CAP = 298,
    ITEM_ID_LEATHER_TUNIC,
    ITEM_ID_LEATHER_PANTS,
    ITEM_ID_LEATHER_BOOTS,
    ITEM_ID_CHAIN_CAP,
    ITEM_ID_CHAIN_TUNIC,
    ITEM_ID_CHAIN_PANTS,
    ITEM_ID_CHAIN_BOOTS,
    ITEM_ID_IRON_CAP,
    ITEM_ID_IRON_TUNIC,
    ITEM_ID_IRON_PANTS,
    ITEM_ID_IRON_BOOTS,
    ITEM_ID_DIAMOND_CAP,
    ITEM_ID_DIAMOND_TUNIC,
    ITEM_ID_DIAMOND_PANTS,
    ITEM_ID_DIAMOND_BOOTS,
    ITEM_ID_GOLD_CAP,
    ITEM_ID_GOLD_TUNIC,
    ITEM_ID_GOLD_PANTS,
    ITEM_ID_GOLD_BOOTS,

    ITEM_ID_FLINT,
    ITEM_ID_PORK,
    ITEM_ID_PORK_COOKED,
    ITEM_ID_PAINTING,
    ITEM_ID_APPLE_GOLDEN,
    ITEM_ID_SIGN,
    ITEM_ID_DOOR_WOOD,
    ITEM_ID_BUCKET,
    ITEM_ID_BUCKET_WATER,
    ITEM_ID_BUCKET_LAVA,
    ITEM_ID_MINECART,
    ITEM_ID_SADDLE,
    ITEM_ID_DOOR_IRON,
    ITEM_ID_REDSTONE,
    ITEM_ID_SNOWBALL,
    ITEM_ID_BOAT,
    ITEM_ID_LEATHER,
    ITEM_ID_BUCKET_MILK,
    ITEM_ID_INGOT_CLAY,
    ITEM_ID_CLAY,
    ITEM_ID_SUGAR_CANE,
    ITEM_ID_PAPER,
    ITEM_ID_BOOK,
    ITEM_ID_SLIMEBALL,
    ITEM_ID_MINECART_CHEST,
    ITEM_ID_MINECART_FURNACE,
    ITEM_ID_EGG,
    ITEM_ID_COMPASS,
    ITEM_ID_FISHING_ROD,
    ITEM_ID_CLOCK,
    ITEM_ID_GLOWSTONE,
    ITEM_ID_FISH,
    ITEM_ID_FISH_COOKED,
    ITEM_ID_DYE,
    ITEM_ID_BONE,
    ITEM_ID_SUGAR,
    ITEM_ID_CAKE,
    ITEM_ID_BED,
    ITEM_ID_REPEATER,
    ITEM_ID_COOKIE,
    ITEM_ID_MAP,
    ITEM_ID_SHEARS,
    ITEM_ID_MELON,
    ITEM_ID_SEEDS_PUMPKIN,
    ITEM_ID_SEEDS_MELON,
    ITEM_ID_BEEF,
    ITEM_ID_BEEF_COOKED,
    ITEM_ID_CHICKEN,
    ITEM_ID_CHICKEN_COOKED,
    ITEM_ID_FLESH,
    ITEM_ID_ENDER_PEARL,

    ITEM_ID_MUSIC_DISC_13 = 2256,
    ITEM_ID_MUSIC_DISC_CAT,
};

enum vehicle_type_t : jbyte
{
    VEHICLE_TYPE_BOAT = 1,
    VEHICLE_TYPE_CART = 10,
    VEHICLE_TYPE_CART_CHEST,
    VEHICLE_TYPE_CART_POWERED,
    VEHICLE_TYPE_TNT = 50,
    VEHICLE_TYPE_ARROW = 60,
    VEHICLE_TYPE_SNOWBALL,
    VEHICLE_TYPE_EGG,
    VEHICLE_TYPE_SAND = 70,
    VEHICLE_TYPE_GRAVEL,
    VEHICLE_TYPE_FISH_FLOAT = 90,
};

enum mob_type_t : jbyte
{
    MOB_TYPE_CREEPER = 50,
    MOB_TYPE_SKELETON,
    MOB_TYPE_SPIDER,
    MOB_TYPE_ZOMBIE_GIANT,
    MOB_TYPE_ZOMBIE,
    MOB_TYPE_SLIME,
    MOB_TYPE_GHAST,
    MOB_TYPE_ZOMBIE_PIG,
    MOB_TYPE_ENDERMAN,
    MOB_TYPE_SPIDER_CAVE,
    MOB_TYPE_SILVERFISH,
    MOB_TYPE_PIG = 90,
    MOB_TYPE_SHEEP,
    MOB_TYPE_COW,
    MOB_TYPE_CHICKEN,
    MOB_TYPE_SQUID,
    MOB_TYPE_WOLF,
};

namespace mc_id
{
enum gamemode_t
{
    GAMEMODE_SURVIVAL = 0,
    GAMEMODE_CREATIVE = 1,
    GAMEMODE_ADVENTURE = 2,
    GAMEMODE_SPECTATOR = 3,
};

enum dimension_t : jbyte
{
    DIMENSION_NETHER = -1,
    DIMENSION_OVERWORLD = 0,

    /** I'm not sure if this is the correct ID */
    DIMENSION_END = 1,
};

/** Mirrors the biome ids from cubiomes */
enum biome_t : int
{
    BIOME_OCEAN,
    BIOME_PLAINS,
    BIOME_DESERT,
    BIOME_MOUNTAINS,
    BIOME_FOREST,
    BIOME_TAIGA,
    BIOME_SWAMP,
    BIOME_RIVER,
    BIOME_NETHER_WASTES,
    BIOME_THE_END,

    BIOME_NUM_BIOMES,
};

enum direction_t
{
    DIR_F0 = 0,
    DIR_F1 = 1,
    DIR_F2 = 2,
    DIR_F3 = 3,

    DIR_TOWARDS_POS_Z = DIR_F0,
    DIR_TOWARDS_NEG_X = DIR_F1,
    DIR_TOWARDS_NEG_Z = DIR_F2,
    DIR_TOWARDS_POS_X = DIR_F3,
};

MC_ID_CONST bool gamemode_is_valid(const int x);

MC_ID_CONST const char* gamemode_get_trans_id(const gamemode_t x);

MC_ID_CONST bool dimension_is_valid(const int x);

MC_ID_CONST const char* get_name_vehicle(const jbyte id);

MC_ID_CONST const char* get_name_mob(const jbyte id);

/**
 * Returns the name of the item with any changes applied by damage (eg. wool or dye)
 */
MC_ID_CONST const char* get_name_from_item_id(const short item_id, const short damage);

struct block_return_t
{
    item_id_t id;
    short damage;
    Uint8 quantity_min;
    Uint8 quantity_max;
};

MC_ID_CONST SDL_FORCE_INLINE bool is_block(const short block_id) { return BETWEEN_EXCL(block_id, BLOCK_ID_NONE, BLOCK_ID_NUM_USED); };

MC_ID_CONST bool is_fluid(const short block_id);

MC_ID_CONST bool is_rail(const short block_id);

MC_ID_CONST bool is_transparent(const short block_id);

MC_ID_CONST bool is_leaves_style_transparent(const short block_id);

MC_ID_CONST bool is_translucent(const short block_id);

MC_ID_CONST bool can_host_hanging(const short block_id);

MC_ID_CONST bool can_host_rail(const short block_id);

MC_ID_CONST Uint8 get_food_value(const short item_id);

MC_ID_CONST float get_food_staturation_ratio(const short item_id);

/**
 * For blocks like rails, ladders, torches, signs, flowers, buttons, pressure plates, levers, and such
 */
MC_ID_CONST bool block_has_collision(const short block_id);

/**
 * Get the corresponding light value for a block
 *
 * Data pulled from the table at https://minecraft.wiki/w/Light?oldid=161926#Light-emitting_blocks
 */
MC_ID_CONST Uint8 get_light_level(const short block_id);

MC_ID_CONST glm::vec3 get_light_color(const short block_id);

/**
 * Get the corresponding return data for a block id
 */
MC_ID_CONST block_return_t get_return_from_block(const short item_id, const short damage, const bool silk_touch = false);

MC_ID_CONST int is_shovel(const short item_id);

MC_ID_CONST int is_sword(const short item_id);

MC_ID_CONST int is_axe(const short item_id);

MC_ID_CONST int is_pickaxe(const short item_id);

MC_ID_CONST int is_hoe(const short item_id);

MC_ID_CONST int is_misc_tool(const short item_id);

MC_ID_CONST SDL_FORCE_INLINE int is_tool(const short item_id)
{
    return is_shovel(item_id) || is_sword(item_id) || is_axe(item_id) || is_pickaxe(item_id) || is_hoe(item_id) || is_misc_tool(item_id);
}

MC_ID_CONST int is_armor_helmet(const short item_id);

MC_ID_CONST int is_armor_chestplate(const short item_id);

MC_ID_CONST int is_armor_leggings(const short item_id);

MC_ID_CONST int is_armor_boots(const short item_id);

MC_ID_CONST SDL_FORCE_INLINE int is_armor(const short item_id)
{
    return is_armor_helmet(item_id) || is_armor_chestplate(item_id) || is_armor_leggings(item_id) || is_armor_boots(item_id);
}

/**
 * Get max stack size for a given id
 */
MC_ID_CONST Uint8 get_max_quantity_for_id(const short item_id);

/**
 * Gets the downfall value for a corresponding biome id
 */
MC_ID_CONST float get_biome_downfall(const int biome_id);

/**
 * Gets the temperature value for a corresponding biome id
 */
MC_ID_CONST float get_biome_temperature(const int biome_id);

/**
 * Gets the name for a corresponding biome id
 */
MC_ID_CONST const char* get_biome_name(const int biome_id);

/**
 * Gets the grass color for a corresponding biome id
 */
MC_ID_CONST glm::vec3 get_biome_color_grass(const int biome_id);

/**
 * Gets the foliage color for a corresponding biome id
 */
MC_ID_CONST glm::vec3 get_biome_color_foliage(const int biome_id);

/**
 * Gets the sky color for a corresponding biome id
 */
MC_ID_CONST glm::vec3 get_biome_color_sky(const int biome_id);

/**
 * Gets the water color for a corresponding biome id
 */
MC_ID_CONST glm::vec3 get_biome_color_water(const int biome_id);

/**
 * Gets the water fog color for a corresponding biome id
 */
MC_ID_CONST glm::vec3 get_biome_color_fog_water(const int biome_id);

/**
 * Gets the fog color for a corresponding biome id
 */
MC_ID_CONST glm::vec3 get_biome_color_fog(const int biome_id);
}
#endif

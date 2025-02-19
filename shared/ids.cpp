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

#include "ids.h"

#define ADD_NAME(id, name) \
    case id:               \
        return name

const char* mc_id::get_name_vehicle(jbyte id)
{
    switch (id)
    {
        ADD_NAME(VEHICLE_TYPE_BOAT, "Boat");
        ADD_NAME(VEHICLE_TYPE_CART, "Minecart");
        ADD_NAME(VEHICLE_TYPE_CART_CHEST, "Minecart with Chest");
        ADD_NAME(VEHICLE_TYPE_CART_POWERED, "Minecart with Furnace");
        ADD_NAME(VEHICLE_TYPE_TNT, "Activated TNT");
        ADD_NAME(VEHICLE_TYPE_ARROW, "Arrow");
        ADD_NAME(VEHICLE_TYPE_SNOWBALL, "Thrown Snowball");
        ADD_NAME(VEHICLE_TYPE_EGG, "Thrown Egg");
        ADD_NAME(VEHICLE_TYPE_SAND, "Falling Sand");
        ADD_NAME(VEHICLE_TYPE_GRAVEL, "Falling Gravel");
        ADD_NAME(VEHICLE_TYPE_FISH_FLOAT, "Fishing Rod Float");
    default:
        return "Unknown Vehicle/Object";
    }
}

const char* mc_id::get_name_mob(jbyte id)
{
    switch (id)
    {
        ADD_NAME(MOB_TYPE_CREEPER, "Creeper");
        ADD_NAME(MOB_TYPE_SKELETON, "Skeleton");
        ADD_NAME(MOB_TYPE_SPIDER, "Spider");
        ADD_NAME(MOB_TYPE_ZOMBIE_GIANT, "Giant Zombie");
        ADD_NAME(MOB_TYPE_ZOMBIE, "Zombie");
        ADD_NAME(MOB_TYPE_SLIME, "Slime");
        ADD_NAME(MOB_TYPE_GHAST, "Ghast");
        ADD_NAME(MOB_TYPE_ZOMBIE_PIG, "Zombie Pigman");
        ADD_NAME(MOB_TYPE_ENDERMAN, "Enderman");
        ADD_NAME(MOB_TYPE_SPIDER_CAVE, "Cave Spider");
        ADD_NAME(MOB_TYPE_SILVERFISH, "Silverfish");
        ADD_NAME(MOB_TYPE_PIG, "Pig");
        ADD_NAME(MOB_TYPE_SHEEP, "Sheep");
        ADD_NAME(MOB_TYPE_COW, "Cow");
        ADD_NAME(MOB_TYPE_CHICKEN, "Chicken");
        ADD_NAME(MOB_TYPE_SQUID, "Squid");
        ADD_NAME(MOB_TYPE_WOLF, "Wolf");
    default:
        return "Unknown Vehicle/Object";
    }
}

const char* mc_id::get_name_from_item_id(short item_id, short damage)
{
    switch (item_id)
    {
        ADD_NAME(BLOCK_ID_NONE, "No item");
        ADD_NAME(BLOCK_ID_AIR, "Air");
        ADD_NAME(BLOCK_ID_STONE, "Stone");
        ADD_NAME(BLOCK_ID_GRASS, "Grass");
        ADD_NAME(BLOCK_ID_DIRT, "Dirt");
        ADD_NAME(BLOCK_ID_COBBLESTONE, "Cobblestone");
        ADD_NAME(BLOCK_ID_WOOD_PLANKS, "Wooden Planks");
    case BLOCK_ID_SAPLING:
    {
        switch (damage)
        {
            ADD_NAME(WOOD_ID_OAK, "Oak Sapling");
            ADD_NAME(WOOD_ID_SPRUCE, "Spruce Sapling");
            ADD_NAME(WOOD_ID_BIRCH, "Birch Sapling");
        default:
            return "Unknown Sapling";
        }
    }
        ADD_NAME(BLOCK_ID_BEDROCK, "Bedrock");

        ADD_NAME(BLOCK_ID_WATER_FLOWING, "Water Flowing");

        ADD_NAME(BLOCK_ID_WATER_SOURCE, "Water Source");

        ADD_NAME(BLOCK_ID_LAVA_FLOWING, "Lava Flowing");

        ADD_NAME(BLOCK_ID_LAVA_SOURCE, "Lava Source");

        ADD_NAME(BLOCK_ID_SAND, "Sand");
        ADD_NAME(BLOCK_ID_GRAVEL, "Gravel");
        ADD_NAME(BLOCK_ID_ORE_GOLD, "Gold Ore");
        ADD_NAME(BLOCK_ID_ORE_IRON, "Iron Ore");
        ADD_NAME(BLOCK_ID_ORE_COAL, "Coal Ore");
    case BLOCK_ID_LOG:
    {
        switch (damage)
        {
            ADD_NAME(WOOD_ID_OAK, "Oak Log");
            ADD_NAME(WOOD_ID_SPRUCE, "Spruce Log");
            ADD_NAME(WOOD_ID_BIRCH, "Birch Log");
        default:
            return "Unknown Log";
        }
    }
        ADD_NAME(BLOCK_ID_LEAVES, "Leaves");
        ADD_NAME(BLOCK_ID_SPONGE, "Sponge");
        ADD_NAME(BLOCK_ID_GLASS, "Glass");
        ADD_NAME(BLOCK_ID_ORE_LAPIS, "Lapis Lazuli Ore");

        ADD_NAME(BLOCK_ID_LAPIS, "Lapis Lazuli Block");
        ADD_NAME(BLOCK_ID_DISPENSER, "Dispenser");
        ADD_NAME(BLOCK_ID_SANDSTONE, "Sandstone");
        ADD_NAME(BLOCK_ID_NOTE_BLOCK, "Note block");
        ADD_NAME(BLOCK_ID_BED, "Bed block");
        ADD_NAME(BLOCK_ID_RAIL_POWERED, "Powered Rail");
        ADD_NAME(BLOCK_ID_RAIL_DETECTOR, "Detector Rail");
        ADD_NAME(BLOCK_ID_PISTON_STICKY, "Stick Piston");
        ADD_NAME(BLOCK_ID_COBWEB, "Cobweb");
        ADD_NAME(BLOCK_ID_FOLIAGE, "Grass");
        ADD_NAME(BLOCK_ID_DEAD_BUSH, "Dead Bush");
        ADD_NAME(BLOCK_ID_PISTON, "Piston");
        ADD_NAME(BLOCK_ID_PISTON_HEAD, "Piston Head");
    case BLOCK_ID_WOOL:
    {
        switch (damage)
        {
            ADD_NAME(WOOL_ID_WHITE, "Wool");
            ADD_NAME(WOOL_ID_ORANGE, "Orange Wool");
            ADD_NAME(WOOL_ID_MAGENTA, "Magenta Wool");
            ADD_NAME(WOOL_ID_LIGHT_BLUE, "Light Blue Wool");
            ADD_NAME(WOOL_ID_YELLOW, "Yellow Wool");
            ADD_NAME(WOOL_ID_LIME, "Lime Wool");
            ADD_NAME(WOOL_ID_PINK, "Pink Wool");
            ADD_NAME(WOOL_ID_GRAY, "Gray Wool");
            ADD_NAME(WOOL_ID_LIGHT_GRAY, "Light Gray Wool");
            ADD_NAME(WOOL_ID_CYAN, "Cyan Wool");
            ADD_NAME(WOOL_ID_PURPLE, "Purple Wool");
            ADD_NAME(WOOL_ID_BLUE, "Blue Wool");
            ADD_NAME(WOOL_ID_BROWN, "Brown Wool");
            ADD_NAME(WOOL_ID_GREEN, "Green Wool");
            ADD_NAME(WOOL_ID_RED, "Red Wool");
            ADD_NAME(WOOL_ID_BLACK, "Black Wool");
        default:
            return "Unknown Wool";
        }
    }

        ADD_NAME(BLOCK_ID_UNKNOWN, "BLOCK_ID_UNKNOWN");
        ADD_NAME(BLOCK_ID_FLOWER_YELLOW, "Dandelion");
        ADD_NAME(BLOCK_ID_FLOWER_RED, "Rose");
        ADD_NAME(BLOCK_ID_MUSHROOM_BLAND, "Mushroom");
        ADD_NAME(BLOCK_ID_MUSHROOM_RED, "Red Mushroom");

        ADD_NAME(BLOCK_ID_GOLD, "Gold Block");
        ADD_NAME(BLOCK_ID_IRON, "Iron Block");

    case BLOCK_ID_SLAB_DOUBLE:
    {
        switch (damage)
        {
            ADD_NAME(SLAB_ID_SMOOTH, "Smooth Stone Double Slab");
            ADD_NAME(SLAB_ID_SANDSTONE, "Sandstone Double Slab");
            ADD_NAME(SLAB_ID_WOOD, "Wood Double Slab");
            ADD_NAME(SLAB_ID_COBBLESTONE, "Cobblestone Double Slab");
            ADD_NAME(SLAB_ID_BRICKS, "Brick Double Slab");
            ADD_NAME(SLAB_ID_BRICKS_STONE, "Stone Brick Double Slab");
        default:
            return "Unknown Double Slab";
        }
    }

    case BLOCK_ID_SLAB_SINGLE:
    {
        switch (damage)
        {
            ADD_NAME(SLAB_ID_SMOOTH, "Smooth Stone Slab");
            ADD_NAME(SLAB_ID_SANDSTONE, "Sandstone Slab");
            ADD_NAME(SLAB_ID_WOOD, "Wood Slab");
            ADD_NAME(SLAB_ID_COBBLESTONE, "Cobblestone Slab");
            ADD_NAME(SLAB_ID_BRICKS, "Brick Slab");
            ADD_NAME(SLAB_ID_BRICKS_STONE, "Stone Brick Slab");
        default:
            return "Unknown Single Slab";
        }
    }

        ADD_NAME(BLOCK_ID_BRICKS, "Bricks");
        ADD_NAME(BLOCK_ID_TNT, "TNT");
        ADD_NAME(BLOCK_ID_BOOKSHELF, "Bookshelf");
        ADD_NAME(BLOCK_ID_COBBLESTONE_MOSSY, "Mossy Cobblestone");
        ADD_NAME(BLOCK_ID_OBSIDIAN, "Obsidian");

        ADD_NAME(BLOCK_ID_TORCH, "Torch");
        ADD_NAME(BLOCK_ID_FIRE, "Fire");
        ADD_NAME(BLOCK_ID_SPAWNER, "Mob Spawner");

        ADD_NAME(BLOCK_ID_STAIRS_WOOD, "Wood Stairs");
        ADD_NAME(BLOCK_ID_CHEST, "Chest");
        ADD_NAME(BLOCK_ID_REDSTONE, "Redstone Wire");
        ADD_NAME(BLOCK_ID_ORE_DIAMOND, "Diamond Ore");
        ADD_NAME(BLOCK_ID_DIAMOND, "Diamond Block");

        ADD_NAME(BLOCK_ID_CRAFTING_TABLE, "Crafting Table");
        ADD_NAME(BLOCK_ID_PLANT_FOOD, "BLOCK_ID_PLANT_FOOD");
        ADD_NAME(BLOCK_ID_DIRT_TILLED, "Tilled Dirt");
        ADD_NAME(BLOCK_ID_FURNACE_OFF, "Furnace Off");
        ADD_NAME(BLOCK_ID_FURNACE_ON, "Furnace On");
        ADD_NAME(BLOCK_ID_SIGN_STANDING, "Sign");
        ADD_NAME(BLOCK_ID_DOOR_WOOD, "Wooden Door");
        ADD_NAME(BLOCK_ID_LADDER, "Ladder");
        ADD_NAME(BLOCK_ID_RAIL, "Rail");
        ADD_NAME(BLOCK_ID_STAIRS_COBBLESTONE, "Cobblestone Stairs");
        ADD_NAME(BLOCK_ID_SIGN_WALL, "Sign");
        ADD_NAME(BLOCK_ID_LEVER, "Lever");
        ADD_NAME(BLOCK_ID_PRESSURE_PLATE_STONE, "Stone Pressure Plate");
        ADD_NAME(BLOCK_ID_DOOR_IRON, "Iron Door");
        ADD_NAME(BLOCK_ID_PRESSURE_PLATE_WOOD, "Wooden Pressure Plate");
        ADD_NAME(BLOCK_ID_ORE_REDSTONE_OFF, "Redstone Ore On");
        ADD_NAME(BLOCK_ID_ORE_REDSTONE_ON, "Redstone Ore Off");
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_OFF, "Redstone Torch Off");
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_ON, "Redstone Torch On");
        ADD_NAME(BLOCK_ID_BUTTON_STONE, "Stone Button");
        ADD_NAME(BLOCK_ID_SNOW, "Snow");
        ADD_NAME(BLOCK_ID_ICE, "Ice");
        ADD_NAME(BLOCK_ID_SNOW_BLOCK, "Snow");
        ADD_NAME(BLOCK_ID_CACTUS, "Cactus");
        ADD_NAME(BLOCK_ID_CLAY, "Clay");
        ADD_NAME(BLOCK_ID_SUGAR_CANE, "Sugar Cane");
        ADD_NAME(BLOCK_ID_JUKEBOX, "Jukebox");
        ADD_NAME(BLOCK_ID_FENCE_WOOD, "Wooden Fence");
        ADD_NAME(BLOCK_ID_PUMPKIN, "Pumpkin");
        ADD_NAME(BLOCK_ID_NETHERRACK, "Netherrack");
        ADD_NAME(BLOCK_ID_SOULSAND, "Soul Sand");
        ADD_NAME(BLOCK_ID_GLOWSTONE, "Glowstone");
        ADD_NAME(BLOCK_ID_NETHER_PORTAL, "Nether Portal");
        ADD_NAME(BLOCK_ID_PUMPKIN_GLOWING, "Jack 'o' Lantern");
        ADD_NAME(BLOCK_ID_CAKE, "Cake");
        ADD_NAME(BLOCK_ID_REPEATER_OFF, "Redstone Repeater Off");
        ADD_NAME(BLOCK_ID_REPEATER_ON, "Redstone Repeater On");
        ADD_NAME(BLOCK_ID_CHEST_LOCKED, "Locked Chest");
        ADD_NAME(BLOCK_ID_TRAPDOOR, "Trapdoor");
        ADD_NAME(BLOCK_ID_UNKNOWN_STONE, "Unknown Stone");

    case BLOCK_ID_BRICKS_STONE:
    {
        switch (damage)
        {
            ADD_NAME(STONE_BRICK_ID_REGULAR, "Stone Bricks");
            ADD_NAME(STONE_BRICK_ID_MOSSY, "Mossy Stone Bricks");
            ADD_NAME(STONE_BRICK_ID_CRACKED, "Cracked Stone Bricks");
        default:
            return "Unknown Stone Bricks";
        }
    }
        ADD_NAME(BLOCK_ID_MUSHROOM_BLOCK_BLAND, "Mushroom Block");
        ADD_NAME(BLOCK_ID_MUSHROOM_BLOCK_RED, "Red Mushroom Block");
        ADD_NAME(BLOCK_ID_IRON_BARS, "Iron Bars");
        ADD_NAME(BLOCK_ID_GLASS_PANE, "Glass Pane");
        ADD_NAME(BLOCK_ID_MELON, "Melon");
        ADD_NAME(BLOCK_ID_STEM_PUMPKIN, "Pumpkin Stem");
        ADD_NAME(BLOCK_ID_STEM_MELON, "Melon Stem");
        ADD_NAME(BLOCK_ID_MOSS, "Vines");
        ADD_NAME(BLOCK_ID_GATE, "Wooden Fence Gate");
        ADD_NAME(BLOCK_ID_STAIRS_BRICK, "Brick Stairs");
        ADD_NAME(BLOCK_ID_STAIRS_BRICK_STONE, "Stone Brick Stairs");

        ADD_NAME(ITEM_ID_IRON_SHOVEL, "Iron Shovel");
        ADD_NAME(ITEM_ID_IRON_PICK, "Iron Pickaxe");
        ADD_NAME(ITEM_ID_IRON_AXE, "Iron Axe");

        ADD_NAME(ITEM_ID_FLINT_STEEL, "Flint and Steel");
        ADD_NAME(ITEM_ID_APPLE, "Apple");
        ADD_NAME(ITEM_ID_BOW, "Bow");
        ADD_NAME(ITEM_ID_ARROW, "Arrow");
        ADD_NAME(ITEM_ID_COAL, "Coal");
        ADD_NAME(ITEM_ID_DIAMOND, "Diamond");
        ADD_NAME(ITEM_ID_INGOT_IRON, "Iron Ingot");
        ADD_NAME(ITEM_ID_INGOT_GOLD, "Gold Ingot");

        ADD_NAME(ITEM_ID_IRON_SWORD, "Iron Sword");

        ADD_NAME(ITEM_ID_WOOD_SWORD, "Wood Sword");
        ADD_NAME(ITEM_ID_WOOD_SHOVEL, "Wood Shovel");
        ADD_NAME(ITEM_ID_WOOD_PICK, "Wood Pickaxe");
        ADD_NAME(ITEM_ID_WOOD_AXE, "Wood Axe");

        ADD_NAME(ITEM_ID_STONE_SWORD, "Stone Sword");
        ADD_NAME(ITEM_ID_STONE_SHOVEL, "Stone Shovel");
        ADD_NAME(ITEM_ID_STONE_PICK, "Stone Pickaxe");
        ADD_NAME(ITEM_ID_STONE_AXE, "Stone Axe");

        ADD_NAME(ITEM_ID_DIAMOND_SWORD, "Diamond Sword");
        ADD_NAME(ITEM_ID_DIAMOND_SHOVEL, "Diamond Shovel");
        ADD_NAME(ITEM_ID_DIAMOND_PICK, "Diamond Pickaxe");
        ADD_NAME(ITEM_ID_DIAMOND_AXE, "Diamond Axe");

        ADD_NAME(ITEM_ID_STICK, "Stick");
        ADD_NAME(ITEM_ID_BOWL, "Bowl");
        ADD_NAME(ITEM_ID_BOWL_STEW_MUSHROOM, "Mushroom Stew");

        ADD_NAME(ITEM_ID_GOLD_SWORD, "Golden Sword");
        ADD_NAME(ITEM_ID_GOLD_SHOVEL, "Golden Shovel");
        ADD_NAME(ITEM_ID_GOLD_PICK, "Golden Pickaxe");
        ADD_NAME(ITEM_ID_GOLD_AXE, "Golden Axe");

        ADD_NAME(ITEM_ID_STRING, "String");
        ADD_NAME(ITEM_ID_FEATHER, "Feather");
        ADD_NAME(ITEM_ID_GUNPOWDER, "Gunpowder");

        ADD_NAME(ITEM_ID_WOOD_HOE, "Wood Hoe");
        ADD_NAME(ITEM_ID_STONE_HOE, "Stone Hoe");
        ADD_NAME(ITEM_ID_IRON_HOE, "Iron Hoe");
        ADD_NAME(ITEM_ID_DIAMOND_HOE, "Diamond Hoe");
        ADD_NAME(ITEM_ID_GOLD_HOE, "Golden Hoe");

        ADD_NAME(ITEM_ID_SEEDS, "Seeds");
        ADD_NAME(ITEM_ID_WHEAT, "Wheat");
        ADD_NAME(ITEM_ID_BREAD, "Bread");

        ADD_NAME(ITEM_ID_LEATHER_CAP, "Leather Cap");
        ADD_NAME(ITEM_ID_LEATHER_TUNIC, "Leather Tunic");
        ADD_NAME(ITEM_ID_LEATHER_PANTS, "Leather Pants");
        ADD_NAME(ITEM_ID_LEATHER_BOOTS, "Leather Boots");
        ADD_NAME(ITEM_ID_CHAIN_CAP, "Chain Helmet");
        ADD_NAME(ITEM_ID_CHAIN_TUNIC, "Chain Chestplate");
        ADD_NAME(ITEM_ID_CHAIN_PANTS, "Chain Leggings");
        ADD_NAME(ITEM_ID_CHAIN_BOOTS, "Chain Boots");
        ADD_NAME(ITEM_ID_IRON_CAP, "Iron Helmet");
        ADD_NAME(ITEM_ID_IRON_TUNIC, "Iron Chestplate");
        ADD_NAME(ITEM_ID_IRON_PANTS, "Iron Leggings");
        ADD_NAME(ITEM_ID_IRON_BOOTS, "Iron Boots");
        ADD_NAME(ITEM_ID_DIAMOND_CAP, "Diamond Helmet");
        ADD_NAME(ITEM_ID_DIAMOND_TUNIC, "Diamond Chestplate");
        ADD_NAME(ITEM_ID_DIAMOND_PANTS, "Diamond Leggings");
        ADD_NAME(ITEM_ID_DIAMOND_BOOTS, "Diamond Boots");
        ADD_NAME(ITEM_ID_GOLD_CAP, "Golden Helmet");
        ADD_NAME(ITEM_ID_GOLD_TUNIC, "Golden Chestplate");
        ADD_NAME(ITEM_ID_GOLD_PANTS, "Golden Leggings");
        ADD_NAME(ITEM_ID_GOLD_BOOTS, "Golden Boots");

        ADD_NAME(ITEM_ID_FLINT, "Flint");
        ADD_NAME(ITEM_ID_PORK, "Raw Porkchop");
        ADD_NAME(ITEM_ID_PORK_COOKED, "Cooked Porkchop");
        ADD_NAME(ITEM_ID_PAINTING, "Painting");
        ADD_NAME(ITEM_ID_APPLE_GOLDEN, "Golden Apple");
        ADD_NAME(ITEM_ID_SIGN, "Sign");
        ADD_NAME(ITEM_ID_DOOR_WOOD, "Wooden Door");
        ADD_NAME(ITEM_ID_BUCKET, "Bucket");
        ADD_NAME(ITEM_ID_BUCKET_WATER, "Water Bucket");
        ADD_NAME(ITEM_ID_BUCKET_LAVA, "Lava Bucket");
        ADD_NAME(ITEM_ID_MINECART, "Minecart");
        ADD_NAME(ITEM_ID_SADDLE, "Saddle");
        ADD_NAME(ITEM_ID_DOOR_IRON, "Iron Door");
        ADD_NAME(ITEM_ID_REDSTONE, "Redstone");
        ADD_NAME(ITEM_ID_SNOWBALL, "Snowball");
        ADD_NAME(ITEM_ID_BOAT, "Boat");
        ADD_NAME(ITEM_ID_LEATHER, "Leather");
        ADD_NAME(ITEM_ID_BUCKET_MILK, "Milk");
        ADD_NAME(ITEM_ID_INGOT_CLAY, "Brick");
        ADD_NAME(ITEM_ID_CLAY, "Clay");
        ADD_NAME(ITEM_ID_SUGAR_CANE, "Sugar Canes");
        ADD_NAME(ITEM_ID_PAPER, "Paper");
        ADD_NAME(ITEM_ID_BOOK, "Book");
        ADD_NAME(ITEM_ID_SLIMEBALL, "Slimeball");
        ADD_NAME(ITEM_ID_MINECART_CHEST, "Minecart with Chest");
        ADD_NAME(ITEM_ID_MINECART_FURNACE, "Minecart with Furnace");
        ADD_NAME(ITEM_ID_EGG, "Egg");
        ADD_NAME(ITEM_ID_COMPASS, "Compass");
        ADD_NAME(ITEM_ID_FISHING_ROD, "Fishing Rod");
        ADD_NAME(ITEM_ID_CLOCK, "Clock");
        ADD_NAME(ITEM_ID_GLOWSTONE, "Glowstone Dust");
        ADD_NAME(ITEM_ID_FISH, "Raw Fish");
        ADD_NAME(ITEM_ID_FISH_COOKED, "Cooked Fish");
    case ITEM_ID_DYE:
    {
        switch (damage)
        {
            ADD_NAME(DYE_ID_BLACK, "Ink Sac");
            ADD_NAME(DYE_ID_RED, "Rose Red");
            ADD_NAME(DYE_ID_GREEN, "Cactus Green");
            ADD_NAME(DYE_ID_BROWN, "Cocoa Beans");
            ADD_NAME(DYE_ID_BLUE, "Lapis Lazuli");
            ADD_NAME(DYE_ID_PURPLE, "Purple Dye");
            ADD_NAME(DYE_ID_CYAN, "Cyan Dye");
            ADD_NAME(DYE_ID_LIGHT_GRAY, "Light Gray Dye");
            ADD_NAME(DYE_ID_GRAY, "Gray Dye");
            ADD_NAME(DYE_ID_PINK, "Pink Dye");
            ADD_NAME(DYE_ID_LIME, "Lime Dye");
            ADD_NAME(DYE_ID_YELLOW, "Dandelion Yellow");
            ADD_NAME(DYE_ID_LIGHT_BLUE, "Light Blue Dye");
            ADD_NAME(DYE_ID_MAGENTA, "Magenta Dye");
            ADD_NAME(DYE_ID_ORANGE, "Orange Dye");
            ADD_NAME(DYE_ID_WHITE, "Bone Meal");
        default:
            return "Unknown dye";
        }
    }
        ADD_NAME(ITEM_ID_BONE, "Bone");
        ADD_NAME(ITEM_ID_SUGAR, "Sugar");
        ADD_NAME(ITEM_ID_CAKE, "Cake");
        ADD_NAME(ITEM_ID_BED, "Bed");
        ADD_NAME(ITEM_ID_REPEATER, "Redstone Repeater");
        ADD_NAME(ITEM_ID_COOKIE, "Cookie");
        ADD_NAME(ITEM_ID_MAP, "Map");
        ADD_NAME(ITEM_ID_SHEARS, "Shears");
        ADD_NAME(ITEM_ID_MELON, "Melon");
        ADD_NAME(ITEM_ID_SEEDS_PUMPKIN, "Pumpkin Seeds");
        ADD_NAME(ITEM_ID_SEEDS_MELON, "Melon Seeds");
        ADD_NAME(ITEM_ID_BEEF, "Raw Beef");
        ADD_NAME(ITEM_ID_BEEF_COOKED, "Steak");
        ADD_NAME(ITEM_ID_CHICKEN, "Raw Chicken");
        ADD_NAME(ITEM_ID_CHICKEN_COOKED, "Cooked Chicken");
        ADD_NAME(ITEM_ID_FLESH, "Rotten Flesh");
        ADD_NAME(ITEM_ID_ENDER_PEARL, "Ender Pearl");

        ADD_NAME(ITEM_ID_MUSIC_DISC_13, "Music Disc - 13");
        ADD_NAME(ITEM_ID_MUSIC_DISC_CAT, "Music Disc - Cat");
    default:
        return "Unknown item";
    }
}

#define MAP_RETURNQ(BLOCK, ITEM, DMG, QTY_MIN, QTY_MAX) \
    case BLOCK:                                         \
        return { ITEM, DMG, QTY_MIN, QTY_MAX }
#define MAP_RETURN(BLOCK, ITEM) MAP_RETURNQ(BLOCK, ITEM, 0, 1, 1)

mc_id::block_return_t mc_id::get_return_from_block(short item_id, short damage, bool silk)
{
    if (silk)
    {
        switch (item_id)
        {
            MAP_RETURN(BLOCK_ID_GRASS, BLOCK_ID_GRASS);
            MAP_RETURN(BLOCK_ID_GLASS, BLOCK_ID_GLASS);
            MAP_RETURN(BLOCK_ID_GLASS_PANE, BLOCK_ID_GLASS_PANE);
            MAP_RETURN(BLOCK_ID_LEAVES, BLOCK_ID_LEAVES);
            MAP_RETURN(BLOCK_ID_ORE_DIAMOND, BLOCK_ID_ORE_DIAMOND);
            MAP_RETURN(BLOCK_ID_ORE_COAL, BLOCK_ID_ORE_COAL);
            MAP_RETURN(BLOCK_ID_ORE_LAPIS, BLOCK_ID_ORE_LAPIS);
            MAP_RETURN(BLOCK_ID_ORE_REDSTONE_ON, BLOCK_ID_ORE_REDSTONE_ON);
            MAP_RETURN(BLOCK_ID_ORE_REDSTONE_OFF, BLOCK_ID_ORE_REDSTONE_ON);
        default:
            break;
        }
    }
    switch (item_id)
    {
        MAP_RETURN(BLOCK_ID_GRASS, BLOCK_ID_DIRT);
        MAP_RETURN(BLOCK_ID_DIRT_TILLED, BLOCK_ID_DIRT);
        MAP_RETURN(BLOCK_ID_REDSTONE, ITEM_ID_REDSTONE);
        MAP_RETURN(BLOCK_ID_REPEATER_ON, ITEM_ID_REPEATER);
        MAP_RETURN(BLOCK_ID_REPEATER_OFF, ITEM_ID_REPEATER);
        MAP_RETURN(BLOCK_ID_TORCH_REDSTONE_OFF, BLOCK_ID_TORCH_REDSTONE_ON);
        MAP_RETURN(BLOCK_ID_SUGAR_CANE, ITEM_ID_SUGAR_CANE);
        MAP_RETURN(BLOCK_ID_CAKE, BLOCK_ID_NONE);
        MAP_RETURN(BLOCK_ID_GLASS, BLOCK_ID_NONE);
        MAP_RETURN(BLOCK_ID_GLASS_PANE, BLOCK_ID_NONE);
        MAP_RETURNQ(BLOCK_ID_ORE_LAPIS, ITEM_ID_DYE, DYE_ID_LAPIS, 1, 6);
        MAP_RETURNQ(BLOCK_ID_ORE_REDSTONE_ON, ITEM_ID_REDSTONE, 0, 1, 6);
        MAP_RETURNQ(BLOCK_ID_ORE_REDSTONE_OFF, ITEM_ID_REDSTONE, 0, 1, 6);
        MAP_RETURNQ(BLOCK_ID_ORE_DIAMOND, ITEM_ID_DIAMOND, 0, 1, 3);
        MAP_RETURNQ(BLOCK_ID_ORE_COAL, ITEM_ID_COAL, 0, 1, 3);
        MAP_RETURN(BLOCK_ID_DOOR_WOOD, ITEM_ID_DOOR_WOOD);
        MAP_RETURN(BLOCK_ID_DOOR_IRON, ITEM_ID_DOOR_IRON);
    default:
        return { (item_id_t)item_id, damage, 1, 1 };
    }
}

#define ADD_IS_ARMOR(is_name, NAME)              \
    int mc_id::is_name(short item_id)            \
    {                                            \
        switch (item_id)                         \
        {                                        \
            ADD_NAME(ITEM_ID_LEATHER_##NAME, 1); \
            ADD_NAME(ITEM_ID_CHAIN_##NAME, 2);   \
            ADD_NAME(ITEM_ID_IRON_##NAME, 3);    \
            ADD_NAME(ITEM_ID_GOLD_##NAME, 4);    \
            ADD_NAME(ITEM_ID_DIAMOND_##NAME, 5); \
        default:                                 \
            return 0;                            \
        }                                        \
    }

ADD_IS_ARMOR(is_armor_helmet, CAP);
ADD_IS_ARMOR(is_armor_chestplate, TUNIC);
ADD_IS_ARMOR(is_armor_leggings, PANTS);
ADD_IS_ARMOR(is_armor_boots, BOOTS);

#define ADD_IS_TOOLS(is_name, NAME)              \
    int mc_id::is_name(short item_id)            \
    {                                            \
        switch (item_id)                         \
        {                                        \
            ADD_NAME(ITEM_ID_WOOD_##NAME, 1);    \
            ADD_NAME(ITEM_ID_STONE_##NAME, 2);   \
            ADD_NAME(ITEM_ID_IRON_##NAME, 3);    \
            ADD_NAME(ITEM_ID_GOLD_##NAME, 4);    \
            ADD_NAME(ITEM_ID_DIAMOND_##NAME, 5); \
        default:                                 \
            return 0;                            \
        }                                        \
    }

ADD_IS_TOOLS(is_shovel, SHOVEL);
ADD_IS_TOOLS(is_axe, AXE);
ADD_IS_TOOLS(is_pickaxe, PICK);
ADD_IS_TOOLS(is_sword, SWORD);
ADD_IS_TOOLS(is_hoe, HOE);

int mc_id::is_misc_tool(short item_id)
{
    switch (item_id)
    {
        ADD_NAME(ITEM_ID_BOW, 1);
        ADD_NAME(ITEM_ID_FLINT_STEEL, 1);
        ADD_NAME(ITEM_ID_FISHING_ROD, 1);
    default:
        return 0;
    }
}

Uint8 mc_id::get_max_quantity_for_id(short item_id)
{
    if (is_tool(item_id) || is_armor(item_id))
        return 1;
    switch (item_id)
    {
        ADD_NAME(ITEM_ID_ENDER_PEARL, 16);
        ADD_NAME(ITEM_ID_SNOWBALL, 16);
        ADD_NAME(ITEM_ID_EGG, 16);
        ADD_NAME(ITEM_ID_BUCKET_WATER, 1);
        ADD_NAME(ITEM_ID_BUCKET_LAVA, 1);
    default:
        return 64;
    }
}

bool mc_id::is_translucent(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_ICE, 1);
        ADD_NAME(BLOCK_ID_WATER_FLOWING, 1);
        ADD_NAME(BLOCK_ID_WATER_SOURCE, 1);
        ADD_NAME(BLOCK_ID_NETHER_PORTAL, 1);
    default:
        return 0;
    }
}

bool mc_id::is_transparent(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_AIR, 1);
        ADD_NAME(BLOCK_ID_GLASS, 1);
        ADD_NAME(BLOCK_ID_GLASS_PANE, 1);
        ADD_NAME(BLOCK_ID_IRON_BARS, 1);
        ADD_NAME(BLOCK_ID_BED, 1);
        ADD_NAME(BLOCK_ID_DOOR_IRON, 1);
        ADD_NAME(BLOCK_ID_DOOR_WOOD, 1);
        ADD_NAME(BLOCK_ID_TORCH, 1);
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_OFF, 1);
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_ON, 1);
        ADD_NAME(BLOCK_ID_RAIL, 1);
        ADD_NAME(BLOCK_ID_FLOWER_RED, 1);
        ADD_NAME(BLOCK_ID_FLOWER_YELLOW, 1);
        ADD_NAME(BLOCK_ID_MUSHROOM_BLAND, 1);
        ADD_NAME(BLOCK_ID_MUSHROOM_RED, 1);
        ADD_NAME(BLOCK_ID_COBWEB, 1);
        ADD_NAME(BLOCK_ID_SAPLING, 1);
        ADD_NAME(BLOCK_ID_SPAWNER, 1);
        ADD_NAME(BLOCK_ID_DEAD_BUSH, 1);
        ADD_NAME(BLOCK_ID_FOLIAGE, 1);
        ADD_NAME(BLOCK_ID_PLANT_FOOD, 1);
        ADD_NAME(BLOCK_ID_REDSTONE, 1);
        ADD_NAME(BLOCK_ID_REPEATER_ON, 1);
        ADD_NAME(BLOCK_ID_REPEATER_OFF, 1);
        ADD_NAME(BLOCK_ID_CAKE, 1);
        ADD_NAME(BLOCK_ID_LEAVES, 1);
        ADD_NAME(BLOCK_ID_RAIL_DETECTOR, 1);
        ADD_NAME(BLOCK_ID_RAIL_POWERED, 1);
        ADD_NAME(BLOCK_ID_FENCE_WOOD, 1);
        ADD_NAME(BLOCK_ID_LADDER, 1);
        ADD_NAME(BLOCK_ID_CHEST, 1);
        ADD_NAME(BLOCK_ID_SUGAR_CANE, 1);
        ADD_NAME(BLOCK_ID_TRAPDOOR, 1);
        ADD_NAME(BLOCK_ID_MOSS, 1);
        ADD_NAME(BLOCK_ID_FIRE, 1);
        ADD_NAME(BLOCK_ID_LAVA_FLOWING, 1);
        ADD_NAME(BLOCK_ID_LAVA_SOURCE, 1);
        ADD_NAME(BLOCK_ID_GATE, 1);
    default:
        return is_translucent(block_id);
    }
}

bool mc_id::is_leaves_style_transparent(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_LEAVES, 1);
    default:
        return false;
    }
}

bool mc_id::block_has_collision(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_SAPLING, 0);
        ADD_NAME(BLOCK_ID_FOLIAGE, 0);
        ADD_NAME(BLOCK_ID_DEAD_BUSH, 0);
        ADD_NAME(BLOCK_ID_FLOWER_RED, 0);
        ADD_NAME(BLOCK_ID_FLOWER_YELLOW, 0);
        ADD_NAME(BLOCK_ID_LADDER, 0);
        ADD_NAME(BLOCK_ID_RAIL, 0);
        ADD_NAME(BLOCK_ID_RAIL_POWERED, 0);
        ADD_NAME(BLOCK_ID_RAIL_DETECTOR, 0);
        ADD_NAME(BLOCK_ID_TORCH, 0);
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_OFF, 0);
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_ON, 0);
        ADD_NAME(BLOCK_ID_PRESSURE_PLATE_STONE, 0);
        ADD_NAME(BLOCK_ID_PRESSURE_PLATE_WOOD, 0);
        ADD_NAME(BLOCK_ID_LEVER, 0);
        ADD_NAME(BLOCK_ID_BUTTON_STONE, 0);
        ADD_NAME(BLOCK_ID_CAKE, 0);
        ADD_NAME(BLOCK_ID_CHEST, 0);
        ADD_NAME(BLOCK_ID_SUGAR_CANE, 0);
        ADD_NAME(BLOCK_ID_FIRE, 0);
        ADD_NAME(BLOCK_ID_PLANT_FOOD, 0);
        ADD_NAME(BLOCK_ID_MOSS, 0);
        ADD_NAME(BLOCK_ID_MUSHROOM_BLAND, 0);
        ADD_NAME(BLOCK_ID_MUSHROOM_RED, 0);
    default:
        return 1;
    }
}

bool mc_id::can_host_hanging(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_AIR, 0);
        ADD_NAME(BLOCK_ID_GLASS, 0);
        ADD_NAME(BLOCK_ID_LEAVES, 0);
        ADD_NAME(BLOCK_ID_COBWEB, 0);
        ADD_NAME(BLOCK_ID_SLAB_SINGLE, 0);
        ADD_NAME(BLOCK_ID_SAPLING, 0);
        ADD_NAME(BLOCK_ID_FOLIAGE, 0);
        ADD_NAME(BLOCK_ID_DEAD_BUSH, 0);
        ADD_NAME(BLOCK_ID_FLOWER_RED, 0);
        ADD_NAME(BLOCK_ID_FLOWER_YELLOW, 0);
        ADD_NAME(BLOCK_ID_LAVA_SOURCE, 0);
        ADD_NAME(BLOCK_ID_WATER_SOURCE, 0);
        ADD_NAME(BLOCK_ID_LAVA_FLOWING, 0);
        ADD_NAME(BLOCK_ID_WATER_FLOWING, 0);
        ADD_NAME(BLOCK_ID_TNT, 0);
        ADD_NAME(BLOCK_ID_PISTON, 0);
        ADD_NAME(BLOCK_ID_PISTON_STICKY, 0);
        ADD_NAME(BLOCK_ID_PISTON_HEAD, 0);
        ADD_NAME(BLOCK_ID_FENCE_WOOD, 0);
        ADD_NAME(BLOCK_ID_GATE, 0);
        ADD_NAME(BLOCK_ID_TRAPDOOR, 0);
        ADD_NAME(BLOCK_ID_DOOR_WOOD, 0);
        ADD_NAME(BLOCK_ID_DOOR_IRON, 0);
        ADD_NAME(BLOCK_ID_CHEST_LOCKED, 0);
        ADD_NAME(BLOCK_ID_GLASS_PANE, 0);
        ADD_NAME(BLOCK_ID_IRON_BARS, 0);
        ADD_NAME(BLOCK_ID_BED, 0);
        ADD_NAME(BLOCK_ID_LADDER, 0);
        ADD_NAME(BLOCK_ID_RAIL, 0);
        ADD_NAME(BLOCK_ID_RAIL_POWERED, 0);
        ADD_NAME(BLOCK_ID_RAIL_DETECTOR, 0);
        ADD_NAME(BLOCK_ID_TORCH, 0);
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_OFF, 0);
        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_ON, 0);
        ADD_NAME(BLOCK_ID_PRESSURE_PLATE_STONE, 0);
        ADD_NAME(BLOCK_ID_PRESSURE_PLATE_WOOD, 0);
        ADD_NAME(BLOCK_ID_LEVER, 0);
        ADD_NAME(BLOCK_ID_BUTTON_STONE, 0);
        ADD_NAME(BLOCK_ID_CAKE, 0);
        ADD_NAME(BLOCK_ID_CHEST, 0);
        ADD_NAME(BLOCK_ID_SUGAR_CANE, 0);
        ADD_NAME(BLOCK_ID_FIRE, 0);
        ADD_NAME(BLOCK_ID_PLANT_FOOD, 0);
        ADD_NAME(BLOCK_ID_MOSS, 0);
        ADD_NAME(BLOCK_ID_MUSHROOM_BLAND, 0);
        ADD_NAME(BLOCK_ID_MUSHROOM_RED, 0);
        ADD_NAME(BLOCK_ID_CACTUS, 0);
    default:
        return 1;
    }
}

bool mc_id::can_host_rail(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_STAIRS_BRICK, 0);
        ADD_NAME(BLOCK_ID_STAIRS_WOOD, 0);
        ADD_NAME(BLOCK_ID_STAIRS_COBBLESTONE, 0);
        ADD_NAME(BLOCK_ID_STAIRS_BRICK_STONE, 0);
    default:
        return can_host_hanging(block_id);
    }
}

Uint8 mc_id::get_light_level(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_FIRE, 15);
        ADD_NAME(BLOCK_ID_PUMPKIN_GLOWING, 15);
        ADD_NAME(BLOCK_ID_LAVA_SOURCE, 15);
        ADD_NAME(BLOCK_ID_LAVA_FLOWING, 15);
        ADD_NAME(BLOCK_ID_GLOWSTONE, 15);
        ADD_NAME(BLOCK_ID_CHEST_LOCKED, 15);

        ADD_NAME(BLOCK_ID_TORCH, 14);

        ADD_NAME(BLOCK_ID_FURNACE_ON, 13);

        ADD_NAME(BLOCK_ID_NETHER_PORTAL, 11);

        ADD_NAME(BLOCK_ID_REPEATER_ON, 9);
        ADD_NAME(BLOCK_ID_ORE_REDSTONE_ON, 9);

        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_ON, 7);

        ADD_NAME(BLOCK_ID_MUSHROOM_BLAND, 1);
    default:
        return 0;
    }
}

glm::vec3 mc_id::get_light_color(short block_id)
{
    switch (block_id)
    {
        ADD_NAME(BLOCK_ID_FIRE, (glm::vec3 { 1.0f, 0.579f, 0.079f }));
        ADD_NAME(BLOCK_ID_PUMPKIN_GLOWING, (glm::vec3 { 1.0f, 0.981f, 0.291f }));
        ADD_NAME(BLOCK_ID_LAVA_SOURCE, (glm::vec3 { 1.0f, 0.459f, 0.113f }));
        ADD_NAME(BLOCK_ID_LAVA_FLOWING, (glm::vec3 { 1.0f, 0.459f, 0.113f }));
        ADD_NAME(BLOCK_ID_GLOWSTONE, (glm::vec3 { 1.0f, 0.832f, 0.590f }));
        ADD_NAME(BLOCK_ID_CHEST_LOCKED, (glm::vec3 { 1.0f, 0.715f, 0.031f }));

        ADD_NAME(BLOCK_ID_TORCH, (glm::vec3 { 1.0f, 0.861f, 0.521f }));

        ADD_NAME(BLOCK_ID_FURNACE_ON, (glm::vec3 { 1.0f, 0.641f, 0.024f }));

        ADD_NAME(BLOCK_ID_NETHER_PORTAL, (glm::vec3 { 0.464f, 0.057f, 1.0f }));

        ADD_NAME(BLOCK_ID_REPEATER_ON, (glm::vec3 { 1.0f, 0.25f, 0.25f }));
        ADD_NAME(BLOCK_ID_ORE_REDSTONE_ON, (glm::vec3 { 1.0f, 0.25f, 0.25f }));

        ADD_NAME(BLOCK_ID_TORCH_REDSTONE_ON, (glm::vec3 { 1.0f, 0.0f, 0.0f }));

        ADD_NAME(BLOCK_ID_MUSHROOM_BLAND, (glm::vec3 { 1.0f, 0.751f, 0.586f }));
    default:
        return glm::vec3 { 0.0f, 0.0f, 0.0f };
    }
}

Uint8 mc_id::get_food_value(short item_id)
{
    switch (item_id)
    {
        ADD_NAME(ITEM_ID_APPLE_GOLDEN, 4);

        ADD_NAME(ITEM_ID_BEEF_COOKED, 8);
        ADD_NAME(ITEM_ID_PORK_COOKED, 8);

        ADD_NAME(ITEM_ID_CHICKEN_COOKED, 6);
        ADD_NAME(ITEM_ID_FISH_COOKED, 5);
        ADD_NAME(ITEM_ID_BREAD, 5);

        ADD_NAME(ITEM_ID_CHICKEN, 2);
        ADD_NAME(ITEM_ID_APPLE, 4);
        ADD_NAME(ITEM_ID_MELON, 2);
        ADD_NAME(ITEM_ID_BEEF, 3);
        ADD_NAME(ITEM_ID_PORK, 3);

        ADD_NAME(ITEM_ID_FLESH, 4);
        ADD_NAME(ITEM_ID_COOKIE, 2);
        ADD_NAME(ITEM_ID_FISH, 2);
    default:
        return 0;
    }
}

float mc_id::get_food_staturation_ratio(short item_id)
{
    switch (item_id)
    {
        ADD_NAME(ITEM_ID_APPLE_GOLDEN, 2.4);

        ADD_NAME(ITEM_ID_BEEF_COOKED, 1.6);
        ADD_NAME(ITEM_ID_PORK_COOKED, 1.6);

        ADD_NAME(ITEM_ID_CHICKEN_COOKED, 1.2);
        ADD_NAME(ITEM_ID_FISH_COOKED, 1.2);
        ADD_NAME(ITEM_ID_BREAD, 1.2);

        ADD_NAME(ITEM_ID_CHICKEN, 0.6);
        ADD_NAME(ITEM_ID_APPLE, 0.6);
        ADD_NAME(ITEM_ID_MELON, 0.6);
        ADD_NAME(ITEM_ID_BEEF, 0.6);
        ADD_NAME(ITEM_ID_PORK, 0.6);

        ADD_NAME(ITEM_ID_FLESH, 0.2);
        ADD_NAME(ITEM_ID_COOKIE, 0.2);
        ADD_NAME(ITEM_ID_FISH, 0.2);
    default:
        return 0;
    }
}

const char* mc_id::gamemode_get_trans_id(gamemode_t x)
{
    switch (x)
    {
        ADD_NAME(GAMEMODE_SURVIVAL, "gameMode.survival");
        ADD_NAME(GAMEMODE_CREATIVE, "gameMode.creative");
        ADD_NAME(GAMEMODE_ADVENTURE, "gameMode.adventure");
        ADD_NAME(GAMEMODE_SPECTATOR, "gameMode.spectator");
    }

    return "Unknown game mode";
}

bool mc_id::gamemode_is_valid(int a) { return GAMEMODE_SURVIVAL <= a && a <= GAMEMODE_SPECTATOR; }

bool mc_id::dimension_is_valid(int a) { return DIMENSION_NETHER == a || a == DIMENSION_OVERWORLD; }

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

#include "game.h"

#include "tetra/tetra_core.h"
#include "tetra/tetra_sdl_gpu.h"

#include <algorithm>

#include "tetra/gui/imgui.h"
#include "tetra/gui/imgui/backends/imgui_impl_sdl3.h"
#include "tetra/gui/imgui/backends/imgui_impl_sdlgpu3.h"

#include "tetra/gui/console.h"
#include "tetra/gui/gui_registrar.h"

#include "tetra/gui/imgui.h"

#include "tetra/util/convar.h"
#include "tetra/util/convar_file.h"
#include "tetra/util/misc.h"
#include "tetra/util/physfs/physfs.h"
#include "tetra/util/stbi.h"

#include "shared/build_info.h"
#include "shared/chunk.h"
#include "shared/ids.h"
#include "shared/misc.h"
#include "shared/packet.h"

#include "sdl_net/include/SDL3_net/SDL_net.h"

#include "connection.h"
#include "level.h"
#include "texture_terrain.h"

#include "gui/mc_gui.h"

#include "shaders/terrain_shader.h"

#include "state.h"
#include "touch.h"

#ifdef SDL_PLATFORM_IOS
const bool state::on_ios = 1;
#else
const bool state::on_ios = 0;
#endif

#ifdef SDL_PLATFORM_ANDROID
const bool state::on_android = 1;
#else
const bool state::on_android = 0;
#endif

#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
const bool state::on_mobile = 1;
#else
const bool state::on_mobile = 0;
#endif

#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
#define CVR_PATH_RESOURCE_PACK_DEFAULT "resources.zip"
#define CVR_DIR_ASSETS_DEFAULT "assets.zip"
#else
#define CVR_PATH_RESOURCE_PACK_DEFAULT ""
#define CVR_DIR_ASSETS_DEFAULT ""
#endif

static convar_int_t cvr_mc_gui_mobile_controls {
    "mc_gui_mobile_controls",
    state::on_mobile,
    0,
    1,
    "Use mobile control scheme",
    CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL,
};

static convar_int_t cvr_mc_gui_mobile_controls_diag {
    "mc_gui_mobile_controls_diag",
    0,
    0,
    1,
    "Draw representation of touch inputs",
    CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL,
};

static convar_float_t cvr_mouse_sensitivity {
    "mouse_sensitivity",
    state::on_mobile ? 0.2f : 0.1f,
    0.01f,
    1.0f,
    "Sensitivity of mouse/touch inputs",
    CONVAR_FLAG_SAVE,
};

static convar_string_t cvr_username("username", "", "Username (duh)", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_assets("dir_assets", CVR_DIR_ASSETS_DEFAULT, "Path to assets (ex: \"~/.minecraft/assets/\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_path_resource_pack("path_base_resources", CVR_PATH_RESOURCE_PACK_DEFAULT,
    "File/Dir to use for base resources (ex: \"~/.minecraft/versions/1.8.9/1.8.9.jar\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_game("dir_game", "", "Path to store game files (Not mandatory)", CONVAR_FLAG_SAVE);

static convar_int_t cvr_autoconnect("dev_autoconnect", 0, 0, 1, "Auto connect to server", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static convar_string_t cvr_autoconnect_addr(
    "dev_server_addr", "localhost", "Address of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_autoconnect_port(
    "dev_server_port", 25565, 0, 65535, "Port of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);

static convar_float_t cvr_r_fov_base("r_fov_base", 75.0f, 30.0f, 120.0f, "Base FOV", CONVAR_FLAG_SAVE);

static convar_float_t cvr_r_crosshair_scale("r_crosshair_scale", 1.0f, 0.0f, 64.0f, "Multiplier for crosshair size", CONVAR_FLAG_SAVE);
static convar_int_t cvr_r_crosshair_widgets("r_crosshair_widgets", 0, 0, 1, "Use widgets texture for crosshair", CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t cvr_r_overlay_inblock("r_overlay_inblock", 1, 0, 1, "Display overlay of block when in a block", CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t cvr_mc_gui_style_editor(
    "mc_gui_style_editor", 0, 0, 1, "Show style editor for the MC GUI system", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

static bool take_screenshot = 0;

static void compile_shaders() { }

static std::vector<game_t*> games;
static game_t* game_selected = nullptr;
game_resources_t* state::game_resources = nullptr;
static int game_selected_idx = 0;
static float music_counter = 0;

static touch_handler_t touch_handler;

static sound_world_t* sound_engine_main_menu = nullptr;

static ImGuiContext* imgui_ctx_main_menu = NULL;
static SDL_GPUGraphicsPipeline* pipeline_imgui_regular = nullptr;
static SDL_GPUGraphicsPipeline* pipeline_imgui_crosshair = nullptr;

static mc_gui::mc_gui_ctx mc_gui_global_ctx;
mc_gui::mc_gui_ctx* mc_gui::global_ctx = &mc_gui_global_ctx;

namespace mc_gui
{
static void init();
static void deinit();
}
/**
 * Block thread until all fences are signaled, then release them
 */
static void wait_and_release_fences(SDL_GPUDevice* const device, std::vector<SDL_GPUFence*>& fences)
{
    for (auto it = fences.begin(); it != fences.end();)
    {
        if (*it == nullptr)
        {
            it = fences.erase(it);
            dc_log_warn("NULL fence removed");
        }
        else
            ++it;
    }

    if (fences.size())
        SDL_WaitForGPUFences(device, true, fences.data(), fences.size());

    for (auto it : fences)
        SDL_ReleaseGPUFence(device, it);

    fences.clear();
}

static bool deinitialize_resources();
static bool initialize_resources()
{
    deinitialize_resources();

    std::vector<SDL_GPUFence*> fences;

    /* In the future parsing of one of the indexes at /assets/indexes/ will need to happen here (For sound) */
    state::game_resources = new game_resources_t();

    fences.push_back(state::game_resources->reload());
    fences.push_back(mc_gui::global_ctx->load_resources());

    /* Ideally we would just reload the font here rather than completely restarting the mc_gui/ImGui context.
     * But for some reason I gave up trying to figure out, only reloading the font resulted in a all black texture
     * (Hours wasted: 2) (This applied specifically to OpenGL) */
    mc_gui::init();

    state::init_terrain_pipelines();

    compile_shaders();

    sound_engine_main_menu = new sound_world_t(4);

    for (game_t* g : games)
        if (g)
            g->reload_resources(state::game_resources);

    wait_and_release_fences(state::gpu_device, fences);

    return true;
}

static bool deinitialize_resources()
{
    delete state::game_resources;
    state::game_resources = 0;

    mc_gui::global_ctx->unload_resources();

    mc_gui::deinit();

    state::destroy_terrain_pipelines();

    for (game_t* g : games)
        if (g)
            g->reload_resources(nullptr, true);

    delete sound_engine_main_menu;
    sound_engine_main_menu = NULL;
    return true;
}

#define can_launch_game_fail(COND)   \
    do                               \
    {                                \
        if (COND)                    \
        {                            \
            dc_log(#COND " failed"); \
            return 0;                \
        }                            \
    } while (0)
/**
 * Quick check to see if the game can be launched, intended for validating if the setup screen can be skipped
 */
static bool can_launch_game()
{
    if (!cvr_username.get().length())
        return 0;

    dc_log("Documents folder: %s", SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS));
    dc_log("PHYSFS_getBaseDir(): %s", PHYSFS_getBaseDir());
    dc_log("cvr_dir_assets: %s", cvr_dir_assets.get().c_str());
    dc_log("cvr_path_resource_pack: %s", cvr_path_resource_pack.get().c_str());
    dc_log("cvr_dir_game: %s", cvr_dir_game.get().c_str());

    SDL_PathInfo info;
    can_launch_game_fail(!SDL_GetPathInfo(cvr_dir_assets.get().c_str(), &info));

    SDL_GetPathInfo(cvr_dir_game.get().c_str(), &info);
    can_launch_game_fail(info.type != SDL_PATHTYPE_DIRECTORY && info.type != SDL_PATHTYPE_NONE);

    can_launch_game_fail(!SDL_GetPathInfo(cvr_path_resource_pack.get().c_str(), &info));

    return 1;
}
#undef can_launch_game_fail

enum engine_state_t : int
{
    ENGINE_STATE_OFFLINE,
    ENGINE_STATE_CONFIGURE,
    ENGINE_STATE_INITIALIZE,
    ENGINE_STATE_RUNNING,
    ENGINE_STATE_SHUTDOWN,
    ENGINE_STATE_EXIT,
};

static engine_state_t engine_state_current = ENGINE_STATE_OFFLINE;
static engine_state_t engine_state_target = ENGINE_STATE_RUNNING;

#define ENUM_SWITCH_NAME(X) \
    case X:                 \
        return #X;
const char* engine_state_name(engine_state_t state)
{
    switch (state)
    {
        ENUM_SWITCH_NAME(ENGINE_STATE_OFFLINE)
        ENUM_SWITCH_NAME(ENGINE_STATE_CONFIGURE)
        ENUM_SWITCH_NAME(ENGINE_STATE_INITIALIZE)
        ENUM_SWITCH_NAME(ENGINE_STATE_RUNNING)
        ENUM_SWITCH_NAME(ENGINE_STATE_SHUTDOWN)
        ENUM_SWITCH_NAME(ENGINE_STATE_EXIT)
    }
    return "Unknown Engine State";
}

static float delta_time = 0.0f;

static bool held_w = 0;
static bool held_a = 0;
static bool held_s = 0;
static bool held_d = 0;
static bool held_space = 0;
static bool held_shift = 0;
static bool held_ctrl = 0;
static bool held_tab = 1;
static bool world_has_input = 0;
static bool window_has_focus = 0;
static bool wireframe = 0;
static bool reload_resources = 0;
#define AO_ALGO_MAX 5

static SDL_Rect touch_region_move;
static SDL_Rect touch_region_move_exclude;
static SDL_Rect touch_region_camera;

static convar_int_t cvr_debug_screen("debug_screen", 0, 0, 1, "Enable debug screen (F3 menu)", CONVAR_FLAG_SAVE);

#include "gui/main_client.menu.cpp"

#define GLM_TO_IM(A) ImVec2((A).x, (A).y)

static void world_interaction_mouse(game_t* const game, Uint8 button)
{
    level_t* level = game->level;
    connection_t* connection = game->connection;

    glm::dvec3 cam_dir;
    cam_dir.x = SDL_cos(glm::radians(level->yaw)) * SDL_cos(glm::radians(level->pitch));
    cam_dir.y = SDL_sin(glm::radians(level->pitch));
    cam_dir.z = SDL_sin(glm::radians(level->yaw)) * SDL_cos(glm::radians(level->pitch));
    cam_dir = glm::normalize(cam_dir);

    glm::dvec3 rotation_point = level->get_camera_pos() /*+ glm::vec3(0.0f, (!crouching) ? 1.625f : 1.275f, 0.0f)*/;

    chunk_cubic_t* cache = NULL;
    itemstack_t block_at_ray;
    glm::ivec3 collapsed_ray;
    glm::dvec3 ray = rotation_point;
    {
        bool found = 0;
        for (int i = 0; !found && i <= 32 * 5; i++, ray += cam_dir / 32.0)
        {
            collapsed_ray = glm::floor(ray);
            if (level->get_block(collapsed_ray, block_at_ray, cache) && block_at_ray.id != BLOCK_ID_AIR && mc_id::is_block(block_at_ray.id))
                found = 1;
        }

        if (!found)
        {
            dc_log("Point: <%.1f, %.1f, %.1f>, Ray: <%.1f, %.1f, %.1f>", rotation_point.x, rotation_point.y, rotation_point.z, ray.x, ray.y, ray.z);
            return;
        }
    }

    /* TODO: Determine correct block face */
    if (button == 1)
    {
        connection_t::tentative_block_t t;
        t.timestamp = SDL_GetTicks();
        t.pos = collapsed_ray;
        if (level->get_block(t.pos, t.old, cache) && t.old.id != BLOCK_ID_AIR)
        {
            level->set_block(t.pos, BLOCK_ID_AIR, 0);

            if (connection)
                connection->push_tentative_block(t);
            packet_player_dig_t p;
            p.x = t.pos.x;
            p.y = t.pos.y - 1;
            p.z = t.pos.z;
            p.face = 1;
            p.status = PLAYER_DIG_STATUS_START_DIG;
            if (connection)
                connection->send_packet(p);
            p.status = PLAYER_DIG_STATUS_FINISH_DIG;
            if (connection)
                connection->send_packet(p);
        }
    }
    else if (button == 2)
    {
        packet_inventory_action_creative_t pack_inv_action;

        const bool can_create = level->gamemode_get() == mc_id::GAMEMODE_CREATIVE || !connection;

        int new_slot_id = level->inventory.hotbar_sel;

        /* Try to find existing */
        for (int i = level->inventory.hotbar_min; level->inventory.items[new_slot_id] != block_at_ray && i <= level->inventory.hotbar_max; i++)
            if (level->inventory.items[i] == block_at_ray)
                new_slot_id = i;

        /* Try to find the nearest open slot for block creation */
        if (can_create && level->inventory.items[new_slot_id] != block_at_ray)
        {
            const int num_squares = level->inventory.hotbar_max - level->inventory.hotbar_min + 1;
            const int cur_square = level->inventory.hotbar_sel - level->inventory.hotbar_min;
            bool found = 0;
            for (int i = 0; !found && i < num_squares; i++)
            {
                int slot_id = level->inventory.hotbar_min + ((i + cur_square) % num_squares);
                if (level->inventory.items[slot_id].id == BLOCK_ID_NONE || level->inventory.items[slot_id].id == BLOCK_ID_AIR)
                    new_slot_id = slot_id, found = 1;
            }
        }

        /* Inform the server of any slot selection changes */
        if (new_slot_id != level->inventory.hotbar_sel)
        {
            packet_hold_change_t pack;
            pack.slot_id = new_slot_id - level->inventory.hotbar_min;
            if (connection)
                connection->send_packet(pack);
        }
        level->inventory.hotbar_sel = new_slot_id;

        /* All actions past this point require creative permission */
        if (!can_create)
            return;

        itemstack_t& hand = level->inventory.items[level->inventory.hotbar_sel];

        if (hand == block_at_ray)
            return;

        hand = block_at_ray;
        hand.quantity = 1;

        pack_inv_action.item_id = hand.id;
        pack_inv_action.damage = hand.damage;
        pack_inv_action.quantity = hand.quantity;
        pack_inv_action.slot = level->inventory.hotbar_sel;

        if (connection)
            connection->send_packet(pack_inv_action);
    }
    /* TODO: Determine correct block face */
    else if (button == 3)
    {
        connection_t::tentative_block_t t;
        t.timestamp = SDL_GetTicks();
        t.pos = collapsed_ray;
        itemstack_t hand = level->inventory.items[level->inventory.hotbar_sel];

        if (level->get_block(t.pos, t.old, cache) && t.old != hand)
        {
            if (mc_id::is_block(hand.id))
            {
                if (connection)
                    connection->push_tentative_block(t);
                level->set_block(t.pos, hand);
            }

            packet_player_place_t p;
            p.x = collapsed_ray.x;
            p.y = collapsed_ray.y;
            p.z = collapsed_ray.z;
            p.direction = 1;
            p.block_item_id = hand.id;
            p.amount = 0;
            p.damage = hand.damage;
            if (connection)
                connection->send_packet(p);
        }
    }
}

/**
 * Draw world overlays
 *
 * @param level Level to draw overlays for (Must not be NULL!)
 * @param bg_draw_list List to draw to
 */
void render_world_overlays(level_t* level, ImDrawList* const bg_draw_list)
{
    /* This is a bit much */
    const glm::vec3 cam_pos = glm::floor(level->get_camera_pos());

    block_id_t type = BLOCK_ID_AIR;
    Uint8 metadata = 0;

    auto chunk_map = level->get_chunk_map();

    auto it = chunk_map->find(glm::ivec3(cam_pos) >> 4);
    if (it != chunk_map->end())
    {
        glm::ivec3 coords_rel = glm::ivec3(cam_pos) & 0x0F;
        block_id_t _type = it->second->get_type(coords_rel.x, coords_rel.y, coords_rel.z);
        if (_type != BLOCK_ID_AIR && _type != BLOCK_ID_NONE)
        {
            type = _type;
            metadata = it->second->get_metadata(coords_rel.x, coords_rel.y, coords_rel.z);
        }
    }

    if (type == BLOCK_ID_AIR || type == BLOCK_ID_NONE)
        return;

    ImVec2 size = ImGui::GetMainViewport()->Size;
    ImVec2 pos0(-4, -4);
    ImVec2 pos1(size + ImVec2(4, 4));
    ImVec2 uv0(0, 0);
    ImVec2 uv1(1, 1);
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    uv0.x = (-size.x / SDL_max(1.0f, size.y)) / 2.0f;
    uv1.x = -uv0.x;

    uv0.x += level->yaw / 360.0f;
    uv1.x += level->yaw / 360.0f;

    uv0.y -= level->pitch / 180.0f;
    uv1.y -= level->pitch / 180.0f;

    if (type == BLOCK_ID_WATER_FLOWING || type == BLOCK_ID_WATER_SOURCE)
    {
        ImTextureID tex_id(mc_gui::global_ctx->tex_id_water);
        bg_draw_list->AddRectFilled(ImVec2(-32, -32), ImGui::GetMainViewport()->Size + ImVec2(32, 32), IM_COL32(0, 0, 64, 255 * 0.5f));
        bg_draw_list->AddImage(tex_id, pos0, pos1, uv0, uv1, IM_COL32(255, 255, 255, 0.125f * 255));
    }
    else if (type == BLOCK_ID_LAVA_FLOWING || type == BLOCK_ID_LAVA_SOURCE)
    {
        /* In the future this should happen by setting fog intensity to high and fog color to red */
        bg_draw_list->AddRectFilled(pos0, pos1, IM_COL32(255, 24, 0, 0.5 * 255));
    }
    else if (mc_id::is_block(type) && !mc_id::is_transparent(type) && state::game_resources && state::game_resources->terrain_atlas)
    {
        ImTextureID tex_id(reinterpret_cast<ImTextureID>(&state::game_resources->terrain_atlas->binding));
        mc_id::terrain_face_t face = state::game_resources->terrain_atlas->get_face(mc_id::FACE_STONE);
#define BLOCK_OVERLAY(ID, FACE_ID)                                      \
    case ID:                                                            \
        face = state::game_resources->terrain_atlas->get_face(FACE_ID); \
        break;
        switch (type)
        {
            BLOCK_OVERLAY(BLOCK_ID_STONE, mc_id::FACE_STONE);
            BLOCK_OVERLAY(BLOCK_ID_GRASS, mc_id::FACE_GRASS_TOP);
            BLOCK_OVERLAY(BLOCK_ID_DIRT, mc_id::FACE_DIRT);
            BLOCK_OVERLAY(BLOCK_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
        case BLOCK_ID_WOOD_PLANKS:
        {
            /* Multiple plank types are not in Minecraft Beta 1.8.x */
            switch (metadata)
            {
                BLOCK_OVERLAY(WOOD_ID_SPRUCE, mc_id::FACE_PLANKS_SPRUCE);
                BLOCK_OVERLAY(WOOD_ID_BIRCH, mc_id::FACE_PLANKS_BIRCH);
            default: /* Fall through */
                BLOCK_OVERLAY(WOOD_ID_OAK, mc_id::FACE_PLANKS_OAK);
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_BEDROCK, mc_id::FACE_BEDROCK);

            BLOCK_OVERLAY(BLOCK_ID_SAND, mc_id::FACE_SAND);
            BLOCK_OVERLAY(BLOCK_ID_GRAVEL, mc_id::FACE_GRAVEL);
            BLOCK_OVERLAY(BLOCK_ID_ORE_GOLD, mc_id::FACE_GOLD_ORE);
            BLOCK_OVERLAY(BLOCK_ID_ORE_IRON, mc_id::FACE_IRON_ORE);
            BLOCK_OVERLAY(BLOCK_ID_ORE_COAL, mc_id::FACE_COAL_ORE);
        case BLOCK_ID_LOG:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(WOOD_ID_SPRUCE, mc_id::FACE_LOG_SPRUCE);
                BLOCK_OVERLAY(WOOD_ID_BIRCH, mc_id::FACE_LOG_BIRCH);
            default: /* Fall through */
                BLOCK_OVERLAY(WOOD_ID_OAK, mc_id::FACE_LOG_OAK);
            }
            break;
        }
        case BLOCK_ID_LEAVES:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(WOOD_ID_SPRUCE, mc_id::FACE_LEAVES_SPRUCE);
                BLOCK_OVERLAY(WOOD_ID_BIRCH, mc_id::FACE_LEAVES_BIRCH);
            default: /* Fall through */
                BLOCK_OVERLAY(WOOD_ID_OAK, mc_id::FACE_LEAVES_OAK);
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_SPONGE, mc_id::FACE_SPONGE);
            BLOCK_OVERLAY(BLOCK_ID_GLASS, mc_id::FACE_GLASS);
            BLOCK_OVERLAY(BLOCK_ID_ORE_LAPIS, mc_id::FACE_LAPIS_ORE);

            BLOCK_OVERLAY(BLOCK_ID_LAPIS, mc_id::FACE_LAPIS_BLOCK);
            BLOCK_OVERLAY(BLOCK_ID_DISPENSER, mc_id::FACE_DISPENSER_FRONT_HORIZONTAL);
            BLOCK_OVERLAY(BLOCK_ID_SANDSTONE, mc_id::FACE_SANDSTONE_NORMAL)
            BLOCK_OVERLAY(BLOCK_ID_NOTE_BLOCK, mc_id::FACE_NOTEBLOCK);
            BLOCK_OVERLAY(BLOCK_ID_BED, mc_id::FACE_BED_HEAD_TOP);
            BLOCK_OVERLAY(BLOCK_ID_RAIL_POWERED, mc_id::FACE_RAIL_GOLDEN_POWERED);
            BLOCK_OVERLAY(BLOCK_ID_RAIL_DETECTOR, mc_id::FACE_RAIL_DETECTOR);
            BLOCK_OVERLAY(BLOCK_ID_PISTON_STICKY, mc_id::FACE_PISTON_TOP_STICKY);
        case BLOCK_ID_FOLIAGE:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(0, mc_id::FACE_DEADBUSH);
                BLOCK_OVERLAY(1, mc_id::FACE_FERN);
            default: /* Fall through */
                BLOCK_OVERLAY(2, mc_id::FACE_TALLGRASS);
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_DEAD_BUSH, mc_id::FACE_DEADBUSH);
            BLOCK_OVERLAY(BLOCK_ID_PISTON, mc_id::FACE_PISTON_TOP_NORMAL);
            BLOCK_OVERLAY(BLOCK_ID_PISTON_HEAD, mc_id::FACE_PISTON_TOP_NORMAL);
        case BLOCK_ID_WOOL:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(WOOL_ID_WHITE, mc_id::FACE_WOOL_COLORED_WHITE)
                BLOCK_OVERLAY(WOOL_ID_ORANGE, mc_id::FACE_WOOL_COLORED_ORANGE)
                BLOCK_OVERLAY(WOOL_ID_MAGENTA, mc_id::FACE_WOOL_COLORED_MAGENTA)
                BLOCK_OVERLAY(WOOL_ID_LIGHT_BLUE, mc_id::FACE_WOOL_COLORED_LIGHT_BLUE)
                BLOCK_OVERLAY(WOOL_ID_YELLOW, mc_id::FACE_WOOL_COLORED_YELLOW)
                BLOCK_OVERLAY(WOOL_ID_LIME, mc_id::FACE_WOOL_COLORED_LIME)
                BLOCK_OVERLAY(WOOL_ID_PINK, mc_id::FACE_WOOL_COLORED_PINK)
                BLOCK_OVERLAY(WOOL_ID_GRAY, mc_id::FACE_WOOL_COLORED_GRAY)
                BLOCK_OVERLAY(WOOL_ID_LIGHT_GRAY, mc_id::FACE_WOOL_COLORED_SILVER)
                BLOCK_OVERLAY(WOOL_ID_CYAN, mc_id::FACE_WOOL_COLORED_CYAN)
                BLOCK_OVERLAY(WOOL_ID_PURPLE, mc_id::FACE_WOOL_COLORED_PURPLE)
                BLOCK_OVERLAY(WOOL_ID_BLUE, mc_id::FACE_WOOL_COLORED_BLUE)
                BLOCK_OVERLAY(WOOL_ID_BROWN, mc_id::FACE_WOOL_COLORED_BROWN)
                BLOCK_OVERLAY(WOOL_ID_GREEN, mc_id::FACE_WOOL_COLORED_GREEN)
                BLOCK_OVERLAY(WOOL_ID_RED, mc_id::FACE_WOOL_COLORED_RED)
                BLOCK_OVERLAY(WOOL_ID_BLACK, mc_id::FACE_WOOL_COLORED_BLACK)
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_FLOWER_YELLOW, mc_id::FACE_FLOWER_DANDELION);
            BLOCK_OVERLAY(BLOCK_ID_FLOWER_RED, mc_id::FACE_FLOWER_ROSE);
            BLOCK_OVERLAY(BLOCK_ID_MUSHROOM_BLAND, mc_id::FACE_MUSHROOM_BROWN);
            BLOCK_OVERLAY(BLOCK_ID_MUSHROOM_RED, mc_id::FACE_MUSHROOM_RED);

            BLOCK_OVERLAY(BLOCK_ID_GOLD, mc_id::FACE_GOLD_BLOCK);
            BLOCK_OVERLAY(BLOCK_ID_IRON, mc_id::FACE_IRON_BLOCK);

        case BLOCK_ID_SLAB_DOUBLE:
        case BLOCK_ID_SLAB_SINGLE:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(SLAB_ID_SMOOTH, mc_id::FACE_STONE_SLAB_SIDE);
                BLOCK_OVERLAY(SLAB_ID_SANDSTONE, mc_id::FACE_SANDSTONE_NORMAL);
                BLOCK_OVERLAY(SLAB_ID_WOOD, mc_id::FACE_PLANKS_OAK);
                BLOCK_OVERLAY(SLAB_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
                BLOCK_OVERLAY(SLAB_ID_BRICKS, mc_id::FACE_BRICK);
                BLOCK_OVERLAY(SLAB_ID_BRICKS_STONE, mc_id::FACE_STONEBRICK);
            default: /* Fall through */
                BLOCK_OVERLAY(SLAB_ID_SMOOTH_SIDE, mc_id::FACE_STONE_SLAB_TOP);
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_BRICKS, mc_id::FACE_BRICK);
            BLOCK_OVERLAY(BLOCK_ID_TNT, mc_id::FACE_TNT_SIDE);
            BLOCK_OVERLAY(BLOCK_ID_BOOKSHELF, mc_id::FACE_BOOKSHELF);
            BLOCK_OVERLAY(BLOCK_ID_COBBLESTONE_MOSSY, mc_id::FACE_COBBLESTONE_MOSSY);
            BLOCK_OVERLAY(BLOCK_ID_OBSIDIAN, mc_id::FACE_OBSIDIAN);

            BLOCK_OVERLAY(BLOCK_ID_TORCH, mc_id::FACE_TORCH_ON);
            BLOCK_OVERLAY(BLOCK_ID_FIRE, mc_id::FACE_FIRE_LAYER_0);
            BLOCK_OVERLAY(BLOCK_ID_SPAWNER, mc_id::FACE_MOB_SPAWNER);

            BLOCK_OVERLAY(BLOCK_ID_STAIRS_WOOD, mc_id::FACE_PLANKS_OAK);
            BLOCK_OVERLAY(BLOCK_ID_ORE_DIAMOND, mc_id::FACE_DIAMOND_ORE);
            BLOCK_OVERLAY(BLOCK_ID_DIAMOND, mc_id::FACE_DIAMOND_BLOCK);
            BLOCK_OVERLAY(BLOCK_ID_CRAFTING_TABLE, mc_id::FACE_CRAFTING_TABLE_SIDE);
            BLOCK_OVERLAY(BLOCK_ID_DIRT_TILLED, mc_id::FACE_DIRT);
            BLOCK_OVERLAY(BLOCK_ID_FURNACE_OFF, mc_id::FACE_FURNACE_FRONT_OFF);
            BLOCK_OVERLAY(BLOCK_ID_FURNACE_ON, mc_id::FACE_FURNACE_FRONT_ON);
            BLOCK_OVERLAY(BLOCK_ID_DOOR_WOOD, mc_id::FACE_DOOR_WOOD_UPPER);
            BLOCK_OVERLAY(BLOCK_ID_LADDER, mc_id::FACE_LADDER);
            BLOCK_OVERLAY(BLOCK_ID_RAIL, mc_id::FACE_RAIL_NORMAL);
            BLOCK_OVERLAY(BLOCK_ID_STAIRS_COBBLESTONE, mc_id::FACE_COBBLESTONE);
            BLOCK_OVERLAY(BLOCK_ID_PRESSURE_PLATE_STONE, mc_id::FACE_STONE);
            BLOCK_OVERLAY(BLOCK_ID_DOOR_IRON, mc_id::FACE_DOOR_IRON_UPPER);
            BLOCK_OVERLAY(BLOCK_ID_PRESSURE_PLATE_WOOD, mc_id::FACE_PLANKS_OAK);
            BLOCK_OVERLAY(BLOCK_ID_ORE_REDSTONE_OFF, mc_id::FACE_REDSTONE_ORE);
            BLOCK_OVERLAY(BLOCK_ID_ORE_REDSTONE_ON, mc_id::FACE_REDSTONE_ORE);
            BLOCK_OVERLAY(BLOCK_ID_TORCH_REDSTONE_OFF, mc_id::FACE_REDSTONE_TORCH_OFF);
            BLOCK_OVERLAY(BLOCK_ID_TORCH_REDSTONE_ON, mc_id::FACE_REDSTONE_TORCH_ON);
            BLOCK_OVERLAY(BLOCK_ID_BUTTON_STONE, mc_id::FACE_STONE);
            BLOCK_OVERLAY(BLOCK_ID_SNOW, mc_id::FACE_SNOW);
            BLOCK_OVERLAY(BLOCK_ID_ICE, mc_id::FACE_ICE);
            BLOCK_OVERLAY(BLOCK_ID_SNOW_BLOCK, mc_id::FACE_SNOW);
            BLOCK_OVERLAY(BLOCK_ID_CACTUS, mc_id::FACE_CACTUS_SIDE);
            BLOCK_OVERLAY(BLOCK_ID_CLAY, mc_id::FACE_CLAY);
            BLOCK_OVERLAY(BLOCK_ID_SUGAR_CANE, mc_id::FACE_REEDS);
            BLOCK_OVERLAY(BLOCK_ID_JUKEBOX, mc_id::FACE_JUKEBOX_SIDE);
        case BLOCK_ID_FENCE_WOOD:
        {
            /* Multiple fence types are not in Minecraft Beta 1.8.x */
            switch (metadata)
            {
                BLOCK_OVERLAY(WOOD_ID_SPRUCE, mc_id::FACE_PLANKS_SPRUCE);
                BLOCK_OVERLAY(WOOD_ID_BIRCH, mc_id::FACE_PLANKS_BIRCH);
            default: /* Fall through */
                BLOCK_OVERLAY(WOOD_ID_OAK, mc_id::FACE_PLANKS_OAK);
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_PUMPKIN, mc_id::FACE_PUMPKIN_FACE_OFF);
            BLOCK_OVERLAY(BLOCK_ID_NETHERRACK, mc_id::FACE_NETHERRACK);
            BLOCK_OVERLAY(BLOCK_ID_SOULSAND, mc_id::FACE_SOUL_SAND);
            BLOCK_OVERLAY(BLOCK_ID_GLOWSTONE, mc_id::FACE_GLOWSTONE);
            BLOCK_OVERLAY(BLOCK_ID_NETHER_PORTAL, mc_id::FACE_PORTAL);
            BLOCK_OVERLAY(BLOCK_ID_PUMPKIN_GLOWING, mc_id::FACE_PUMPKIN_FACE_ON)
            BLOCK_OVERLAY(BLOCK_ID_CAKE, mc_id::FACE_CAKE_TOP);
            BLOCK_OVERLAY(BLOCK_ID_REPEATER_OFF, mc_id::FACE_REPEATER_OFF);
            BLOCK_OVERLAY(BLOCK_ID_REPEATER_ON, mc_id::FACE_REPEATER_ON);
            BLOCK_OVERLAY(BLOCK_ID_CHEST_LOCKED, mc_id::FACE_DEBUG);
            BLOCK_OVERLAY(BLOCK_ID_TRAPDOOR, mc_id::FACE_TRAPDOOR);
        case BLOCK_ID_UNKNOWN_STONE:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(1, mc_id::FACE_COBBLESTONE);
                BLOCK_OVERLAY(2, mc_id::FACE_STONEBRICK);
            default: /* Fall through */
                BLOCK_OVERLAY(0, mc_id::FACE_STONE);
            }
            break;
        }
        case BLOCK_ID_BRICKS_STONE:
        {
            switch (metadata)
            {
                BLOCK_OVERLAY(STONE_BRICK_ID_MOSSY, mc_id::FACE_STONEBRICK_MOSSY);
                BLOCK_OVERLAY(STONE_BRICK_ID_CRACKED, mc_id::FACE_STONEBRICK_CRACKED);
            default: /* Fall through */
                BLOCK_OVERLAY(STONE_BRICK_ID_REGULAR, mc_id::FACE_STONEBRICK);
            }
            break;
        }
            BLOCK_OVERLAY(BLOCK_ID_MUSHROOM_BLOCK_BLAND, mc_id::FACE_MUSHROOM_BLOCK_SKIN_BROWN);
            BLOCK_OVERLAY(BLOCK_ID_MUSHROOM_BLOCK_RED, mc_id::FACE_MUSHROOM_BLOCK_SKIN_RED);
            BLOCK_OVERLAY(BLOCK_ID_IRON_BARS, mc_id::FACE_IRON_BARS);
            BLOCK_OVERLAY(BLOCK_ID_GLASS_PANE, mc_id::FACE_GLASS);
            BLOCK_OVERLAY(BLOCK_ID_MELON, mc_id::FACE_MELON_SIDE)
            BLOCK_OVERLAY(BLOCK_ID_MOSS, mc_id::FACE_VINE);
            BLOCK_OVERLAY(BLOCK_ID_STAIRS_BRICK, mc_id::FACE_BRICK);
            BLOCK_OVERLAY(BLOCK_ID_STAIRS_BRICK_STONE, mc_id::FACE_STONEBRICK);
        }
        ImVec2 off(SDL_max(center.x, center.y), SDL_max(center.x, center.y));

        if (cvr_r_overlay_inblock.get() && level->gamemode_get() != mc_id::GAMEMODE_SPECTATOR)
        {
            bg_draw_list->AddImage(tex_id, center - off, center + off, GLM_TO_IM(face.corners[1]), GLM_TO_IM((face.corners[2])));
            bg_draw_list->AddRectFilled(ImVec2(-32, -32), ImGui::GetMainViewport()->Size + ImVec2(32, 32), IM_COL32(0, 0, 0, 255 * 0.5f));
        }
    }
}

void do_debug_screen(mc_gui::mc_gui_ctx* ctx, game_t* game, ImDrawList* drawlist);
void do_debug_crosshair(mc_gui::mc_gui_ctx* ctx, game_t* game, ImDrawList* drawlist);

static ImVec2 dvec2_to_ImVec2(const glm::dvec2 x) { return ImVec2(x.x, x.y); }
static glm::dvec2 ImVec2_to_dvec2(const ImVec2 x) { return glm::dvec2(x.x, x.y); }

static void gui_game_manager()
{
    ImGui::SetNextWindowSize(ImVec2(580, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20.0f, ImGui::GetMainViewport()->GetWorkCenter().y), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.5));
    ImGui::Begin("Game manager");

    if (ImGui::Button("Rebuild resources"))
        reload_resources = 1;

    static std::string new_username = cvr_username.get();
    static std::string new_addr = cvr_autoconnect_addr.get();
    static Uint16 new_port = cvr_autoconnect_port.get();
    const Uint16 port_step = 1;

    ImGui::InputText("Username", &new_username);
    ImGui::InputText("Address", &new_addr);
    ImGui::InputScalar("Port", ImGuiDataType_U16, &new_port, &port_step);

    if (ImGui::Button("Init Game (Server)"))
        games.push_back(new game_t(new_addr, new_port, new_username, state::game_resources));
    ImGui::SameLine();
    if (ImGui::Button("Init Game (Test World)"))
    {
        game_t* new_game = new game_t(state::game_resources);
        new_game->create_testworld();
        games.push_back(new_game);
    }
    if (ImGui::Button("Init Game (Light Test Simplex World)"))
    {
        game_t* new_game = new game_t(state::game_resources);
        new_game->create_light_test_decorated_simplex({ 16, 8, 16 });
        games.push_back(new_game);
    }
    ImGui::SameLine();
    if (ImGui::Button("Init Game (Light Test SDL_rand World)"))
    {
        game_t* new_game = new game_t(state::game_resources);
        new_game->create_light_test_sdl_rand({ 16, 8, 16 });
        games.push_back(new_game);
    }

    for (auto it = games.begin(); it != games.end();)
    {
        ImGui::PushID((*it)->game_id);
        char text[64];
        snprintf(text, SDL_arraysize(text), "Game %d", (*it)->game_id);
        ImGui::RadioButton(text, &game_selected_idx, (*it)->game_id);
        ImGui::SameLine();
        if (ImGui::Button("Destroy"))
        {
            delete *it;
            it = games.erase(it);
        }
        else
            it = next(it);
        ImGui::PopID();
    }

    ImGui::End();
}

static void loop_stage_ensure_game_selected()
{
    game_selected = nullptr;

    for (game_t* g : games)
    {
        if (g->game_id == game_selected_idx)
        {
            game_selected = g;
            game_selected->level->sound_engine.resume();
        }
        else
            g->level->sound_engine.suspend();
    }

    if (!game_selected && games.size())
    {
        game_selected = games[0];
        game_selected_idx = game_selected->game_id;
    }
}

static void loop_stage_process_queued_input()
{
    bool warp_mouse_to_center = 0;
    bool mouse_wants_grabbed = world_has_input;
    if (cvr_mc_gui_mobile_controls.get())
        mouse_wants_grabbed = 0;
    if (SDL_GetWindowMouseGrab(state::window) != mouse_wants_grabbed)
    {
        warp_mouse_to_center = 1;
        SDL_SetWindowMouseGrab(state::window, mouse_wants_grabbed);
    }
    if (SDL_GetWindowRelativeMouseMode(state::window) != mouse_wants_grabbed)
    {
        warp_mouse_to_center = 1;
        SDL_SetWindowRelativeMouseMode(state::window, mouse_wants_grabbed);
    }

    if (warp_mouse_to_center)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
        SDL_WarpMouseInWindow(state::window, center.x, center.y);
    }

    if (game_selected && !client_menu_manager.stack_size() && (!game_selected->connection || game_selected->connection->get_in_world()))
        world_has_input = 1;

    if (!game_selected)
        world_has_input = 0;

    if ((ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) && tetra::imgui_ctx_main_wants_input())
        world_has_input = 0;

    if (!game_selected || client_menu_manager.stack_size())
        world_has_input = 0;

    if (!window_has_focus)
    {
        world_has_input = 0;
        if (game_selected && !client_menu_manager.stack_size() && (!game_selected->connection || game_selected->connection->get_in_world()))
            client_menu_manager.stack_push("menu.game");
    }

    if (!world_has_input)
    {
        held_w = 0;
        held_a = 0;
        held_s = 0;
        held_d = 0;
        held_space = 0;
        held_shift = 0;
        held_ctrl = 0;
        held_tab = 0;
    }

    for (game_t* g : games)
        if (g->connection && g->level)
            g->connection->run(g->level);

    bool in_flight = (game_selected && game_selected->level->gamemode_get() != mc_id::GAMEMODE_SURVIVAL
        && game_selected->level->gamemode_get() != mc_id::GAMEMODE_ADVENTURE);

    /* Base player speed is ~4.317 m/s, Source: https://minecraft.wiki/w/Sprinting */
    float camera_speed = 4.3175f * delta_time;

    /* Ground sprinting increases speed by 30%, Source: https://minecraft.wiki/w/Sprinting */
    if (held_ctrl)
        camera_speed *= in_flight ? 2.0f : 1.3f;

    /* Speed while in flight is around 250% of ground speed, Source: https://minecraft.wiki/w/Flying */
    if (in_flight)
        camera_speed *= 2.5f;

    /* Until spectator speed is changeable by some method, this will do */
    if (game_selected && game_selected->level->gamemode_get() == mc_id::GAMEMODE_SPECTATOR)
        camera_speed *= 1.5f;

    if (game_selected)
    {
        level_t* level = game_selected->level;

        float sensitivity = cvr_mouse_sensitivity.get();

        level->pitch -= touch_handler.get_dy() * sensitivity;
        if (level->pitch > 89.95f)
            level->pitch = 89.95f;
        if (level->pitch < -89.95f)
            level->pitch = -89.95f;

        level->yaw = SDL_fmodf(level->yaw + touch_handler.get_dx() * sensitivity, 360.0f);

        if (level->yaw < 0.0f)
            level->yaw += 360.0f;

        if (held_w)
            level->foot_pos += camera_speed * glm::vec3(SDL_cosf(glm::radians(level->yaw)), 0, SDL_sinf(glm::radians(level->yaw)));
        if (held_s)
            level->foot_pos -= camera_speed * glm::vec3(SDL_cosf(glm::radians(level->yaw)), 0, SDL_sinf(glm::radians(level->yaw)));
        if (held_a)
            level->foot_pos -= camera_speed * glm::vec3(-SDL_sinf(glm::radians(level->yaw)), 0, SDL_cosf(glm::radians(level->yaw)));
        if (held_d)
            level->foot_pos += camera_speed * glm::vec3(-SDL_sinf(glm::radians(level->yaw)), 0, SDL_cosf(glm::radians(level->yaw)));
        if (held_space)
            level->foot_pos.y += camera_speed;
        if (held_shift)
            level->foot_pos.y -= camera_speed;

        level->modifier_sprint.set_use(held_ctrl);
        level->modifier_fly.set_use(in_flight);

        level->fov = cvr_r_fov_base.get();
        level->fov *= level->modifier_sprint.get_modifier();
        level->fov *= level->modifier_fly.get_modifier();

        ImVec2 move_factors = touch_handler.get_move_factors(held_ctrl);
        level->foot_pos.y += camera_speed * touch_handler.get_vertical_factor();
        level->foot_pos += move_factors.y * camera_speed * glm::vec3(SDL_cosf(glm::radians(level->yaw)), 0, SDL_sinf(glm::radians(level->yaw)));
        level->foot_pos += move_factors.x * camera_speed * glm::vec3(-SDL_sinf(glm::radians(level->yaw)), 0, SDL_cosf(glm::radians(level->yaw)));

        if (touch_handler.get_button_left_hold())
            world_interaction_mouse(game_selected, 1);
        if (touch_handler.get_button_right_hold())
            world_interaction_mouse(game_selected, 3);
    }
}

static void loop_stage_prerender(glm::ivec2 win_size, bool& render_world)
{
    render_world = 0;

    music_counter -= delta_time;

    /* Decide main menu music stuff immediately */
    {
        if (game_selected)
            sound_engine_main_menu->kill_music();
        else if (music_counter < 0)
        {
            /* Try to spawn music again in 0.5min-1.5min */
            music_counter = 30.0f + SDL_randf() * 60.0f;

            sound_info_t sinfo;

            if (sound_engine_main_menu && state::game_resources && state::game_resources->sound_resources && !sound_engine_main_menu->is_music_playing())
            {
                if (state::game_resources->sound_resources->get_sound("minecraft:music.menu", sinfo))
                    sound_engine_main_menu->request_source(sinfo, glm::f64vec3(0.0), 1);
            }
        }
    }

    ImGuiContext* last_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui::NewFrame();

    int& menu_scale = mc_gui::global_ctx->menu_scale;
    {
        const ImVec2 viewport_size = ImGui::GetMainViewport()->Size;
        const glm::ivec2 scales = glm::ivec2(viewport_size.x, viewport_size.y) / menu_scale_step;
        int new_scale = SDL_max(SDL_min(scales.x, scales.y), 1);

        if (cvr_mc_gui_scale.get())
            new_scale = SDL_min(new_scale, cvr_mc_gui_scale.get());

        if (new_scale != menu_scale)
        {
            dc_log("New GUI Scale: %d (%d %d)", new_scale, scales.x, scales.y);
            ImGui::GetIO().FontGlobalScale = new_scale;

            ImGuiStyle& style = ImGui::GetStyle();

            style.ScaleAllSizes(double(new_scale) / double(SDL_max(menu_scale, 1)));

            style.ItemSpacing = ImVec2(4, 8) * new_scale;

            menu_scale = new_scale;
        }
    }

    if (convar_t::dev() && cvr_mc_gui_style_editor.get())
    {
        ImGui::SetWindowFontScale(1.0f / float(menu_scale));
        ImGui::ShowStyleEditor();
    }

    if (!game_selected) /* No game */
        client_menu_manager.set_default("menu.title");
    else if (game_selected->connection) /* External world */
    {
        if (game_selected->connection->get_in_world())
            client_menu_manager.set_default("in_game");
        else
            client_menu_manager.set_default("loading");
    }
    else /* Test world */
    {
        client_menu_manager.set_default("in_game");
    }

    ImDrawList* const bg_draw_list = ImGui::GetBackgroundDrawList();

    bg_draw_list->ChannelsSplit(2);
    bg_draw_list->ChannelsSetCurrent(1);

    client_menu_return_t menu_ret = client_menu_manager.run_last_in_stack(win_size, bg_draw_list);
    bg_draw_list->ChannelsSetCurrent(0);

    /* client_menu_manager.run_last_in_stack() may delete the game */
    bool in_world = !!(game_selected);
    if (in_world && game_selected->connection)
        in_world = game_selected->connection->get_in_world();

    render_world = menu_ret.allow_world && in_world;

    if (render_world)
    {
        render_world_overlays(game_selected->level, bg_draw_list);

        render_hotbar(mc_gui::global_ctx, bg_draw_list);

        if (cvr_debug_screen.get())
        {
            do_debug_crosshair(mc_gui::global_ctx, game_selected, bg_draw_list);
            do_debug_screen(mc_gui::global_ctx, game_selected, bg_draw_list);
        }
        else
        {
            /* Render crosshair
             * NOTE: If a function called by client_menu_manager.run_last_in_stack() adds to
             * the background draw list then the crosshair will be drawn on top!
             */
            const ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
            const float scale = cvr_r_crosshair_scale.get() * SDL_min(3, mc_gui::global_ctx->menu_scale);
            const ImVec2 pos0(center - ImVec2(8.0f, 8.0f) * scale);
            const ImVec2 pos1(center + ImVec2(8.0f, 8.0f) * scale);
            const ImVec2 uv0(240.0f / 256.0f, 0.0f);
            const ImVec2 uv1(1.0f, 16.0f / 256.0f);
            bg_draw_list->AddCallback(ImDrawCallback_ChangePipeline, pipeline_imgui_crosshair);
            if (cvr_r_crosshair_widgets.get())
                bg_draw_list->AddImage(mc_gui::global_ctx->tex_id_widgets, pos0, pos1, uv0, uv1);
            else
                bg_draw_list->AddImage(mc_gui::global_ctx->tex_id_crosshair, pos0, pos1);
            bg_draw_list->AddCallback(ImDrawCallback_ChangePipeline, nullptr);
        }

        /* Draw world dim */
        if (client_menu_manager.stack_size())
            bg_draw_list->AddRectFilled(ImVec2(-32, -32), ImGui::GetMainViewport()->Size + ImVec2(32, 32), IM_COL32(32, 32, 32, 255 * 0.5f));
    }

    bool display_pano = (menu_ret.allow_pano && !in_world);
    bool display_dirt = (menu_ret.allow_dirt && ((!menu_ret.allow_pano && !in_world) || (in_world && !menu_ret.allow_world)));

    if (display_pano || display_dirt)
    {
        ImTextureID tex_id = mc_gui::global_ctx->tex_id_bg;

        double side_len = 32.0 * double(SDL_max(1, mc_gui::global_ctx->menu_scale));

        glm::dvec2 pos0 = glm::dvec2(0);
        glm::dvec2 pos1 = ImVec2_to_dvec2(ImGui::GetMainViewport()->Size) + glm::dvec2(side_len);

        glm::dvec2 uv0(0.0);
        glm::dvec2 uv1 = pos1 / side_len;

        bg_draw_list->AddImage(tex_id, dvec2_to_ImVec2(pos0), dvec2_to_ImVec2(pos1), dvec2_to_ImVec2(uv0), dvec2_to_ImVec2(uv1), IM_COL32(64, 64, 64, 255));
    }

    bg_draw_list->ChannelsMerge();

    if (cvr_mc_gui_mobile_controls_diag.get())
        touch_handler.draw_imgui(ImGui::GetForegroundDrawList(), ImVec2(0, 0), ImGui::GetMainViewport()->Size * 0.25f);

    ImGui::Render();
    ImGui::SetCurrentContext(last_ctx);

    if (game_selected)
        game_selected->level->render_stage_prepare(win_size);
}

static void loop_stage_render(
    SDL_GPUCommandBuffer* gpu_command_buffer, SDL_GPUTexture* gpu_target, const ImVec4 clear_color, const glm::ivec2 win_size, bool render_world)
{
    if (!game_selected)
        render_world = 0;

    ImGuiContext* last_ctx = ImGui::GetCurrentContext();

    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui::SetCurrentContext(last_ctx);
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (gpu_target == nullptr || is_minimized || gpu_command_buffer == nullptr)
        return;

    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    SDL_PushGPUDebugGroup(gpu_command_buffer, "[mc_gui]: Copy pass");
    Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, gpu_command_buffer);
    SDL_PopGPUDebugGroup(gpu_command_buffer);
    ImGui::SetCurrentContext(last_ctx);

    if (render_world)
    {
        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(gpu_command_buffer);

        SDL_PushGPUDebugGroup(gpu_command_buffer, "[Level]: Copy pass");
        game_selected->level->render_stage_copy(copy_pass);
        state::game_resources->terrain_atlas->update(copy_pass);
        SDL_PopGPUDebugGroup(gpu_command_buffer);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_GPUColorTargetInfo tinfo_color = {};
    tinfo_color.texture = gpu_target;
    tinfo_color.clear_color = SDL_FColor { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
    tinfo_color.load_op = SDL_GPU_LOADOP_CLEAR;
    tinfo_color.store_op = SDL_GPU_STOREOP_STORE;
    tinfo_color.mip_level = 0;
    tinfo_color.layer_or_depth_plane = 0;
    tinfo_color.cycle = false;
    SDL_GPUTextureCreateInfo cinfo_depth_tex = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = state::gpu_tex_format_best_depth_only,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = Uint32(win_size.x),
        .height = Uint32(win_size.y),
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0,
    };
    SDL_GPUDepthStencilTargetInfo tinfo_depth = {
        .texture = SDL_CreateGPUTexture(state::gpu_device, &cinfo_depth_tex),
        .clear_depth = 1.0,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = 0,
        .clear_stencil = 0,
        .padding1 = 0,
        .padding2 = 0,
    };
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(gpu_command_buffer, &tinfo_color, 1, &tinfo_depth);
    SDL_ReleaseGPUTexture(state::gpu_device, tinfo_depth.texture);

    if (render_world)
    {
        ImGui::SetCurrentContext(last_ctx);
        SDL_PushGPUDebugGroup(gpu_command_buffer, "[Level]: Render pass");
        game_selected->level->render_stage_render(gpu_command_buffer, render_pass, win_size, delta_time);
        SDL_PopGPUDebugGroup(gpu_command_buffer);
        ImGui::SetCurrentContext(imgui_ctx_main_menu);
    }

    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    SDL_PushGPUDebugGroup(gpu_command_buffer, "[mc_gui]: Render pass");
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, gpu_command_buffer, render_pass, pipeline_imgui_regular);
    SDL_PopGPUDebugGroup(gpu_command_buffer);
    ImGui::SetCurrentContext(last_ctx);

    SDL_EndGPURenderPass(render_pass);
}

static void process_event(SDL_Event& event, bool* done)
{
    if (tetra::process_event(event))
        *done = true;

    if (event.type == SDL_EVENT_QUIT)
        *done = true;

    if (event.window.windowID != SDL_GetWindowID(state::window) && event.type != SDL_EVENT_FINGER_DOWN && event.type != SDL_EVENT_FINGER_CANCELED
        && event.type != SDL_EVENT_FINGER_MOTION && event.type != SDL_EVENT_FINGER_UP)
        return;

    switch (event.window.type)
    {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        *done = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        TRACE("Focus Lost");
        world_has_input = false;
        window_has_focus = 0;
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        TRACE("Focus Gained");
        window_has_focus = 1;
        break;
    default:
        break;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F2 && !event.key.repeat)
        take_screenshot = 1;

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F3 && !event.key.repeat)
        cvr_debug_screen.set(!cvr_debug_screen.get());

    if (tetra::imgui_ctx_main_wants_input())
        return;

    game_t* game = game_selected;

    {
        ImGuiContext* last_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx_main_menu);
        ImGui_ImplSDL3_ProcessEvent(&event);
        ImGui::SetCurrentContext(last_ctx);
    }

    if (!game)
        return;

    level_t* level = game->level;
    connection_t* connection = game->connection;
    bool in_world = game;
    if (in_world && connection)
        in_world = connection->get_in_world();

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE && in_world)
    {
        if (client_menu_manager.stack_size())
            client_menu_manager.stack_clear();
        else
            client_menu_manager.stack_push("menu.game");

        world_has_input = !client_menu_manager.stack_size();

        TRACE("%d %d", client_menu_manager.stack_size(), world_has_input);
    }

    touch_handler.set_world_focus(world_has_input && cvr_mc_gui_mobile_controls.get());
    if (!world_has_input)
        return;
    /* World interaction past this point */
    touch_handler.feed_event(event);

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !cvr_mc_gui_mobile_controls.get())
        world_interaction_mouse(game, event.button.button);

    if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        if (SDL_fabsf(event.wheel.y) >= 0.99f)
        {
            packet_hold_change_t pack;
            int num_slots = level->inventory.hotbar_max - level->inventory.hotbar_min + 1;
            pack.slot_id = (level->inventory.hotbar_sel - int(event.wheel.y) - level->inventory.hotbar_min) % num_slots;
            if (pack.slot_id < 0)
                pack.slot_id += num_slots;
            level->inventory.hotbar_sel = level->inventory.hotbar_min + pack.slot_id;
            if (connection)
                connection->send_packet(pack);
        }
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION && !cvr_mc_gui_mobile_controls.get())
    {
        float sensitivity = cvr_mouse_sensitivity.get();

        if (event.motion.yrel != 0.0f)
        {
            level->pitch -= event.motion.yrel * sensitivity;
            if (level->pitch > 89.95f)
                level->pitch = 89.95f;
            if (level->pitch < -89.95f)
                level->pitch = -89.95f;
        }

        if (event.motion.xrel != 0.0f)
            level->yaw = SDL_fmodf(level->yaw + event.motion.xrel * sensitivity, 360.0f);

        if (level->yaw < 0.0f)
            level->yaw += 360.0f;
    }

    if (event.type == SDL_EVENT_KEY_DOWN)
        switch (event.key.scancode)
        {
        case SDL_SCANCODE_END:
            *done = 1;
            break;
        case SDL_SCANCODE_W:
            held_w = 1;
            break;
        case SDL_SCANCODE_S:
            held_s = 1;
            break;
        case SDL_SCANCODE_A:
            held_a = 1;
            break;
        case SDL_SCANCODE_D:
            held_d = 1;
            break;
        case SDL_SCANCODE_SPACE:
            held_space = 1;
            break;
        case SDL_SCANCODE_LSHIFT:
            held_shift = 1;
            break;
        case SDL_SCANCODE_LCTRL:
            held_ctrl = 1;
            break;
        case SDL_SCANCODE_TAB:
            held_tab = 1;
            break;
        case SDL_SCANCODE_B:
        {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            break;
        }
        case SDL_SCANCODE_P:
        {
            glm::ivec3 chunk_coords = glm::ivec3(level->foot_pos) >> 4;
            for (chunk_cubic_t* c : level->get_chunk_vec())
            {
                if (chunk_coords != c->pos)
                    continue;
                c->free_renderer_resources();
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
            }
            break;
        }
        case SDL_SCANCODE_N:
        {
            for (chunk_cubic_t* c : level->get_chunk_vec())
            {
                glm::ivec3 chunk_coords = glm::ivec3(level->foot_pos) >> 4;
                if (chunk_coords != c->pos)
                    continue;
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
            }
            break;
        }
        case SDL_SCANCODE_1: /* Fall through */
        case SDL_SCANCODE_2: /* Fall through */
        case SDL_SCANCODE_3: /* Fall through */
        case SDL_SCANCODE_4: /* Fall through */
        case SDL_SCANCODE_5: /* Fall through */
        case SDL_SCANCODE_6: /* Fall through */
        case SDL_SCANCODE_7: /* Fall through */
        case SDL_SCANCODE_8: /* Fall through */
        case SDL_SCANCODE_9:
        {
            packet_hold_change_t pack;
            pack.slot_id = event.key.scancode - SDL_SCANCODE_1;
            level->inventory.hotbar_sel = level->inventory.hotbar_min + pack.slot_id;
            if (connection)
                connection->send_packet(pack);
            break;
        }
        case SDL_SCANCODE_M:
        {
            glm::ivec3 chunk_coords = glm::ivec3(level->foot_pos) >> 4;
            for (chunk_cubic_t* c : level->get_chunk_vec())
            {
                if (SDL_abs(chunk_coords.x - c->pos.x) > 1 || SDL_abs(chunk_coords.y - c->pos.y) > 1 || SDL_abs(chunk_coords.z - c->pos.z) > 1)
                    continue;
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
            }
            break;
        }
        case SDL_SCANCODE_C:
        {
            state::game_resources->ao_algorithm = (state::game_resources->ao_algorithm + 1) % (AO_ALGO_MAX + 1);
            dc_log("Setting ao_algorithm to %d", state::game_resources->ao_algorithm);
            break;
        }
        case SDL_SCANCODE_X:
        {
            state::game_resources->use_texture = !state::game_resources->use_texture;
            dc_log("Setting use_texture to %d", state::game_resources->use_texture);
            break;
        }
        case SDL_SCANCODE_R:
        {
            compile_shaders();
            break;
        }
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_GRAVE:
            world_has_input = false;
            break;
        default:
            break;
        }

    if (event.type == SDL_EVENT_KEY_UP)
        switch (event.key.scancode)
        {
        case SDL_SCANCODE_W:
            held_w = 0;
            break;
        case SDL_SCANCODE_S:
            held_s = 0;
            break;
        case SDL_SCANCODE_A:
            held_a = 0;
            break;
        case SDL_SCANCODE_D:
            held_d = 0;
            break;
        case SDL_SCANCODE_SPACE:
            held_space = 0;
            break;
        case SDL_SCANCODE_LSHIFT:
            held_shift = 0;
            break;
        case SDL_SCANCODE_LCTRL:
            held_ctrl = 0;
            break;
        case SDL_SCANCODE_TAB:
            held_tab = 0;
            break;
        default:
            break;
        }
}

static convar_int_t cvr_gui_renderer("gui_renderer", 0, 0, 1, "Show renderer internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_lightmap("gui_lightmap", 0, 0, 1, "Show lightmap internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_sound("gui_sound", 0, 0, 1, "Show sound engine internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_engine_state("gui_engine_state", 0, 0, 1, "Show engine state menu", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_inventory("gui_inventory", 0, 0, 1, "Show primitive inventory window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

static bool engine_state_step()
{
    /* Ensure engine only steps forwards */
    SDL_assert(engine_state_current <= engine_state_target);
    if (!(engine_state_current < engine_state_target))
        return 0;

    switch (engine_state_current)
    {
    case ENGINE_STATE_OFFLINE:
    {
        if (can_launch_game() && engine_state_target > ENGINE_STATE_CONFIGURE)
        {
            engine_state_current = ENGINE_STATE_INITIALIZE;
            dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
            return engine_state_step();
        }
        else
        {
            engine_state_current = ENGINE_STATE_CONFIGURE;
            dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
            return 0;
        }
    }
    case ENGINE_STATE_CONFIGURE:
    {
        if (engine_state_target == ENGINE_STATE_EXIT)
            engine_state_current = ENGINE_STATE_EXIT;
        return 0;
    }
    case ENGINE_STATE_INITIALIZE:
    {
        PHYSFS_mkdir("/game");
        if (cvr_dir_game.get().length())
            PHYSFS_mount(cvr_dir_game.get().c_str(), "/game", 0);
        if (!PHYSFS_mount(cvr_dir_assets.get().c_str(), "/assets", 0))
            util::die("Unable to mount assets");
        if (!PHYSFS_mount(cvr_path_resource_pack.get().c_str(), "/_resources/", 0))
            util::die("Unable to mount base resource pack");

        initialize_resources();

        engine_state_current = ENGINE_STATE_RUNNING;
        dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
        return engine_state_step();
    }
    case ENGINE_STATE_RUNNING:
        engine_state_current = ENGINE_STATE_SHUTDOWN;
        return engine_state_step();
    case ENGINE_STATE_SHUTDOWN:

        deinitialize_resources();

        engine_state_current = ENGINE_STATE_EXIT;
        dc_log("Engine state moving to %s", engine_state_name(engine_state_current));
        return engine_state_step();
    case ENGINE_STATE_EXIT:
        return 0;
    }

    SDL_assert(0);
    return 0;
}

static bool engine_state_menu()
{
    if (!cvr_gui_engine_state.get())
        return false;

    ImGui::BeginCVR("Engine State Viewer/Manipulator", &cvr_gui_engine_state);

    if (ImGui::BeginTable("Engine State Table", 3))
    {
        float char_width = ImGui::CalcTextSize("ABCDEF").x / 6.0f;
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, char_width * 15.0f);
        ImGui::TableSetupColumn("State Name", ImGuiTableColumnFlags_WidthFixed, char_width * 25.0f);
        ImGui::TableSetupColumn("Manipulate", ImGuiTableColumnFlags_WidthFixed, char_width * 18.0f);

        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Current state:");
        ImGui::TableNextColumn();
        ImGui::Text("%s", engine_state_name(engine_state_current));
        ImGui::TableNextColumn();
        ImGui::InputInt("##Current state", (int*)&engine_state_current);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Target state:");
        ImGui::TableNextColumn();
        ImGui::Text("%s", engine_state_name(engine_state_target));
        ImGui::TableNextColumn();
        ImGui::InputInt("##Target state", (int*)&engine_state_target);

        ImGui::EndTable();
    }
    ImGui::End();

    return cvr_gui_engine_state.get();
}

static gui_register_menu register_engine_state_menu(engine_state_menu);

static void stbi_physfs_write_func(void* context, void* data, int size) { PHYSFS_writeBytes((PHYSFS_File*)context, data, size); }
static void screenshot_func(const glm::ivec2 win_size)
{
    if (!take_screenshot)
        return;
    take_screenshot = 0;

    dc_log_error("Screenshot functionality has not yet been updated for SDL_GPU");
    return;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    Uint8* buf = (Uint8*)malloc(win_size.x * win_size.y * 3);
    glReadPixels(0, 0, win_size.x, win_size.y, GL_RGB, GL_UNSIGNED_BYTE, buf);

    SDL_Time cur_time;
    SDL_GetCurrentTime(&cur_time);

    SDL_DateTime dt;
    SDL_TimeToDateTime(cur_time, &dt, 1);

    char buf_path[256];
    snprintf(buf_path, SDL_arraysize(buf_path), "screenshots/Screenshot_%04d-%02d-%02d_%02d.%02d.%02d.%02d.png", dt.year, dt.month, dt.day, dt.hour, dt.minute,
        dt.second, dt.nanosecond / 10000000);

    stbi_flip_vertically_on_write(true);

    if (!PHYSFS_mkdir("screenshots"))
    {
        dc_log_error("Error saving screenshot: Unable to create output directory");
        free(buf);
        return;
    }

    if (PHYSFS_exists(buf_path))
    {
        dc_log_error("Error saving screenshot: \"%s\" already exists", buf_path);
        free(buf);
        return;
    }

    PHYSFS_File* fd = PHYSFS_openWrite(buf_path);

    if (!fd)
    {
        PHYSFS_ErrorCode errcode = PHYSFS_getLastErrorCode();
        dc_log_error("Error saving screenshot: PHYSFS %d (%s)", errcode, PHYSFS_getErrorByCode(errcode));
        free(buf);
        return;
    }

    /* TODO: Move this part to a separate thread */
    int result = stbi_write_png_to_func(stbi_physfs_write_func, fd, win_size.x, win_size.y, 3, buf, win_size.x * 3);

    if (result)
        dc_log("Saved screenshot to %s", buf_path);
    else
        dc_log("Error saving screenshot: stbi_write_png_to_func() returned %d", result);

    free(buf);
}

static convar_int_t cvr_profile_light("profile_light", 0, 0, 1, "Profile lighting engine then exit", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static void profile_light()
{
    game_t game(state::game_resources);

    game.level->enable_timer_log_light = 1;

    const glm::ivec3 world_configs[] = {
        { 24, 16, 24 },
        { 24, 8, 24 },

        { 16, 16, 16 },

        { 8, 64, 8 },
        { 8, 32, 8 },
        { 8, 16, 8 },
        { 8, 8, 8 },

        { 4, 256, 4 },
        { 4, 128, 4 },
        { 4, 64, 4 },
        { 4, 32, 4 },
        { 4, 16, 4 },
        { 4, 8, 4 },
    };

    level_t::performance_timer_t timers[3][5];

    /* Nothing special about this seed value */
    Uint64 r_state = 0x41a870ef60fef764;
    size_t overall_block_emptiness[3] = { 0 };
    size_t overall_block_total[3] = { 0 };

    for (int pass = 0; pass < IM_ARRAYSIZE(world_configs) * 2; pass++)
    {
        const glm::ivec3 world_size = world_configs[pass % IM_ARRAYSIZE(world_configs)];
        const int world_volume = world_size.x * world_size.y * world_size.z;
        const bool using_sdl_rand = !(pass < IM_ARRAYSIZE(world_configs));
#define SPACER "================================"
        dc_log(SPACER " Pass %d-%d (%dx%dx%d) (%d) " SPACER, using_sdl_rand, pass, world_size.x, world_size.y, world_size.z, world_volume);
        game.level->clear();

        if (!using_sdl_rand)
            game.create_light_test_decorated_simplex(world_size);
        else
            game.create_light_test_sdl_rand(world_size);

        size_t emptiness = 0;
        size_t total = 0;
        for (chunk_cubic_t* c : game.level->get_chunk_vec())
        {
            c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
            for (int pos_it = 0; pos_it < SUBCHUNK_SIZE_VOLUME; pos_it++)
            {
                const int y = (pos_it) & 0x0F;
                const int z = (pos_it >> 4) & 0x0F;
                const int x = (pos_it >> 8) & 0x0F;
                emptiness += c->get_type(x, y, z) == 0;
            }
            total += SUBCHUNK_SIZE_VOLUME;
        }

        dc_log("Emptiness: %.3f%%", double((emptiness * 1000000) / total) / 10000.0);

        game.level->render_stage_prepare({ 10, 10 });

        /* Add to individual counters */
        timers[using_sdl_rand][0] += game.level->last_perf_light_pass1;
        timers[using_sdl_rand][1] += game.level->last_perf_light_pass2;
        timers[using_sdl_rand][2] += game.level->last_perf_light_pass3;
        timers[using_sdl_rand][3] += game.level->last_perf_mesh_pass;
        timers[using_sdl_rand][4] += timers[using_sdl_rand][0];
        timers[using_sdl_rand][4] += timers[using_sdl_rand][1];
        timers[using_sdl_rand][4] += timers[using_sdl_rand][2];
        overall_block_emptiness[using_sdl_rand] += emptiness;
        overall_block_total[using_sdl_rand] += total;

        /* Add to combined counters */
        timers[2][0] += game.level->last_perf_light_pass1;
        timers[2][1] += game.level->last_perf_light_pass2;
        timers[2][2] += game.level->last_perf_light_pass3;
        timers[2][3] += game.level->last_perf_mesh_pass;
        timers[2][4] += timers[2][0];
        timers[2][4] += timers[2][1];
        timers[2][4] += timers[2][2];
        overall_block_emptiness[2] += emptiness;
        overall_block_total[2] += total;
    }

    int j = 0;
    int i = 0;
    /* chunk.cpp world */
    dc_log(SPACER " Results (SimplexNoise + SDL_rand_bits world) " SPACER);
    dc_log("Emptiness: %.3f%%", double((overall_block_emptiness[j] * 1000000) / overall_block_total[j]) / 10000.0);
    for (; i < 3; i++)
        if (timers[j][i].built)
            dc_log("Lit %zu chunks in %.2f ms (%.2f us per) (Pass %d)", timers[j][i].built, timers[j][i].duration / 1000.0 / 1000.0,
                timers[j][i].duration / timers[j][i].built / 1000.0, i % 5);

    j++, i = 0;

    /* SDL_rand_bits world */
    dc_log(SPACER " Results (SDL_rand_bits world) " SPACER);
    dc_log("Emptiness: %.3f%%", double((overall_block_emptiness[j] * 1000000) / overall_block_total[j]) / 10000.0);
    for (; i < 3; i++)
        if (timers[j][i].built)
            dc_log("Lit %zu chunks in %.2f ms (%.2f us per) (Pass %d)", timers[j][i].built, timers[j][i].duration / 1000.0 / 1000.0,
                timers[j][i].duration / timers[j][i].built / 1000.0, i % 5);

    j++, i = 0;

    /* Both worlds */
    dc_log(SPACER " Results (Both worlds) " SPACER);
    dc_log("Emptiness: %.3f%%", double((overall_block_emptiness[j] * 1000000) / overall_block_total[j]) / 10000.0);
    for (; i < 3; i++)
        if (timers[j][i].built)
            dc_log("Lit %zu chunks in %.2f ms (%.2f us per) (Pass %d)", timers[j][i].built, timers[j][i].duration / 1000.0 / 1000.0,
                timers[j][i].duration / timers[j][i].built / 1000.0, i % 5);
}

#include "sound/sound_world.h"

static glm::uvec2 win_size(0, 0);

SDL_Window* state::window = nullptr;
SDL_GPUDevice* state::gpu_device = nullptr;
SDL_GPUTexture* state::gpu_debug_texture = nullptr;
SDL_GPUSampler* state::gpu_debug_sampler = nullptr;
SDL_GPUTextureFormat state::gpu_tex_format_best_depth_only = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

static void upload_debug_texture(SDL_GPUCopyPass* const copy_pass)
{
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = 32,
        .height = 32,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = SDL_CreateProperties(),
    };
    SDL_SetStringProperty(tex_info.props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, "Missing texture (Texture)");

    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.props = SDL_CreateProperties();
    SDL_SetStringProperty(sampler_info.props, SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, "Missing texture (Sampler)");

    if (!(state::gpu_debug_texture = SDL_CreateGPUTexture(state::gpu_device, &tex_info)))
        util::die("Unable to create debug texture, SDL_CreateGPUTexture: %s", SDL_GetError());

    if (!(state::gpu_debug_sampler = SDL_CreateGPUSampler(state::gpu_device, &sampler_info)))
        util::die("Unable to create debug texture, SDL_CreateGPUSampler: %s", SDL_GetError());

    SDL_GPUTransferBufferCreateInfo tbo_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = tex_info.height * tex_info.width * 4,
        .props = 0,
    };

    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(state::gpu_device, &tbo_info);
    if (tbo == nullptr)
        util::die("Failed to acquire TBO to upload debug texture");

    Uint8* tbo_pointer = (Uint8*)SDL_MapGPUTransferBuffer(state::gpu_device, tbo, false);
    if (tbo_pointer == nullptr)
        util::die("Failed to map TBO to upload debug texture, SDL_MapGPUTransferBuffer: %s", SDL_GetError());

    Uint64 r_state = 0x881abfd3ab1e0024;
    for (Uint32 y = 0; y < tex_info.height; y++)
        for (Uint32 x = 0; x < tex_info.width; x++)
        {
            tbo_pointer[(y * tex_info.width + x) * 4 + 0] = SDL_rand_r(&r_state, 0x0F + 1) + ((x % 2 == y % 2) ? 0 : 0xF0);
            tbo_pointer[(y * tex_info.width + x) * 4 + 1] = SDL_rand_r(&r_state, 0x0F + 1);
            tbo_pointer[(y * tex_info.width + x) * 4 + 2] = SDL_rand_r(&r_state, 0x0F + 1) + ((x % 2 == y % 2) ? 0 : 0xF0);
            tbo_pointer[(y * tex_info.width + x) * 4 + 3] = 0xFF;
        }

    SDL_UnmapGPUTransferBuffer(state::gpu_device, tbo);

    SDL_GPUTextureTransferInfo region_tbo = {
        .transfer_buffer = tbo,
        .offset = 0,
        .pixels_per_row = tex_info.width,
        .rows_per_layer = tex_info.height,
    };

    SDL_GPUTextureRegion region_tex = {
        .texture = state::gpu_debug_texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = tex_info.width,
        .h = tex_info.height,
        .d = 1,
    };

    SDL_UploadToGPUTexture(copy_pass, &region_tbo, &region_tex, false);
    SDL_ReleaseGPUTransferBuffer(state::gpu_device, tbo);
}

void setup_static_gpu_state()
{
    state::window = tetra::window;
    state::gpu_device = tetra::gpu_device;

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state::gpu_device);
    if (command_buffer == nullptr)
        util::die("Failed to acquire command buffer to upload debug texture, SDL_AcquireGPUCommandBuffer: %s", SDL_GetError());

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (command_buffer == nullptr)
        util::die("Failed to acquire copy pass to upload debug texture, SDL_BeginGPUCopyPass: %s", SDL_GetError());

    upload_debug_texture(copy_pass);

    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);

    SDL_GPUTextureFormat best_depth_formats[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    };
    const char* best_depth_formats_str[SDL_arraysize(best_depth_formats)] = { "D32_FLOAT", "D24_UNORM_S8_UINT", "D24_UNORM", "D16_UNORM" };
    for (int i = 0; i < IM_ARRAYSIZE(best_depth_formats); i++)
    {
        if (!SDL_GPUTextureSupportsFormat(state::gpu_device, best_depth_formats[i], SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
            continue;
        dc_log("Best depth only texture format: SDL_GPU_TEXTUREFORMAT_%s", best_depth_formats_str[i]);
        state::gpu_tex_format_best_depth_only = best_depth_formats[i];
        break;
    }

    SDL_WaitForGPUFences(state::gpu_device, true, &fence, 1);
    SDL_ReleaseGPUFence(state::gpu_device, fence);
}

#include <SDL3/SDL_main.h>
int main(int argc, char* argv[])
{
    /* KDevelop fully buffers the output and will not display anything */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    char window_title[256];
    snprintf(window_title, SDL_arraysize(window_title), "mcs_b181_client (%s)-%s (%s)", build_info::ver_string::client().c_str(), build_info::build_mode,
        build_info::git::refspec);
    dc_log("%s", window_title);

    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "mcs_b181_client");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, build_info::ver_string::client().c_str());
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "io.github.icrashstuff.mcs_b181_client");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "Ian Hangartner (icrashstuff)");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Copyright (c) 2024-2025 Ian Hangartner (icrashstuff)");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/icrashstuff/mcs_b181");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");

    tetra::init("icrashstuff", "mcs_b181", "mcs_b181_client", argc, (const char**)argv, false);

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    if (!cvr_username.get().length())
    {
        cvr_username.set_default(std::string("Player") + std::to_string(SDL_rand_bits() % 65536));
        cvr_username.set(cvr_username.get_default());
    }

    if (state::on_ios && cvr_path_resource_pack.get() == cvr_path_resource_pack.get_default())
    {
        cvr_path_resource_pack.set(SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS) + cvr_path_resource_pack.get_default());
        cvr_path_resource_pack.set_default(cvr_path_resource_pack.get());
    }

    if (state::on_ios && cvr_dir_assets.get() == cvr_dir_assets.get_default())
    {
        cvr_dir_assets.set(SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS) + cvr_dir_assets.get_default());
        cvr_dir_assets.set_default(cvr_dir_assets.get());
    }

    if (!SDLNet_Init())
        util::die("SDLNet_Init: %s", SDL_GetError());

    if (tetra::init_gui(window_title, SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL) != 0)
        util::die("tetra::init_gui");

    setup_static_gpu_state();

    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_INVALID, "[SDL_LOG][UNKNOWN]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_TRACE, "[SDL_LOG][TRACE]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_VERBOSE, "[SDL_LOG][VERBOSE]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_DEBUG, "[SDL_LOG][DEBUG]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_INFO, "[SDL_LOG][INFO]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_WARN, "[SDL_LOG][WARN]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_ERROR, "[SDL_LOG][ERROR]: ");
    SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_CRITICAL, "[SDL_LOG][CRITICAL]: ");

    /* No reason to allow cvr_profile_light in non-dev environments */
    if (!convar_t::dev())
        cvr_profile_light.set(0);

    /* Prevent someone from unexpectedly killing the application */
    cvr_profile_light.set_pre_callback([](int, int) -> bool { return 0; });

    if (cvr_profile_light.get())
        SDL_HideWindow(state::window);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    dev_console::add_command("chat", [=](int argc, const char** argv) -> int {
        if (!game_selected)
        {
            dc_log_error("No game loaded");
            return 1;
        }

        if (!game_selected->connection)
        {
            dc_log_error("Game is not external");
            return 1;
        }

        packet_chat_message_t pack_msg;

        for (int i = 1; i < argc; i++)
        {
            pack_msg.msg += std::string(argv[i]);
            if (i != argc - 1)
                pack_msg.msg += " ";
        }

        dc_log("Sending message \"%s\"", pack_msg.msg.c_str());

        if (!game_selected->connection->send_packet(pack_msg))
        {
            dc_log("Failed to send message!");
            return 1;
        }

        return 0;
    });
    dev_console::add_command("game_external", [=](int argc, const char** argv) -> int {
        if (argc != 2 && argc != 3)
        {
            dc_log_error("Usage: %s address [port]", argv[0]);
            return 1;
        }

        int port = 25565;
        if (argc == 3 && !int_from_str(argv[2], port))
        {
            dc_log_error("Unable to parse \"%s\" as int", argv[2]);
            return 1;
        }

        if (port < 0 || port >= 0xFFFF)
        {
            dc_log_warn("Port must be in range [0,65535]");
            return 1;
        }

        dc_log("Creating external game, addr: \"%s\", port: %d", argv[1], port);

        game_t* new_game = new game_t(argv[1], port, cvr_username.get(), state::game_resources);
        games.push_back(new_game);
        game_selected_idx = new_game->game_id;

        return 0;
    });

    int done = 0;
    Uint64 last_loop_time = 0;
    delta_time = 0;
    while (!done)
    {
        SDL_Event event;
        bool should_cleanup = false;
        while (SDL_PollEvent(&event))
            process_event(event, &should_cleanup);
        if (should_cleanup)
            engine_state_target = ENGINE_STATE_EXIT;

        if (reload_resources)
        {
            deinitialize_resources();
            initialize_resources();
            reload_resources = 0;
        }

        tetra::configure_swapchain_if_needed();
        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state::gpu_device);
        SDL_GPUTexture* swapchain_texture = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state::window, &swapchain_texture, &win_size.x, &win_size.y))
            swapchain_texture = nullptr;

        if (!swapchain_texture)
        {
            tetra::limit_framerate();
            continue;
        }

        tetra::start_frame(false);
        Uint64 loop_start_time = SDL_GetTicksNS();
        delta_time = (double)last_loop_time / 1000000000.0;

        ImVec4 clear_color(0.1f, 0.1f, 0.1f, 1.0f);
        if (game_selected)
        {
            glm::vec3 col(0.0f);
            const glm::vec3 pos = game_selected->level->get_camera_pos();
            const float kernel[] = { 0.1f, 0.125f, 0.175f, 0.20f, 0.175f, 0.125f, 0.1f };
            const int offsets[IM_ARRAYSIZE(kernel)] = { -2, -1, 0, 1, 2 };
            for (int v = 0; v < IM_ARRAYSIZE(kernel) * IM_ARRAYSIZE(kernel) * IM_ARRAYSIZE(kernel); v++)
            {
                const int i = v % IM_ARRAYSIZE(kernel);
                const int j = (v / IM_ARRAYSIZE(kernel)) % IM_ARRAYSIZE(kernel);
                const int k = v / (IM_ARRAYSIZE(kernel) * IM_ARRAYSIZE(kernel));

                const glm::vec3 offset(offsets[i], offsets[j], offsets[k]);
                const float blur = kernel[i] * kernel[j] * kernel[k];

                col += mc_id::get_biome_color_sky(game_selected->level->get_biome_at(glm::round(pos + offset))) * blur;
            }

            clear_color = ImVec4(col.r, col.g, col.b, 1.0f);
        }

        engine_state_step();

        tetra::show_imgui_ctx_main(engine_state_current != ENGINE_STATE_RUNNING);
        tetra::show_imgui_ctx_overlay(!game_selected || !cvr_debug_screen.get());

        if (tetra::imgui_ctx_main_wants_input())
            world_has_input = 0;

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        switch (engine_state_current)
        {
        case ENGINE_STATE_OFFLINE:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Offline", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Hmm...\nYou should not be here");
            ImGui::End();
            break;
        }
        case ENGINE_STATE_CONFIGURE:
        {
            /* This is maybe a bit unnecessary as no normal person should ever be in this situation but, it doesn't hurt to have this here */
            if (cvr_profile_light.get())
                SDL_ShowWindow(state::window);

            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Configuration Required! (Resources for Minecraft Release 1.7.x/1.8.x (Not beta) are required)", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

            float width = viewport->WorkSize.x * 0.75f;

            float width_new = ImGui::CalcTextSize(cvr_username.get().c_str()).x;
            if (width < width_new)
                width = width_new;
            width_new = ImGui::CalcTextSize(cvr_dir_assets.get().c_str()).x;
            if (width < width_new)
                width = width_new;
            width_new = ImGui::CalcTextSize(cvr_path_resource_pack.get().c_str()).x;
            if (width < width_new)
                width = width_new;
            width_new = ImGui::CalcTextSize(cvr_dir_game.get().c_str()).x;
            if (width < width_new)
                width = width_new;

            if (width > viewport->WorkSize.x)
                width = viewport->WorkSize.x;

            width *= 0.6f;

            ImGui::SetNextItemWidth(width);
            cvr_username.imgui_edit();
            ImGui::SetNextItemWidth(width);
            cvr_dir_assets.imgui_edit();
            ImGui::SetNextItemWidth(width);
            cvr_path_resource_pack.imgui_edit();
            ImGui::SetNextItemWidth(width);
            cvr_dir_game.imgui_edit();

            if (ImGui::Button("ENGAGE!"))
            {
                convar_file_parser::write();
                engine_state_current = ENGINE_STATE_OFFLINE;
            }
            ImGui::End();
            break;
        }
        case ENGINE_STATE_INITIALIZE:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Initializing", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Initializing! (Hello)");
            ImGui::End();
            break;
        }
        case ENGINE_STATE_RUNNING:
        {
            if (cvr_profile_light.get())
            {
                profile_light();

                engine_state_target = ENGINE_STATE_EXIT;

                break;
            }

            gui_game_manager();
            loop_stage_ensure_game_selected();

            loop_stage_process_queued_input();

            bool render_world = 0;
            loop_stage_prerender(win_size, render_world);
            loop_stage_render(command_buffer, swapchain_texture, clear_color, win_size, render_world);

            if (game_selected && game_selected->level && cvr_gui_renderer.get())
            {
                level_t* level = game_selected->level;

                ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5, 0.5));
                ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Running", &cvr_gui_renderer);

                ImGui::Text("Foot: <%.1f, %.1f, %.1f>", level->foot_pos.x, level->foot_pos.y, level->foot_pos.z);

                ImGui::SliderFloat("Camera Pitch", &level->pitch, -89.95f, 89.95f);
                ImGui::SliderFloat("Camera Yaw", &level->yaw, 0.0f, 360.0f);
                ImGui::SliderFloat("Camera Offset Height", &level->camera_offset_height, -4.0f, 4.0f);
                ImGui::SliderFloat("Camera Offset Radius", &level->camera_offset_radius, -4.0f, 4.0f);
                ImGui::InputFloat("Foot X", &level->foot_pos.x, 1.0f);
                ImGui::InputFloat("Foot Y", &level->foot_pos.y, 1.0f);
                ImGui::InputFloat("Foot Z", &level->foot_pos.z, 1.0f);

                static bool cause_damage_tilt = 0;
                ImGui::Checkbox("Force damage tilt", &cause_damage_tilt);
                ImGui::SameLine();
                if (ImGui::Button("Cause damage tilt") || cause_damage_tilt)
                    level->damage_tilt = 1.0f;

                if (ImGui::Button("Rebuild resources"))
                {
                    deinitialize_resources();
                    initialize_resources();
                }

                ImGui::SameLine();
                if (ImGui::Button("Mark all meshes for relight"))
                {
                    for (chunk_cubic_t* c : level->get_chunk_vec())
                        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
                }

                if (ImGui::Button("Clear meshes"))
                    level->clear_mesh(false);

                ImGui::SameLine();
                if (ImGui::Button("Clear meshes & GL"))
                    level->clear_mesh(true);

                if (ImGui::Button("Rebuild shaders"))
                    compile_shaders();

                level->get_terrain()->imgui_view();

                ImGui::End();
            }

            if (state::game_resources && (game_selected || sound_engine_main_menu) && state::game_resources->sound_resources && cvr_gui_sound.get())
            {
                ImGui::SetNextWindowPos(viewport->Size * ImVec2(0.0075f, 0.1875f), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.0));
                ImGui::SetNextWindowSize(viewport->Size * ImVec2(0.425f, 0.8f), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Sound", &cvr_gui_sound);

                sound_world_t* sound_engine = nullptr;

                {
                    ImGui::BeginChild("Sound Slots", ImGui::GetContentRegionAvail() * ImVec2(0.5f, 1.0f), ImGuiChildFlags_Borders);

                    static int use_game_sound_engine = 1;
                    static const char* sound_engine_names[] = { "Main menu sound engine", "Current game sound engine", NULL };
                    ImGui::Combo("Sound engine selector", &use_game_sound_engine, sound_engine_names, 2);

                    if (!use_game_sound_engine)
                    {
                        sound_engine = sound_engine_main_menu;
                        ImGui::Text("music_counter: %.0f", music_counter);
                        ImGui::SameLine();
                        if (ImGui::Button("Clear music_counter"))
                            music_counter = 0.0;
                    }
                    else if (game_selected)
                    {
                        sound_engine = &game_selected->level->sound_engine;
                        ImGui::Text("music: %.1f%%", game_selected->level->music * 100.0f);
                        ImGui::SameLine();
                        if (ImGui::Button("music = 0.0"))
                            game_selected->level->music = 0.0;
                        ImGui::SameLine();
                        if (ImGui::Button("music = 1.0"))
                            game_selected->level->music = 1.0;
                    }
                    ImGui::Separator();

                    if (sound_engine)
                        sound_engine->imgui_contents();
                    else
                        ImGui::Text("Sound engine not connected");
                    ImGui::EndChild();
                }
                ImGui::SameLine();
                {
                    bool play_sound = 0;
                    sound_info_t sound_to_play;

                    ImGui::BeginChild("Sound List", ImGui::GetContentRegionAvail() * ImVec2(0.0f, 1.0f), ImGuiChildFlags_Borders);
                    state::game_resources->sound_resources->imgui_contents(play_sound, sound_to_play);
                    ImGui::EndChild();

                    if (play_sound && sound_engine)
                        sound_engine->request_source(sound_to_play, glm::f64vec3(0.0f), true);

                    ImGui::End();
                }
            }

            if (game_selected && game_selected->level && cvr_gui_lightmap.get())
            {
                level_t* level = game_selected->level;

                ImGui::SetNextWindowPos(viewport->Size * ImVec2(0.0075f, 0.1875f), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.0));
                ImGui::SetNextWindowSize(viewport->Size * ImVec2(0.425f, 0.8f), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Lightmap", &cvr_gui_lightmap);
                level->lightmap.imgui_contents();
                ImGui::End();
            }

            if (game_selected && game_selected->level && cvr_gui_inventory.get())
            {
                level_t* level = game_selected->level;

                ImGui::SetNextWindowPos(viewport->Size * ImVec2(0.0075f, 0.1875f), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.0));
                ImGui::SetNextWindowSize(viewport->Size * ImVec2(0.425f, 0.8f), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Inventory", &cvr_gui_inventory);

                level->inventory.imgui();

                ImGui::End();
            }

            if (game_selected)
                game_selected->level->sound_engine.update(glm::f64vec3(0, 0, 0), game_selected->level->camera_direction, glm::vec3(0, 1, 0));
            else
                sound_engine_main_menu->update(glm::f64vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

            break;
        }
        case ENGINE_STATE_SHUTDOWN:
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Shutdown", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Shutting down! (Goodbye)");
            ImGui::End();
            break;
        }
        case ENGINE_STATE_EXIT:
            done = 1;
            break;
        default:
            dc_log_fatal("engine_state_current set to %d (%s)", engine_state_current, engine_state_name(engine_state_current));
            done = 1;
            break;
        }

        connection_t::cull_dead_sockets(0);

        tetra::end_frame(command_buffer, swapchain_texture, engine_state_current != ENGINE_STATE_RUNNING);
        SDL_SubmitGPUCommandBuffer(command_buffer);
        screenshot_func(win_size);
        tetra::limit_framerate();
        last_loop_time = SDL_GetTicksNS() - loop_start_time;
    }

    for (game_t* g : games)
        delete g;

    SDL_ReleaseGPUTexture(state::gpu_device, state::gpu_debug_texture);
    SDL_ReleaseGPUSampler(state::gpu_device, state::gpu_debug_sampler);

    connection_t::cull_dead_sockets(100);

    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    SDLNet_Quit();
    tetra::deinit_gui();
    state::window = tetra::window;
    state::gpu_device = tetra::gpu_device;
    tetra::deinit();

    return 0;
}

void play_gui_button_sound()
{
    if (!(sound_engine_main_menu && state::game_resources && state::game_resources->sound_resources))
        return;

    sound_info_t sinfo;
    if (state::game_resources->sound_resources->get_sound("minecraft:gui.button.press", sinfo))
        sound_engine_main_menu->request_source(sinfo, glm::f64vec3(0.0f), true);
    sound_engine_main_menu->update(glm::f64vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
}

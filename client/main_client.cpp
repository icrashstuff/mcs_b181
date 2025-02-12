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

#include "tetra/tetra.h"
#include "tetra/tetra_gl.h"

#include <algorithm>

#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL3/SDL_opengl.h>

#include "tetra/gui/imgui-1.91.1/backends/imgui_impl_opengl3.h"
#include "tetra/gui/imgui-1.91.1/backends/imgui_impl_sdl3.h"
#include "tetra/gui/imgui.h"

#include "tetra/gui/console.h"
#include "tetra/gui/gui_registrar.h"

#include "tetra/gui/imgui.h"

#include "tetra/util/convar.h"
#include "tetra/util/convar_file.h"
#include "tetra/util/misc.h"
#include "tetra/util/physfs/physfs.h"
#include "tetra/util/stbi.h"

#include "shared/chunk.h"
#include "shared/ids.h"
#include "shared/misc.h"
#include "shared/packet.h"

#include "sdl_net/include/SDL3_net/SDL_net.h"

#include "connection.h"
#include "level.h"
#include "shaders.h"
#include "texture_terrain.h"

#include "gui/mc_gui.h"
#include "gui/panorama.h"

static convar_string_t cvr_username("username", "", "Username (duh)", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_assets("dir_assets", "", "Path to assets (ex: \"~/.minecraft/assets/\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_path_resource_pack(
    "path_base_resources", "", "File/Dir to use for base resources (ex: \"~/.minecraft/versions/1.6.4/1.6.4.jar\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_game("dir_game", "", "Path to store game files (Not mandatory)", CONVAR_FLAG_SAVE);

static convar_int_t cvr_autoconnect("dev_autoconnect", 0, 0, 1, "Auto connect to server", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static convar_string_t cvr_autoconnect_addr(
    "dev_server_addr", "localhost", "Address of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);
static convar_int_t cvr_autoconnect_port(
    "dev_server_port", 25565, 0, 65535, "Port of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);

static convar_float_t cvr_r_fov_base("r_fov_base", 75.0f, 30.0f, 120.0f, "Base FOV", CONVAR_FLAG_SAVE);

static convar_float_t cvr_r_crosshair_scale("r_crosshair_scale", 1.0f, 0.0f, 64.0f, "Multiplier for crosshair size", CONVAR_FLAG_SAVE);

static convar_int_t cvr_mc_gui_style_editor(
    "mc_gui_style_editor", 0, 0, 1, "Show style editor for the MC GUI system", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

static panorama_t* panorama = NULL;
static bool take_screenshot = 0;

static void compile_shaders() { shader_t::build_all(); }

static std::vector<game_t*> games;
static game_t* game_selected = nullptr;
static game_resources_t* game_resources = nullptr;

static ImGuiContext* imgui_ctx_main_menu = NULL;

static mc_gui::mc_gui_ctx mc_gui_global_ctx;
mc_gui::mc_gui_ctx* mc_gui::global_ctx = &mc_gui_global_ctx;

static bool initialize_resources()
{
    /* In the future parsing of one of the indexes at /assets/indexes/ will need to happen here (For sound) */
    game_resources = new game_resources_t();

    panorama = new panorama_t();

    mc_gui::global_ctx->load_resources();

    compile_shaders();

    for (game_t* g : games)
        if (g)
            g->reload_resources(game_resources);

    return true;
}

static bool deinitialize_resources()
{
    delete game_resources;
    delete panorama;

    mc_gui::global_ctx->unload_resources();

    for (game_t* g : games)
        if (g)
            g->reload_resources(nullptr, true);

    panorama = NULL;
    game_resources = 0;
    return true;
}

/**
 * Quick check to see if the game can be launched, intended for validating if the setup screen can be skipped
 */
static bool can_launch_game()
{
    if (!cvr_username.get().length())
        return 0;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(cvr_dir_assets.get().c_str(), &info))
        return 0;

    if (info.type != SDL_PATHTYPE_DIRECTORY)
        return 0;

    SDL_GetPathInfo(cvr_dir_game.get().c_str(), &info);
    if (info.type != SDL_PATHTYPE_DIRECTORY && info.type != SDL_PATHTYPE_NONE)
        return 0;

    if (!SDL_GetPathInfo(cvr_path_resource_pack.get().c_str(), &info))
        return 0;

    return 1;
}

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
static bool mouse_grabbed = 0;
static bool wireframe = 0;
#define AO_ALGO_MAX 5

#include "main_client.menu.cpp"

static void normal_loop()
{
    bool warp_mouse_to_center = 0;
    if (SDL_GetWindowMouseGrab(tetra::window) != mouse_grabbed)
    {
        warp_mouse_to_center = 1;
        SDL_SetWindowMouseGrab(tetra::window, mouse_grabbed);
    }

    if (SDL_GetWindowRelativeMouseMode(tetra::window) != mouse_grabbed)
    {
        warp_mouse_to_center = 1;
        SDL_SetWindowRelativeMouseMode(tetra::window, mouse_grabbed);
    }

    if (warp_mouse_to_center)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
        SDL_WarpMouseInWindow(tetra::window, center.x, center.y);
    }

    ImGui::SetNextWindowSize(ImVec2(580, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20.0f, ImGui::GetMainViewport()->GetWorkCenter().y), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.5));
    ImGui::Begin("Render Selector");
    static bool show_level = true;
    ImGui::Checkbox("Forcibly Show Level", &show_level);

    if (ImGui::Button("Rebuild resources"))
    {
        deinitialize_resources();
        initialize_resources();
    }

    static std::string new_username = cvr_username.get();
    static std::string new_addr = cvr_autoconnect_addr.get();
    static Uint16 new_port = cvr_autoconnect_port.get();
    const Uint16 port_step = 1;

    ImGui::InputText("Username", &new_username);
    ImGui::InputText("Address", &new_addr);
    ImGui::InputScalar("Port", ImGuiDataType_U16, &new_port, &port_step);

    int do_init_game = 0;

    if (ImGui::Button("Init Game (Server)"))
        do_init_game = 1;
    ImGui::SameLine();
    if (ImGui::Button("Init Game (Test World)"))
        do_init_game = 2;

    if (do_init_game)
        games.push_back(new game_t(new_addr, new_port, new_username, do_init_game == 2, game_resources));

    static int game_selected_idx = 0;

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

    game_selected = nullptr;

    for (game_t* g : games)
        if (g->game_id == game_selected_idx)
            game_selected = g;

    if (!game_selected && games.size())
    {
        game_selected = games[0];
        game_selected_idx = game_selected->game_id;
    }

    ImGui::End();

    game_t* game = game_selected;

    if (!show_level || !game_selected)
        mouse_grabbed = 0;

    if ((ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) && tetra::imgui_ctx_main_wants_input())
        mouse_grabbed = 0;

    if (!game || client_menu_manager.stack_size())
        mouse_grabbed = 0;

    if (game && !client_menu_manager.stack_size() && (!game->connection || game->connection->get_status() == connection_t::CONNECTION_ACTIVE))
        mouse_grabbed = 1;

    if (!mouse_grabbed)
    {
        held_w = 0;
        held_a = 0;
        held_s = 0;
        held_d = 0;
        held_space = 0;
        held_shift = 0;
        held_ctrl = 0;
    }

    float camera_speed = 3.5f * delta_time * (held_ctrl ? 4.0f : 1.0f);

    glm::ivec2 win_size;
    SDL_GetWindowSize(tetra::window, &win_size.x, &win_size.y);

    for (game_t* g : games)
        if (g->connection && g->level)
            g->connection->run(g->level);

    if (game)
    {
        level_t* level = game->level;

        if (held_w)
            level->camera_pos += camera_speed * glm::vec3(SDL_cosf(glm::radians(level->yaw)), 0, SDL_sinf(glm::radians(level->yaw)));
        if (held_s)
            level->camera_pos -= camera_speed * glm::vec3(SDL_cosf(glm::radians(level->yaw)), 0, SDL_sinf(glm::radians(level->yaw)));
        if (held_a)
            level->camera_pos -= camera_speed * glm::vec3(-SDL_sinf(glm::radians(level->yaw)), 0, SDL_cosf(glm::radians(level->yaw)));
        if (held_d)
            level->camera_pos += camera_speed * glm::vec3(-SDL_sinf(glm::radians(level->yaw)), 0, SDL_cosf(glm::radians(level->yaw)));
        if (held_space)
            level->camera_pos.y += camera_speed;
        if (held_shift)
            level->camera_pos.y -= camera_speed;

        if (held_ctrl)
            level->fov += delta_time * 30.0f;
        else
            level->fov -= delta_time * 30.0f;

        float fov_base = cvr_r_fov_base.get();

        if (level->fov > fov_base + 2.0f)
            level->fov = fov_base + 2.0f;
        else if (level->fov < fov_base)
            level->fov = fov_base;
    }

    ImGuiContext* last_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    int& menu_scale = mc_gui::global_ctx->menu_scale;
    {
        const glm::ivec2 scales = win_size / menu_scale_step;
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

    client_menu_manager.set_default(game_selected ? "nomenu" : "menu.title");
    client_menu_return_t menu_ret = client_menu_manager.run_last_in_stack(win_size);

    /* client_menu_manager.run_last_in_stack() may delete the game */
    game = game_selected;
    bool in_world = game;
    if (in_world && game->connection)
        in_world = game->connection->get_status() == connection_t::CONNECTION_ACTIVE;

    if (menu_ret.allow_world && in_world)
    {
        game->level->lightmap.update();
        game->level->get_terrain()->update();
        game->level->render(win_size);

        /* Render crosshair
         * NOTE: If a function called by client_menu_manager.run_last_in_stack() adds to
         * the background draw list then the crosshair will be drawn on top!
         */
        const ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
        const float scale = cvr_r_crosshair_scale.get() * mc_gui::global_ctx->menu_scale;
        const ImVec2 pos0(center - ImVec2(8.0f, 8.0f) * scale);
        const ImVec2 pos1(center + ImVec2(8.0f, 8.0f) * scale);
        const ImVec2 uv0(240.0f / 256.0f, 0.0f);
        const ImVec2 uv1(1.0f, 16.0f / 256.0f);
        ImGui::GetBackgroundDrawList()->AddImage(reinterpret_cast<ImTextureID>(mc_gui::global_ctx->tex_id_widgets), pos0, pos1, uv0, uv1);

        if (client_menu_manager.stack_size())
            ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ImGui::GetMainViewport()->Size, IM_COL32(32, 32, 32, 255 * 0.5f));
    }

    if (menu_ret.allow_pano && !in_world)
        panorama->render(win_size);

    if (menu_ret.allow_dirt && ((!menu_ret.allow_pano && !in_world) || (in_world && !menu_ret.allow_world)))
    {
        ImTextureID tex_id = reinterpret_cast<ImTextureID>(mc_gui::global_ctx->tex_id_bg);
        ImVec2 size = ImGui::GetMainViewport()->Size;
        ImGui::GetBackgroundDrawList()->AddImage(tex_id, ImVec2(0, 0), size, ImVec2(0, 0), size / (32.0f * float(SDL_max(1, mc_gui::global_ctx->menu_scale))));
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 0, 0, 255 * 0.75f));
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::SetCurrentContext(last_ctx);
}

static void process_event(SDL_Event& event, bool* done)
{
    if (tetra::process_event(event))
        *done = true;

    if (event.type == SDL_EVENT_QUIT)
        *done = true;

    if (event.window.windowID != SDL_GetWindowID(tetra::window))
        return;

    switch (event.window.type)
    {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        *done = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        mouse_grabbed = false;
        break;
    default:
        break;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F2 && !event.key.repeat)
        take_screenshot = 1;

    if (tetra::imgui_ctx_main_wants_input())
        return;

    game_t* game = game_selected;

    if (!game)
    {
        ImGuiContext* last_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx_main_menu);
        ImGui_ImplSDL3_ProcessEvent(&event);
        ImGui::SetCurrentContext(last_ctx);
        return;
    }

    level_t* level = game->level;
    connection_t* connection = game->connection;

    /* TODO: Check for in-world */
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE
        && (!connection || connection->get_status() == connection_t::CONNECTION_ACTIVE))
    {
        if (client_menu_manager.stack_size())
            client_menu_manager.stack_clear();
        else
            client_menu_manager.stack_push("menu.game");

        mouse_grabbed = !client_menu_manager.stack_size();

        dc_log("%d %d", client_menu_manager.stack_size(), mouse_grabbed);
    }

    if (!mouse_grabbed && client_menu_manager.stack_size())
    {
        ImGuiContext* last_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx_main_menu);
        ImGui_ImplSDL3_ProcessEvent(&event);
        ImGui::SetCurrentContext(last_ctx);
        return;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (!mouse_grabbed && !client_menu_manager.stack_size())
            mouse_grabbed = 1;
        else if (mouse_grabbed)
        {
            // TODO: Add place block function
            glm::vec3 cam_dir;
            cam_dir.x = SDL_cosf(glm::radians(level->yaw)) * SDL_cosf(glm::radians(level->pitch));
            cam_dir.y = SDL_sinf(glm::radians(level->pitch));
            cam_dir.z = SDL_sinf(glm::radians(level->yaw)) * SDL_cosf(glm::radians(level->pitch));
            glm::ivec3 cam_pos = level->camera_pos + glm::normalize(cam_dir) * 2.5f;

            if (event.button.button == 1)
            {
                connection_t::tentative_block_t t;
                t.timestamp = SDL_GetTicks();
                t.pos = cam_pos;
                if (level->get_block(t.pos, t.old) && t.old.id != BLOCK_ID_AIR)
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
            else if (event.button.button == 2)
            {
                // TODO: Pick block
            }
            else if (event.button.button == 3)
            {
                connection_t::tentative_block_t t;
                t.timestamp = SDL_GetTicks();
                t.pos = cam_pos;
                itemstack_t hand = level->inventory.items[level->inventory.hotbar_sel];

                if (level->get_block(t.pos, t.old) && t.old != hand)
                {
                    if (mc_id::is_block(hand.id))
                    {
                        if (connection)
                            connection->push_tentative_block(t);
                        level->set_block(t.pos, hand);
                    }

                    packet_player_place_t p;
                    p.x = t.pos.x;
                    p.y = t.pos.y - 1;
                    p.z = t.pos.z;
                    p.direction = 1;
                    p.block_item_id = hand.id;
                    p.amount = 0;
                    p.damage = hand.damage;
                    if (connection)
                        connection->send_packet(p);
                }
            }
        }
    }

    if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        if (SDL_fabsf(event.wheel.y) >= 0.99f)
        {
            packet_hold_change_t pack;
            int num_slots = level->inventory.hotbar_max - level->inventory.hotbar_min + 1;
            pack.slot_id = (level->inventory.hotbar_sel + int(event.wheel.y) - level->inventory.hotbar_min) % num_slots;
            if (pack.slot_id < 0)
                pack.slot_id += num_slots;
            level->inventory.hotbar_sel = level->inventory.hotbar_min + pack.slot_id;
            if (connection)
                connection->send_packet(pack);
        }
    }

    if (mouse_grabbed && event.type == SDL_EVENT_MOUSE_MOTION)
    {
        const float sensitivity = 0.1f;

        if (event.motion.yrel != 0.0f)
        {
            level->pitch -= event.motion.yrel * sensitivity;
            if (level->pitch > 89.0f)
                level->pitch = 89.0f;
            if (level->pitch < -89.0f)
                level->pitch = -89.0f;
        }

        if (event.motion.xrel != 0.0f)
            level->yaw = SDL_fmodf(level->yaw + event.motion.xrel * sensitivity, 360.0f);
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
        case SDL_SCANCODE_B:
        {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            break;
        }
        case SDL_SCANCODE_P:
        {
            glm::ivec3 chunk_coords = glm::ivec3(level->camera_pos) >> 4;
            for (chunk_cubic_t* c : level->get_chunk_vec())
            {
                if (chunk_coords != c->pos)
                    continue;
                c->free_gl();
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
            }
            break;
        }
        case SDL_SCANCODE_N:
        {
            for (chunk_cubic_t* c : level->get_chunk_vec())
            {
                glm::ivec3 chunk_coords = glm::ivec3(level->camera_pos) >> 4;
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
            glm::ivec3 chunk_coords = glm::ivec3(level->camera_pos) >> 4;
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
            game_resources->ao_algorithm = (game_resources->ao_algorithm + 1) % (AO_ALGO_MAX + 1);
            dc_log("Setting ao_algorithm to %d", game_resources->ao_algorithm);
            game_resources->terrain_shader->set_uniform("ao_algorithm", game_resources->ao_algorithm);
            break;
        }
        case SDL_SCANCODE_X:
        {
            game_resources->use_texture = !game_resources->use_texture;
            dc_log("Setting use_texture to %d", game_resources->use_texture);
            game_resources->terrain_shader->set_uniform("use_texture", game_resources->use_texture);
            break;
        }
        case SDL_SCANCODE_R:
        {
            compile_shaders();
            break;
        }
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_GRAVE:
            mouse_grabbed = false;
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
        default:
            break;
        }
}

/* This isn't the best way to do this, but it will do for now */
static bool render_water_overlay()
{
    if (!game_selected || !game_selected->level)
        return false;

    glm::ivec3 cam_pos = game_selected->level->camera_pos;
    glm::ivec3 chunk_coords = cam_pos >> 4;

    bool found_water = false;
    for (chunk_cubic_t* c : game_selected->level->get_chunk_vec())
    {
        if (!c || chunk_coords != c->pos)
            continue;

        block_id_t type = c->get_type(cam_pos.x & 0x0F, cam_pos.y & 0x0F, cam_pos.z & 0x0F);
        found_water = (type == BLOCK_ID_WATER_FLOWING) || (type == BLOCK_ID_WATER_SOURCE);
    }
    if (!found_water)
        return false;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Water", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground))
    {
        mc_id::terrain_face_t face = game_selected->level->get_terrain()->get_face(mc_id::FACE_WATER_STILL);
        ImVec2 uv0(face.corners[0].x, face.corners[0].y);
        ImVec2 uv1(face.corners[3].x, face.corners[3].y);
        ImGui::Image((void*)(size_t)game_selected->level->get_terrain()->tex_id_main, ImGui::GetMainViewport()->WorkSize, uv0, uv1);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    return true;
}

static gui_register_overlay reg_render_water_overlay(render_water_overlay);

static bool render_status_msg()
{
    if (!game_selected || !game_selected->connection)
        return false;

    if (!game_selected->connection->status_msg.length())
        return false;

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->GetWorkCenter().x, 0), ImGuiCond_Always, ImVec2(0.5f, 0));

    ImVec2 size0 = ImGui::CalcTextSize(game_selected->connection->status_msg.c_str());
    ImVec2 size1 = ImGui::CalcTextSize(game_selected->connection->status_msg_sub.c_str());

    if (game_selected->connection->status_msg_sub.length())
        size1.y += ImGui::GetStyle().ItemSpacing.y * 2.0f;

    ImVec2 win_size = ImVec2(SDL_max(size0.x, size1.x) + 10.0f, size0.y + size1.y) + ImGui::GetStyle().WindowPadding * 1.05f;

    ImGui::SetNextWindowSize(win_size, ImGuiCond_Always);

    if (ImGui::Begin("Status MSG", NULL, ImGuiWindowFlags_NoDecoration))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(0, 0));
        ImGui::SeparatorText(game_selected->connection->status_msg.c_str());
        if (game_selected->connection->status_msg_sub.length())
            ImGui::SeparatorText(game_selected->connection->status_msg_sub.c_str());
        ImGui::PopStyleVar(3);
    }

    ImGui::End();

    return true;
}

static gui_register_overlay reg_render_status_msg(render_status_msg);

static convar_int_t cvr_gui_renderer("gui_renderer", 0, 0, 1, "Show renderer internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_lightmap("gui_lightmap", 0, 0, 1, "Show lightmap internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cvr_gui_panorama("gui_panorama", 0, 0, 1, "Show panorama internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
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

        mc_gui::init();

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

        mc_gui::deinit();

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

static int win_width, win_height;
static void stbi_physfs_write_func(void* context, void* data, int size) { PHYSFS_writeBytes((PHYSFS_File*)context, data, size); }
static void screenshot_callback()
{
    if (!take_screenshot)
        return;
    take_screenshot = 0;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    Uint8* buf = (Uint8*)malloc(win_width * win_height * 3);
    glReadPixels(0, 0, win_width, win_height, GL_RGB, GL_UNSIGNED_BYTE, buf);

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

    int result = stbi_write_png_to_func(stbi_physfs_write_func, fd, win_width, win_height, 3, buf, win_width * 3);

    if (result)
        dc_log("Saved screenshot to %s", buf_path);
    else
        dc_log("Error saving screenshot: stbi_write_png_to_func() returned %d", result);

    free(buf);
}

int main(const int argc, const char** argv)
{
    tetra::init("icrashstuff", "mcs_b181", "mcs_b181_client", argc, argv);

    if (!cvr_username.get().length())
    {
        cvr_username.set_default(std::string("Player") + std::to_string(SDL_rand_bits() % 65536));
        cvr_username.set(cvr_username.get_default());
    }

    if (!SDLNet_Init())
        util::die("SDLNet_Init: %s", SDL_GetError());

    tetra::set_render_api(tetra::RENDER_API_GL_CORE, 3, 3);

    tetra::init_gui("mcs_b181_client");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
        tetra::start_frame(false);
        Uint64 loop_start_time = SDL_GetTicksNS();
        delta_time = (double)last_loop_time / 1000000000.0;
        SDL_GetWindowSize(tetra::window, &win_width, &win_height);
        glViewport(0, 0, win_width, win_height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        engine_state_step();

        tetra::show_imgui_ctx_main(engine_state_current != ENGINE_STATE_RUNNING);

        if (tetra::imgui_ctx_main_wants_input())
            mouse_grabbed = 0;

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
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
            ImGui::Begin("Configuration Required!", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

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
            if (game_selected && game_selected->level && cvr_gui_renderer.get())
            {
                level_t* level = game_selected->level;

                ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5, 0.5));
                ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Running", &cvr_gui_renderer);

                ImGui::Text("Camera: <%.1f, %.1f, %.1f>", level->camera_pos.x, level->camera_pos.y, level->camera_pos.z);

                ImGui::SliderFloat("Camera Pitch", &level->pitch, -89.0f, 89.0f);
                ImGui::SliderFloat("Camera Yaw", &level->yaw, 0.0f, 360.0f);
                ImGui::InputFloat("Camera X", &level->camera_pos.x, 1.0f);
                ImGui::InputFloat("Camera Y", &level->camera_pos.y, 1.0f);
                ImGui::InputFloat("Camera Z", &level->camera_pos.z, 1.0f);

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

            if (panorama && cvr_gui_panorama.get())
            {
                ImGui::SetNextWindowPos(viewport->Size * ImVec2(0.0075f, 0.1875f), ImGuiCond_FirstUseEver, ImVec2(0.0, 0.0));
                ImGui::SetNextWindowSize(viewport->Size * ImVec2(0.425f, 0.8f), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Panorama", &cvr_gui_panorama);
                panorama->imgui_widgets();
                ImGui::End();
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

            normal_loop();

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

        tetra::end_frame(0, screenshot_callback);
        last_loop_time = SDL_GetTicksNS() - loop_start_time;
    }

    for (game_t* g : games)
        delete g;

    tetra::deinit();
    SDLNet_Quit();
    SDL_Quit();
}

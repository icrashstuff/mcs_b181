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
#include "tetra/tetra.h"

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
#include "shared/packet.h"

#include "shaders.h"
#include "texture_terrain.h"

#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL3/SDL_opengl.h>

static convar_string_t cvr_username("username", "", "Username (duh)", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_assets("dir_assets", "", "Path to assets (ex: \"~/.minecraft/assets/\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_path_resource_pack(
    "path_base_resources", "", "File/Dir to use for base resources (ex: \"~/.minecraft/versions/1.6.4/1.6.4.jar\")", CONVAR_FLAG_SAVE);
static convar_string_t cvr_dir_game("dir_game", "", "Path to store game files (Not mandatory)", CONVAR_FLAG_SAVE);

static convar_int_t cvr_autoconnect("dev_autoconnect", 0, 0, 1, "Auto connect to server", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);
static convar_string_t cvr_autoconnect_addr(
    "dev_server_addr", "localhost:25565", "Address of server to autoconnect to when dev_autoconnect is specified", CONVAR_FLAG_DEV_ONLY);

texture_terrain_t* terrain = NULL;
shader_t* shader = NULL;
chunk_t* chunk = NULL;
std::vector<terrain_vertex_t> vtx;
std::vector<Uint32> ind;

struct chunk_gl_t
{
    bool empty = 0;
    bool dirty = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    /**
     * One of GL_UNSIGNED_INT, GL_UNSIGNED_SHORT, or GL_UNSIGNED_BYTE
     */
    GLenum index_type = 0;
    size_t index_count = 0;

    Uint32 world_x = 0;
    Uint32 world_y = 0;
    Uint32 world_z = 0;

    chunk_t* chunk;
};

struct chunk_cache_t
{
    std::vector<chunk_gl_t> chunks;

    void build_chunk() { }
};

int ao_algorithm = 1;
int use_texture = 1;

void compile_shaders()
{
    Uint64 tick_shader_start = SDL_GetTicksNS();
    shader->build();
    shader->set_uniform("ao_algorithm", ao_algorithm);
    shader->set_uniform("use_texture", use_texture);
    dc_log("Compiled shaders in %.2f ms", (SDL_GetTicksNS() - tick_shader_start) / 1000000.0);
}

GLuint vao, vbo, ebo;
bool initialize_resources()
{
    /* In the future parsing of one of the indexes at /assets/indexes/ will need to happen here (For sound) */

    terrain = new texture_terrain_t("/_resources/assets/minecraft/textures/");
    shader = new shader_t("/shaders/terrain.vert", "/shaders/terrain.frag");

    chunk = new chunk_t();

    compile_shaders();

    chunk->generate_from_seed_over(0, -1, 0);
    for (int x = 3; x < 13; x++)
        for (int z = 3; z < 13; z++)
            for (int y = 55; y < 67; y++)
            {
                chunk->set_type(x, y, z, BLOCK_ID_ORE_COAL);
                chunk->set_light_sky(x, y, z, SDL_min((SDL_fabs(x - 7.5) + SDL_fabs(z - 7.5)) * 2.2, 15));
            }

    for (int x = 4; x < 12; x++)
        for (int z = 4; z < 12; z++)
            for (int y = 56; y < 66; y++)
                chunk->set_type(x, y, z, 0);

    for (int dat_it = 0; dat_it < CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z; dat_it++)
    {
        const int y = (dat_it) & 0x7F;
        const int z = (dat_it >> 7) & 0x0F;
        const int x = (dat_it >> 11) & 0x0F;

        block_id_t type = chunk->get_type(x, y, z);
        if (type == BLOCK_ID_AIR)
            continue;

        float r = float((type) & 3) / 3.0f, g = float((type >> 2) & 3) / 3.0f, b = float((type >> 4) & 3) / 3.0f;
        r = 1.0, g = 1.0, b = 1.0;

        Uint8 light_block = chunk->get_light_block(x, y, z);
        Uint8 light_sky = chunk->get_light_sky(x, y, z);

        /** Index: [x+1][y+1][z+1] */
        block_id_t stypes[3][3][3];
        for (int i = -1; i < 2; i++)
            for (int j = -1; j < 2; j++)
                for (int k = -1; k < 2; k++)
                    stypes[i + 1][j + 1][k + 1] = chunk->get_type_fallback(x + i, y + j, z + k, BLOCK_ID_AIR);

        mc_id::terrain_face_t f;

        if (type == BLOCK_ID_STONE)
            f = terrain->get_face(mc_id::FACE_STONE);
        else if (type == BLOCK_ID_SAND)
            f = terrain->get_face(mc_id::FACE_SAND);
        else if (type == BLOCK_ID_SANDSTONE)
            f = terrain->get_face(mc_id::FACE_SANDSTONE_NORMAL);
        else if (type == BLOCK_ID_DIRT)
            f = terrain->get_face(mc_id::FACE_DIRT);
        else if (type == BLOCK_ID_GRAVEL)
            f = terrain->get_face(mc_id::FACE_GRAVEL);
        else if (type == BLOCK_ID_ORE_COAL)
            f = terrain->get_face(mc_id::FACE_COAL_ORE);
        else if (type == BLOCK_ID_ORE_IRON)
            f = terrain->get_face(mc_id::FACE_IRON_ORE);
        else if (type == BLOCK_ID_ORE_GOLD)
            f = terrain->get_face(mc_id::FACE_GOLD_ORE);
        else if (type == BLOCK_ID_ORE_LAPIS)
            f = terrain->get_face(mc_id::FACE_LAPIS_ORE);
        else if (type == BLOCK_ID_ORE_REDSTONE_ON || type == BLOCK_ID_ORE_REDSTONE_OFF)
            f = terrain->get_face(mc_id::FACE_REDSTONE_ORE);
        else if (type == BLOCK_ID_ORE_DIAMOND)
            f = terrain->get_face(mc_id::FACE_DIAMOND_ORE);
        else if (type == BLOCK_ID_BRICKS_STONE)
            f = terrain->get_face(mc_id::FACE_STONEBRICK);
        else if (type == BLOCK_ID_LAVA_SOURCE)
            f = terrain->get_face(mc_id::FACE_LAVA_STILL);
        else
            f = terrain->get_face(mc_id::FACE_DEBUG);

        /* Positive Y */
        if (mc_id::is_transparent(stypes[1][2][1]))
        {
            Uint8 ao[] = {
                Uint8((stypes[0][2][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][2] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][2] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[2] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[3] });
        }

        /* Negative Y */
        if (mc_id::is_transparent(stypes[1][0][1]))
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][0][2] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][2] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[2] });
        }

        /* Positive X */
        if (mc_id::is_transparent(stypes[2][1][1]))
        {
            Uint8 ao[] = {
                Uint8((stypes[2][0][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][2] != BLOCK_ID_AIR) + (stypes[2][0][1] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][2] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR) + (stypes[2][2][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[2] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[0] });
        }

        /* Negative X */
        if (mc_id::is_transparent(stypes[0][1][1]))
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR)),
                Uint8((stypes[0][0][2] != BLOCK_ID_AIR) + (stypes[0][0][1] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][2] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR) + (stypes[0][2][1] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[2] });
        }

        /* Positive Z */
        if (mc_id::is_transparent(stypes[1][1][2]))
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][2] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][2] != BLOCK_ID_AIR) + (stypes[0][1][2] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][2] != BLOCK_ID_AIR) + (stypes[1][0][2] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][2] != BLOCK_ID_AIR) + (stypes[1][2][2] != BLOCK_ID_AIR) + (stypes[2][1][2] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 1), Uint16(z + 1), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 1), Uint16(z + 1), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[0] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 0), Uint16(z + 1), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 0), Uint16(z + 1), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[2] });
        }

        /* Negative Z */
        if (mc_id::is_transparent(stypes[1][1][0]))
        {
            Uint8 ao[] = {
                Uint8((stypes[0][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR)),
                Uint8((stypes[0][2][0] != BLOCK_ID_AIR) + (stypes[0][1][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR)),
                Uint8((stypes[2][0][0] != BLOCK_ID_AIR) + (stypes[1][0][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR)),
                Uint8((stypes[2][2][0] != BLOCK_ID_AIR) + (stypes[1][2][0] != BLOCK_ID_AIR) + (stypes[2][1][0] != BLOCK_ID_AIR)),
            };

            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 0), Uint16(z + 0), ao[0] }, { r, g, b, light_block, light_sky }, f.corners[3] });
            vtx.push_back({ { 1, Uint16(x + 0), Uint16(y + 1), Uint16(z + 0), ao[1] }, { r, g, b, light_block, light_sky }, f.corners[1] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 0), Uint16(z + 0), ao[2] }, { r, g, b, light_block, light_sky }, f.corners[2] });
            vtx.push_back({ { 1, Uint16(x + 1), Uint16(y + 1), Uint16(z + 0), ao[3] }, { r, g, b, light_block, light_sky }, f.corners[0] });
        }
    }

    dc_log("Vertices: %zu", vtx.size());
    terrain_vertex_t::create_vao(&vao);
    terrain_vertex_t::create_vbo(&vbo, &ebo, vtx);

    return true;
}

bool deinitialize_resources()
{
    delete terrain;
    delete shader;
    delete chunk;
    return true;
}

/**
 * Quick check to see if the game can be launched, intended for validating if the setup screen can be skipped
 */
bool can_launch_game()
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

static engine_state_t engine_state_current = ENGINE_STATE_OFFLINE;
static engine_state_t engine_state_target = ENGINE_STATE_RUNNING;

bool engine_state_step()
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

static convar_int_t cvr_gui_engine_state("gui_engine_state", 0, 0, 1, "Show engine state menu", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

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

float yaw;
float pitch;

float delta_time;

bool held_w = 0;
bool held_a = 0;
bool held_s = 0;
bool held_d = 0;
bool held_space = 0;
bool held_shift = 0;
bool held_ctrl = 0;
bool mouse_grabbed = 0;
bool wireframe = 0;
#define AO_ALGO_MAX 5

#define FOV_MAX 77.0f
#define FOV_MIN 75.0f
float fov = FOV_MIN;

glm::vec3 camera_pos = glm::vec3(0.0f, 1.5f, 0.0f);
glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);

void normal_loop()
{
    terrain->update();

    if (SDL_GetWindowMouseGrab(tetra::window) != mouse_grabbed)
        SDL_SetWindowMouseGrab(tetra::window, mouse_grabbed);

    if (SDL_GetWindowRelativeMouseMode(tetra::window) != mouse_grabbed)
        SDL_SetWindowRelativeMouseMode(tetra::window, mouse_grabbed);

    if ((ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) && tetra::imgui_ctx_main_wants_input())
        mouse_grabbed = 0;

    if (!mouse_grabbed)
    {
        held_w = 0;
        held_a = 0;
        held_s = 0;
        held_d = 0;
    }

    float camera_speed = 3.5f * delta_time * (held_ctrl ? 4.0f : 1.0f);

    if (held_w)
        camera_pos += camera_speed * glm::vec3(SDL_cosf(glm::radians(yaw)), 0, SDL_sinf(glm::radians(yaw)));
    if (held_s)
        camera_pos -= camera_speed * glm::vec3(SDL_cosf(glm::radians(yaw)), 0, SDL_sinf(glm::radians(yaw)));
    if (held_a)
        camera_pos -= camera_speed * glm::vec3(-SDL_sinf(glm::radians(yaw)), 0, SDL_cosf(glm::radians(yaw)));
    if (held_d)
        camera_pos += camera_speed * glm::vec3(-SDL_sinf(glm::radians(yaw)), 0, SDL_cosf(glm::radians(yaw)));
    if (held_space)
        camera_pos.y += camera_speed;
    if (held_shift)
        camera_pos.y -= camera_speed;

    if (held_ctrl)
        fov += delta_time * 30.0f;
    else
        fov -= delta_time * 30.0f;

    if (fov > FOV_MAX)
        fov = FOV_MAX;
    else if (fov < FOV_MIN)
        fov = FOV_MIN;

    glUseProgram(shader->id);

    int win_width, win_height;
    SDL_GetWindowSize(tetra::window, &win_width, &win_height);
    shader->set_projection(glm::perspective(glm::radians(fov), (float)win_width / (float)win_height, 0.03125f, 256.0f));

    glm::vec3 direction;
    direction.x = SDL_cosf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
    direction.y = SDL_sinf(glm::radians(pitch));
    direction.z = SDL_sinf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
    camera_front = glm::normalize(direction);

    shader->set_camera(glm::lookAt(camera_pos, camera_pos + camera_front, camera_up));

    shader->set_model(glm::mat4(1.0f));

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, vtx.size() / 4 * 6, GL_UNSIGNED_INT, 0);
}

void process_event(SDL_Event& event, bool* done)
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

    if (tetra::imgui_ctx_main_wants_input())
        return;

    static Uint64 last_button_down = -1;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        last_button_down = event.common.timestamp;

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        if (event.common.timestamp > last_button_down && event.common.timestamp - last_button_down < (250 * 1000 * 1000))
            mouse_grabbed = true;

    if (mouse_grabbed && event.type == SDL_EVENT_MOUSE_MOTION)
    {
        const float sensitivity = 0.1f;

        if (event.motion.yrel != 0.0f)
        {
            pitch -= event.motion.yrel * sensitivity;
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }

        if (event.motion.xrel != 0.0f)
            yaw = SDL_fmodf(yaw + event.motion.xrel * sensitivity, 360.0f);
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
        case SDL_SCANCODE_C:
        {
            ao_algorithm = (ao_algorithm + 1) % (AO_ALGO_MAX + 1);
            dc_log("Setting ao_algorithm to %d", ao_algorithm);
            shader->set_uniform("ao_algorithm", ao_algorithm);
            break;
        }
        case SDL_SCANCODE_X:
        {
            use_texture = !use_texture;
            dc_log("Setting use_texture to %d", use_texture);
            shader->set_uniform("use_texture", use_texture);
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

static convar_int_t cvr_gui_renderer("gui_renderer", 0, 0, 1, "Show renderer internals window", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

int main(const int argc, const char** argv)
{
    tetra::init("icrashstuff", "mcs_b181", "mcs_b181_client", argc, argv);

    if (!cvr_username.get().length())
    {
        cvr_username.set_default(std::string("Player") + std::to_string(SDL_rand_bits() % 65536));
        cvr_username.set(cvr_username.get_default());
    }

    tetra::set_render_api(tetra::RENDER_API_GL_CORE, 3, 3);

    tetra::init_gui("mcs_b181_client");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

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
        int win_width, win_height;
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
            if (cvr_gui_renderer.get())
            {
                ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5, 0.5));
                ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
                ImGui::BeginCVR("Running", &cvr_gui_renderer);

                ImGui::Text("Camera: <%.1f, %.1f, %.1f>", camera_pos.x, camera_pos.y, camera_pos.z);

                if (ImGui::Button("Rebuild atlas"))
                {
                    delete terrain;
                    terrain = new texture_terrain_t("/_resources/assets/minecraft/textures/");
                }
                if (ImGui::Button("Rebuild shaders"))
                    compile_shaders();

                terrain->imgui_view();

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

        tetra::end_frame(0);
        last_loop_time = SDL_GetTicksNS() - loop_start_time;
    }

    tetra::deinit();
    SDL_Quit();
}

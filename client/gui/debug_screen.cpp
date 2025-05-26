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

#include "client/connection.h"
#include "client/game.h"
#include "client/gui/mc_gui.h"
#include "shared/build_info.h"
#include "tetra/gui/imgui.h"
#include "tetra/util/convar.h"

#include "client/state.h"
#include "client/sys/device_state.h"
#include "tetra/tetra_sdl_gpu.h"

static void IM_FMTARGS(5) add_text(mc_gui::mc_gui_ctx* ctx, ImDrawList* const drawlist, bool right_align, ImVec2& cursor, const char* fmt, ...)
{
    char buf[2048];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, SDL_arraysize(buf), fmt, args);
    va_end(args);

    ImVec2 text_size = ImGui::CalcTextSize(buf);

    ImVec2 upper_left = right_align ? ImVec2(cursor.x - text_size.x, cursor.y) : cursor;

    drawlist->AddRectFilled(upper_left - ImVec2(2, 1) * ctx->menu_scale, upper_left + text_size + ImVec2(1, 0) * ctx->menu_scale, IM_COL32(0, 0, 0, 128));

    mc_gui::add_text(drawlist, upper_left, buf);

    cursor.y += text_size.y + ctx->menu_scale;
}

static float get_y_spacing() { return ImGui::GetStyle().ItemSpacing.y; }

static const char* get_direction_name(int dir)
{
    switch (dir)
    {
    default:
    case 0:
        return "(South) (+Z)";
    case 1:
        return "(West) (-X)";
    case 2:
        return "(North) (-Z)";
    case 3:
        return "(East) (+X)";
    }
}

void do_debug_screen(mc_gui::mc_gui_ctx* ctx, game_t* game, ImDrawList* drawlist)
{
    const level_t* const lvl = game->level;
    ImVec2 cursor_l(ctx->menu_scale * 2, ctx->menu_scale * 1.5f);
    ImVec2 cursor_r(ImGui::GetMainViewport()->Size.x - ctx->menu_scale * 2, ctx->menu_scale * 1.5f);

    add_text(ctx, drawlist, 0, cursor_l, "mcs_b181_client (%s) (%s) (%.0f FPS)", build_info::ver_string::client().c_str(), build_info::build_mode,
        ImGui::GetIO().Framerate);

    size_t mem_chunk = 0;

    /* Chunk stats */
    {
        const std::vector<chunk_cubic_t*>& cvec = game->level->get_chunk_vec();
        size_t num_total = cvec.size();
        size_t num_dirty = 0;
        size_t num_dirty_visible = 0;
        size_t num_meshed = 0;
        size_t num_visible = 0;
        for (auto it : cvec)
        {
            num_visible += it->visible;
            num_meshed += !!(it->mesh_handle);
            num_dirty += it->dirty_level != chunk_cubic_t::DIRTY_LEVEL_NONE;
            num_dirty_visible += it->visible ? (it->dirty_level != chunk_cubic_t::DIRTY_LEVEL_NONE) : 0;
        }
        mem_chunk = num_total * sizeof(chunk_cubic_t);

        add_text(ctx, drawlist, 0, cursor_l, "C: %zu/%zu, M: %zu, D: %zu/%zu, Q: %zu", num_visible, num_total, num_meshed, num_dirty_visible, num_dirty,
            game->level->get_mesh_queue_size());
    }

    /* Entity stats */
    {
        size_t num_total = 0;
        size_t num_total_server = 0;
        if (game->connection)
            num_total_server = game->connection->get_size_ent_id_map();
        for (auto it : game->level->ecs.get_entities()->each())
            num_total++, void(it);
        add_text(ctx, drawlist, 0, cursor_l, "E: %zu, S: %zu", num_total, num_total_server);
    }

    cursor_l.y += get_y_spacing();

    add_text(ctx, drawlist, 0, cursor_l, "x: %.3f", game->level->foot_pos.x);
    add_text(ctx, drawlist, 0, cursor_l, "y: %.3f", game->level->foot_pos.y);
    add_text(ctx, drawlist, 0, cursor_l, "z: %.3f", game->level->foot_pos.z);
    int dir = (int(game->level->yaw + 360.f - 45.f) % 360) / 90;
    add_text(ctx, drawlist, 0, cursor_l, "f: %d %s (%.0f)", dir, get_direction_name(dir), game->level->yaw);

    cursor_l.y += get_y_spacing();
    {
        glm::ivec3 p_eye(glm::round(lvl->get_camera_pos()));
        chunk_cubic_t* c_eye = lvl->get_chunk(p_eye >> 4);
        Uint8 l_eye_b = 0, l_eye_s = 0;
        if (c_eye)
        {
            l_eye_b = c_eye->get_light_block(p_eye.x & 0x0F, p_eye.y & 0x0F, p_eye.z & 0x0F);
            l_eye_s = c_eye->get_light_sky(p_eye.x & 0x0F, p_eye.y & 0x0F, p_eye.z & 0x0F);
        }

        glm::ivec3 p_foot(glm::round(lvl->foot_pos));
        chunk_cubic_t* c_foot = lvl->get_chunk(p_foot >> 4);
        Uint8 l_foot_b = 0, l_foot_s = 0;
        if (c_foot)
        {
            l_foot_b = c_foot->get_light_block(p_foot.x & 0x0F, p_foot.y & 0x0F, p_foot.z & 0x0F);
            l_foot_s = c_foot->get_light_sky(p_foot.x & 0x0F, p_foot.y & 0x0F, p_foot.z & 0x0F);
        }

        add_text(ctx, drawlist, 0, cursor_l, "Light (Eye): B: %d, S: %d", l_eye_b, l_eye_s);
        add_text(ctx, drawlist, 0, cursor_l, "Light (Foot): B: %d, S: %d", l_foot_b, l_foot_s);
    }
    cursor_l.y += get_y_spacing();

    add_text(ctx, drawlist, 0, cursor_l, "Seed: %ld", game->level->mc_seed);

    add_text(ctx, drawlist, 0, cursor_l, "Biome: %s", mc_id::get_biome_name(game->level->get_biome_at(game->level->foot_pos)));
    add_text(ctx, drawlist, 0, cursor_l, "Time: %ld (Day: %ld)", ((game->level->mc_time % 24000) + 24000) % 24000, game->level->mc_time / 24000);
    add_text(ctx, drawlist, 0, cursor_l, "Mood: %.0f%%", game->level->mood * 100.0f);
    add_text(ctx, drawlist, 0, cursor_l, "Sound: %d/%d, Music: %.0f%%%s", lvl->sound_engine.get_num_slots_active(), lvl->sound_engine.get_num_slots(),
        lvl->music * 100.0f, (lvl->sound_engine.is_music_playing() ? " (Music Playing)" : ""));

    /* ======================== RIGHT SIDE ======================== */

    add_text(ctx, drawlist, 1, cursor_r, "Chunk mesh memory: %.1lf/%.1lf MiB", double(lvl->mesh_buffer.get_allocations_in_bytes() >> 10) / 1024.0,
        double(lvl->mesh_buffer.get_size_in_bytes() >> 10) / 1024.0);
    add_text(ctx, drawlist, 1, cursor_r, "Chunk data memory: %zu MiB", mem_chunk >> 20);
    add_text(ctx, drawlist, 1, cursor_r, "Alloc: %u, Pend: %u, Free: %u%s", lvl->mesh_buffer.get_num_allocations(), lvl->mesh_buffer.get_num_pending_releases(),
        lvl->mesh_buffer.get_num_avail_regions(), lvl->mesh_buffer.get_resize_in_progress() ? " (Resizing)" : "");

    /* Memory usage */
#ifdef SDL_PLATFORM_LINUX
    {
        FILE* fd = fopen("/proc/self/status", "r");
        if (fd)
        {
            char* line = NULL;
            size_t line_len = 0;
            long rss = 0;
            long rss_anon = 0;

            while (getline(&line, &line_len, fd) != -1 && line)
            {
                if (line_len == 0)
                    continue;

                if (line[line_len - 1] == '\n')
                    line[line_len - 1] = '\0';

                if (strncmp(line, "VmRSS", 5) == 0)
                {
                    char label[512], suffix[512];
                    sscanf(line, "%511s %ld %511s", label, &rss, suffix);
                }
                else if (strncmp(line, "RssAnon", 7) == 0)
                {
                    char label[512], suffix[512];
                    sscanf(line, "%511s %ld %511s", label, &rss_anon, suffix);
                }
            }
            add_text(ctx, drawlist, 1, cursor_r, "RSS: %ld MB, ANON: %ld MB", rss >> 10, rss_anon >> 10);
            free(line);
            fclose(fd);
        }
    }
#endif

    cursor_r.y += get_y_spacing();

    int num_cores = SDL_GetNumLogicalCPUCores();
    add_text(ctx, drawlist, 1, cursor_r, "CPU: %dx, RAM: %.1f GiB", num_cores, SDL_GetSystemRAM() / 1024.f);
    add_text(ctx, drawlist, 1, cursor_r, "OS: %s", SDL_GetPlatform());
    add_text(ctx, drawlist, 1, cursor_r, "Thermal state: %s%s", device_state::thermal_state_to_string(device_state::get_thermal_state()),
        (device_state::get_lower_power_mode() ? " (LP)" : ""));

    cursor_r.y += get_y_spacing();

    ImVec2 viewport_size = ImGui::GetMainViewport()->Size;
    add_text(ctx, drawlist, 1, cursor_r, "Viewport: %dx%d", int(viewport_size.x), int(viewport_size.y));
    SDL_PropertiesID gpu_props = SDL_GetGPUDeviceProperties(state::gpu_device);
    const char* gpu_name = SDL_GetStringProperty(gpu_props, SDL_PROP_GPU_DEVICE_NAME_STRING, NULL);
    const char* gpu_driver_name = SDL_GetStringProperty(gpu_props, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, NULL);
    const char* gpu_driver_version = SDL_GetStringProperty(gpu_props, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, NULL);
    const char* gpu_driver_info = SDL_GetStringProperty(gpu_props, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, NULL);
    if (gpu_name)
        add_text(ctx, drawlist, 1, cursor_r, "%s", gpu_name);
    if (gpu_driver_name && gpu_driver_version)
        add_text(ctx, drawlist, 1, cursor_r, "%s (%s)", gpu_driver_name, gpu_driver_version);
    else if (gpu_driver_name)
        add_text(ctx, drawlist, 1, cursor_r, "%s", gpu_driver_name);
    if (gpu_driver_info)
        add_text(ctx, drawlist, 1, cursor_r, "%s", gpu_driver_info);
    add_text(ctx, drawlist, 1, cursor_r, "%s %s", SDL_GetGPUDeviceDriver(state::gpu_device),
        tetra::SDL_GPUShaderFormat_to_string(SDL_GetGPUShaderFormats(state::gpu_device)).c_str());

    /* Highlighted block info */
    {
        glm::dvec3 cam_dir;
        cam_dir.x = SDL_cos(glm::radians(game->level->yaw)) * SDL_cos(glm::radians(game->level->pitch));
        cam_dir.y = SDL_sin(glm::radians(game->level->pitch));
        cam_dir.z = SDL_sin(glm::radians(game->level->yaw)) * SDL_cos(glm::radians(game->level->pitch));
        cam_dir = glm::normalize(cam_dir);

        glm::dvec3 rotation_point = game->level->get_camera_pos() /*+ glm::vec3(0.0f, (!crouching) ? 1.625f : 1.275f, 0.0f)*/;

        chunk_cubic_t* cache = NULL;
        {
            itemstack_t block_at_ray;
            glm::ivec3 collapsed_ray;
            glm::dvec3 ray = rotation_point;
            bool found = 0;
            for (int i = 0; !found && i <= 32 * 5; i++, ray += cam_dir / 32.0)
            {
                collapsed_ray = glm::floor(ray);
                if (game->level->get_block(collapsed_ray, block_at_ray, cache) && block_at_ray.id != BLOCK_ID_AIR && !mc_id::is_fluid(block_at_ray.id))
                    found = 1;
            }

            if (found)
            {
                cursor_r.y += get_y_spacing();
                add_text(ctx, drawlist, 1, cursor_r, "Targeted Solid: %d, %d, %d", collapsed_ray.x, collapsed_ray.y, collapsed_ray.z);
                const char* name = mc_id::get_name_from_item_id(block_at_ray.id, block_at_ray.damage);
                add_text(ctx, drawlist, 1, cursor_r, "%s (%d/%d)", name, block_at_ray.id, block_at_ray.damage);
            }
        }

        {
            itemstack_t block_at_ray;
            glm::ivec3 collapsed_ray;
            glm::dvec3 ray = rotation_point;
            bool found = 0;
            for (int i = 0; !found && i <= 32 * 5; i++, ray += cam_dir / 32.0)
            {
                collapsed_ray = glm::floor(ray);
                if (game->level->get_block(collapsed_ray, block_at_ray, cache) && block_at_ray.id != BLOCK_ID_AIR && mc_id::is_fluid(block_at_ray.id))
                    found = 1;
            }

            if (found)
            {
                cursor_r.y += get_y_spacing();
                add_text(ctx, drawlist, 1, cursor_r, "Targeted Fluid: %d, %d, %d", collapsed_ray.x, collapsed_ray.y, collapsed_ray.z);
                const char* name = mc_id::get_name_from_item_id(block_at_ray.id, block_at_ray.damage);
                add_text(ctx, drawlist, 1, cursor_r, "%s (%d/%d)", name, block_at_ray.id, block_at_ray.damage);
            }
        }
    }

    /* Frametime graph */
    {
        static float frametimes[512] = { 0.0f };
        static int frametimes_pos = 0;

        static double frametime_short_avg = 0.0f;
        {
            Uint64 sdl_tick_cur = SDL_GetTicksNS();
            static Uint64 sdl_tick_last = sdl_tick_cur;
            static double frametimes_short_avg[4] = { 0.0f };
            frametimes_short_avg[0] = frametimes_short_avg[1];
            frametimes_short_avg[1] = frametimes_short_avg[2];
            frametimes_short_avg[2] = frametimes_short_avg[3];
            frametimes_short_avg[3] = double(sdl_tick_cur - sdl_tick_last) / 1000000.0;
            sdl_tick_last = sdl_tick_cur;
            frametime_short_avg = (frametimes_short_avg[0] + frametimes_short_avg[1] + frametimes_short_avg[2] + frametimes_short_avg[3]) / 4.0;
        }

        frametimes_pos = (frametimes_pos + 1) % SDL_arraysize(frametimes);
        frametimes[frametimes_pos] = glm::mix(frametime_short_avg, 1000.0 / double(ImGui::GetIO().Framerate), 0.5);

        float frametimes_sorted[SDL_arraysize(frametimes)];
        memcpy(frametimes_sorted, frametimes, sizeof(frametimes_sorted));

        SDL_qsort(frametimes_sorted, SDL_arraysize(frametimes), sizeof(*frametimes), [](const void* _a, const void* _b) -> int {
            const float a = *(float*)_a;
            const float b = *(float*)_b;

            if (a < b)
                return -1;
            if (a > b)
                return 1;
            return 0;
        });
        double frametime_avg = 0.0;
        double frametime_min = frametimes_sorted[0];
        double frametime_max = frametimes_sorted[IM_ARRAYSIZE(frametimes_sorted) - 1];

        for (int i = 0; i < IM_ARRAYSIZE(frametimes); i++)
            frametime_avg += frametimes[i];

        frametime_avg /= double(IM_ARRAYSIZE(frametimes));

        static convar_int_t* r_fps_limiter = (convar_int_t*)convar_t::get_convar("r_fps_limiter");
        assert(r_fps_limiter);

        double target = frametime_avg;
        if (r_fps_limiter && r_fps_limiter->get())
            target = 1000.0 / double(r_fps_limiter->get());

        double max_delta = SDL_max(fabs(target - frametime_min), fabs(target - frametime_max));

        max_delta = SDL_max(max_delta, target * 0.03125);

        double min = target - max_delta;
        double max = target + max_delta;

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Size * ImVec2(0, 1), ImGuiCond_Always, ImVec2(0, 1));

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1) * ctx->menu_scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 192));
        ImGui::PushStyleColor(ImGuiCol_PlotLines, IM_COL32(128, 255, 128, 224));

        ImGui::Begin("Frametimes window", NULL, ctx->default_win_flags | ImGuiWindowFlags_NoInputs);

        ImGui::Spacing();
        ImVec2 cursor_f = ImGui::GetCursorScreenPos();
        add_text(ctx, drawlist, 0, cursor_f, "Frametimes: AVG: %.4lf ms, R: [%.4lf, %.4lf]", frametime_avg, frametime_min, frametime_max);
        add_text(ctx, drawlist, 0, cursor_f, "Graph: center %.4lf ms, radius: %.4lf ms", target, max_delta);
        ImGui::SetCursorScreenPos(cursor_f);

        ImGui::PlotLines("##Frametimes", frametimes, IM_ARRAYSIZE(frametimes), frametimes_pos, NULL, min, max, ImVec2(240, 120) * ctx->menu_scale);

        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }
}

void do_debug_crosshair(mc_gui::mc_gui_ctx*, game_t* game, ImDrawList* drawlist)
{
    const ImVec2 work_size = ImGui::GetMainViewport()->WorkSize;
    const ImVec2 work_center = ImGui::GetMainViewport()->GetWorkCenter();

    glm::mat4 mat_rot = glm::rotate(glm::mat4(1.0f), glm::radians(game->level->yaw + 90.0f), glm::vec3(0, 1, 0));
    mat_rot = glm::rotate(mat_rot, glm::radians(game->level->pitch), glm::vec3(1, 0, 0));

    glm::vec4 x(1, 0, 0, 1);
    glm::vec4 y(0, -1, 0, 1);
    glm::vec4 z(0, 0, -1, 1);

    x = x * mat_rot;
    y = y * mat_rot;
    z = z * mat_rot;

    float scale = SDL_min(work_size.x, work_size.y) * 0.03125f;
    float weight = scale / 16.0f;

    int dir = (int(game->level->yaw + 360.f - 45.f) % 360) / 90;

    drawlist->AddLine(work_center, work_center + ImVec2(z.x, z.y) * scale, IM_COL32_BLACK, weight + 2.5f);
    drawlist->AddLine(work_center, work_center + ImVec2(x.x, x.y) * scale, IM_COL32_BLACK, weight + 2.5f);
    drawlist->AddLine(work_center, work_center + ImVec2(y.x, y.y) * scale, IM_COL32_BLACK, weight + 2.5f);

    if (dir != 0 && dir != 3)
        drawlist->AddLine(work_center, work_center + ImVec2(y.x, y.y) * scale, IM_COL32(0, 255, 0, 255), weight);

    drawlist->AddLine(work_center, work_center + ImVec2(z.x, z.y) * scale, IM_COL32(127, 127, 255, 255), weight);
    drawlist->AddLine(work_center, work_center + ImVec2(x.x, x.y) * scale, IM_COL32(255, 0, 0, 255), weight);

    if (dir == 0 || dir == 3)
        drawlist->AddLine(work_center, work_center + ImVec2(y.x, y.y) * scale, IM_COL32(0, 255, 0, 255), weight);
}

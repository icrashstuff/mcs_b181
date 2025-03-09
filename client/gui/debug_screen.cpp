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

#include "client/game.h"
#include "client/gui/mc_gui.h"
#include "shared/build_info.h"
#include "tetra/gui/imgui.h"
#include "tetra/util/convar.h"

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
    ;
}

static float get_y_spacing() { return ImGui::GetStyle().ItemSpacing.y; }

void do_debug_screen(mc_gui::mc_gui_ctx* ctx, game_t* game, ImDrawList* drawlist)
{
    ImVec2 cursor_l(ctx->menu_scale * 2, ctx->menu_scale * 1.5f);
    ImVec2 cursor_r(ImGui::GetMainViewport()->Size.x - ctx->menu_scale * 2, ctx->menu_scale * 1.5f);

    add_text(ctx, drawlist, 0, cursor_l, "mcs_b181_client (%s) (%.0f FPS)", build_info::ver_string::client().c_str(), ImGui::GetIO().Framerate);

    size_t mem_chunk_guess = 0;
    size_t mem_chunk_mesh_guess = 0;

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
            num_meshed += it->vbo != 0;
            num_dirty += it->dirty_level != chunk_cubic_t::DIRTY_LEVEL_NONE;
            num_dirty_visible += it->visible ? (it->dirty_level != chunk_cubic_t::DIRTY_LEVEL_NONE) : 0;

            mem_chunk_guess += sizeof(chunk_cubic_t);
            mem_chunk_mesh_guess += sizeof(terrain_vertex_t) * (it->index_count + it->index_count_overlay + it->index_count_translucent) / 6 * 4;
        }
        mem_chunk_guess += cvec.capacity() * sizeof(cvec[0]);

        add_text(ctx, drawlist, 0, cursor_l, "C: %zu/%zu, M: %zu, D: %zu/%zu", num_visible, num_total, num_meshed, num_dirty_visible, num_dirty);
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

    add_text(ctx, drawlist, 0, cursor_l, "x: %.3f", game->level->camera_pos.x);
    add_text(ctx, drawlist, 0, cursor_l, "y: %.3f", game->level->camera_pos.y);
    add_text(ctx, drawlist, 0, cursor_l, "z: %.3f", game->level->camera_pos.z);
    add_text(ctx, drawlist, 0, cursor_l, "f: %d (%.0f)", (int(game->level->yaw + 360.f - 45.f) % 360) / 90, game->level->yaw);

    cursor_l.y += get_y_spacing();

    add_text(ctx, drawlist, 0, cursor_l, "Seed: %ld", game->level->mc_seed);

    add_text(ctx, drawlist, 0, cursor_l, "Biome: %s", mc_id::get_biome_name(game->level->get_biome_at(game->level->camera_pos)));
    add_text(ctx, drawlist, 0, cursor_l, "Time: %ld (Day: %ld)", ((game->level->mc_time % 24000) + 24000) % 24000, game->level->mc_time / 24000);

    /* ======================== RIGHT SIDE ======================== */

    add_text(ctx, drawlist, 1, cursor_r, "Chunk mesh memory: %.1lf MB", double(mem_chunk_mesh_guess >> 10) / 1024.0);
    add_text(ctx, drawlist, 1, cursor_r, "Chunk data memory: %zu MB", mem_chunk_guess >> 20);

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
                if (line[line_len] == '\n')
                    line[line_len] = '\0';

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
    add_text(ctx, drawlist, 1, cursor_r, "CPU: %dx", num_cores);

    cursor_r.y += get_y_spacing();

    ImVec2 viewport_size = ImGui::GetMainViewport()->Size;
    add_text(ctx, drawlist, 1, cursor_r, "Viewport: %dx%d (%s)", int(viewport_size.x), int(viewport_size.y), glGetString(GL_VENDOR));
    add_text(ctx, drawlist, 1, cursor_r, "%s", glGetString(GL_RENDERER));
    add_text(ctx, drawlist, 1, cursor_r, "%s", glGetString(GL_VERSION));
    add_text(ctx, drawlist, 1, cursor_r, "GLSL: %s, GLEW %s", glGetString(GL_SHADING_LANGUAGE_VERSION), glewGetString(GLEW_VERSION));

    /* Highlighted block info */
    {
        glm::dvec3 cam_dir;
        cam_dir.x = SDL_cos(glm::radians(game->level->yaw)) * SDL_cos(glm::radians(game->level->pitch));
        cam_dir.y = SDL_sin(glm::radians(game->level->pitch));
        cam_dir.z = SDL_sin(glm::radians(game->level->yaw)) * SDL_cos(glm::radians(game->level->pitch));
        cam_dir = glm::normalize(cam_dir);

        glm::dvec3 rotation_point = game->level->camera_pos /*+ glm::vec3(0.0f, (!crouching) ? 1.625f : 1.275f, 0.0f)*/;

        {
            itemstack_t block_at_ray;
            glm::ivec3 collapsed_ray;
            glm::dvec3 ray = rotation_point;
            bool found = 0;
            for (int i = 0; !found && i <= 32 * 5; i++, ray += cam_dir / 32.0)
            {
                collapsed_ray = glm::floor(ray);
                if (game->level->get_block(collapsed_ray, block_at_ray) && block_at_ray.id != BLOCK_ID_AIR && !mc_id::is_fluid(block_at_ray.id))
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
                if (game->level->get_block(collapsed_ray, block_at_ray) && block_at_ray.id != BLOCK_ID_AIR && mc_id::is_fluid(block_at_ray.id))
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

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

#include "texture_terrain.h"

#include "texture_ids.h"

#define STBRP_STATIC
#include "tetra/gui/console.h"
#include "tetra/gui/imgui-1.91.1/imstb_rectpack.h"
#include "tetra/gui/imgui.h"

#include "tetra/util/convar.h"
#include "tetra/util/physfs/physfs.h"
#include "tetra/util/stbi.h"

#include "jzon/Jzon.h"

static convar_int_t r_dump_mipmaps_terrain("r_dump_mipmaps_terrain", 1, 0, 1, "Dump terrain atlas mipmaps to screenshots folder on atlas rebuild",
    CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL);

/**
 * Performs a case insensitive check if a string ends with another
 */
static bool path_ends_width(const char* s, const char* end)
{
    size_t len = strlen(s);
    size_t len_end = strlen(end);

    if (len < len_end)
        return 0;
    else
        for (size_t j = 0; j < len_end; j++)
            if (tolower(s[len - j]) != end[len_end - j])
                return 0;

    return 1;
}

texture_terrain_t::texture_terrain_t(std::string path_textures)
{
    Uint64 start_tick = SDL_GetTicksNS();

    std::pair<std::string, bool> paths[] = {
        { "/blocks/", false },
        { "/block/", false },
        { "/items/", true },
        { "/item/", true },
    };
    for (size_t i_path = 0; i_path < SDL_arraysize(paths); i_path++)
    {
        bool is_item = paths[i_path].second;
        std::string s = path_textures + paths[i_path].first;
        char** rc = PHYSFS_enumerateFiles(s.c_str());

        for (char** it = rc; *it != NULL; it++)
        {
            if (!path_ends_width(*it, ".png"))
                continue;
            std::string tpath((s + *it));
            texture_pre_pack_t t;
            t.data_stbi = stbi_physfs_load(tpath.c_str(), &t.w, &t.h, NULL, 4);
            t.is_item = is_item;
            t.name = strdup(*it);

            /** TODO: Check for .mcmeta files */
            if (t.h > t.w && t.h % t.w == 0)
            {
                t.animated = 1;
                t.frame_num_individual = t.h / t.w;
                t.frame_offsets_len = t.frame_num_individual;
                t.frame_time = 1;
                t.h = t.w;
            }
            else
            {
                t.frame_num_individual = 1;
                t.frame_offsets_len = 1;
                t.animated = 0;
            }

            /* Make sure texture is power of two and is not wide */
            if (t.w == 0 || (t.w & (t.w - 1)) || t.h == 0 || (t.h & (t.h - 1)) || t.w > t.h)
            {
                stbi_image_free(t.data_stbi);
                t.data_stbi = NULL;
                t.w = 0;
                t.h = 0;
                t.animated = 0;
                t.frame_num_individual = 1;
                t.frame_offsets_len = 1;
            }

            PHYSFS_file* fd = NULL;
            if (t.animated)
                fd = PHYSFS_openRead((tpath + ".mcmeta").c_str());
            if (fd)
            {
                Jzon::Node node;
                {
                    std::string json_dat;
                    int num_read = 0;
                    do
                    {
                        char buf[1024];
                        num_read = PHYSFS_readBytes(fd, buf, SDL_arraysize(buf));
                        json_dat.append(buf, num_read);
                    } while (num_read > 0);
                    PHYSFS_close(fd);
                    fd = NULL;

                    {
                        Jzon::Parser parser;
                        node = parser.parseString(json_dat);
                    }
                }

                Jzon::Node node_anim = node.get("animation");
                Jzon::Node node_frametime = node_anim.get("frametime");
                Jzon::Node node_frames = node_anim.get("frames");
                Jzon::Node node_interpolate = node_anim.get("interpolate");

                if (node_frametime.isNumber())
                    t.frame_time = node_frametime.toInt();

                if (node_interpolate.isBool())
                    t.interpolate = node_interpolate.toBool();

                if (node_frames.isArray())
                {
                    t.frame_offsets_len = node_frames.getCount();
                    t.frame_offsets = (int*)calloc(t.frame_offsets_len, sizeof(*t.frame_offsets));
                    for (int i = 0; i < t.frame_offsets_len && t.frame_offsets; i++)
                    {
                        Jzon::Node node_frame = node_frames.get(i);
                        if (!node_frame.isNumber())
                            continue;
                        t.frame_offsets[i] = SDL_clamp(node_frame.toInt(), 0, SDL_max(t.frame_num_individual - 1, 0));
                    }
                }
            }

            textures.push_back(t);
        }
        PHYSFS_freeList(rc);
    }

    /* Missing texture */
    {
        texture_pre_pack_t t = {
            .w = 0,
            .h = 0,
        };
        textures.push_back(t);
    }

    size_t min_area = 0;
    size_t min_height = 1;
    size_t min_width = 1;
    for (size_t i = 0; i < textures.size(); i++)
    {
        if (textures[i].w <= 0 || textures[i].h <= 0 || textures[i].data_stbi == NULL)
        {
            textures[i].w = 16;
            textures[i].h = 16;
        }
        min_area += textures[i].w * textures[i].h;
        min_height = SDL_max(min_height, textures[i].h);
        min_width = SDL_max(min_width, textures[i].w);
    }

    /* Make min_width and min_height powers of two */
    min_height--;
    min_width--;
    for (int i = 1; i < 30; i++)
    {
        min_height |= min_height >> i;
        min_width |= min_width >> i;
    }
    min_height++;
    min_width++;

    /* Save the packer from some useless iterations */
    for (int iterations = 0; min_height * min_width < min_area && iterations < 128; iterations++)
    {
        if (min_width <= min_height)
            min_width <<= 1;
        else
            min_height <<= 1;
    }

    std::vector<stbrp_rect> rects;
    for (int i = 0; i < (int)textures.size(); i++)
    {
        texture_pre_pack_t t = textures[i];
        stbrp_rect r;
        r.id = i;
        r.w = t.w;
        r.h = t.h;
        rects.push_back(r);
    }

    /* Try to pack the textures, and if unsuccessful increase a dimension an try again */
    bool all_was_packed = 0;
    for (int iterations = 0; !all_was_packed && iterations < 128; iterations++)
    {
        std::vector<stbrp_node> nodes;
        nodes.resize(min_width);

        stbrp_context packer;
        stbrp_init_target(&packer, min_width, min_height, nodes.data(), nodes.size());

        all_was_packed = stbrp_pack_rects(&packer, rects.data(), rects.size());
        if (!all_was_packed)
        {
            size_t mw = min_width;
            size_t mh = min_height;
            if (min_width <= min_height)
                min_width <<= 1;
            else
                min_height <<= 1;
            dc_log_warn("Unable to pack all textures into %zux%zu, increasing atlas size to %zux%zu", mw, mh, min_width, min_height);
        }
    }

    tex_base_height = min_height;
    tex_base_width = min_width;
    tex_filled_area = min_area;

    dc_log("Loaded %zu block/item textures with dimensions: %zux%zu (%.2f%% used)", textures.size(), min_width, min_height,
        tex_filled_area * 100.0 / double(tex_base_width * tex_base_height));

    Uint8* data = (Uint8*)malloc(min_height * min_width * 4);
    for (size_t i_h = 0; i_h < min_height; i_h++)
    {
        const size_t i_h_remaining = i_h / 4 % 2;
        for (size_t i_w = 0; i_w < min_width; i_w++)
        {
            data[(i_h * min_width + i_w) * 4 + 0] = 0;
            data[(i_h * min_width + i_w) * 4 + 1] = 0xFF * (i_w / 4 % 2 == i_h_remaining);
            data[(i_h * min_width + i_w) * 4 + 2] = 0x00 * (i_w / 4 % 2 == i_h_remaining);
            data[(i_h * min_width + i_w) * 4 + 3] = 0xFF;
        }
    }

    for (size_t i = 0; i < rects.size(); i++)
    {
        if (!rects[i].was_packed)
            continue;
        textures[rects[i].id].x = rects[i].x;
        textures[rects[i].id].y = rects[i].y;
        texture_pre_pack_t t = textures[rects[i].id];

        for (int y = 0; y < t.h; y++)
        {
            if (t.data_stbi != NULL)
                memcpy(data + ((y + t.y) * tex_base_width + t.x) * 4, t.data_stbi + y * t.w * 4, t.w * 4);
            else
            {
                for (GLsizei i_w = t.x; i_w < t.w + t.x; i_w++)
                {
                    data[((y + t.y) * tex_base_width + i_w) * 4 + 0] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                    data[((y + t.y) * tex_base_width + i_w) * 4 + 1] = 0;
                    data[((y + t.y) * tex_base_width + i_w) * 4 + 2] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                    data[((y + t.y) * tex_base_width + i_w) * 4 + 3] = 0xFF;
                }
            }
        }
    }

    for (size_t i = 0; i < textures.size(); i++)
    {
        mc_id::terrain_face_id_t fid = mc_id::get_face_from_fname(textures[i].name);
        texture_faces[fid] = texture_post_pack_t(textures[i], fid, tex_base_width, tex_base_height);
        if (fid == mc_id::FACE_LAVA_FLOW_STRAIGHT)
            texture_faces[mc_id::FACE_LAVA_FLOW_DIAGONAL] = texture_post_pack_t(textures[i], mc_id::FACE_LAVA_FLOW_DIAGONAL, tex_base_width, tex_base_height);
        else if (fid == mc_id::FACE_LAVA_FLOW_DIAGONAL)
            texture_faces[mc_id::FACE_LAVA_FLOW_STRAIGHT] = texture_post_pack_t(textures[i], mc_id::FACE_LAVA_FLOW_STRAIGHT, tex_base_width, tex_base_height);
        else if (fid == mc_id::FACE_WATER_FLOW_STRAIGHT)
            texture_faces[mc_id::FACE_WATER_FLOW_DIAGONAL] = texture_post_pack_t(textures[i], mc_id::FACE_WATER_FLOW_DIAGONAL, tex_base_width, tex_base_height);
        else if (fid == mc_id::FACE_WATER_FLOW_DIAGONAL)
            texture_faces[mc_id::FACE_WATER_FLOW_STRAIGHT] = texture_post_pack_t(textures[i], mc_id::FACE_WATER_FLOW_STRAIGHT, tex_base_width, tex_base_height);
        if (textures[i].animated)
            anim_textures.push_back(textures[i]);
        else
        {
            if (textures[i].data_stbi)
            {
                stbi_image_free(textures[i].data_stbi);
                textures[i].data_stbi = NULL;
            }
        }
    }

    raw_mipmaps.push_back(data);

    glGenTextures(1, &tex_id_main);
    glBindTexture(GL_TEXTURE_2D, tex_id_main);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    update();

    dc_log("Built terrain atlas in %.1f ms", (double)(SDL_GetTicksNS() - start_tick) / 1000000.0);

    if (r_dump_mipmaps_terrain.get())
        dump_mipmaps();
}

void texture_terrain_t::update()
{
    Uint64 cur_sdl_tick = SDL_GetTicks();
    if (cur_sdl_tick - time_last_update < 25)
        return;
    time_last_update = cur_sdl_tick;

    /* Update flailing clocks/compasses */
    for (size_t i = 0; i < anim_textures.size(); i++)
    {
        texture_pre_pack_t t = anim_textures[i];

        if (t.is_item && t.name && strcmp(t.name, "compass.png") == 0)
        {
            if (compass_flail)
            {
                if (compass_flail_dir_countdown-- < 0)
                {
                    compass_flail_dir_countdown = SDL_rand_bits() % 20;
                    compass_flail_dir = SDL_rand_bits() & 16 ? 1 : -1;
                }
                compass_flail_rotation += (SDL_randf() * 10.0 - SDL_randf() * 2.0) * compass_flail_dir;
            }
        }
        else if (t.is_item && t.name && strcmp(t.name, "clock.png") == 0)
        {
            if (clock_flail)
            {
                if (clock_flail_dir_countdown-- < 0)
                {
                    clock_flail_dir_countdown = SDL_rand_bits() % 20;
                    clock_flail_dir = SDL_rand_bits() & 16 ? 1 : -1;
                }
                if (clock_flail_dir_countdown == 1)
                    clock_flail_dir *= 2;
                clock_flail_mc_time += (SDL_rand(500) - SDL_rand(225)) * clock_flail_dir;
                if (clock_flail_mc_time < 0)
                    clock_flail_mc_time = 24000 + clock_flail_mc_time;
                clock_flail_mc_time %= 24000;
            }
        }
    }

    double cur_mc_tick = cur_sdl_tick / 50.0;

    /* Actually update animations */
    for (size_t mip_level = 0; mip_level < raw_mipmaps.size(); mip_level++)
    {
        Uint8* data = raw_mipmaps[mip_level];
        for (size_t i = 0; i < anim_textures.size(); i++)
        {
            texture_pre_pack_t t = anim_textures[i];

            double frame_cur_f = SDL_fmod(cur_mc_tick / (double)t.frame_time, t.frame_offsets_len);

            if (t.is_item && t.name && strcmp(t.name, "compass.png") == 0)
            {
                float rotation = compass_flail ? compass_flail_rotation : compass_rotation;
                frame_cur_f = ((((int)rotation) % 360 + 360) % 360) / 360.0;
                frame_cur_f *= (double)t.frame_offsets_len;
            }
            else if (t.is_item && t.name && strcmp(t.name, "clock.png") == 0)
            {
                Uint64 time = clock_flail ? clock_flail_mc_time : clock_mc_time;
                frame_cur_f = (time % 24000) / (24000 / t.frame_offsets_len);
            }

            int frame_off_cur_i = frame_cur_f;
            int frame_off_cur_n = (frame_off_cur_i + 1) % t.frame_offsets_len;
            float frame_blend = frame_cur_f - frame_off_cur_i;
            frame_blend = SDL_clamp(frame_blend, 0.0f, 1.0f);

            int frame_cur_i = 0;
            if (t.frame_offsets)
                frame_cur_i = t.frame_offsets[frame_off_cur_i];
            else
                frame_cur_i = frame_off_cur_i;

            int frame_cur_n = 0;
            if (t.frame_offsets)
                frame_cur_n = t.frame_offsets[frame_off_cur_n];
            else
                frame_cur_n = frame_off_cur_n;

            if (frame_off_cur_n < 0 || (frame_off_cur_n + 1) * t.h > t.h * t.frame_offsets_len)
                continue;

            t.data_mipmaped[0] = t.data_stbi;

            for (int y = 0; y < t.h; y++)
            {
                if (t.data_mipmaped[mip_level] != NULL)
                {
                    if (!t.interpolate)
                        memcpy(data + ((y + t.y) * tex_base_width + t.x) * 4, t.data_mipmaped[mip_level] + (y + frame_cur_i * t.h) * t.w * 4, t.w * 4);
                    else
                    {
                        int startx = ((y + t.y) * tex_base_width + t.x) * 4;
                        for (int j = 0; j < t.w * 4; j++)
                        {
                            Uint8 col0 = *(t.data_mipmaped[mip_level] + (y + frame_cur_i * t.h) * t.w * 4 + j) * (1.0 - frame_blend);
                            Uint8 col1 = *(t.data_mipmaped[mip_level] + (y + frame_cur_n * t.h) * t.w * 4 + j) * frame_blend;
                            data[startx + j] = col0 + col1;
                        }
                    }
                }
                else
                {
                    for (GLsizei i_w = t.x; i_w < t.w + t.x; i_w++)
                    {
                        data[((y + t.y) * tex_base_width + i_w) * 4 + 0] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                        data[((y + t.y) * tex_base_width + i_w) * 4 + 1] = 0;
                        data[((y + t.y) * tex_base_width + i_w) * 4 + 2] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                        data[((y + t.y) * tex_base_width + i_w) * 4 + 3] = 0xFF;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < raw_mipmaps.size(); i++)
        glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, tex_base_width, tex_base_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw_mipmaps[i]);
}

void texture_terrain_t::dump_mipmaps()
{
    if (!PHYSFS_mkdir("/game/screenshots"))
    {
        dc_log_error("Unable to create screenshots folder! (PhysFS err code: %d)", PHYSFS_getLastErrorCode());
        return;
    }
    for (size_t i = 0; i < raw_mipmaps.size(); i++)
    {
        if (!raw_mipmaps[i])
            continue;
        char buf[64];
        snprintf(buf, SDL_arraysize(buf), "/game/screenshots/terrain_%zu.png", i);
        stbi_physfs_write_png(buf, tex_base_width >> i, tex_base_height >> i, 4, raw_mipmaps[i], tex_base_width * 4);
    }
}

static bool DragUint64(
    const char* label, Uint64* v, float v_speed = 1.0f, Uint64 v_min = 0, Uint64 v_max = 0, const char* format = "%zu", ImGuiSliderFlags flags = 0)
{
    return ImGui::DragScalar(label, ImGuiDataType_U64, v, v_speed, &v_min, &v_max, format, flags);
}

bool texture_terrain_t::imgui_view(const char* title)
{
    if (title == NULL)
        title = "texture_terrain_t::imgui_view";
    ImGui::PushID(this);
    ImGui::SetNextWindowSizeConstraints(ImVec2(tex_base_width, 0), ImVec2(-1, -1));

    if (!ImGui::BeginChild(title))
    {
        ImGui::EndChild();
        ImGui::PopID();
        return false;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::Text("Num animated textures: %zu", anim_textures.size());
    ImGui::Checkbox("Compass flail", &compass_flail);
    ImGui::Checkbox("Clock flail", &clock_flail);
    ImGui::SliderFloat("Compass rotation", &compass_rotation, 0, 360);
    DragUint64("Clock position", &clock_mc_time, 100);

    if (ImGui::Button("Dump all mipmap levels"))
        dump_mipmaps();

    ImGui::Text("Num mimap levels: %zu", raw_mipmaps.size());
    ImGui::Text("Atlas size: %zux%zu (%.2f%% used)", tex_base_width, tex_base_height, tex_filled_area * 100.0 / double(tex_base_width * tex_base_height));

    if (ImGui::BeginCombo("Texture Selector", mc_id::get_face_id_name(imgui_selected_face)))
    {
        for (int i = 0; i < mc_id::FACE_COUNT; i++)
        {
            mc_id::terrain_face_id_t cur = (mc_id::terrain_face_id_t)i;
            if (ImGui::Selectable(mc_id::get_face_id_name(cur), imgui_selected_face == cur))
                imgui_selected_face = cur;
            if (imgui_selected_face == cur)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    float my_tex_w = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2 - ImGui::GetStyle().ScrollbarSize;
    float my_tex_h = (my_tex_w * (double)tex_base_height) / (double)tex_base_width;

    ImGui::SetCursorPosX((ImGui::GetStyle().ScrollbarSize + ImGui::GetStyle().WindowPadding.x / 2.0f) / 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (imgui_selected_face == mc_id::FACE_ATLAS)
        ImGui::Image((ImTextureID)(size_t)tex_id_main, ImVec2(my_tex_w, my_tex_h), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
    else
    {
        texture_post_pack_t t = texture_faces[imgui_selected_face];
        ImVec2 corner0(t.face.corners[0].x, t.face.corners[0].y);
        ImVec2 corner1(t.face.corners[3].x, t.face.corners[3].y);
        ImGui::Image((ImTextureID)(size_t)tex_id_main, ImVec2(t.w * 4, t.h * 4), corner0, corner1);
        ImGui::SameLine();
        ImGui::Text("%s\nSize: %dx%d", mc_id::get_face_fname(imgui_selected_face), t.w, t.h);
    }

    /* Pulled from imgui_demo.cpp and lightly modified*/
    if (imgui_selected_face == mc_id::FACE_ATLAS && ImGui::BeginItemTooltip())
    {
        ImGuiIO& io = ImGui::GetIO();
        float region_sz = 40.0f;
        float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
        float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
        float zoom = 4.0f;
        if (region_x < 0.0f)
            region_x = 0.0f;
        else if (region_x > my_tex_w - region_sz)
            region_x = my_tex_w - region_sz;
        if (region_y < 0.0f)
            region_y = 0.0f;
        else if (region_y > my_tex_h - region_sz)
            region_y = my_tex_h - region_sz;
        ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
        ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);
        ImGui::Text("Min: (%.2f, %.2f)", tex_base_width * uv0.x, tex_base_height * uv0.y);
        ImGui::Text("Max: (%.2f, %.2f)", tex_base_width * uv1.x, tex_base_height * uv1.y);
        ImGui::Image((ImTextureID)(size_t)tex_id_main, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1);
        ImGui::EndTooltip();
    }

    ImGui::EndChild();
    ImGui::PopID();
    return true;
}

texture_terrain_t::~texture_terrain_t()
{
    glDeleteTextures(1, &tex_id_main);

    for (size_t i = 0; i < raw_mipmaps.size(); i++)
        free(raw_mipmaps[i]);

    for (size_t i = 0; i < textures.size(); i++)
    {
        if (textures[i].data_stbi)
            stbi_image_free(textures[i].data_stbi);
        if (textures[i].name)
            free(textures[i].name);
    }
}

void terrain_vertex_t::create_vao(GLuint* vao)
{
    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
}

void terrain_vertex_t::create_vbo(GLuint* vbo, GLuint* ebo, const std::vector<terrain_vertex_t>& vtx, const std::vector<Uint8>& ind)
{
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);

    glGenBuffers(1, ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);

    glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vtx[0]), vtx.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size() * sizeof(ind[0]), ind.data(), GL_STATIC_DRAW);

    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
}

void terrain_vertex_t::create_vbo(GLuint* vbo, GLuint* ebo, const std::vector<terrain_vertex_t>& vtx, const std::vector<Uint16>& ind)
{
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);

    glGenBuffers(1, ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);

    glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vtx[0]), vtx.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size() * sizeof(ind[0]), ind.data(), GL_STATIC_DRAW);

    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
}

void terrain_vertex_t::create_vbo(GLuint* vbo, GLuint* ebo, const std::vector<terrain_vertex_t>& vtx, const std::vector<Uint32>& ind)
{
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);

    glGenBuffers(1, ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);

    glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vtx[0]), vtx.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size() * sizeof(ind[0]), ind.data(), GL_STATIC_DRAW);

    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
}

GLenum terrain_vertex_t::create_vbo(GLuint* vbo, GLuint* ebo, const std::vector<terrain_vertex_t>& vtx)
{
    const size_t quads = vtx.size() / 4;
    const size_t required_indicies = quads * 6;
    /*
    if(required_indicies == 0)
        return GL_NONE;
    else if(required_indicies < 0xFF)
    {
        std::vector<Uint8> ind;
        for (size_t i = 0; i < quads; i++)
        {
            ind.push_back(i * 4 + 0);
            ind.push_back(i * 4 + 1);
            ind.push_back(i * 4 + 2);
            ind.push_back(i * 4 + 1);
            ind.push_back(i * 4 + 2);
            ind.push_back(i * 4 + 3);
        }
        create_vbo(vbo, ebo, vtx, ind);
       return GL_UNSIGNED_BYTE;
    }
    else if (required_indicies < 0xFFFF)
    {
        std::vector<Uint16> ind;
        for (size_t i = 0; i < quads; i++)
        {
            ind.push_back(i * 4 + 0);
            ind.push_back(i * 4 + 1);
            ind.push_back(i * 4 + 2);
            ind.push_back(i * 4 + 1);
            ind.push_back(i * 4 + 2);
            ind.push_back(i * 4 + 3);
        }
        create_vbo(vbo, ebo, vtx, ind);
       return GL_UNSIGNED_SHORT;
    }
    else
    */
    {
        std::vector<Uint32> ind;
        for (size_t i = 0; i < quads; i++)
        {
            ind.push_back(i * 4 + 0);
            ind.push_back(i * 4 + 1);
            ind.push_back(i * 4 + 2);
            ind.push_back(i * 4 + 2);
            ind.push_back(i * 4 + 1);
            ind.push_back(i * 4 + 3);
        }
        create_vbo(vbo, ebo, vtx, ind);
        return GL_UNSIGNED_INT;
    }
}

#define STB_RECT_PACK_IMPLEMENTATION
#include "tetra/gui/imgui-1.91.1/imstb_rectpack.h"

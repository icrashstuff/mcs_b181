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

#define MCS_B181_CLIENT_TEXTURE_IDS_H_IMPL
#include "texture_ids.h"

#include "texture_terrain.h"

#include "tetra/gui/imgui.h"
#include "tetra/util/stb_rect_pack.h"

#include "tetra/util/convar.h"
#include "tetra/util/physfs/physfs.h"
#include "tetra/util/stbi.h"

#include "tetra/log.h"

#include "jzon/Jzon.h"

#include "gpu/gpu.h"

static convar_int_t r_dump_mipmaps_terrain("r_dump_mipmaps_terrain", 1, 0, 1, "Dump terrain atlas mipmaps to screenshots folder on atlas rebuild",
    CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t r_mipmap_levels(
    "r_mipmap_max_initial_levels", TERRAIN_MAX_MIPMAP_LEVELS, 0, TERRAIN_MAX_MIPMAP_LEVELS, "Maximum number of mipmap levels", CONVAR_FLAG_SAVE);

static convar_float_t r_mipmap_bias("r_mipmap_bias", -1.125f, -2.0, 2.0f, "Bias value for switching between mipmap levels", CONVAR_FLAG_SAVE);

/**
 * Performs a case insensitive check if a string ends with another
 */
static bool path_ends_width(const char* const s, const char* const end)
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

texture_terrain_t::texture_terrain_t(const std::string& path_textures)
{
    const Uint64 start_tick = SDL_GetTicksNS();

    const std::pair<std::string, bool> paths[] = {
        { "/blocks/", false },
        { "/block/", false },
        { "/items/", true },
        { "/item/", true },
    };
    std::vector<texture_pre_pack_t> textures;
    for (size_t i_path = 0; i_path < SDL_arraysize(paths); i_path++)
    {
        const bool is_item = paths[i_path].second;
        const std::string s = path_textures + paths[i_path].first;
        char** rc = PHYSFS_enumerateFiles(s.c_str());

        for (char** it = rc; *it != NULL; it++)
        {
            if (!path_ends_width(*it, ".png"))
                continue;
            const std::string tpath((s + *it));
            texture_pre_pack_t t;
            t.data_stbi = stbi_physfs_load(tpath.c_str(), &t.w, &t.h, NULL, 4);
            t.is_item = is_item;
            t.name = *it;

            /** TODO: Check for .mcmeta files */
            if (t.h > t.w && t.h % t.w == 0 && t.w && t.h)
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

            PHYSFS_File* fd = NULL;
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
                    t.frame_offsets.resize(t.frame_offsets_len, 0);
                    for (int i = 0; i < t.frame_offsets_len; i++)
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

    /* Debug texture */
    {
        texture_pre_pack_t t = {
            .w = 0,
            .h = 0,
        };
        textures.push_back(t);
    }

    /* Ensure all textures have data and are at least 16x16 */
    for (size_t i = 0; i < textures.size(); i++)
    {
        if (textures[i].w <= 0 || textures[i].h <= 0 || textures[i].data_stbi == NULL)
        {
            textures[i].w = 16;
            textures[i].h = 16;
            textures[i].data_mipmaped[0].resize(16 * 16 * 4);
            for (int i_w = 0; i_w < 16; i_w++)
            {
                for (int i_h = 0; i_h < 16; i_h++)
                {
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 0] = 0xFF * (i_w / 8 % 2 == i_h / 8 % 2);
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 1] = 0;
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 2] = 0xFF * (i_w / 8 % 2 == i_h / 8 % 2);
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 3] = 0xFF;
                }
            }
        }
        else if (textures[i].w < 16)
        {
            textures[i].data_mipmaped[0].resize(16 * 16 * 4);

            const int scale = 16 / textures[i].w;

            for (int i_w = 0; i_w < 16; i_w++)
            {
                for (int i_h = 0; i_h < 16 * textures[i].frame_num_individual; i_h++)
                {
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 0] = textures[i].data_stbi[((i_w / scale) + (i_h / scale) * textures[i].w) * 4 + 0];
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 1] = textures[i].data_stbi[((i_w / scale) + (i_h / scale) * textures[i].w) * 4 + 1];
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 2] = textures[i].data_stbi[((i_w / scale) + (i_h / scale) * textures[i].w) * 4 + 2];
                    textures[i].data_mipmaped[0][(i_w + i_h * 16) * 4 + 3] = textures[i].data_stbi[((i_w / scale) + (i_h / scale) * textures[i].w) * 4 + 3];
                }
            }
            textures[i].w = 16;
            textures[i].h = 16;
        }
        else
        {
            textures[i].data_mipmaped[0].resize(textures[i].w * textures[i].h * 4 * textures[i].frame_num_individual);
            memcpy(textures[i].data_mipmaped[0].data(), textures[i].data_stbi, textures[i].data_mipmaped[0].size());
        }
        stbi_image_free(textures[i].data_stbi);
        textures[i].data_stbi = NULL;
    }

    /* Mipmap textures */
    for (size_t i = 0; i < textures.size(); i++)
    {
        for (size_t mip_lvl = 1; mip_lvl < SDL_arraysize(textures[0].data_mipmaped); mip_lvl++)
        {
            const int new_w = textures[i].w / (1 << mip_lvl);
            const int new_h = textures[i].h / (1 << mip_lvl);

            textures[i].data_mipmaped[mip_lvl].resize(textures[i].data_mipmaped[mip_lvl - 1].size() / 4);

            Uint8* result = textures[i].data_mipmaped[mip_lvl].data();
            Uint8* source = textures[i].data_mipmaped[mip_lvl - 1].data();

            for (int i_w = 0; i_w < new_w; i_w++)
            {
                for (int i_h = 0; i_h < new_h * textures[i].frame_num_individual; i_h++)
                {
                    float r = 0, g = 0, b = 0, a = 0, at = 0;
                    at = source[((i_w * 2 + 0) + (i_h * 2 + 0) * new_w * 2) * 4 + 3] / 255.f;
                    r += source[((i_w * 2 + 0) + (i_h * 2 + 0) * new_w * 2) * 4 + 0] / 255.f * at;
                    g += source[((i_w * 2 + 0) + (i_h * 2 + 0) * new_w * 2) * 4 + 1] / 255.f * at;
                    b += source[((i_w * 2 + 0) + (i_h * 2 + 0) * new_w * 2) * 4 + 2] / 255.f * at;
                    a += at;

                    at = source[((i_w * 2 + 1) + (i_h * 2 + 0) * new_w * 2) * 4 + 3] / 255.f;
                    r += source[((i_w * 2 + 1) + (i_h * 2 + 0) * new_w * 2) * 4 + 0] / 255.f * at;
                    g += source[((i_w * 2 + 1) + (i_h * 2 + 0) * new_w * 2) * 4 + 1] / 255.f * at;
                    b += source[((i_w * 2 + 1) + (i_h * 2 + 0) * new_w * 2) * 4 + 2] / 255.f * at;
                    a += at;

                    at = source[((i_w * 2 + 0) + (i_h * 2 + 1) * new_w * 2) * 4 + 3] / 255.f;
                    r += source[((i_w * 2 + 0) + (i_h * 2 + 1) * new_w * 2) * 4 + 0] / 255.f * at;
                    g += source[((i_w * 2 + 0) + (i_h * 2 + 1) * new_w * 2) * 4 + 1] / 255.f * at;
                    b += source[((i_w * 2 + 0) + (i_h * 2 + 1) * new_w * 2) * 4 + 2] / 255.f * at;
                    a += at;

                    at = source[((i_w * 2 + 1) + (i_h * 2 + 1) * new_w * 2) * 4 + 3] / 255.f;
                    r += source[((i_w * 2 + 1) + (i_h * 2 + 1) * new_w * 2) * 4 + 0] / 255.f * at;
                    g += source[((i_w * 2 + 1) + (i_h * 2 + 1) * new_w * 2) * 4 + 1] / 255.f * at;
                    b += source[((i_w * 2 + 1) + (i_h * 2 + 1) * new_w * 2) * 4 + 2] / 255.f * at;
                    a += at;

                    if (a <= 0.001f)
                        ((Uint32*)result)[i_w + i_h * new_w] = 0;
                    else
                    {
                        result[(i_w + i_h * new_w) * 4 + 0] = SDL_clamp(r * 255.f / a, 0, 0xFF);
                        result[(i_w + i_h * new_w) * 4 + 1] = SDL_clamp(g * 255.f / a, 0, 0xFF);
                        result[(i_w + i_h * new_w) * 4 + 2] = SDL_clamp(b * 255.f / a, 0, 0xFF);
                        result[(i_w + i_h * new_w) * 4 + 3] = SDL_clamp(a * 255.f / 4.f, 0, 0xFF);
                    }
                }
            }
        }
    }

    size_t min_area = 0;
    size_t min_height = 1;
    size_t min_width = 1;
    for (size_t i = 0; i < textures.size(); i++)
    {
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

    /* Actually create the atlases */
    for (int mip_lvl = 0; mip_lvl <= r_mipmap_levels.get(); mip_lvl++)
    {
        size_t atlas_height = tex_base_height >> mip_lvl;
        size_t atlas_width = tex_base_width >> mip_lvl;

        Uint8* data = (Uint8*)malloc(atlas_height * atlas_width * 4);

        for (size_t i_h = 0; i_h < atlas_height; i_h++)
        {
            const size_t i_h_remaining = i_h / 4 % 2;
            for (size_t i_w = 0; i_w < atlas_width; i_w++)
            {
                data[(i_h * atlas_width + i_w) * 4 + 0] = 0;
                data[(i_h * atlas_width + i_w) * 4 + 1] = 0xFF * (i_w / 4 % 2 == i_h_remaining);
                data[(i_h * atlas_width + i_w) * 4 + 2] = 0x00 * (i_w / 4 % 2 == i_h_remaining);
                data[(i_h * atlas_width + i_w) * 4 + 3] = 0xFF;
            }
        }

        for (size_t i = 0; i < rects.size(); i++)
        {
            if (!rects[i].was_packed)
                continue;
            textures[rects[i].id].x = rects[i].x;
            textures[rects[i].id].y = rects[i].y;
            texture_pre_pack_t t = textures[rects[i].id];
            t.x >>= mip_lvl;
            t.y >>= mip_lvl;
            t.w >>= mip_lvl;
            t.h >>= mip_lvl;

            for (int y = 0; y < t.h; y++)
            {
                if (t.data_mipmaped[mip_lvl].size())
                    memcpy(data + ((y + t.y) * atlas_width + t.x) * 4, t.data_mipmaped[mip_lvl].data() + y * t.w * 4, t.w * 4);
                else
                {
                    for (int i_w = t.x; i_w < t.w + t.x; i_w++)
                    {
                        data[((y + t.y) * atlas_width + i_w) * 4 + 0] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                        data[((y + t.y) * atlas_width + i_w) * 4 + 1] = 0x7F * (i_w / 8 % 2 == y / 8 % 2);
                        data[((y + t.y) * atlas_width + i_w) * 4 + 2] = 0x00;
                        data[((y + t.y) * atlas_width + i_w) * 4 + 3] = 0xFF;
                    }
                }
            }
        }
        raw_mipmaps.push_back(data);
    }

    for (size_t i = 0; i < textures.size(); i++)
    {
        mc_id::terrain_face_id_t fid = mc_id::get_face_from_fname(textures[i].name.c_str());
        if (!textures[i].is_item)
            texture_faces[fid] = texture_post_pack_t(textures[i], fid, tex_base_width, tex_base_height);

        if (textures[i].animated)
            anim_textures.push_back(textures[i]);

        if (textures[i].data_stbi)
        {
            stbi_image_free(textures[i].data_stbi);
            textures[i].data_stbi = NULL;
        }
    }

    SDL_GPUTextureCreateInfo cinfo_texture = {};
    cinfo_texture.type = SDL_GPU_TEXTURETYPE_2D;
    cinfo_texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    cinfo_texture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    cinfo_texture.width = Uint32(tex_base_width);
    cinfo_texture.height = Uint32(tex_base_height);
    cinfo_texture.layer_count_or_depth = 1;
    cinfo_texture.num_levels = Uint32(raw_mipmaps.size());
    SDL_GPUSamplerCreateInfo cinfo_sampler = {};
    cinfo_sampler.min_filter = cinfo_sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    cinfo_sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    cinfo_sampler.address_mode_u = cinfo_sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    cinfo_sampler.max_lod = raw_mipmaps.size() - 1;

    binding.texture = gpu::create_texture(cinfo_texture, "Terrain texture");
    binding.sampler = gpu::create_sampler(cinfo_sampler, "Terrain sampler");

    dc_log("Built terrain atlas in %.1f ms", (double)(SDL_GetTicksNS() - start_tick) / 1000000.0);

    if (r_dump_mipmaps_terrain.get())
        dump_mipmaps();
}

void texture_terrain_t::update(SDL_GPUCopyPass* copy_pass)
{
    Uint64 cur_sdl_tick = SDL_GetTicks();
    if (cur_sdl_tick - time_last_update < 45 || !copy_pass)
        return;
    time_last_update = cur_sdl_tick;

    /* Update flailing clocks/compasses */
    for (size_t i = 0; i < anim_textures.size(); i++)
    {
        const texture_pre_pack_t& t = anim_textures[i];

        if (t.is_item && t.name == "compass.png")
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
        else if (t.is_item && t.name == "clock.png")
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

    const double cur_mc_tick = cur_sdl_tick / 50.0;

    /* Actually update animations */
    for (size_t mip_lvl = 0; mip_lvl < raw_mipmaps.size(); mip_lvl++)
    {
        size_t atlas_width = tex_base_width >> mip_lvl;
        Uint8* data = raw_mipmaps[mip_lvl];
        for (size_t i = 0; i < anim_textures.size(); i++)
        {
            const texture_pre_pack_t& t = anim_textures[i];
            const int t_x = t.x >> mip_lvl;
            const int t_y = t.y >> mip_lvl;
            const int t_w = t.w >> mip_lvl;
            const int t_h = t.h >> mip_lvl;

            double frame_cur_f = SDL_fmod(cur_mc_tick / (double)t.frame_time, t.frame_offsets_len);

            if (t.is_item && t.name == "compass.png")
            {
                float rotation = compass_flail ? compass_flail_rotation : compass_rotation;
                frame_cur_f = ((((int)rotation) % 360 + 360) % 360) / 360.0;
                frame_cur_f *= (double)t.frame_offsets_len;
            }
            else if (t.is_item && t.name == "clock.png")
            {
                Uint64 time = clock_flail ? clock_flail_mc_time : clock_mc_time;
                frame_cur_f = (time % 24000) / (24000 / t.frame_offsets_len);
            }

            int frame_off_cur_i = frame_cur_f;
            int frame_off_cur_n = (frame_off_cur_i + 1) % t.frame_offsets_len;
            float frame_blend = frame_cur_f - frame_off_cur_i;
            frame_blend = SDL_clamp(frame_blend, 0.0f, 1.0f);

            int frame_cur_i = 0;
            if (t.frame_offsets.size())
                frame_cur_i = t.frame_offsets[frame_off_cur_i];
            else
                frame_cur_i = frame_off_cur_i;

            int frame_cur_n = 0;
            if (t.frame_offsets.size())
                frame_cur_n = t.frame_offsets[frame_off_cur_n];
            else
                frame_cur_n = frame_off_cur_n;

            if (frame_off_cur_n < 0 || (frame_off_cur_n + 1) > t.frame_offsets_len)
                continue;

            for (int y = 0; y < t_h; y++)
            {
                if (t.data_mipmaped[mip_lvl].size())
                {
                    if (!t.interpolate)
                        memcpy(data + ((y + t_y) * atlas_width + t_x) * 4, t.data_mipmaped[mip_lvl].data() + (y + frame_cur_i * t_h) * t_w * 4, t_w * 4);
                    else
                    {
                        int startx = ((y + t_y) * atlas_width + t_x) * 4;
                        for (int j = 0; j < t_w * 4; j++)
                        {
                            Uint8 col0 = t.data_mipmaped[mip_lvl][(y + frame_cur_i * t_h) * t_w * 4 + j] * (1.0 - frame_blend);
                            Uint8 col1 = t.data_mipmaped[mip_lvl][(y + frame_cur_n * t_h) * t_w * 4 + j] * frame_blend;
                            data[startx + j] = col0 + col1;
                        }
                    }
                }
                else
                {
                    for (int i_w = t_x; i_w < t_w + t_x; i_w++)
                    {
                        data[((y + t_y) * atlas_width + i_w) * 4 + 0] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                        data[((y + t_y) * atlas_width + i_w) * 4 + 1] = 0;
                        data[((y + t_y) * atlas_width + i_w) * 4 + 2] = 0xFF * (i_w / 8 % 2 == y / 8 % 2);
                        data[((y + t_y) * atlas_width + i_w) * 4 + 3] = 0xFF;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < raw_mipmaps.size(); i++)
        gpu::upload_to_texture2d(
            copy_pass, binding.texture, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 0, i, tex_base_width >> i, tex_base_height >> i, raw_mipmaps[i], false);
}

void texture_terrain_t::dump_mipmaps()
{
    if (!PHYSFS_mkdir("/game/screenshots"))
    {
        dc_log_error("Unable to create screenshots folder! (PhysFS err code: %d)", PHYSFS_getLastErrorCode());
        return;
    }

    stbi_flip_vertically_on_write(false);

    for (size_t i = 0; i < raw_mipmaps.size(); i++)
    {
        if (!raw_mipmaps[i])
            continue;
        char buf[64];
        snprintf(buf, SDL_arraysize(buf), "/game/screenshots/terrain_%zu.png", i);
        stbi_physfs_write_png(buf, tex_base_width >> i, tex_base_height >> i, 4, raw_mipmaps[i], (tex_base_width >> i) * 4);
    }
}

static bool DragUint64(const char* const label, Uint64* const v, const float v_speed = 1.0f, const Uint64 v_min = 0, const Uint64 v_max = 0,
    const char* const format = "%zu", const ImGuiSliderFlags flags = 0)
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

    ImTextureID tex_id_main(reinterpret_cast<ImTextureID>(&binding));

    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (imgui_selected_face == mc_id::FACE_ATLAS)
        ImGui::Image(tex_id_main, ImVec2(my_tex_w, my_tex_h), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
    else
    {
        texture_post_pack_t t = texture_faces[imgui_selected_face];
        ImVec2 corner0(t.face.corners[0].x, t.face.corners[0].y);
        ImVec2 corner1(t.face.corners[3].x, t.face.corners[3].y);
        ImGui::Image(tex_id_main, ImVec2(t.w * 4, t.h * 4), corner0, corner1);
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
        ImGui::Image(tex_id_main, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1);
        ImGui::EndTooltip();
    }

    ImGui::EndChild();
    ImGui::PopID();
    return true;
}

texture_terrain_t::~texture_terrain_t()
{
    gpu::release_texture(binding.texture);
    gpu::release_sampler(binding.sampler);

    for (size_t i = 0; i < raw_mipmaps.size(); i++)
        free(raw_mipmaps[i]);
}

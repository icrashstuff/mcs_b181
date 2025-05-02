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
#include "lightmap.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gpu/texture.h"
#include "migration_gl.h"
#include "state.h"

#define BASE_R 1.0
#define BASE_G 0.8
#define BASE_B 0.6

#include "tetra/util/convar.h"

static convar_float_t r_light_brightness("r_light_brightness", 0.0, 0.0, 1.0, "Lightmap base brightness", CONVAR_FLAG_SAVE);

SDL_FORCE_INLINE float curve(const float base, const float x) { return (glm::pow(base, x) - 1) / (base - 1); }

lightmap_t::lightmap_t(const lightmap_preset_t preset)
{
    r_state = SDL_rand_bits() | (Uint64(SDL_rand_bits()) << 32);

    last_flicker_tick = SDL_GetTicks() - ticks_per_flicker_tick * 2;

    set_preset(preset);
}

void lightmap_t::set_preset(const lightmap_preset_t preset)
{
    switch (preset)
    {
    case LIGHTMAP_PRESET_OVERWORLD:
    {
        color_block = { 1.0f, 0.85f, 0.7f };
        color_night = { 0.125f, 0.125f, 0.3f };
        color_day = { 1.0f, 0.975f, 0.95f };
        color_minimum = { 0.05f, 0.04f, 0.03f };
        break;
    }
    case LIGHTMAP_PRESET_NETHER:
    {
        color_block = { 1.0f, 0.702f, 0.6f };
        color_night = { 1.0f, 0.847f, 0.792f };
        color_day = color_night;
        color_minimum = { 0.286f, 0.243f, 0.208f };
        break;
    }
    case LIGHTMAP_PRESET_END:
    {
        color_block = { 1.0f, 0.9f, 0.8f };
        color_night = { 0.0f, 0.0f, 0.0f };
        color_day = { 0.0f, 0.0f, 0.0f };
        color_minimum = { 0.234375f, 0.296875f, 0.265625f };
        break;
    }
    }
}

lightmap_t::~lightmap_t()
{
    SDL_ReleaseGPUTexture(state::gpu_device, tex_id);
    SDL_ReleaseGPUSampler(state::gpu_device, sampler_linear);
    SDL_ReleaseGPUSampler(state::gpu_device, sampler_nearest);
}

SDL_FORCE_INLINE float mix_for_time_of_day(int time_of_day)
{
    float mix = 0.0f;
    mix = 3.5f * SDL_sinf(time_of_day * SDL_PI_F / 12000.0f);
    mix = SDL_clamp(mix, -1.0f, 1.0f);

    mix += 0.0625f * SDL_sinf(time_of_day * SDL_PI_F / 12000.0f);
    mix /= 1.046875f;

    mix = SDL_clamp(mix, -1.0f, 1.0f);

    mix = mix * 0.5f + 0.5f;

    return mix;
}

void lightmap_t::imgui_contents()
{
    ImGui::BeginGroup();

    ImGui::PlotLines("Raw Flicker Values", flicker_graph_values, SDL_arraysize(flicker_graph_values), flicker_graph_pos, NULL, -1.0f, 1.0f, ImVec2(0, 64));

    float mixes[6000];

    for (int i = 0; i < IM_ARRAYSIZE(mixes); i++)
        mixes[i] = mix_for_time_of_day((24000 / SDL_arraysize(mixes)) * i);

    ImGui::PlotLines("mix_for_time_of_day", mixes, SDL_arraysize(mixes), 0, NULL, 0.0f, 1.0f, ImVec2(0, 64));

    r_light_brightness.imgui_edit();

    ImGui::SliderFloat("Flicker Strength", &flicker_strength, 0.0f, 1.0f);
    ImGui::SliderFloat("Flicker Midpoint", &flicker_midpoint, 0.0f, 1.0f);

    if (ImGui::Button("Preset: Overworld"))
        set_preset(LIGHTMAP_PRESET_OVERWORLD);
    ImGui::SameLine();
    if (ImGui::Button("Preset: Nether"))
        set_preset(LIGHTMAP_PRESET_NETHER);
    ImGui::SameLine();
    if (ImGui::Button("Preset: End"))
        set_preset(LIGHTMAP_PRESET_END);

    ImGui::ColorEdit3("Color: Block", glm::value_ptr(color_block));
    ImGui::ColorEdit3("Color: Day", glm::value_ptr(color_day));
    ImGui::ColorEdit3("Color: Night", glm::value_ptr(color_night));
    ImGui::ColorEdit3("Color: Minimum", glm::value_ptr(color_minimum));

    Uint64 flicker_ticks_min = 1;
    Uint64 flicker_ticks_max = 50;
    ImGui::SliderScalar("SDL ticks/flicker ticks", ImGuiDataType_U64, &ticks_per_flicker_tick, &flicker_ticks_min, &flicker_ticks_max);

    ImGui::SliderInt("Time override", &time_of_day_override, -1, 24000);

    float tex_size = ImGui::GetContentRegionAvail().x / 2.2f;

    SDL_GPUTextureSamplerBinding binding_linear = { .texture = tex_id, .sampler = sampler_linear };
    SDL_GPUTextureSamplerBinding binding_nearest = { .texture = tex_id, .sampler = sampler_nearest };

    if (!binding_linear.texture)
        binding_linear.texture = state::gpu_debug_texture;
    if (!binding_nearest.texture)
        binding_nearest.texture = state::gpu_debug_texture;

    if (!binding_linear.sampler)
        binding_linear.sampler = state::gpu_debug_sampler;
    if (!binding_nearest.sampler)
        binding_nearest.sampler = state::gpu_debug_sampler;

    ImGui::Image(reinterpret_cast<ImTextureID>(&binding_linear), ImVec2(tex_size, tex_size), ImVec2(0, 0), ImVec2(1, 1));
    ImGui::SameLine();
    ImGui::Image(reinterpret_cast<ImTextureID>(&binding_nearest), ImVec2(tex_size, tex_size), ImVec2(0, 0), ImVec2(1, 1));

    ImGui::EndGroup();
}

void lightmap_t::update(SDL_GPUCopyPass* const copy_pass)
{
    Uint64 sdl_cur_tick = SDL_GetTicks();
    if (sdl_cur_tick - last_flicker_tick < ticks_per_flicker_tick)
        return;

    for (; sdl_cur_tick - last_flicker_tick >= ticks_per_flicker_tick && ticks_per_flicker_tick; last_flicker_tick += ticks_per_flicker_tick)
    {
        float a = (SDL_randf_r(&r_state) - 0.5f) * 0.0625;
        if (flicker_value > 1.0f && a > 0.0f)
            a = -a;
        if (flicker_value < -1.0f && a < 0.0f)
            a = -a;
        flicker_value += a;

        flicker_graph_pos = (flicker_graph_pos + 1) % SDL_arraysize(flicker_graph_values);

        flicker_graph_values[flicker_graph_pos] = flicker_value;
    }

    dat.resize(width * height * 4);

    int time_of_day = mc_time % 24000;

    if (time_of_day_override > -1)
        time_of_day = time_of_day_override;

    float mix = mix_for_time_of_day(time_of_day);

    float flick = flicker_value * SDL_fabsf(flicker_value) * flicker_strength * 0.5f + flicker_midpoint;

    float gamma = r_light_brightness.get();

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const float u = x / float(width - 1);
            const float v = y / float(height - 1);

            /* Block Light */
            float r = curve(8.0f, u * 1.2f) * color_block.r * flick;
            float g = curve(8.0f, u * 1.2f) * color_block.g * flick;
            float b = curve(8.0f, u * 1.2f) * color_block.b * flick;

            r += curve(32, v) * glm::mix(color_night.r, color_day.r, mix);
            g += curve(32, v) * glm::mix(color_night.g, color_day.g, mix);
            b += curve(32, v) * glm::mix(color_night.b, color_day.b, mix);

            r = (r + color_minimum.r) / (1.0f + color_minimum.r);
            g = (g + color_minimum.g) / (1.0f + color_minimum.g);
            b = (b + color_minimum.b) / (1.0f + color_minimum.b);

            r = (r + gamma) / (1.0f + gamma);
            g = (g + gamma) / (1.0f + gamma);
            b = (b + gamma) / (1.0f + gamma);

            dat[(y * width + x) * 4 + 0] = SDL_clamp(r, 0.0f, 1.0f) * 255;
            dat[(y * width + x) * 4 + 1] = SDL_clamp(g, 0.0f, 1.0f) * 255;
            dat[(y * width + x) * 4 + 2] = SDL_clamp(b, 0.0f, 1.0f) * 255;
            dat[(y * width + x) * 4 + 3] = 255;
        }
    }

    if (!tex_id)
    {
        SDL_GPUTextureCreateInfo cinfo_tex = {
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = 16,
            .height = 16,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = SDL_GPU_SAMPLECOUNT_1,

            .props = SDL_CreateProperties(),
        };

        SDL_SetStringProperty(cinfo_tex.props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, "[Level]: Lightmap");

        tex_id = SDL_CreateGPUTexture(state::gpu_device, &cinfo_tex);

        SDL_DestroyProperties(cinfo_tex.props);
    }

    if (!sampler_linear || !sampler_nearest)
    {
        SDL_GPUSamplerCreateInfo cinfo_sampler = {
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .mip_lod_bias = 0.0f,
            .max_anisotropy = 0.0f,
            .compare_op = SDL_GPU_COMPAREOP_INVALID,
            .min_lod = 0,
            .max_lod = 0,
            .enable_anisotropy = 0,
            .enable_compare = 0,
        };

        cinfo_sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
        if (!sampler_linear)
            sampler_linear = SDL_CreateGPUSampler(state::gpu_device, &cinfo_sampler);
        cinfo_sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
        if (!sampler_nearest)
            sampler_nearest = SDL_CreateGPUSampler(state::gpu_device, &cinfo_sampler);
    }

    gpu::upload_to_texture2d(copy_pass, tex_id, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 0, 0, width, height, dat.data(), 1);
}

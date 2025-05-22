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

#include "tetra/util/convar.h"

static convar_float_t r_lightmap_gamma {
    "r_light_gamma",
    2.2,
    0.1,
    10.0,
    "Lightmap gamma (y) value\n"
    "If confused, read: https://learnopengl.com/Advanced-Lighting/Gamma-Correction\n"
    "And/or read: https://blog.johnnovak.net/2016/09/21/what-every-coder-should-know-about-gamma/",
    CONVAR_FLAG_SAVE,
};

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

lightmap_t::~lightmap_t() { }

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

float lightmap_t::get_mix_for_time(const Sint64 mc_time) { return mix_for_time_of_day(mc_time % 24000); }

void lightmap_t::imgui_contents()
{
    ImGui::BeginGroup();

    ImGui::PlotLines("Raw Flicker Values", flicker_graph_values, SDL_arraysize(flicker_graph_values), flicker_graph_pos, NULL, -1.0f, 1.0f, ImVec2(0, 64));

    float mixes[6000];

    for (int i = 0; i < IM_ARRAYSIZE(mixes); i++)
        mixes[i] = mix_for_time_of_day((24000 / SDL_arraysize(mixes)) * i);

    ImGui::PlotLines("mix_for_time_of_day", mixes, SDL_arraysize(mixes), 0, NULL, 0.0f, 1.0f, ImVec2(0, 64));

    r_lightmap_gamma.imgui_edit();

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

    ImGui::EndGroup();
}

void lightmap_t::update()
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

    int time_of_day = mc_time % 24000;

    if (time_of_day_override > -1)
        time_of_day = time_of_day_override;

    uniform.light_flicker = glm::vec3(flicker_value * SDL_fabsf(flicker_value) * flicker_strength * 0.5f + flicker_midpoint);
    uniform.sky_color = glm::mix(color_night, color_day, mix_for_time_of_day(time_of_day));
    uniform.block_color = color_block;
    uniform.minimum_color = color_minimum;
    uniform.gamma = r_lightmap_gamma.get();
}

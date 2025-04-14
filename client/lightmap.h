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
#ifndef CLIENT_IGHTMAP_H_INCLUDED
#define CLIENT_IGHTMAP_H_INCLUDED

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vector>

class lightmap_t
{
public:
    enum lightmap_preset_t
    {
        LIGHTMAP_PRESET_OVERWORLD,
        LIGHTMAP_PRESET_NETHER,
        LIGHTMAP_PRESET_END,
    };
    lightmap_t(const lightmap_preset_t preset = LIGHTMAP_PRESET_OVERWORLD);

    ~lightmap_t();

    void set_world_time(const Uint64 _mc_time) { mc_time = _mc_time; };

    void set_preset(const lightmap_preset_t preset);

    /**
     * Updates and uploads light map
     */
    void update(SDL_GPUCopyPass* const copy_pass);

    void imgui_contents();

    SDL_GPUTexture* tex_id = nullptr;

    SDL_GPUSampler* sampler_linear = nullptr;
    SDL_GPUSampler* sampler_nearest = nullptr;

private:
    float flicker_strength = 0.25f;
    /**
     * Value from [-1.0, 1.0]
     */
    float flicker_value = 0.0f;

    float flicker_midpoint = 0.9f;

    Uint64 last_flicker_tick = 0;
    Uint64 ticks_per_flicker_tick = 5;
    Uint64 r_state = 0;
    Uint64 mc_time = 0;

    int time_of_day_override = -1;

    int width = 16;
    int height = 16;

    glm::vec3 color_block = { 0.0f, 0.0f, 0.0f };
    glm::vec3 color_night = { 0.0f, 0.0f, 0.0f };
    glm::vec3 color_day = { 0.0f, 0.0f, 0.0f };
    glm::vec3 color_minimum = { 0.0f, 0.0f, 0.0f };

    std::vector<Uint8> dat;

    float flicker_graph_values[512] = {};
    int flicker_graph_pos = 0;
};

#endif

#version 450 core
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
/* ================ BEGIN Fragment inputs ================ */
layout(location = 0) in struct
{
    vec4 color;
    vec2 uv;
    float ao;
    float light_block;
    float light_sky;
} frag;
/* ================ END Fragment inputs ================ */

/* ================ BEGIN Fragment outputs ================ */
layout(location = 0) out vec4 out_color;
/* ================ END Fragment outputs ================ */

/**
 * Modified excerpt from SDL_gpu.h about resource bindings
 *
 * Fragment shaders:
 * - set=2: Sampled textures, followed by storage textures, followed by storage buffers
 * - set=3: Uniform buffers
 */

layout(set = 2, binding = 0) uniform sampler2D tex_atlas;
layout(set = 2, binding = 1) uniform sampler2D tex_lightmap;

const int ao_algorithm = 1;

layout(std140, set = 3, binding = 0) uniform ubo_frag_t
{
    bool use_texture;
}
ubo_frag;

void main()
{
    out_color = frag.color;

    if (ubo_frag.use_texture)
        out_color *= texture(tex_atlas, frag.uv, -1.0);
    else
        out_color.a *= texture(tex_atlas, frag.uv, -1.0).a;

    /* TODO-OPT: Remove this? */
    if (out_color.a < 0.0625)
        discard;

    switch (ao_algorithm)
    {
    case 0:
        break;
    case 1:
        out_color.xyz *= 1.0 - frag.ao / 10.0;
    case 2:
        out_color.xyz *= clamp(1.1 - (-0.1 / (frag.ao - 1.25)), 0.0, 1.0);
        break;
    case 3:
        out_color.xyz *= clamp(1.15 - (-0.2 / (frag.ao - 1.25)), 0.0, 1.0);
        break;
    case 4:
        out_color.xyz *= (1.0 - (pow(64.0, frag.ao / 2.0) - 1) / 64.0);
        break;
    case 5:
        out_color.xyz *= (1.0 - (pow(64.0, frag.ao / 1.5) - 1) / 64.0);
        break;
    default:
        break;
    }

    out_color *= texture(tex_lightmap, vec2(frag.light_block, frag.light_sky));
}

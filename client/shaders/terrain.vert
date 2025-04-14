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
/* ================ BEGIN Vertex inputs ================ */
/**
 * X:  [0....9]
 * Y:  [10..19]
 * Z:  [20..29]
 * AO: [30..31]
 */
layout(location = 0) in uint vtx_pos_ao;
/**
 * R: [0....7]
 * G: [8...15]
 * B: [16..23]
 * Light Block: [24..27]
 * Light Sky:   [28..31]
 */
layout(location = 1) in uint vtx_coloring;
/**
 * Max atlas size of 32767^2
 *
 * U: [0...15] / (2^15 + 1)
 * V: [16..31] / (2^15 + 1)
 */
layout(location = 2) in uint vtx_texturing;
/* ================ END Vertex inputs ================ */

/* ================ BEGIN Vertex outputs ================ */
layout(location = 0) out struct
{
    vec4 color;
    vec2 uv;
    float ao;
    float light_block;
    float light_sky;
} frag;
/* ================ END Vertex outputs ================ */

/**
 * Modified excerpt from SDL_gpu.h about resource bindings
 *
 * Vertex shaders:
 * - set=0: Sampled textures, followed by storage textures, followed by storage buffers
 * - set=1: Uniform buffers
 */

layout(std140, set = 1, binding = 0) uniform ubo_world_t
{
    mat4 camera;
    mat4 projection;
}
ubo_world;

layout(std140, set = 1, binding = 1) uniform ubo_tint_t
{
    vec4 tint;
}
ubo_tint;

layout(std140, set = 1, binding = 2) uniform ubo_model_t
{
    vec4 model;
}
ubo_model;

void main()
{
    vec3 pos;
    pos.x = float(int((vtx_pos_ao      ) & 1023u) - 256) / 32.0;
    pos.y = float(int((vtx_pos_ao >> 10) & 1023u) - 256) / 32.0;
    pos.z = float(int((vtx_pos_ao >> 20) & 1023u) - 256) / 32.0;
    gl_Position = ubo_world.projection * ubo_world.camera * (vec4(pos, 1.0) + ubo_model.model);

    frag.ao = float((vtx_pos_ao >> 30) & 3u) / 3.0;

    frag.color.r = ubo_tint.tint.r * float((vtx_coloring      ) & 255u) / 255.0;
    frag.color.g = ubo_tint.tint.g * float((vtx_coloring >>  8) & 255u) / 255.0;
    frag.color.b = ubo_tint.tint.b * float((vtx_coloring >> 16) & 255u) / 255.0;
    frag.color.a = ubo_tint.tint.a;

    frag.light_block = float((vtx_coloring >> 24) & 15u) / 15.0;
    frag.light_sky   = float((vtx_coloring >> 28) & 15u) / 15.0;

    float u = float((vtx_texturing      ) & 65535u) / 32768.0;
    float v = float((vtx_texturing >> 16) & 65535u) / 32768.0;
    frag.uv = vec2(u, v);
}

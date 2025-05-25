#version 460 core
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
struct vertex_t
{
    /**
     * X:  [0....9]
     * Y:  [10..19]
     * Z:  [20..29]
     * AO: [30..31]
     */
    uint pos_ao;
    /**
     * R: [0....7]
     * G: [8...15]
     * B: [16..23]
     * Light Block: [24..27]
     * Light Sky:   [28..31]
     */
    uint coloring;
    /**
     * Max atlas size of 32767^2
     *
     * U: [0...15] / (2^15 + 1)
     * V: [16..31] / (2^15 + 1)
     */
    uint texturing;
};

layout(std430, set = 0, binding = 0) readonly buffer vertex_data_t { vertex_t data[]; }
vtx_data;

layout(std430, set = 0, binding = 1) readonly buffer draw_pos_t { ivec4 pos[]; }
draw_pos;
/* ================ END Vertex inputs ================ */

/* ================ BEGIN Vertex outputs ================ */
layout(location = 0) out struct
{
    vec4 color;
    vec2 uv;
    float ao;
    float light_block;
    float light_sky;
    float fog_dist;
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

layout(std140, set = 1, binding = 1) uniform ubo_tint_t { vec4 tint; }
ubo_tint;

void main()
{
    uint idx_vtx = gl_VertexIndex & 3;
    uint idx_draw = gl_VertexIndex >> 2;

    vertex_t vtx = vtx_data.data[gl_InstanceIndex * 4 + idx_vtx];

    vec3 pos;
    pos.x = float(int(bitfieldExtract(vtx.pos_ao,  0, 10)) - 256 + draw_pos.pos[idx_draw].x * 512) / 32.0;
    pos.y = float(int(bitfieldExtract(vtx.pos_ao, 10, 10)) - 256 + draw_pos.pos[idx_draw].y * 512) / 32.0;
    pos.z = float(int(bitfieldExtract(vtx.pos_ao, 20, 10)) - 256 + draw_pos.pos[idx_draw].z * 512) / 32.0;
    vec4 camera_space_pos = ubo_world.camera * vec4(pos, 1.0);

    frag.fog_dist = length(camera_space_pos.xyz);

    gl_Position = ubo_world.projection * camera_space_pos;

    frag.ao = float(bitfieldExtract(vtx.pos_ao, 30, 2)) / 3.0;

    frag.color.r = ubo_tint.tint.r * float(bitfieldExtract(vtx.coloring, 0,  8)) / 255.0;
    frag.color.g = ubo_tint.tint.g * float(bitfieldExtract(vtx.coloring, 8,  8)) / 255.0;
    frag.color.b = ubo_tint.tint.b * float(bitfieldExtract(vtx.coloring, 16, 8)) / 255.0;
    frag.color.a = ubo_tint.tint.a;

    frag.light_block = float(bitfieldExtract(vtx.coloring, 24, 4)) / 15.0;
    frag.light_sky   = float(bitfieldExtract(vtx.coloring, 28, 4)) / 15.0;

    float u = float(bitfieldExtract(vtx.texturing,  0, 16)) / 32768.0;
    float v = float(bitfieldExtract(vtx.texturing, 16, 16)) / 32768.0;
    frag.uv = vec2(u, v);
}

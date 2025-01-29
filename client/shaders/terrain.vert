#version 330 core
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
/**
 * X:  [0....8]
 * Y:  [9...17]
 * Z:  [18..26]
 * AO: [27..29]
 * Unused: [30..31]
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

out vec3 frag_color;
out vec2 frag_uv;
out float frag_ao;
out float frag_light_block;
out float frag_light_sky;

uniform mat4 model;
uniform mat4 camera;
uniform mat4 projection;

uniform vec4 tint = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    vec3 pos;
    pos.x = float(int((vtx_pos_ao      ) & 511u ) - 128) / 16.0;
    pos.y = float(int((vtx_pos_ao >>  9) & 511u ) - 128) / 16.0;
    pos.z = float(int((vtx_pos_ao >> 18) & 511u ) - 128) / 16.0;
    vec4 position = camera * model * vec4(pos, 1.0);
    gl_Position = projection * position;

    frag_ao = float((vtx_pos_ao >> 27) & 3u) / 3.0;

    frag_color.r = tint.r * float((vtx_coloring) & 255u) / 255.0;
    frag_color.g = tint.g * float((vtx_coloring >> 8) & 255u) / 255.0;
    frag_color.b = tint.b * float((vtx_coloring >> 16) & 255u) / 255.0;

    frag_light_block = float((vtx_coloring >> 24) & 15u) / 15.0;
    frag_light_sky = float((vtx_coloring >> 28) & 15u) / 15.0;

    float u = float((vtx_texturing) & 65535u) / 32768.0;
    float v = float((vtx_texturing >> 16) & 65535u) / 32768.0;
    frag_uv = vec2(u, v);
}

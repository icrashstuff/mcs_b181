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
/* ================ BEGIN Fragment outputs ================ */
layout(location = 0) out vec4 out_color;
/* ================ END Fragment outputs ================ */

/* ================ BEGIN Fragment inputs ================ */
layout(location = 0) flat in vec3 camera_pos;
layout(location = 1) in vec3 cloud_pos;
/* ================ END Fragment inputs ================ */

/**
 * Modified excerpt from SDL_gpu.h about resource bindings
 *
 * Fragment shaders:
 * - set=2: Sampled textures, followed by storage textures, followed by storage buffers
 * - set=3: Uniform buffers
 */

layout(set = 2, binding = 0) uniform sampler2D tex_cloud;

layout(std140, set = 3, binding = 0) uniform ubo_frag_t
{
    vec4 fog_color;
    /* Not used in this shader */
    uint use_texture;
    float fog_min;
    float fog_max;
} ubo_frag;

layout(std140, set = 3, binding = 1) uniform ubo_cloud_frag_t
{
    vec4 color;
    /* U (as in UV) offset for cloud texture, expected range is [0.0, 1.0) */
    float cloud_offset;
} ubo_cloud_frag;

void main()
{
    vec2 inv_tsize = vec2(1.0 / 16.0) / vec2(textureSize(tex_cloud, 0));

    out_color = texture(tex_cloud, cloud_pos.xz * inv_tsize + vec2(ubo_cloud_frag.cloud_offset, 0.0), 0) * ubo_cloud_frag.color;

    if(out_color.a < 0.5)
        discard;

    /* Simple distance fog */
    float fog_factor = smoothstep(ubo_frag.fog_max, ubo_frag.fog_max * 1.5, distance(camera_pos.xz, cloud_pos.xz));
    out_color.xyz = mix(out_color.rgb, ubo_frag.fog_color.rgb, fog_factor);
}

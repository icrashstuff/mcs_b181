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
layout(location = 0) in vec2 fragcoords;
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

layout(std140, set = 3, binding = 0) uniform ubo_lighting_t
{
    vec4 light_color; /* vec4 for alignment reasons */
    vec2 uv_size;
    vec2 cursor_pos;
    float ambient_brightness;
}
ubo_lighting;

layout(set = 2, binding = 0) uniform sampler2D tex_base;
layout(set = 2, binding = 1) uniform sampler2D tex_normal;

void main()
{
    vec2 texcoords = ubo_lighting.uv_size * fragcoords;
    vec3 color = texture(tex_base, texcoords).rgb;
    vec3 normal = normalize(texture(tex_normal, texcoords).rgb * 2.0 - 1.0);
    
    /* Ambient */
    vec3 ambient = ubo_lighting.ambient_brightness * color;

    /* Diffuse */
#define USE_CURSOR 0
#if USE_CURSOR
    vec2 cursor_pos = vec2(ubo_lighting.cursor_pos.x, 1.0 - ubo_lighting.cursor_pos.y);
    vec3 light_dir = normalize(vec3(cursor_pos * ubo_lighting.uv_size, 2) - vec3(fragcoords * ubo_lighting.uv_size, 0.0));
#else
    vec3 light_dir = normalize(vec3(0.5, 0.5, 0.25) - vec3(fragcoords, 0.0));
#endif
    float lambert = dot(light_dir, normal);
    lambert = pow(lambert, 1.0/2.2); /* Gamma correction, dunno if this is "correct", but it looks nicer */
    vec3 diffuse = max(lambert, 0.0) * color * ubo_lighting.light_color.xyz;
    
    out_color = vec4(ambient + diffuse, 1.0);
}

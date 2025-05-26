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

/* ================ BEGIN Vertex outputs ================ */
layout(location = 0) flat out vec3 out_camera_pos;
layout(location = 1) out vec3 out_cloud_pos;
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

layout(std140, set = 1, binding = 1) uniform ubo_cloud_vert_t
{
    vec4 camera_pos;
    float height;
    float render_distance;
}
ubo_cloud_vert;

void main()
{
    vec3 pos = vec3(ubo_cloud_vert.render_distance * 16.0 * 1.6, ubo_cloud_vert.height, ubo_cloud_vert.render_distance * 16.0 * 1.6);

    if(bool(bitfieldExtract(gl_VertexIndex, 0, 1)))
        pos.x *= -1.0;

    if(bool(bitfieldExtract(gl_VertexIndex, 1, 1)))
        pos.z *= -1.0;

    pos.xz += ubo_cloud_vert.camera_pos.xz;

    out_cloud_pos = pos;
    out_camera_pos = ubo_cloud_vert.camera_pos.xyz;

    gl_Position = ubo_world.projection * ubo_world.camera * vec4(pos, 1.0);
}

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
#ifndef MCS_B181__CLIENT__SHADERS__TERRAIN_SHADER
#define MCS_B181__CLIENT__SHADERS__TERRAIN_SHADER

#include <SDL3/SDL_gpu.h>

namespace state
{
extern SDL_GPUGraphicsPipeline* pipeline_shader_terrain_opaque_no_alpha;
extern SDL_GPUGraphicsPipeline* pipeline_shader_terrain_opaque_alpha_test;
extern SDL_GPUGraphicsPipeline* pipeline_shader_terrain_overlay;
extern SDL_GPUGraphicsPipeline* pipeline_shader_terrain_depth_peel_0;
extern SDL_GPUGraphicsPipeline* pipeline_shader_terrain_depth_peel_n;
void init_terrain_pipelines();
void destroy_terrain_pipelines();
};
#endif

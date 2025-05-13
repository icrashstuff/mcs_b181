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
#include "background.frag.msl.h"
#include "background.frag.spv.h"
#include "background.vert.msl.h"
#include "background.vert.spv.h"

#include "../state.h"
#include "background_shader.h"
#include "client/gpu/pipeline.h"
#include "tetra/log.h"
#include <SDL3/SDL.h>

SDL_GPUGraphicsPipeline* state::pipeline_background = nullptr;

static SDL_GPUShader* shader_vert = nullptr;
static SDL_GPUShader* shader_frag = nullptr;

#include <stdio.h>
void state::init_background_pipelines()
{
    destroy_background_pipelines();
    SDL_GPUShaderCreateInfo cinfo_shader_vert = {};
    cinfo_shader_vert.entrypoint = "main";
    cinfo_shader_vert.stage = SDL_GPU_SHADERSTAGE_VERTEX;

    SDL_GPUShaderCreateInfo cinfo_shader_frag = {};
    cinfo_shader_frag.entrypoint = "main";
    cinfo_shader_frag.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    cinfo_shader_frag.num_uniform_buffers = 1;
    cinfo_shader_frag.num_samplers = 2;

    SDL_GPUShaderFormat formats = gpu::get_shader_formats();
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        cinfo_shader_vert.code = background_vert_spv;
        cinfo_shader_vert.code_size = background_vert_spv_len;
        cinfo_shader_vert.format = SDL_GPU_SHADERFORMAT_SPIRV;

        cinfo_shader_frag.code = background_frag_spv;
        cinfo_shader_frag.code_size = background_frag_spv_len;
        cinfo_shader_frag.format = SDL_GPU_SHADERFORMAT_SPIRV;
    }
    else if (formats & SDL_GPU_SHADERFORMAT_MSL)
    {
        cinfo_shader_vert.entrypoint = "main0";
        cinfo_shader_vert.code = background_vert_msl;
        cinfo_shader_vert.code_size = background_vert_msl_len;
        cinfo_shader_vert.format = SDL_GPU_SHADERFORMAT_MSL;

        cinfo_shader_frag.entrypoint = "main0";
        cinfo_shader_frag.code = background_frag_msl;
        cinfo_shader_frag.code_size = background_frag_msl_len;
        cinfo_shader_frag.format = SDL_GPU_SHADERFORMAT_MSL;
    }

    if (!(shader_vert = gpu::create(cinfo_shader_vert, "Menu background shader (vert)")))
        return;

    if (!(shader_frag = gpu::create(cinfo_shader_frag, "Menu background shader (frag)")))
        return;

    SDL_GPUVertexAttribute vertex_attributes = {};

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions = {};

    SDL_GPUVertexInputState vertex_input_state = {};
    vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_descriptions;
    vertex_input_state.num_vertex_buffers = 0;
    vertex_input_state.vertex_attributes = &vertex_attributes;
    vertex_input_state.num_vertex_attributes = 0;

    SDL_GPURasterizerState rasterizer_state = {};
    rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUColorTargetDescription color_target_desc[1] = {};
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(state::gpu_device, state::window);

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.color_target_descriptions = color_target_desc;
    target_info.num_color_targets = SDL_arraysize(color_target_desc);
    target_info.depth_stencil_format = state::gpu_tex_format_best_depth_only;
    target_info.has_depth_stencil_target = 1;

    SDL_GPUGraphicsPipelineCreateInfo cinfo_pipeline = {};
    cinfo_pipeline.vertex_shader = shader_vert;
    cinfo_pipeline.fragment_shader = shader_frag;
    cinfo_pipeline.vertex_input_state = vertex_input_state;
    cinfo_pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    cinfo_pipeline.rasterizer_state = rasterizer_state;
    cinfo_pipeline.target_info = target_info;

    pipeline_background = gpu::create(cinfo_pipeline, "Menu background pipeline (opaque)");
}

void state::destroy_background_pipelines()
{
    gpu::release(pipeline_background);
    gpu::release(shader_vert);
    gpu::release(shader_frag);
}

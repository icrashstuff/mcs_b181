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
#include "compiled/terrain.frag.alpha_test.msl.h"
#include "compiled/terrain.frag.alpha_test.smolv.h"
#include "compiled/terrain.frag.depth_peel_0.msl.h"
#include "compiled/terrain.frag.depth_peel_0.smolv.h"
#include "compiled/terrain.frag.depth_peel_n.msl.h"
#include "compiled/terrain.frag.depth_peel_n.smolv.h"
#include "compiled/terrain.frag.opaque.msl.h"
#include "compiled/terrain.frag.opaque.smolv.h"
#include "compiled/terrain.vert.msl.h"
#include "compiled/terrain.vert.smolv.h"

#include "client/gpu/pipeline.h"
#include "client/state.h"
#include "terrain_shader.h"
#include "tetra/log.h"
#include <SDL3/SDL.h>

SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_opaque_no_alpha = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_opaque_alpha_test = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_overlay = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_depth_peel_0 = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_depth_peel_n = nullptr;

#define SHADERS_DO_ACTION(ACTION)        \
    ACTION(terrain, vert, )              \
    ACTION(terrain, frag, _opaque)       \
    ACTION(terrain, frag, _alpha_test)   \
    ACTION(terrain, frag, _depth_peel_0) \
    ACTION(terrain, frag, _depth_peel_n)

#define DECLARE_SHADERS(base, stage, suffix) static SDL_GPUShader* shader_##stage##suffix = nullptr;

#define ASSIGN_SHADER_CODE(base, stage, suffix)                                      \
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV)                                        \
    {                                                                                \
        cinfo_shader_##stage##suffix.code = base##_##stage##suffix##_smolv;          \
        cinfo_shader_##stage##suffix.code_size = base##_##stage##suffix##_smolv_len; \
        cinfo_shader_##stage##suffix.format = SDL_GPU_SHADERFORMAT_SPIRV;            \
    }                                                                                \
    else if (formats & SDL_GPU_SHADERFORMAT_MSL)                                     \
    {                                                                                \
        cinfo_shader_##stage##suffix.entrypoint = "main0";                           \
        cinfo_shader_##stage##suffix.code = base##_##stage##suffix##_msl;            \
        cinfo_shader_##stage##suffix.code_size = base##_##stage##suffix##_msl_len;   \
        cinfo_shader_##stage##suffix.format = SDL_GPU_SHADERFORMAT_MSL;              \
    }

#define COMPILE_SHADER_CODE(base, stage, suffix)                                                                              \
    if (!(shader_##stage##suffix = gpu::create(cinfo_shader_##stage##suffix, "%s shader (%s) (%s)", #base, #stage, #suffix))) \
    {                                                                                                                         \
        dc_log_error("Failed to compile");                                                                                    \
        return;                                                                                                               \
    }

#define RELEASE_SHADERS(base, stage, suffix) gpu::release(shader_##stage##suffix);

SHADERS_DO_ACTION(DECLARE_SHADERS);

void state::init_terrain_pipelines()
{
    destroy_terrain_pipelines();
    SDL_GPUShaderCreateInfo cinfo_shader_vert = {};
    cinfo_shader_vert.entrypoint = "main";
    cinfo_shader_vert.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    cinfo_shader_vert.num_storage_buffers = 2;
    cinfo_shader_vert.num_uniform_buffers = 2;

    SDL_GPUShaderCreateInfo cinfo_shader_frag_alpha_test = {};
    cinfo_shader_frag_alpha_test.entrypoint = "main";
    cinfo_shader_frag_alpha_test.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    cinfo_shader_frag_alpha_test.num_samplers = 1;
    cinfo_shader_frag_alpha_test.num_uniform_buffers = 2;
    SDL_GPUShaderCreateInfo cinfo_shader_frag_opaque = cinfo_shader_frag_alpha_test;
    SDL_GPUShaderCreateInfo cinfo_shader_frag_depth_peel_0 = cinfo_shader_frag_alpha_test;
    SDL_GPUShaderCreateInfo cinfo_shader_frag_depth_peel_n = cinfo_shader_frag_alpha_test;

    cinfo_shader_frag_depth_peel_n.num_samplers++;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(state::gpu_device);

    SHADERS_DO_ACTION(ASSIGN_SHADER_CODE);
    SHADERS_DO_ACTION(COMPILE_SHADER_CODE);

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
    rasterizer_state.depth_bias_constant_factor = 0;
    rasterizer_state.depth_bias_clamp = 0;
    rasterizer_state.depth_bias_slope_factor = 0;
    rasterizer_state.enable_depth_bias = 0;
    rasterizer_state.enable_depth_clip = 0;

    SDL_GPUMultisampleState multisample_state = {};
    multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    multisample_state.sample_mask = 0;
    multisample_state.enable_mask = 0;

    SDL_GPUDepthStencilState depth_stencil_state = {};
    depth_stencil_state.enable_depth_test = 1;
    depth_stencil_state.enable_depth_write = 1;
    depth_stencil_state.enable_stencil_test = 0;

    SDL_GPUColorTargetDescription color_target_desc[1] = {};
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(state::gpu_device, state::window);
    color_target_desc[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target_desc[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_desc[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target_desc[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_target_desc[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_desc[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target_desc[0].blend_state.color_write_mask
        = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    color_target_desc[0].blend_state.enable_blend = 0;
    color_target_desc[0].blend_state.enable_color_write_mask = 0;

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.color_target_descriptions = color_target_desc;
    target_info.num_color_targets = SDL_arraysize(color_target_desc);
    target_info.depth_stencil_format = state::gpu_tex_format_best_depth_only;
    target_info.has_depth_stencil_target = 1;

    SDL_GPUGraphicsPipelineCreateInfo cinfo_pipeline = {};
    cinfo_pipeline.vertex_shader = shader_vert;
    cinfo_pipeline.fragment_shader = shader_frag_alpha_test;
    cinfo_pipeline.vertex_input_state = vertex_input_state;
    cinfo_pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    cinfo_pipeline.rasterizer_state = rasterizer_state;
    cinfo_pipeline.multisample_state = multisample_state;
    cinfo_pipeline.depth_stencil_state = depth_stencil_state;
    cinfo_pipeline.target_info = target_info;

    cinfo_pipeline.fragment_shader = shader_frag_opaque;
    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 1;
    pipeline_shader_terrain_opaque_no_alpha = gpu::create(cinfo_pipeline, "Terrain pipeline (opaque no-alpha test)");

    cinfo_pipeline.fragment_shader = shader_frag_alpha_test;
    pipeline_shader_terrain_opaque_alpha_test = gpu::create(cinfo_pipeline, "Terrain pipeline (opaque alpha-test)");

    cinfo_pipeline.fragment_shader = shader_frag_alpha_test;
    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 0;
    pipeline_shader_terrain_overlay = gpu::create(cinfo_pipeline, "Terrain pipeline (overlay)");

    cinfo_pipeline.fragment_shader = shader_frag_depth_peel_0;
    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 1;
    pipeline_shader_terrain_depth_peel_0 = gpu::create(cinfo_pipeline, "Terrain pipeline (depth peel layer 0)");

    cinfo_pipeline.fragment_shader = shader_frag_depth_peel_n;
    pipeline_shader_terrain_depth_peel_n = gpu::create(cinfo_pipeline, "Terrain pipeline (depth peel layers 1...n)");
}

void state::destroy_terrain_pipelines()
{
    gpu::release(pipeline_shader_terrain_opaque_no_alpha);
    gpu::release(pipeline_shader_terrain_opaque_alpha_test);
    gpu::release(pipeline_shader_terrain_overlay);
    gpu::release(pipeline_shader_terrain_depth_peel_0);
    gpu::release(pipeline_shader_terrain_depth_peel_n);

    SHADERS_DO_ACTION(RELEASE_SHADERS);
}

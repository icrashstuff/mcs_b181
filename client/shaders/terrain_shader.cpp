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
#include "terrain.frag.msl.h"
#include "terrain.frag.spv.h"
#include "terrain.vert.msl.h"
#include "terrain.vert.spv.h"

#include "../state.h"
#include "terrain_shader.h"
#include "tetra/log.h"
#include <SDL3/SDL.h>

SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_opaque = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_overlay = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_translucent = nullptr;
SDL_GPUGraphicsPipeline* state::pipeline_shader_terrain_translucent_depth = nullptr;

static SDL_GPUShader* shader_vert = nullptr;
static SDL_GPUShader* shader_frag = nullptr;

static bool create_shader(SDL_GPUShader*& shader, SDL_GPUShaderCreateInfo& cinfo, const char* name)
{
    cinfo.props = SDL_CreateProperties();
    SDL_SetStringProperty(cinfo.props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, name);
    SDL_ClearError();

    shader = SDL_CreateGPUShader(state::gpu_device, &cinfo);
    if (!shader)
        dc_log_error("Error creating shader \"%s\": %s", name, SDL_GetError());

    SDL_DestroyProperties(cinfo.props);

    cinfo.props = 0;

    if (!shader)
        return false;
    else
        return true;
}

static bool create_pipeline(SDL_GPUGraphicsPipeline*& pipeline, SDL_GPUGraphicsPipelineCreateInfo& cinfo, const char* name)
{
    cinfo.props = SDL_CreateProperties();
    SDL_SetStringProperty(cinfo.props, SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING, name);
    SDL_ClearError();

    pipeline = SDL_CreateGPUGraphicsPipeline(state::gpu_device, &cinfo);
    if (!pipeline)
        dc_log_error("Error creating shader \"%s\": %s", name, SDL_GetError());

    SDL_DestroyProperties(cinfo.props);

    cinfo.props = 0;

    if (!pipeline)
        return false;
    else
        return true;
}

static void release_pipeline(SDL_GPUGraphicsPipeline*& pipeline)
{
    SDL_ReleaseGPUGraphicsPipeline(state::gpu_device, pipeline);
    pipeline = nullptr;
}

static void release_shader(SDL_GPUShader*& shader)
{
    SDL_ReleaseGPUShader(state::gpu_device, shader);
    shader = nullptr;
}

#include <stdio.h>
void state::init_terrain_pipelines()
{
    destroy_terrain_pipelines();
    SDL_GPUShaderCreateInfo cinfo_shader_vert = {
        .code_size = 0,
        .code = NULL,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_INVALID,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 3,
        .props = 0,
    };
    SDL_GPUShaderCreateInfo cinfo_shader_frag = {
        .code_size = 0,
        .code = NULL,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_INVALID,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 2,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 1,
        .props = 0,
    };

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(state::gpu_device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        cinfo_shader_vert.code = terrain_vert_spv;
        cinfo_shader_vert.code_size = terrain_vert_spv_len;
        cinfo_shader_vert.format = SDL_GPU_SHADERFORMAT_SPIRV;
        cinfo_shader_frag.code = terrain_frag_spv;
        cinfo_shader_frag.code_size = terrain_frag_spv_len;
        cinfo_shader_frag.format = SDL_GPU_SHADERFORMAT_SPIRV;
    }
    else if (formats & SDL_GPU_SHADERFORMAT_MSL)
    {
        cinfo_shader_vert.entrypoint = "main0";
        cinfo_shader_vert.code = terrain_vert_msl;
        cinfo_shader_vert.code_size = terrain_vert_msl_len;
        cinfo_shader_vert.format = SDL_GPU_SHADERFORMAT_MSL;
        cinfo_shader_frag.entrypoint = "main0";
        cinfo_shader_frag.code = terrain_frag_msl;
        cinfo_shader_frag.code_size = terrain_frag_msl_len;
        cinfo_shader_frag.format = SDL_GPU_SHADERFORMAT_MSL;
    }

    if (!create_shader(shader_vert, cinfo_shader_vert, "Terrain shader (vert)"))
        return;

    if (!create_shader(shader_frag, cinfo_shader_frag, "Terrain shader (frag)"))
        return;

    SDL_GPUVertexAttribute vertex_attributes[] = {
        SDL_GPUVertexAttribute {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            .offset = 0,
        },
        SDL_GPUVertexAttribute {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            .offset = 4,
        },
        SDL_GPUVertexAttribute {
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            .offset = 8,
        },
    };

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[] = {
        SDL_GPUVertexBufferDescription {
            .slot = 0,
            .pitch = 12,
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            .instance_step_rate = 0,
        },
    };

    SDL_GPUVertexInputState vertex_input_state = {
        .vertex_buffer_descriptions = vertex_buffer_descriptions,
        .num_vertex_buffers = SDL_arraysize(vertex_buffer_descriptions),
        .vertex_attributes = vertex_attributes,
        .num_vertex_attributes = SDL_arraysize(vertex_attributes),
    };
    SDL_GPURasterizerState rasterizer_state = {
        .fill_mode = SDL_GPU_FILLMODE_FILL,
        .cull_mode = SDL_GPU_CULLMODE_BACK,
        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        .depth_bias_constant_factor = 0,
        .depth_bias_clamp = 0,
        .depth_bias_slope_factor = 0,
        .enable_depth_bias = 0,
        .enable_depth_clip = 0,
    };
    SDL_GPUMultisampleState multisample_state = {
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .sample_mask = 0,
        .enable_mask = 0,
    };
    SDL_GPUDepthStencilState depth_stencil_state = {};
    depth_stencil_state.enable_depth_test = 1;
    depth_stencil_state.enable_depth_write = 1;
    depth_stencil_state.enable_stencil_test = 0;

    SDL_GPUColorTargetDescription color_target_descriptions[] = {
        SDL_GPUColorTargetDescription {
            SDL_GetGPUSwapchainTextureFormat(state::gpu_device, state::window),
            SDL_GPUColorTargetBlendState {
                .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .color_blend_op = SDL_GPU_BLENDOP_ADD,
                .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A,
                .enable_blend = 0,
                .enable_color_write_mask = 0,
                .padding1 = 0,
                .padding2 = 0,
            },
        },
    };

    SDL_GPUGraphicsPipelineTargetInfo target_info = {
        .color_target_descriptions = color_target_descriptions,
        .num_color_targets = SDL_arraysize(color_target_descriptions),
        .depth_stencil_format = state::gpu_tex_format_best_depth_only,
        .has_depth_stencil_target = 1,
    };

    SDL_GPUGraphicsPipelineCreateInfo cinfo_pipeline = {
        .vertex_shader = shader_vert,
        .fragment_shader = shader_frag,
        .vertex_input_state = vertex_input_state,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = rasterizer_state,
        .multisample_state = multisample_state,
        .depth_stencil_state = depth_stencil_state,
        .target_info = target_info,
        .props = 0,
    };

    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 1;
    color_target_descriptions[0].blend_state.enable_blend = 0;
    create_pipeline(pipeline_shader_terrain_opaque, cinfo_pipeline, "Terrain pipeline (opaque)");

    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 0;
    color_target_descriptions[0].blend_state.enable_blend = 0;
    create_pipeline(pipeline_shader_terrain_overlay, cinfo_pipeline, "Terrain pipeline (overlay)");

    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 1;
    color_target_descriptions[0].blend_state.enable_blend = 0;
    color_target_descriptions[0].blend_state.enable_color_write_mask = 1;
    color_target_descriptions[0].blend_state.color_write_mask = 0;
    create_pipeline(pipeline_shader_terrain_translucent_depth, cinfo_pipeline, "Terrain pipeline (translucent depth-only)");

    cinfo_pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
    cinfo_pipeline.depth_stencil_state.enable_depth_test = 1;
    cinfo_pipeline.depth_stencil_state.enable_depth_write = 0;
    color_target_descriptions[0].blend_state.enable_blend = 1;
    color_target_descriptions[0].blend_state.enable_color_write_mask = 0;
    create_pipeline(pipeline_shader_terrain_translucent, cinfo_pipeline, "Terrain pipeline (translucent)");
}

void state::destroy_terrain_pipelines()
{
    release_pipeline(pipeline_shader_terrain_opaque);
    release_pipeline(pipeline_shader_terrain_overlay);
    release_pipeline(pipeline_shader_terrain_translucent);
    release_pipeline(pipeline_shader_terrain_translucent_depth);
    release_shader(shader_vert);
    release_shader(shader_frag);
}

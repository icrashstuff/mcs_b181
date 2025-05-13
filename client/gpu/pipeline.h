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
#pragma once

#include <SDL3/SDL_gpu.h>

/* Shaders */
namespace gpu
{
/**
 * Get GPU shader formats
 *
 * @returns a bit-flag of the shader formats supported by the current GPU device
 */
SDL_GPUShaderFormat get_shader_formats();

/**
 * Create a named GPU Shader
 *
 * @param cinfo Creation info
 * @param fmt Shader name format string (Uses stb_sprintf) (NULL for no name)
 *
 * @returns a shader handle, or NULL on error
 */
SDL_GPUShader* create(const SDL_GPUShaderCreateInfo& cinfo, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(2);

/**
 * Release a GPU shader
 *
 * You must not reference the shader after calling this function.
 *
 * @param shader Shader to release
 * @param set_shader_to_null Set shader parameter to null after releasing
 */
void release(SDL_GPUShader*& shader, const bool set_shader_to_null = true);
}

/* Graphics pipelines */
namespace gpu
{
/**
 * Create a named GPU graphics Pipeline
 *
 * @param cinfo Creation info
 * @param fmt Pipeline name format string (Uses stb_sprintf) (NULL for no name)
 *
 * @returns a pipeline handle, or NULL on error
 */
SDL_GPUGraphicsPipeline* create(const SDL_GPUGraphicsPipelineCreateInfo& cinfo, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(2);

/**
 * Release a GPU graphics pipeline
 *
 * You must not reference the pipeline after calling this function.
 *
 * @param pipeline Pipeline to release
 * @param set_pipeline_to_null Set pipeline parameter to null after releasing
 */
void release(SDL_GPUGraphicsPipeline*& pipeline, const bool set_pipeline_to_null = true);
}

/* Compute pipelines */
namespace gpu
{
/**
 * Create a named GPU compute Pipeline
 *
 * @param cinfo Creation info
 * @param fmt Pipeline name format string (Uses stb_sprintf) (NULL for no name)
 *
 * @returns a pipeline handle, or NULL on error
 */
SDL_GPUComputePipeline* create(const SDL_GPUComputePipelineCreateInfo& cinfo, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(2);

/**
 * Release a GPU compute pipeline
 *
 * You must not reference the pipeline after calling this function.
 *
 * @param pipeline Pipeline to release
 * @param set_pipeline_to_null Set pipeline parameter to null after releasing
 */
void release(SDL_GPUComputePipeline*& pipeline, const bool set_pipeline_to_null = true);
}

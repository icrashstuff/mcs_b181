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
#include "pipeline.h"

#include "internal.h"

#include "smol-v/smolv.h"

/**
 * Decode SMOL-V to SPIR-V if SMOL-V code is present
 *
 * @returns false on decoding error, true on success or SMOL-V magic not found
 */
static bool decode_if_smolv(SDL_GPUShaderCreateInfo& cinfo, smolv::ByteArray& output)
{
    if (cinfo.format != SDL_GPU_SHADERFORMAT_SPIRV)
        return true;

    if (cinfo.code == NULL)
        return true;

    if (cinfo.code_size < 4)
        return true;

    /* Check for magic (SMOL, but in reverse) */
    if (memcmp(cinfo.code, "LOMS", 4) != 0)
        return true;

    const Uint8* code = cinfo.code;
    const size_t code_size = cinfo.code_size;

    output.resize(smolv::GetDecodedBufferSize(code, code_size));

    cinfo.code = output.data();
    cinfo.code_size = output.size();

    return smolv::Decode(code, code_size, output.data(), output.size());
}

SDL_GPUShader* gpu::create(const SDL_GPUShaderCreateInfo& cinfo, const char* fmt, ...)
{
    SDL_GPUShaderCreateInfo cinfo_named = cinfo;

    smolv::ByteArray decoded;
    if (!decode_if_smolv(cinfo_named, decoded))
        return nullptr;

    cinfo_named.props = SDL_CreateProperties();
    if (cinfo.props)
        SDL_CopyProperties(cinfo.props, cinfo_named.props);

    if (fmt)
    {
        char name[1024] = "";

        va_list args;
        va_start(args, fmt);
        stbsp_vsnprintf(name, SDL_arraysize(name), fmt, args);
        va_end(args);

        SDL_SetStringProperty(cinfo_named.props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, name);
    }

    SDL_ClearError();

    SDL_GPUShader* ret = SDL_CreateGPUShader(state::gpu_device, &cinfo_named);

    if (!ret)
        dc_log_error("Failed to acquire %s! SDL_CreateGPU%s: %s", "Shader", "Shader", SDL_GetError());

    SDL_DestroyProperties(cinfo_named.props);

    return ret;
}

CREATE_FUNC_DEF(create, GraphicsPipeline, SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING);
CREATE_FUNC_DEF(create, ComputePipeline, SDL_PROP_GPU_COMPUTEPIPELINE_CREATE_NAME_STRING);
RELEASE_FUNC_DEF(release, Shader);
RELEASE_FUNC_DEF(release, GraphicsPipeline);
RELEASE_FUNC_DEF(release, ComputePipeline);

SDL_GPUShaderFormat gpu::get_shader_formats() { return SDL_GetGPUShaderFormats(state::gpu_device); }

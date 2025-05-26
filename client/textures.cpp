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
#include "textures.h"

#include "gpu/gpu.h"

#include "state.h"

#include "tetra/log.h"
#include "tetra/util/stbi.h"

#include <string>

static SDL_GPUSampler* sampler_edge_clamp = nullptr;
static SDL_GPUSampler* sampler_edge_repeat = nullptr;
static SDL_GPUSampler* sampler_edge_mirrored_repeat = nullptr;

#define DO_X()                                                                         \
    X(state::textures::environment::clouds, repeat, "environment/clouds.png")          \
    X(state::textures::environment::end_sky, repeat, "environment/end_sky.png")        \
    X(state::textures::environment::moon_phases, clamp, "environment/moon_phases.png") \
    X(state::textures::environment::rain, repeat, "environment/rain.png")              \
    X(state::textures::environment::snow, repeat, "environment/snow.png")              \
    X(state::textures::environment::sun, clamp, "environment/sun.png")

#define X(binding, edge, path) SDL_GPUTextureSamplerBinding binding = {};
DO_X();
#undef X

static SDL_GPUTexture* create_texture_from_data(
    SDL_GPUCopyPass* const copy_pass, const void* const data, const Uint32 x, const Uint32 y, const std::string& label)
{
    SDL_GPUTexture* ret = state::gpu_debug_texture;
    if (!data || x == 0 || y == 0 || !copy_pass)
        return ret;

    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = x;
    tex_info.height = y;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;

    if (!(ret = gpu::create_texture(tex_info, "Texture: %s", label.c_str())))
        ret = state::gpu_debug_texture;

    if (ret && ret != state::gpu_debug_texture)
    {
        if (!gpu::upload_to_texture2d(copy_pass, ret, tex_info.format, 0, 0, x, y, data, false))
            dc_log_error("Failed to upload texture!");
    }

    return ret;
}

static SDL_GPUTexture* create_texture(SDL_GPUCopyPass* const copy_pass, std::string path, const std::string& prefix = "/_resources/assets/minecraft/textures/")
{
    const std::string label = path;
    path = prefix + path;

    int x, y, channels;
    Uint8* data_widgets = stbi_physfs_load(path.c_str(), &x, &y, &channels, 4);

    if (!data_widgets)
        dc_log_error("Unable to load texture: \"%s\"", path.c_str());

    SDL_GPUTexture* ret = create_texture_from_data(copy_pass, data_widgets, x, y, label);

    stbi_image_free(data_widgets);

    return ret;
}

void state::init_textures(SDL_GPUCopyPass* copy_pass)
{
    destroy_textures();
    SDL_GPUSamplerCreateInfo cinfo_sampler = {};

    cinfo_sampler.address_mode_u = cinfo_sampler.address_mode_v = cinfo_sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_edge_repeat = gpu::create_sampler(cinfo_sampler, "textures.cpp::sampler_edge_repeat");

    cinfo_sampler.address_mode_u = cinfo_sampler.address_mode_v = cinfo_sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    sampler_edge_mirrored_repeat = gpu::create_sampler(cinfo_sampler, "textures.cpp::sampler_edge_mirrored_repeat");

    cinfo_sampler.address_mode_u = cinfo_sampler.address_mode_v = cinfo_sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_edge_clamp = gpu::create_sampler(cinfo_sampler, "textures.cpp::sampler_edge_clamp");

    if (!sampler_edge_clamp)
        sampler_edge_clamp = state::gpu_debug_sampler;

    if (!sampler_edge_repeat)
        sampler_edge_repeat = state::gpu_debug_sampler;

    if (!sampler_edge_mirrored_repeat)
        sampler_edge_mirrored_repeat = state::gpu_debug_sampler;

#define X(binding, edge, path) binding = { create_texture(copy_pass, path), sampler_edge_##edge };
    DO_X();
#undef X
}

void state::destroy_textures()
{
    gpu::release_sampler(sampler_edge_clamp);
    gpu::release_sampler(sampler_edge_repeat);

#define X(binding, edge, path)                 \
    {                                          \
        gpu::release_texture(binding.texture); \
        binding = {};                          \
    }
    DO_X();
#undef X
}

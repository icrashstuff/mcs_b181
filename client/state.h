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
 *
 * TODO: Fold/Flatten game_resources_t into this?
 */
#ifndef MCS_B181__CLIENT__STATE_H_INCLUDED
#define MCS_B181__CLIENT__STATE_H_INCLUDED

#include <SDL3/SDL.h>

#include "shaders/background_shader.h"
#include "shaders/clouds_shader.h"
#include "shaders/composite_shader.h"
#include "shaders/terrain_shader.h"

#include "textures.h"

/* Forward declaration(s) */
struct game_resources_t;

namespace state
{
extern game_resources_t* game_resources;
extern SDL_Window* window;
extern SDL_GPUDevice* sdl_gpu_device;
/** Guaranteed to exist */
extern SDL_GPUTexture* gpu_debug_texture;
/** Guaranteed to exist */
extern SDL_GPUSampler* gpu_debug_sampler;
extern SDL_GPUTextureFormat gpu_tex_format_best_depth_only;

extern const bool on_ios;
extern const bool on_android;
extern const bool on_mobile;
}

#endif

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

CREATE_FUNC_DEF(create, Shader, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING);
CREATE_FUNC_DEF(create, GraphicsPipeline, SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING);
CREATE_FUNC_DEF(create, ComputePipeline, SDL_PROP_GPU_COMPUTEPIPELINE_CREATE_NAME_STRING);
RELEASE_FUNC_DEF(release, Shader);
RELEASE_FUNC_DEF(release, GraphicsPipeline);
RELEASE_FUNC_DEF(release, ComputePipeline);

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

#include "tetra/licenses.h"

static tetra::project_t mcs_b181_projects[] = {
    { "mcs_b181", "Copyright (c) 2024-2025 Ian Hangartner", tetra::PROJECT_VENDORED, { tetra::license_MIT } },

    // { "SDL3", "Copyright (C) 1997-2025 Sam Lantinga, and others", tetra::PROJECT_VENDORED, { tetra::license_Zlib } },
    { "Zlib", "Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler", tetra::PROJECT_VENDORED, { tetra::license_Zlib } },

    { "EnTT", "Copyright (c) 2017-2024 Michele Caini, and others", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "Cubiomes", "Copyright (c) 2020-2024 Cubitect, and others", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "stb_vorbis", "Copyright (c) 2007-2024 Sean Barrett, and others", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "SMOL-V", "Copyright (c) 2016-2024 Aras Pranckevicius", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "Jzon", "Copyright (c) 2015 Johannes Häggqvist", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "Simplex Noise", "Copyright (c) 2012-2018 Sebastien Rombauts", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "Volk", "Copyright (c) 2018-2025 Arseny Kapoulkine", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
    { "SDL3 Net", "Copyright (C) 1997-2024 Sam Lantinga, and others", tetra::PROJECT_VENDORED, { tetra::license_Zlib } },
    { "Vulkan Memory Allocator", "Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved.", tetra::PROJECT_VENDORED, { tetra::license_MIT } },
};

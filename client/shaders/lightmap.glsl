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

#ifndef LIGHTMAP_SET
#error "LIGHTMAP_SET must be defined!"
#endif

#ifndef LIGHTMAP_BINDING
#error "LIGHTMAP_BINDING must be defined!"
#endif

layout(std140, set = LIGHTMAP_SET, binding = LIGHTMAP_BINDING) uniform ubo_lightmap_t
{
    vec3 minimum_color;
    vec3 sky_color;
    vec3 block_color;
    vec3 light_flicker;
    float gamma;
}
ubo_lightmap;

vec3 calculate_lighting(float block, float sky)
{
    vec3 light_factor = vec3(0);

    light_factor += ubo_lightmap.block_color * ubo_lightmap.light_flicker * pow(block, ubo_lightmap.gamma);
    light_factor += ubo_lightmap.sky_color                                * pow(sky,   ubo_lightmap.gamma);
    light_factor = (light_factor + ubo_lightmap.minimum_color) / (1.f + ubo_lightmap.minimum_color);

    return clamp(light_factor, vec3(0.f), vec3(1.f));
}

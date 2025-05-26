// clang-format off
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
/* This file is auto-generated, do not edit! */
static const unsigned char background_frag_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct ubo_lighting_t
{
    float4 light_color;
    float2 uv_size;
    char _m2_pad[8];
    float ambient_brightness;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float2 fragcoords [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant ubo_lighting_t& ubo_lighting [[buffer(0)]], texture2d<float> tex_base [[texture(0)]], texture2d<float> tex_normal [[texture(1)]], sampler tex_baseSmplr [[sampler(0)]], sampler tex_normalSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 _22 = ubo_lighting.uv_size * in.fragcoords;
    float3 _33 = tex_base.sample(tex_baseSmplr, _22).xyz;
    out.out_color = float4((_33 * ubo_lighting.ambient_brightness) + ((_33 * fast::max(powr(dot(fast::normalize(float3(0.5, 0.5, 0.25) - float3(in.fragcoords, 0.0)), fast::normalize((tex_normal.sample(tex_normalSmplr, _22).xyz * 2.0) - float3(1.0))), 0.4545454680919647216796875), 0.0)) * ubo_lighting.light_color.xyz), 1.0);
    return out;
}

)";
static const unsigned int background_frag_msl_len = sizeof(background_frag_msl);

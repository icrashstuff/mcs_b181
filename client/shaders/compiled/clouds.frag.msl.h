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
static const unsigned char clouds_frag_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct ubo_cloud_frag_t
{
    float4 color;
    float cloud_offset;
};

struct ubo_frag_t
{
    float4 fog_color;
    char _m1_pad[8];
    float fog_max;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float3 camera_pos [[user(locn0), flat]];
    float3 cloud_pos [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant ubo_frag_t& ubo_frag [[buffer(0)]], constant ubo_cloud_frag_t& ubo_cloud_frag [[buffer(1)]], texture2d<float> tex_cloud [[texture(0)]], sampler tex_cloudSmplr [[sampler(0)]])
{
    main0_out out = {};
    out.out_color = tex_cloud.sample(tex_cloudSmplr, ((in.cloud_pos.xz * (float2(0.0625) / float2(int2(tex_cloud.get_width(), tex_cloud.get_height())))) + float2(ubo_cloud_frag.cloud_offset, 0.0)), bias(0.0)) * ubo_cloud_frag.color;
    if (out.out_color.w < 0.5)
    {
        discard_fragment();
    }
    float4 _80 = out.out_color;
    float3 _87 = mix(_80.xyz, ubo_frag.fog_color.xyz, float3(smoothstep(ubo_frag.fog_max, ubo_frag.fog_max * 1.5, distance(in.camera_pos.xz, in.cloud_pos.xz))));
    out.out_color.x = _87.x;
    out.out_color.y = _87.y;
    out.out_color.z = _87.z;
    return out;
}

)";
static const unsigned int clouds_frag_msl_len = sizeof(clouds_frag_msl);

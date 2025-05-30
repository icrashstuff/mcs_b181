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
static const unsigned char terrain_frag_depth_peel_0_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct ubo_lightmap_t
{
    float3 minimum_color;
    float3 sky_color;
    float3 block_color;
    packed_float3 light_flicker;
    float gamma;
};

struct _69
{
    float4 color;
    float2 uv;
    float ao;
    float light_block;
    float light_sky;
    float fog_dist;
};

struct ubo_frag_t
{
    float4 fog_color;
    uint use_texture;
    float fog_min;
    float fog_max;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float4 frag_color [[user(locn0)]];
    float2 frag_uv [[user(locn1)]];
    float frag_ao [[user(locn2)]];
    float frag_light_block [[user(locn3)]];
    float frag_light_sky [[user(locn4)]];
    float frag_fog_dist [[user(locn5)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant ubo_frag_t& ubo_frag [[buffer(0)]], constant ubo_lightmap_t& ubo_lightmap [[buffer(1)]], texture2d<float> tex_atlas [[texture(0)]], sampler tex_atlasSmplr [[sampler(0)]])
{
    main0_out out = {};
    _69 frag = {};
    frag.color = in.frag_color;
    frag.uv = in.frag_uv;
    frag.ao = in.frag_ao;
    frag.light_block = in.frag_light_block;
    frag.light_sky = in.frag_light_sky;
    frag.fog_dist = in.frag_fog_dist;
    out.out_color = frag.color;
    if (ubo_frag.use_texture == 1u)
    {
        out.out_color *= tex_atlas.sample(tex_atlasSmplr, frag.uv, bias(-1.0));
    }
    else
    {
        out.out_color.w *= tex_atlas.sample(tex_atlasSmplr, frag.uv, bias(-1.0)).w;
    }
    if (out.out_color.w < 0.00390625)
    {
        discard_fragment();
    }
    float4 _133 = out.out_color;
    float3 _135 = _133.xyz * (1.0 - (frag.ao * 0.100000001490116119384765625));
    out.out_color.x = _135.x;
    out.out_color.y = _135.y;
    out.out_color.z = _135.z;
    float4 _153 = out.out_color;
    float3 _155 = _153.xyz * fast::clamp(1.10000002384185791015625 - ((-0.100000001490116119384765625) / (frag.ao - 1.25)), 0.0, 1.0);
    out.out_color.x = _155.x;
    out.out_color.y = _155.y;
    out.out_color.z = _155.z;
    out.out_color *= float4(fast::clamp(((((ubo_lightmap.block_color * float3(ubo_lightmap.light_flicker)) * powr(frag.light_block, ubo_lightmap.gamma)) + (ubo_lightmap.sky_color * powr(frag.light_sky, ubo_lightmap.gamma))) + ubo_lightmap.minimum_color) / (float3(1.0) + ubo_lightmap.minimum_color), float3(0.0), float3(1.0)), 1.0);
    float4 _242 = out.out_color;
    float3 _250 = mix(_242.xyz, ubo_frag.fog_color.xyz, float3(smoothstep(ubo_frag.fog_min, ubo_frag.fog_max, frag.fog_dist)));
    out.out_color.x = _250.x;
    out.out_color.y = _250.y;
    out.out_color.z = _250.z;
    return out;
}

)";
static const unsigned int terrain_frag_depth_peel_0_msl_len = sizeof(terrain_frag_depth_peel_0_msl);

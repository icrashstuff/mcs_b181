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
static const unsigned char clouds_vert_msl[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct ubo_cloud_vert_t
{
    float4 camera_pos;
    float height;
    float render_distance;
};

struct ubo_world_t
{
    float4x4 camera;
    float4x4 projection;
};

struct main0_out
{
    float3 out_camera_pos [[user(locn0)]];
    float3 out_cloud_pos [[user(locn1)]];
    float4 gl_Position [[position]];
};

vertex main0_out main0(constant ubo_world_t& ubo_world [[buffer(0)]], constant ubo_cloud_vert_t& ubo_cloud_vert [[buffer(1)]], uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    float _22 = ubo_cloud_vert.render_distance * 25.6000003814697265625;
    float3 _30 = float3(_22, ubo_cloud_vert.height, _22);
    float3 _112;
    if (extract_bits(int(gl_VertexIndex), uint(0), uint(1)) != int(0u))
    {
        float3 _104 = _30;
        _104.x = ubo_cloud_vert.render_distance * (-25.6000003814697265625);
        _112 = _104;
    }
    else
    {
        _112 = _30;
    }
    float3 _113;
    if (extract_bits(int(gl_VertexIndex), uint(1), uint(1)) != int(0u))
    {
        float3 _107 = _112;
        _107.z = _112.z * (-1.0);
        _113 = _107;
    }
    else
    {
        _113 = _112;
    }
    float2 _65 = _113.xz + ubo_cloud_vert.camera_pos.xz;
    float _67 = _65.x;
    float3 _109 = _113;
    _109.x = _67;
    float _69 = _65.y;
    _109.z = _69;
    out.out_cloud_pos = _109;
    out.out_camera_pos = ubo_cloud_vert.camera_pos.xyz;
    out.gl_Position = (ubo_world.projection * ubo_world.camera) * float4(_67, _113.y, _69, 1.0);
    return out;
}

)";
static const unsigned int clouds_vert_msl_len = sizeof(clouds_vert_msl);

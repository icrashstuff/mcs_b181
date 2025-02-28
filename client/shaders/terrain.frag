#version 330 core
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
in vec3 frag_color;
in vec2 frag_uv;
in float frag_ao;
in float frag_light_block;
in float frag_light_sky;

out vec4 out_color;

uniform sampler2D tex_atlas;
uniform sampler2D tex_lightmap;

uniform int ao_algorithm = 1;
uniform bool use_texture = true;
uniform bool allow_translucency = true;

void main()
{
    out_color = vec4(frag_color.xyz, 1.0);

    if (use_texture)
        out_color *= texture2D(tex_atlas, frag_uv);
    else
        out_color.a *= texture2D(tex_atlas, frag_uv).a;

    if (out_color.a < 0.0625)
        discard;
    else if(!allow_translucency)
        out_color.a = 1.0;

    switch (ao_algorithm)
    {
    case 0:
        break;
    case 1:
        out_color.xyz *= 1.0 - frag_ao / 10.0;
    case 2:
        out_color.xyz *= clamp(1.1 - (-0.1 / (frag_ao - 1.25)), 0.0, 1.0);
        break;
    case 3:
        out_color.xyz *= clamp(1.15 - (-0.2 / (frag_ao - 1.25)), 0.0, 1.0);
        break;
    case 4:
        out_color.xyz *= (1.0 - (pow(64.0, frag_ao / 2.0) - 1) / 64.0);
        break;
    case 5:
        out_color.xyz *= (1.0 - (pow(64.0, frag_ao / 1.5) - 1) / 64.0);
        break;
    default:
        break;
    }

    out_color *= texture2D(tex_lightmap,vec2(frag_light_block, frag_light_sky));
}

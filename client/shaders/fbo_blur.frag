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
in vec2 frag_uv;

out vec4 out_color;

uniform sampler2D tex;
uniform int radius = 3;
uniform float blur_x = 1.0;
uniform float blur_y = 0.0;
uniform float gradient_mix = 0.0f;

#define PI 3.14159265358979323846

/**
 * One dimensional Gaussian blur function
 */
float gauss(float x, float sigma)
{
    return inversesqrt(2 * PI * sigma * sigma) * exp(-(x * x) / (2 * sigma * sigma));
}

void main()
{
    vec2 tsize = textureSize(tex, 0);

    float sigma = float(radius) / 3.004;
    int limit = radius;

    vec3 col = vec3(0.0);
    for (int i = -limit; i <= limit; i++)
    {
        float i_gauss = gauss(i, sigma);
        vec2 off = vec2(float(i) * blur_x / tsize.x, float(i) * blur_y / tsize.y);

        col += vec3(texture(tex, frag_uv.xy + off)) * i_gauss;
    }

    vec3 col_grad = vec3(mix(0.0, 1.0, frag_uv.y));

    col = mix(col, col_grad, gradient_mix);

    out_color = vec4(col, 1.0);
}

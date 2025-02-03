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

#ifndef TETRA_CLIENT_GUI_PANORAMA_H_INCLUDED
#define TETRA_CLIENT_GUI_PANORAMA_H_INCLUDED

#include <GL/glew.h>
#include <SDL3/SDL_stdinc.h>
#include <glm/glm.hpp>

#include "client/shaders.h"

/**
 * Shows a continually rotating Gaussian blurred cube-mapped panorama
 */
class panorama_t
{
public:
    panorama_t();

    ~panorama_t();

    /**
     * Draw Dear ImGui widgets for controlling the panorama
     */
    void imgui_widgets();

    /**
     * Renders, blurs, and outputs the panorama
     *
     * NOTE: This will output at depth = 1.0f
     *
     * @param win_size Used to calculate aspect ratio
     * @param disable_depth_writes Disable depth writing for the final output
     */
    void render(const glm::ivec2 win_size, const bool disable_depth_writes = true);

private:
    /* Resources for the panorama cube */
    shader_t pano_shader = shader_t("/shaders/pano.vert", "/shaders/pano.frag", "[Panorama][Cube]");
    GLuint pano_tex = 0;
    GLuint pano_vao = 0;
    GLuint pano_vbo = 0;

    /* Resources for the frame buffers and rendering them */
    shader_t fbo_shader_blur = shader_t("/shaders/fbo.vert", "/shaders/fbo_blur.frag", "[Panorama][Blur]");
    shader_t fbo_shader_out = shader_t("/shaders/fbo.vert", "/shaders/fbo_out.frag", "[Panorama][Out]");
    GLuint fbo_tex[2] = { 0 };
    GLuint fbo[2] = { 0 };
    GLuint fbo_vao = { 0 };
    GLuint fbo_vbo = { 0 };

    int period_yaw = 98304;
    int period_pitch = 131072;
    int period_roll = 49152;

    float last_yaw = 0.0f;
    float last_pitch = 0.0f;
    float last_roll = 0.0f;

    glm::ivec2 last_win_size;

    float range_pitch = 80.0f;
    float range_roll = 4.0f;

    float zero_pitch = 15.0f;
    float zero_roll = 0.0f;

    float fov = 70.0f;
    int blur_radius = 6;

    /** Offset applied to SDL_GetTicks() to randomize the starting position of the euler angles */
    Uint64 tick_offset = 0;

    int average_tex_width = 0;

    /**
     * Resize output textures
     *
     * NOTE: This will unbind the frame buffer
     *
     * @param win_size Used to calculate aspect ratio
     *
     * @returns True if win_size is valid and the FBOs are complete, false otherwise
     */
    bool resize(const glm::ivec2 win_size);
};

#endif

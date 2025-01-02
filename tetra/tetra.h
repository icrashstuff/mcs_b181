/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
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

#ifndef MCS_B181_TETRA_H
#define MCS_B181_TETRA_H

#include <SDL3/SDL.h>

namespace tetra
{
/**
 * Should be called immediately, Can only be called once
 */
void init(const char* organization, const char* appname, const char* cfg_path, int argc, const char** argv);

enum render_api_t
{
    RENDER_API_GL_CORE,
    RENDER_API_GL_COMPATIBILITY,
    RENDER_API_GL_ES,
};

/**
 * Set render api and version for tetra to use
 *
 * NOTE: No checks are made for invalid variables
 */
void set_render_api(render_api_t api, int major, int minor);

/**
 * Returns 0 on successful init, some non-zero value on failure
 *
 * Can only be called once
 */
int init_gui(const char* window_title);

/**
 * Feed events to imgui
 *
 * Returns true if application should exit, false otherwise
 */
bool process_event(SDL_Event event);

/**
 * Returns -1 on failure, 0 for application should exit, 1 for application should continue
 *
 * @param event_loop Handle SDL events inside start_frame()
 */
int start_frame(bool event_loop = true);

/**
 * Renders the frame, and optionally limits the frame rate if gui_fps_limiter is set
 *
 * @param clear_frame Clear OpenGL color buffer
 */
void end_frame(bool clear_frame = true);

/**
 * Deinit tetra, can only be called once
 */
void deinit();

/**
 * Window created by tetra::init_gui()
 */
extern SDL_Window* window;

/**
 * OpenGL Context created by tetra::init_gui()
 */
extern SDL_GLContext gl_context;
};

#endif

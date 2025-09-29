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
 *
 * Basic example program:
 * main.cpp
 * ========
 * #include "tetra/gui/imgui.h"
 * #include "tetra/tetra_core.h"
 * #include "tetra/tetra_sdl_gpu.h"
 * int main(const int argc, const char** argv)
 * {
 *     tetra::init("icrashstuff", "Tetra example", "config_prefix", argc, argv);
 *
 *     tetra::init_gui("Hello World");
 *
 *     int done = 0;
 *     while (!done)
 *     {
 *         done = !tetra::start_frame();
 *
 *         ImGui::Begin("Hello");
 *         ImGui::Text("Hello world from tetra!");
 *         ImGui::End();
 *
 *         tetra::end_frame();
 *     }
 *
 *     tetra::deinit_gui();
 *     tetra::deinit();
 * }
 *
 * CMakeLists.txt
 * ==============
 * cmake_minimum_required(VERSION 3.27)
 *
 * # Make option() honor normal variables
 * cmake_policy(SET CMP0077 NEW)
 *
 * project(tetra_example LANGUAGES CXX C)
 *
 * set(CMAKE_INCLUDE_CURRENT_DIR ON)
 * set(tetra_example_SRC
 *     main.cpp
 * )
 *
 * set(TETRA_BUILD_SDL_GPU_COMPONENT ON)
 *
 * # Tetra must be in the "tetra" subfolder
 * add_subdirectory(tetra/)
 *
 * add_executable(tetra_example ${tetra_example_SRC})
 *
 * target_link_libraries(tetra_example tetra::core)
 * target_link_libraries(tetra_example tetra::sdl_gpu)
 */

#ifndef TETRA__TETRA_SDL_GPU_H__INCLUDED
#define TETRA__TETRA_SDL_GPU_H__INCLUDED

#include <SDL3/SDL.h>
#include <string>

namespace tetra
{
/**
 * Get the shader formats that ImGui supports
 */
SDL_GPUShaderFormat get_imgui_shader_formats();

/**
 * Obtains/Sets up an SDL_GPUDevice and SDL_Window
 *
 * @param window_title Initial window title
 * @param shader_formats Shader formats supported by the application
 *
 * @returns 0 on successful init, some non-zero value on failure (Though it will likely call util::die() if an error occurs)
 */
int init_gui(const char* const window_title, const SDL_GPUShaderFormat shader_formats = get_imgui_shader_formats());

/**
 * Deinit gui, call this before tetra::deinit()
 */
void deinit_gui();

/**
 * Returns -1 on failure, 0 for application should exit, 1 for application should continue
 *
 * @param event_loop Handle SDL events inside start_frame()
 */
int start_frame(bool event_loop = true);

/**
 * Feed events to imgui
 *
 * Returns true if application should exit, false otherwise
 */
bool process_event(const SDL_Event& event);

/**
 * Change visibility of main imgui context
 *
 * This works by not feeding the context any events and discarding all render data
 *
 * NOTE: If the dev console is shown it will take priority over values set through here
 * NOTE: gui_registrar::render_menus() is still called
 */
void show_imgui_ctx_main(bool shown);

/**
 * @returns True if either the main imgui context is shown or the dev console is forcing it, False otherwise
 */
bool imgui_ctx_main_wants_input();

/**
 * Change visibility of overlay imgui context
 *
 * This works by discarding all render data
 *
 * NOTE: gui_registrar::render_overlays() is still called
 */
void show_imgui_ctx_overlay(bool shown);

/**
 * Simple version of tetra::end_frame()
 *
 * Renders the frame, and optionally limits the frame rate if gui_fps_limiter is set
 */
void end_frame();

/**
 * Complex version of tetra::end_frame()
 *
 * @param command_buffer Command buffer to render on (Passing a nullptr is safe)
 * @param texture Texture to render ImGui to (Passing a nullptr is safe)
 * @param clear_texture Clear texture to solid black
 */
void end_frame(SDL_GPUCommandBuffer* const command_buffer, SDL_GPUTexture* const texture, bool clear_texture);

/**
 * Reconfigure swapchain if needed (ie. vsync changes)
 *
 * NOTE: This function *MUST* be called *BEFORE* acquiring a swapchain texture
 */
void configure_swapchain_if_needed();

/**
 * Limits framerate (ie. This function will attempt to ensure that two calls are spaced at least '(1000.0f / r_fps_limiter.get())' ms apart)
 */
void limit_framerate();

/**
 * Window created by tetra::init_gui()
 */
extern SDL_Window* window;

/**
 * Device acquired by tetra::init_gui()
 */
extern SDL_GPUDevice* gpu_device;

/**
 * Convert a shader format flag-set to a string
 */
std::string SDL_GPUShaderFormat_to_string(SDL_GPUShaderFormat formats);

/**
 * Get the shader formats supported by a particular driver
 *
 * WARNING: This function takes a *long* time to execute
 * WARNING: This function does not play well with valgrind
 *
 * @param driver_name Driver name as reported by SDL_GetGPUDriver or SDL_GetGPUDeviceDriver
 *
 * @returns Supported shader format flag-set for the specified driver
 */
SDL_GPUShaderFormat get_shaders_supported_by_driver(const char* const driver_name);
}

#endif

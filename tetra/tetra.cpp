/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) Copyright (c) 2014-2024 Omar Cornut and Dear ImGui Contributors
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
// Dear ImGui: standalone example application for SDL3 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "gui/imgui-1.91.1/backends/imgui_impl_opengl3.h"
#include "gui/imgui-1.91.1/backends/imgui_impl_sdl3.h"
#include "gui/imgui.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

// This example doesn't compile with Emscripten yet! Awaiting SDL3 support.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include "tetra.h"
#include "tetra/util/cli_parser.h"
#include "tetra/util/convar.h"
#include "tetra/util/convar_file.h"
#include "tetra/util/physfs/physfs.h"

#include "tetra/gui/console.h"
#include "tetra/gui/gui_registrar.h"
#include "tetra/gui/styles.h"

static SDL_Window* window;
static SDL_GLContext gl_context;

static bool was_init = false;
static bool was_init_gui = false;
static bool was_deinit = false;

static convar_int_t dev("dev", 0, 0, 1, "Enables developer focused features", CONVAR_FLAG_HIDDEN | CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t gui_fps_limiter("gui_fps_limiter", 300, 0, SDL_MAX_SINT32 - 1, "Max FPS, 0 to disable", CONVAR_FLAG_SAVE);
static convar_int_t gui_vsync("gui_vsync", 1, 0, 1, "Enable/Disable vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t gui_adapative_vsync("gui_adapative_vsync", 1, 0, 1, "Enable disable adaptive vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t gui_show_demo_window("gui_show_demo_window", 0, 0, 1, "Show Dear ImGui demo window", CONVAR_FLAG_INT_IS_BOOL);

void tetra::init(int argc, const char** argv)
{
    if (was_init)
        return;

    dc_log("Init main");

    was_init = true;

    convar_t::atexit_init();
    atexit(convar_t::atexit_callback);

    /* Parse command line */
    cli_parser::parse(argc, argv);

    /* Set dev before any other variables in case their callbacks require dev */
    if (cli_parser::get_value(dev.get_name()))
        dev.set(1);
    dev.set_pre_callback([=](int, int) -> bool { return false; });

    if (dev.get())
    {
        /* KDevelop fully buffers the output and will not display anything */
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        fflush_unlocked(stdout);
        fflush_unlocked(stderr);
        dc_log("Developer convar set");
    }

    assert(PHYSFS_init(argv[0]));
    assert(PHYSFS_setSaneConfig("icrashstuff", "mcs_b181", NULL, 0, 0));

    /* Set convars from config */
    convar_file_parser::read();

    /* Set convars from command line */
    cli_parser::apply();

    const PHYSFS_ArchiveInfo** supported_archives = PHYSFS_supportedArchiveTypes();

    for (int i = 0; supported_archives[i] != NULL; i++)
        dc_log("Supported archive: [%s]", supported_archives[i]->extension);
}

int tetra::init_gui(const char* window_title)
{
    if (was_init_gui || !was_init)
        return 0;

    dc_log("Init gui");

    was_init_gui = true;

    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

    if (dev.get())
        window_flags &= ~SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(window_title, 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_ShowWindow(window);

    /* This weirdness is to trick DWM into making the window floating */
    if (dev.get())
        SDL_SetWindowResizable(window, true);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    SDL_GL_MakeCurrent(window, gl_context);
    gui_vsync.set_post_callback(
        [=]() {
            bool vsync_enable = gui_vsync.get();
            bool adapative_vsync_enable = gui_adapative_vsync.get();
            if (vsync_enable && adapative_vsync_enable && SDL_GL_SetSwapInterval(-1) == 0)
                return;
            SDL_GL_SetSwapInterval(vsync_enable);
        },
        true);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.IniFilename = NULL; // Disable imgui.ini

    style_colors_rotate_hue(0, 160, 1, 1);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return 0;
}

int tetra::start_frame()
{
    if (!was_init_gui)
        return -1;

    bool done = false;

    ImGuiIO& io = ImGui::GetIO();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            done = true;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
            done = true;

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_GRAVE && event.key.repeat == 0)
            dev_console::show_hide();
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    return !done;
}

void tetra::end_frame()
{
    if (!was_init_gui)
        return;

    ImGuiIO& io = ImGui::GetIO();

    bool open = gui_show_demo_window.get();
    if (open)
    {
        ImGui::ShowDemoWindow(&open);
        if (open != gui_show_demo_window.get())
            gui_show_demo_window.set(open);
    }

    gui_registrar::render_menus();
    gui_registrar::render_overlays();

    dev_console::render();

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    Uint64 now = SDL_GetTicks();
    static Uint64 reference_time = 0;
    static Uint64 frames_since_reference = 0;
    if (gui_fps_limiter.get())
    {
        Uint64 elasped_time_ideal = frames_since_reference * 1000 / gui_fps_limiter.get();
        Sint64 delay = reference_time + elasped_time_ideal - now;
        // Reset when difference between the real and ideal worlds gets problematic
        if (delay < -100 || 100 < delay)
        {
            reference_time = now;
            frames_since_reference = 0;
        }
        else if (delay > 0)
            SDL_Delay(delay);
    }
    frames_since_reference += 1;
}

void tetra::deinit()
{
    if (!was_init_gui)
        return;
    if (was_deinit)
        return;
    was_deinit = true;

    convar_file_parser::write();

    convar_t::atexit_callback();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);

    assert(PHYSFS_deinit());
}

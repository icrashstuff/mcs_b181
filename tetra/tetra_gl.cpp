/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Portions Copyright (c) 2014-2024 Omar Cornut and Dear ImGui Contributors
 * SPDX-FileCopyrightText: Portions Copyright (c) 2024-2025 Ian Hangartner <icrashstuff at outlook dot com>
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

#include "gui/imgui.h"
#include "gui/imgui/backends/imgui_impl_opengl3.h"
#include "gui/imgui/backends/imgui_impl_sdl3.h"
#include <SDL3/SDL.h>
#include <stdio.h>

#include <GL/glew.h>
#include <GL/glu.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#warning "OpenGL ES is untested"
#else
#include <SDL3/SDL_opengl.h>
#endif

#include "tetra_core.h"
#include "tetra_gl.h"
#include "util/cli_parser.h"
#include "util/convar.h"
#include "util/convar_file.h"
#include "util/misc.h"
#include "util/physfs/physfs.h"

#include "gui/console.h"
#include "gui/gui_registrar.h"
#include "gui/proggy_tiny.cpp"
#include "gui/styles.h"

#include "tetra_internal.h"

namespace tetra
{
namespace gl
{
    bool is_available_glObjectLabel = 0;
    int init_counter = 0;
}
}

SDL_Window* tetra::window = NULL;
SDL_GLContext tetra::gl_context = NULL;

static ImGuiContext* im_ctx_main = NULL;
static ImGuiContext* im_ctx_overlay = NULL;

static bool gamepad_was_init = false;

static bool im_ctx_shown_main = true;
static bool im_ctx_shown_overlay = true;

static tetra::render_api_t render_api = tetra::RENDER_API_GL_CORE;
static int render_api_version_major = 3;
static int render_api_version_minor = 3;

static convar_int_t r_debug_gl("r_debug_gl", 0, 0, 1, "Sets SDL_GL_CONTEXT_DEBUG_FLAG", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t r_debug_gl_async("r_debug_gl_async", 0, 0, 1, "Enables asynchronous OpenGL debug messages", CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t cvr_width("width", 1280, -1, SDL_MAX_SINT32, "Initial window width", CONVAR_FLAG_SAVE);
static convar_int_t cvr_height("height", 720, -1, SDL_MAX_SINT32, "Initial window height", CONVAR_FLAG_SAVE);
static convar_int_t cvr_resizable("resizable", 1, 0, 1, "Enable/Disable window resizing", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);

static convar_int_t cvr_x("x", -1, -1, SDL_MAX_SINT32, "Initial window position (X coordinate) [-1: Centered]");
static convar_int_t cvr_y("y", -1, -1, SDL_MAX_SINT32, "Initial window position (Y coordinate) [-1: Centered]");
static convar_int_t cvr_centered_display("centered_display", 0, 0, SDL_MAX_SINT32, "Display to use for window centering", CONVAR_FLAG_SAVE);

static convar_int_t r_fps_limiter("r_fps_limiter", 300, 0, SDL_MAX_SINT32 - 1, "Max FPS, 0 to disable", CONVAR_FLAG_SAVE);
static convar_int_t r_vsync("r_vsync", 1, 0, 1, "Enable/Disable vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t r_adapative_vsync("r_adapative_vsync", 1, 0, 1, "Enable disable adaptive vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t gui_demo_window("gui_demo_window", 0, 0, 1, "Show Dear ImGui demo window", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);

/**
 * Calculate a new value for dev_console::add_log_font_width by dividing the width of the string by it's length and adding some padding
 */
static void calc_dev_font_width(const char* str)
{
    float len = SDL_utf8strlen(str);
    dev_console::add_log_font_width = (ImGui::CalcTextSize(str).x / len) + ImGui::GetStyle().ItemSpacing.x * 2;
}

void tetra::set_render_api(render_api_t api, int major, int minor)
{
    if (tetra::gl::init_counter)
        return;
    render_api = api;
    render_api_version_major = major;
    render_api_version_minor = minor;
}

/* TODO: Get Object Labels? */
static void GLAPIENTRY debug_msg_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void*)
{
    if (length >= 0)
        dc_log_trace("%.*s", length, message);
    else
        dc_log_trace("%s", message);
}

void tetra::gl_obj_label(GLenum identifier, GLuint name, const GLchar* fmt, ...)
{
    if (!gl::is_available_glObjectLabel)
        return;

    /** Spec says the minimum max label length is 256 characters, which seems like a reasonable place to limit this buffer */
    char label[256];
    va_list args;
    va_start(args, fmt);

    vsnprintf(label, SDL_arraysize(label), fmt, args);

    va_end(args);

    glObjectLabel(identifier, name, -1, label);
}

int tetra::init_gui(const char* window_title)
{
    if (!tetra::internal::is_initialized_core())
    {
        dc_log_error("[tetra_gl]: Tetra core *must* be initialized before initializing tetra_gl");
        return 0;
    }

    if (tetra::gl::init_counter++)
    {
        dc_log_warn("[tetra_gl]: Skipping initialization as tetra_gl has already been initialized (You are probably doing something wrong!)");
        return 0;
    }

    dc_log("[tetra_gl]: Init started");

    Uint64 start_tick = SDL_GetTicksNS();

    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
        util::die("Error: SDL_Init(SDL_INIT_VIDEO):\n%s\n", SDL_GetError());

    gamepad_was_init = SDL_Init(SDL_INIT_GAMEPAD);
    if (!gamepad_was_init)
        dc_log_error("Error: Unable to initialize SDL Gamepad Subsystem:\n%s\n", SDL_GetError());

#if defined(__APPLE__)
    SDL_GLContextFlag sdl_gl_context_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG; // Always required on Mac (According to Dear ImGui)
#else
    SDL_GLContextFlag sdl_gl_context_flags = 0;
#endif

    if (r_debug_gl.get())
        sdl_gl_context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, sdl_gl_context_flags);

    if (render_api == RENDER_API_GL_CORE)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    else if (render_api == RENDER_API_GL_COMPATIBILITY)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    else if (render_api == RENDER_API_GL_ES)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, render_api_version_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, render_api_version_minor);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

    if (cvr_resizable.get())
        window_flags |= SDL_WINDOW_RESIZABLE;

    if (convar_t::dev())
        window_flags &= ~SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(window_title, cvr_width.get(), cvr_height.get(), window_flags);
    if (window == nullptr)
        util::die("Error: SDL_CreateWindow():\n%s\n", SDL_GetError());

    int win_x = cvr_x.get();
    int win_y = cvr_y.get();

    if (win_x == -1)
        win_x = SDL_WINDOWPOS_CENTERED_DISPLAY(cvr_centered_display.get());

    if (win_y == -1)
        win_y = SDL_WINDOWPOS_CENTERED_DISPLAY(cvr_centered_display.get());

    SDL_SetWindowPosition(window, win_x, win_y);

    gl_context = SDL_GL_CreateContext(window);

    glGetIntegerv(GL_MAJOR_VERSION, &render_api_version_major);
    glGetIntegerv(GL_MINOR_VERSION, &render_api_version_minor);

    dc_log("Init GLEW");
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK)
        util::die("Error: Unable to initialize GLEW! (%s)\n", glewGetErrorString(glewError));

    dc_log("OpenGL info");
    dc_log("*** GL Vendor:     %s ***", glGetString(GL_VENDOR));
    dc_log("*** GL Version:    %s ***", glGetString(GL_VERSION));
    dc_log("*** GL Renderer:   %s ***", glGetString(GL_RENDERER));
    dc_log("*** GLEW Version:  %s ***", glewGetString(GLEW_VERSION));
    dc_log("*** GLSL Version:  %s ***", glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (r_debug_gl.get() && (render_api_version_major * 10000 + render_api_version_minor) >= 40003 && render_api != RENDER_API_GL_ES)
    {
        glEnable(GL_DEBUG_OUTPUT);
        if (r_debug_gl_async.get())
            glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        else
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

        glDebugMessageCallback(debug_msg_callback, NULL);

        gl::is_available_glObjectLabel = true;
    }

    SDL_GL_MakeCurrent(tetra::window, tetra::gl_context);

    SDL_ShowWindow(window);

    /* This weirdness is to trick DWM into making the window floating */
    if (convar_t::dev())
        SDL_SetWindowResizable(window, cvr_resizable.get());

    cvr_resizable.set_pre_callback([=](int, int _new) -> bool { return SDL_SetWindowResizable(window, _new); }, false);

    SDL_GL_MakeCurrent(window, gl_context);
    r_vsync.set_post_callback(
        [=]() {
            bool vsync_enable = r_vsync.get();
            bool adapative_vsync_enable = r_adapative_vsync.get();
            if (vsync_enable && adapative_vsync_enable && SDL_GL_SetSwapInterval(-1) == 0)
                return;
            SDL_GL_SetSwapInterval(vsync_enable);
        },
        true);

    /* Setup Main Dear ImGui context */
    IMGUI_CHECKVERSION();
    im_ctx_main = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    if (gamepad_was_init)
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.IniFilename = NULL; // Disable imgui.ini

    style_colors_rotate_hue(0, 160, 1, 1);

    /* Decide on GLSL version string for ImGui */
    const char* suffix = "";
    char imgui_glsl_version[128];
    int glsl_major = render_api_version_major;
    int glsl_minor = render_api_version_minor;

    if (glsl_major == 2)
    {
        glsl_major = 1;
        glsl_minor += 1;
    }
    else if (render_api != RENDER_API_GL_ES && glsl_major == 3 && glsl_minor < 3)
    {
        glsl_major = 1;
        glsl_minor += 3;
    }

    if (render_api == RENDER_API_GL_ES && glsl_major > 2)
        suffix = " es";
    if (render_api == RENDER_API_GL_CORE && glsl_major > 2)
        suffix = " core";

    snprintf(imgui_glsl_version, SDL_arraysize(imgui_glsl_version), "#version %d%d0%s", glsl_major, glsl_minor, suffix);

    dc_log_trace("Dear ImGui glsl version string: \"%s\"", imgui_glsl_version);

    /* Setup Platform/Renderer backends */
    if (!ImGui_ImplSDL3_InitForOpenGL(window, gl_context))
        util::die("Failed to initialize Dear Imgui SDL2 backend\n");
    if (!ImGui_ImplOpenGL3_Init(imgui_glsl_version))
        util::die("Failed to initialize Dear Imgui OpenGL3 backend\n");
    io.Fonts->AddFontDefault();
    ImFontConfig dc_overlay_fcfg;
    strncpy(dc_overlay_fcfg.Name, "Proggy Tiny 10px", IM_ARRAYSIZE(dc_overlay_fcfg.Name));
    dev_console::overlay_font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(proggy_tiny_compressed_data_base85, 10.0f, &dc_overlay_fcfg);

    /* Setup Overlay Context */
    im_ctx_overlay = ImGui::CreateContext(io.Fonts);
    {
        ImGui::SetCurrentContext(im_ctx_overlay);
        ImGui::GetIO().IniFilename = NULL;
        ImGui::GetIO().ConfigFlags = ImGuiConfigFlags_None;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        if (!ImGui_ImplSDL3_InitForOpenGL(window, gl_context))
            util::die("Failed to initialize Dear Imgui SDL2 backend\n");
        if (!ImGui_ImplOpenGL3_Init(imgui_glsl_version))
            util::die("Failed to initialize Dear Imgui OpenGL3 backend\n");
    }
    ImGui::SetCurrentContext(im_ctx_main);

    dc_log("[tetra_gl]: Init finished in %.1f ms", ((SDL_GetTicksNS() - start_tick) / 100000) / 10.0f);

    return 0;
}

bool tetra::process_event(SDL_Event event)
{
    if (!tetra::gl::init_counter)
        return false;

    ImGui::SetCurrentContext(im_ctx_main);

    if (im_ctx_shown_main || dev_console::shown)
        ImGui_ImplSDL3_ProcessEvent(&event);

    if (event.type == SDL_EVENT_QUIT)
        return true;

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
        return true;

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_GRAVE && event.key.repeat == 0)
        dev_console::show_hide();

    return false;
}

int tetra::start_frame(bool event_loop)
{
    if (!tetra::gl::init_counter)
        return -1;

    bool done = false;

    SDL_Event event;
    while (event_loop && !done && SDL_PollEvent(&event))
        done = process_event(event);

    // Start the Dear ImGui frame
    ImGui::SetCurrentContext(im_ctx_overlay);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetCurrentContext(im_ctx_main);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    return !done;
}

void tetra::show_imgui_ctx_main(bool shown) { im_ctx_shown_main = shown; }

bool tetra::imgui_ctx_main_wants_input() { return dev_console::shown || im_ctx_shown_main; }

void tetra::show_imgui_ctx_overlay(bool shown) { im_ctx_shown_overlay = shown; }

void tetra::end_frame(bool clear_frame, void (*cb_screenshot)(void))
{
    if (!tetra::gl::init_counter)
        return;

    ImGuiIO& io = ImGui::GetIO();

    bool open = gui_demo_window.get();
    if (open)
    {
        ImGui::ShowDemoWindow(&open);
        if (open != gui_demo_window.get())
            gui_demo_window.set(open);
    }

    gui_registrar::render_menus();

    dev_console::render();

    // Rendering
    if (clear_frame)
    {
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    ImGui::SetCurrentContext(im_ctx_main);
    if (im_ctx_shown_main || dev_console::shown)
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    else
        ImGui::EndFrame();

    ImGui::SetCurrentContext(im_ctx_overlay);
    gui_registrar::render_overlays();
    if (im_ctx_shown_overlay)
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    else
        ImGui::EndFrame();
    ImGui::SetCurrentContext(im_ctx_main);

    calc_dev_font_width("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");

    if (cb_screenshot)
        cb_screenshot();

    SDL_GL_SwapWindow(window);

    static tetra::iteration_limiter_t limiter;

    limiter.set_limit(r_fps_limiter.get());
    limiter.wait();
}

void tetra::deinit_gui()
{
    if (!tetra::internal::is_initialized_core())
        dc_log_warn("[tetra_gl]: Tetra core should be deinitialized *after* tetra_gl");

    tetra::gl::init_counter--;

    if (tetra::gl::init_counter < 0)
    {
        dc_log_error("[tetra_gl]: Init counter is less than 0, resetting to 0");
        tetra::gl::init_counter = 0;
        return;
    }

    if (tetra::gl::init_counter != 0)
        return;

    ImGui::SetCurrentContext(im_ctx_overlay);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    im_ctx_overlay = NULL;

    ImGui::SetCurrentContext(im_ctx_main);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    im_ctx_main = NULL;

    SDL_GL_DestroyContext(gl_context);
    gl_context = NULL;
    SDL_DestroyWindow(window);
    window = NULL;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (gamepad_was_init)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gamepad_was_init = false;
    }
}

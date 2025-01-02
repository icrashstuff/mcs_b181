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

#include <GL/glew.h>
#include <GL/glu.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#warning "OpenGL ES is untested"
#else
#include <SDL3/SDL_opengl.h>
#endif

#include "tetra.h"
#include "tetra/util/cli_parser.h"
#include "tetra/util/convar.h"
#include "tetra/util/convar_file.h"
#include "tetra/util/misc.h"
#include "tetra/util/physfs/physfs.h"

#include "tetra/gui/console.h"
#include "tetra/gui/gui_registrar.h"
#include "tetra/gui/proggy_tiny.cpp"
#include "tetra/gui/styles.h"

SDL_Window* tetra::window = NULL;
SDL_GLContext tetra::gl_context = NULL;

static ImGuiContext* im_ctx_main = NULL;
static ImGuiContext* im_ctx_overlay = NULL;

static bool im_ctx_shown_main = true;
static bool im_ctx_shown_overlay = true;

static bool was_init = false;
static bool was_init_gui = false;
static bool was_deinit = false;

static tetra::render_api_t render_api = tetra::RENDER_API_GL_CORE;
static int render_api_version_major = 3;
static int render_api_version_minor = 3;

static convar_int_t gfx_debug_gl("gfx_debug_gl", 0, 0, 1, "Sets SDL_GL_CONTEXT_DEBUG_FLAG", CONVAR_FLAG_DEV_ONLY);

static convar_int_t gui_fps_limiter("gui_fps_limiter", 300, 0, SDL_MAX_SINT32 - 1, "Max FPS, 0 to disable", CONVAR_FLAG_SAVE);
static convar_int_t gui_vsync("gui_vsync", 1, 0, 1, "Enable/Disable vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t gui_adapative_vsync("gui_adapative_vsync", 1, 0, 1, "Enable disable adaptive vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t gui_show_demo_window("gui_show_demo_window", 0, 0, 1, "Show Dear ImGui demo window", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);

ImFont* dev_console::overlay_font = NULL;

std::atomic<float> dev_console::add_log_font_width = { 7.0f };

/**
 * Calculate a new value for dev_console::add_log_font_width by dividing the width of the string by it's length and adding some padding
 */
static void calc_dev_font_width(const char* str)
{
    float len = SDL_utf8strlen(str);
    dev_console::add_log_font_width = (ImGui::CalcTextSize(str).x / len) + ImGui::GetStyle().ItemSpacing.x * 2;
}

void tetra::init(const char* organization, const char* appname, const char* cfg_path_prefix, int argc, const char** argv)
{
    if (was_init)
        return;

    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, appname);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, __DATE__);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, organization);

    dc_log("Init main");

    was_init = true;

    convar_t::atexit_init();
    atexit(convar_t::atexit_callback);

    /* Parse command line */
    cli_parser::parse(argc, argv);

    {
        convar_int_t* dev = (convar_int_t*)convar_t::get_convar("dev");

        /* Set dev before any other variables in case their callbacks require dev */
        if (cli_parser::get_value(dev->get_name()))
            dev->set(1);
        dev->set_pre_callback([=](int, int) -> bool { return false; });
    }

    if (convar_t::dev())
    {
        /* KDevelop fully buffers the output and will not display anything */
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        fflush(stdout);
        fflush(stderr);
        dc_log("Developer convar set");

        convar_int_t* console_overlay = (convar_int_t*)convar_t::get_convar("console_overlay");

        if (console_overlay)
            console_overlay->set(3);
    }

    PHYSFS_init(argv[0]);
    PHYSFS_setSaneConfig(organization, appname, NULL, 0, 0);

    /* Set convars from config */
    convar_file_parser::set_config_prefix(cfg_path_prefix);
    convar_file_parser::read();

    /* Set convars from command line */
    cli_parser::apply();

    if (cli_parser::get_value("-help") || cli_parser::get_value("help") || cli_parser::get_value("h"))
    {
        dc_log_internal("Usage: %s [ -convar_name [convar_value], ...]", argv[0]);
        dc_log_internal("");
        dc_log_internal("Example:");
        dc_log_internal("  %s -dev -%s %d", argv[0], gui_vsync.get_name(), gui_vsync.get());
        dc_log_internal("");
        dc_log_internal("List of all console variables *without* flag CONVAR_FLAG_DEV_ONLY and associated help text (In no particular order)");
        dc_log_internal("==================================================================================================================");
        std::vector<convar_t*>* cvrs = convar_t::get_convar_list();
        for (size_t i = 0; i < cvrs->size(); i++)
        {
            if (cvrs->at(i)->get_convar_flags() & CONVAR_FLAG_DEV_ONLY)
                continue;
            cvrs->at(i)->log_help();
            dc_log_internal("");
        }
        if (convar_t::dev())
        {
            dc_log_internal("List of all console variables with flag CONVAR_FLAG_DEV_ONLY and associated help text (In no particular order)");
            dc_log_internal("==============================================================================================================");
            for (size_t i = 0; i < cvrs->size(); i++)
            {
                if (!(cvrs->at(i)->get_convar_flags() & CONVAR_FLAG_DEV_ONLY))
                    continue;
                cvrs->at(i)->log_help();
                dc_log_internal("");
            }
        }
        else
        {
            dc_log_internal("Console variables with flag CONVAR_FLAG_DEV_ONLY omitted, add `-dev` to the command line to list them.");
        }
        exit(0);
    }

    const PHYSFS_ArchiveInfo** supported_archives = PHYSFS_supportedArchiveTypes();

    for (int i = 0; supported_archives[i] != NULL; i++)
        dc_log("Supported archive: [%s]", supported_archives[i]->extension);
}

void tetra::set_render_api(render_api_t api, int major, int minor)
{
    if (was_init_gui)
        return;
    render_api = api;
    render_api_version_major = major;
    render_api_version_minor = minor;
}

int tetra::init_gui(const char* window_title)
{
    if (was_init_gui || !was_init)
        return 0;

    dc_log("Init gui");

    Uint64 start_tick = SDL_GetTicksNS();

    was_init_gui = true;

    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
        util::die("Error: SDL_Init(SDL_INIT_VIDEO):\n%s\n", SDL_GetError());

    bool gamepad_was_init = SDL_Init(SDL_INIT_GAMEPAD);
    if (!gamepad_was_init)
        dc_log_error("Error: Unable to initialize SDL Gamepad Subsystem:\n%s\n", SDL_GetError());

#if defined(__APPLE__)
    SDL_GLContextFlag sdl_gl_context_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG; // Always required on Mac (According to Dear ImGui)
#else
    SDL_GLContextFlag sdl_gl_context_flags = 0;
#endif

    if (gfx_debug_gl.get())
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
    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

    if (convar_t::dev())
        window_flags &= ~SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(window_title, 1280, 720, window_flags);
    if (window == nullptr)
        util::die("Error: SDL_CreateWindow():\n%s\n", SDL_GetError());

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(tetra::window, tetra::gl_context);

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

    SDL_ShowWindow(window);

    /* This weirdness is to trick DWM into making the window floating */
    if (convar_t::dev())
        SDL_SetWindowResizable(window, true);

    gl_context = SDL_GL_CreateContext(window);

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
    im_ctx_main = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    if (gamepad_was_init)
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.IniFilename = NULL; // Disable imgui.ini

    style_colors_rotate_hue(0, 160, 1, 1);

    glGetIntegerv(GL_MAJOR_VERSION, &render_api_version_major);
    glGetIntegerv(GL_MINOR_VERSION, &render_api_version_minor);

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

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL3_InitForOpenGL(window, gl_context))
        util::die("Failed to initialize Dear Imgui SDL2 backend\n");
    if (!ImGui_ImplOpenGL3_Init(imgui_glsl_version))
        util::die("Failed to initialize Dear Imgui OpenGL3 backend\n");
    io.Fonts->AddFontDefault();
    ImFontConfig dc_overlay_fcfg;
    strncpy(dc_overlay_fcfg.Name, "Proggy Tiny 10px", IM_ARRAYSIZE(dc_overlay_fcfg.Name));
    dev_console::overlay_font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(proggy_tiny_compressed_data_base85, 10.0f, &dc_overlay_fcfg);

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

    dc_log("Init gui finished in %.1f ms", ((SDL_GetTicksNS() - start_tick) / 100000) / 10.0f);

    return 0;
}

bool tetra::process_event(SDL_Event event)
{
    if (!was_init_gui)
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
    if (!was_init_gui)
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

void tetra::end_frame(bool clear_frame)
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
    if (was_deinit)
        return;
    was_deinit = true;

    convar_file_parser::write();

    convar_t::atexit_callback();

    if (was_init_gui)
    {
        ImGui::SetCurrentContext(im_ctx_overlay);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        ImGui::SetCurrentContext(im_ctx_main);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
    }

    PHYSFS_deinit();
}

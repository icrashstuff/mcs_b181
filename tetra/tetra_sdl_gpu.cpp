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

#include "tetra_sdl_gpu.h"
#include "tetra_core.h"

#include "tetra_internal.h"

#include "gui/imgui.h"
#include "gui/imgui/backends/imgui_impl_sdl3.h"
#include "gui/imgui/backends/imgui_impl_sdlgpu3.h"

#include "util/convar.h"
#include "util/misc.h"

#include "gui/console.h"
#include "gui/gui_registrar.h"
#include "gui/proggy_tiny.cpp"
#include "gui/styles.h"

namespace tetra
{
namespace sdl_gpu
{
    int init_counter = 0;
}
}

SDL_Window* tetra::window = NULL;
SDL_GPUDevice* tetra::gpu_device = NULL;

static ImGuiContext* im_ctx_main = NULL;
static ImGuiContext* im_ctx_overlay = NULL;

static bool gamepad_was_init = false;

static bool im_ctx_shown_main = true;
static bool im_ctx_shown_overlay = true;

static convar_int_t r_debug_gpu {
    "r_debug_gpu",
    false,
    false,
    true,
    "Attempt to create SDL_GPUDevice in debug mode (Gracefully fails)",
    CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY,
};

static convar_int_t cvr_width("width", 1280, -1, SDL_MAX_SINT32, "Initial window width", CONVAR_FLAG_SAVE);
static convar_int_t cvr_height("height", 720, -1, SDL_MAX_SINT32, "Initial window height", CONVAR_FLAG_SAVE);
static convar_int_t cvr_resizable("resizable", 1, 0, 1, "Enable/Disable window resizing", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);

static convar_int_t cvr_x("x", -1, -1, SDL_MAX_SINT32, "Initial window position (X coordinate) [-1: Centered]");
static convar_int_t cvr_y("y", -1, -1, SDL_MAX_SINT32, "Initial window position (Y coordinate) [-1: Centered]");
static convar_int_t cvr_centered_display("centered_display", 0, 0, SDL_MAX_SINT32, "Display to use for window centering", CONVAR_FLAG_SAVE);

static convar_int_t r_fps_limiter("r_fps_limiter", 300, 0, SDL_MAX_SINT32 - 1, "Max FPS, 0 to disable", CONVAR_FLAG_SAVE);
static convar_int_t r_vsync("r_vsync", 1, 0, 1, "Enable/Disable vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);
static convar_int_t gui_demo_window("gui_demo_window", 0, 0, 1, "Show Dear ImGui demo window", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);

/**
 * Calculate a new value for dev_console::add_log_font_width by dividing the width of the string by it's length and adding some padding
 */
static void calc_dev_font_width(const char* str)
{
    float len = SDL_utf8strlen(str);
    dev_console::add_log_font_width = (ImGui::CalcTextSize(str).x / len) + ImGui::GetStyle().ItemSpacing.x * 2;
}

#define CHECK_REMOVE_AND_NAME_SHADER_FLAG(VAR_IN, VAR_STR, NAME) \
    do                                                           \
    {                                                            \
        if ((VAR_IN) & (SDL_GPU_SHADERFORMAT_##NAME))            \
        {                                                        \
            (VAR_IN) &= ~(SDL_GPU_SHADERFORMAT_##NAME);          \
            (VAR_STR) += #NAME "|";                              \
        }                                                        \
    } while (0)
std::string tetra::SDL_GPUShaderFormat_to_string(SDL_GPUShaderFormat x)
{
    std::string s = "(";
    CHECK_REMOVE_AND_NAME_SHADER_FLAG(x, s, PRIVATE);
    CHECK_REMOVE_AND_NAME_SHADER_FLAG(x, s, SPIRV);
    CHECK_REMOVE_AND_NAME_SHADER_FLAG(x, s, DXBC);
    CHECK_REMOVE_AND_NAME_SHADER_FLAG(x, s, DXIL);
    CHECK_REMOVE_AND_NAME_SHADER_FLAG(x, s, MSL);
    CHECK_REMOVE_AND_NAME_SHADER_FLAG(x, s, METALLIB);

    if (x)
        s += "UNKNOWN|";

    if (s.back() == '|')
        s.back() = ')';
    else
        s += ")";

    return s;
}
#undef CHECK_REMOVE_AND_NAME_SHADER_FLAG

SDL_GPUShaderFormat tetra::get_shaders_supported_by_driver(const char* const driver_name)
{
    SDL_GPUShaderFormat out = 0;

    for (SDL_GPUShaderFormat flag = 1; flag != 0; flag <<= 1)
        if (SDL_GPUSupportsShaderFormats(flag, driver_name))
            out |= flag;

    return out;
}

static bool swapchain_should_reconfigure = 1;
static SDL_GPUSwapchainComposition swapchain_composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
static SDL_GPUPresentMode swapchain_present_mode = SDL_GPU_PRESENTMODE_VSYNC;

void tetra::configure_swapchain_if_needed()
{
    if (!swapchain_should_reconfigure)
        return;
    swapchain_should_reconfigure = 0;

    if (!SDL_WindowSupportsGPUSwapchainComposition(gpu_device, window, swapchain_composition))
    {
        swapchain_composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        dc_log_error("SDL_WindowSupportsGPUSwapchainComposition() returned false, falling back to SDL_GPU_SWAPCHAINCOMPOSITION_SDR");
    }

    if (!SDL_WindowSupportsGPUPresentMode(gpu_device, window, swapchain_present_mode))
    {
        swapchain_present_mode = SDL_GPU_PRESENTMODE_VSYNC;
        dc_log_error("SDL_WindowSupportsGPUPresentMode() returned false, falling back to SDL_GPU_PRESENTMODE_VSYNC");
    }

    SDL_SetGPUSwapchainParameters(gpu_device, window, swapchain_composition, swapchain_present_mode);
}

SDL_GPUShaderFormat tetra::get_imgui_shader_formats() { return SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL; }

int tetra::init_gui(const char* const window_title, const SDL_GPUShaderFormat shader_formats_application)
{
    if (!tetra::internal::is_initialized_core())
    {
        dc_log_error("[tetra_sdl_gpu]: Tetra core *must* be initialized before initializing tetra_sdl_gpu");
        return 0;
    }

    if (tetra::sdl_gpu::init_counter++)
    {
        dc_log_warn("[tetra_sdl_gpu]: Skipping initialization as tetra_sdl_gpu has already been initialized (You are probably doing something wrong!)");
        return 0;
    }

    dc_log("[tetra_sdl_gpu]: Init started");

    Uint64 start_tick = SDL_GetTicksNS();

    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
        util::die("Error: SDL_Init(SDL_INIT_VIDEO):\n%s\n", SDL_GetError());

    gamepad_was_init = SDL_Init(SDL_INIT_GAMEPAD);
    if (!gamepad_was_init)
        dc_log_error("Error: Unable to initialize SDL Gamepad Subsystem:\n%s\n", SDL_GetError());

    Uint32 window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

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

    /* ================ BEGIN: Create tetra::gpu_device ================ */
    dc_log("Init SDL_GPU");

    /* Temporarily raise log priority for SDL_GPU */
    SDL_LogPriority old_log_priority = SDL_GetLogPriority(SDL_LOG_CATEGORY_GPU);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_TRACE);

    dc_log("Available GPU Drivers: %d", SDL_GetNumGPUDrivers());
    for (int i = 0; i < SDL_GetNumGPUDrivers(); i++)
    {
        const char* driver_name = SDL_GetGPUDriver(i);
        dc_log("- Driver %d: \"%s\"", i, driver_name);
    }

    const SDL_GPUShaderFormat shader_formats_imgui = get_imgui_shader_formats();
    const SDL_GPUShaderFormat shader_formats_common = shader_formats_application & shader_formats_imgui;

    dc_log("Shader formats supported by Application: 0x%08X %s", shader_formats_application, SDL_GPUShaderFormat_to_string(shader_formats_application).c_str());
    dc_log("Shader formats supported by ImGui: 0x%08X %s", shader_formats_imgui, SDL_GPUShaderFormat_to_string(shader_formats_imgui).c_str());
    dc_log("Shader formats supported by Application and ImGui: 0x%08X %s", shader_formats_common, SDL_GPUShaderFormat_to_string(shader_formats_common).c_str());

    if (shader_formats_common == 0)
        util::die("No common shader format detected!\n"
                  "Formats supported by Application: 0x%08X %s\n"
                  "Formats supported by ImGui: 0x%08X %s",
            shader_formats_application, SDL_GPUShaderFormat_to_string(shader_formats_application).c_str(), shader_formats_imgui,
            SDL_GPUShaderFormat_to_string(shader_formats_imgui).c_str());

    tetra::gpu_device = SDL_CreateGPUDevice(shader_formats_common, r_debug_gpu.get(), NULL);
    if (r_debug_gpu.get() && !tetra::gpu_device)
    {
        dc_log_error("Failed to acquire debug device!, trying to acquire non-debug device :(");
        tetra::gpu_device = SDL_CreateGPUDevice(shader_formats_common, false, NULL);
    }
    if (!tetra::gpu_device)
        util::die("SDL_CreateGPUDevice() failed: %s", SDL_GetError());

    dc_log("GPU context driver: \"%s\"", SDL_GetGPUDeviceDriver(tetra::gpu_device));
    dc_log("GPU context shader formats: %s", SDL_GPUShaderFormat_to_string(SDL_GetGPUShaderFormats(tetra::gpu_device)).c_str());

    if (!SDL_ClaimWindowForGPUDevice(tetra::gpu_device, tetra::window))
        util::die("SDL_ClaimWindowForGPUDevice() failed: %s", SDL_GetError());

    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, old_log_priority);

    /* ================ END: Create tetra::gpu_device ================ */

    /* This weirdness is to trick DWM (The suckless project not the windows component) into making the window floating */
    SDL_ShowWindow(window);
    if (convar_t::dev())
        SDL_SetWindowResizable(window, cvr_resizable.get());

    cvr_resizable.set_pre_callback([=](int, int _new) -> bool { return SDL_SetWindowResizable(window, _new); }, false);

    r_vsync.set_post_callback(
        [=]() {
            bool vsync_enable = r_vsync.get();
            swapchain_should_reconfigure = 1;
            if (vsync_enable)
                swapchain_present_mode = SDL_GPU_PRESENTMODE_VSYNC;
            else if (SDL_WindowSupportsGPUPresentMode(gpu_device, window, SDL_GPU_PRESENTMODE_MAILBOX))
                swapchain_present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
            else if (SDL_WindowSupportsGPUPresentMode(gpu_device, window, SDL_GPU_PRESENTMODE_IMMEDIATE))
                swapchain_present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
            else
                swapchain_present_mode = SDL_GPU_PRESENTMODE_VSYNC;
        },
        true);

    configure_swapchain_if_needed();
    ImGui_ImplSDLGPU3_InitInfo imgui_init_info = {};
    imgui_init_info.Device = gpu_device;
    imgui_init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
    imgui_init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;

    /* ================ BEGIN: Setup Main Dear ImGui context ================ */
    IMGUI_CHECKVERSION();
    im_ctx_main = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    if (gamepad_was_init)
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.IniFilename = NULL; // Disable imgui.ini

    style_colors_rotate_hue(0, 160, 1, 1);

    if (!ImGui_ImplSDL3_InitForSDLGPU(window))
        util::die("Failed to initialize Dear Imgui SDL3 backend\n");
    if (!ImGui_ImplSDLGPU3_Init(&imgui_init_info))
        util::die("Failed to initialize Dear Imgui SDLGPU3 backend\n");
    io.Fonts->AddFontDefault();
    /* ================ END: Setup Main Dear ImGui context ================ */
    //
    //
    //
    //
    //
    /* ================ BEGIN: Setup Overlay Dear ImGui context ================ */
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
        if (!ImGui_ImplSDL3_InitForSDLGPU(window))
            util::die("Failed to initialize Dear Imgui SDL3 backend\n");
        if (!ImGui_ImplSDLGPU3_Init(&imgui_init_info)) /* TODO: Replace this with a stub backend */
            util::die("Failed to initialize Dear Imgui SDLGPU3 backend\n");
    }
    ImGui::SetCurrentContext(im_ctx_main);
    /* ================ END: Setup Overlay Dear ImGui context ================ */

    dc_log("[tetra_sdl_gpu]: Init finished in %.1f ms", ((SDL_GetTicksNS() - start_tick) / 100000) / 10.0f);

    return 0;
}

bool tetra::process_event(SDL_Event event)
{
    if (!tetra::sdl_gpu::init_counter)
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

void tetra::show_imgui_ctx_main(bool shown) { im_ctx_shown_main = shown; }

bool tetra::imgui_ctx_main_wants_input() { return dev_console::shown || im_ctx_shown_main; }

void tetra::show_imgui_ctx_overlay(bool shown) { im_ctx_shown_overlay = shown; }

int tetra::start_frame(bool event_loop)
{
    if (!tetra::sdl_gpu::init_counter)
        return -1;

    bool done = false;

    SDL_Event event;
    while (event_loop && !done && SDL_PollEvent(&event))
        done = process_event(event);

    // Start the Dear ImGui frame
    ImGui::SetCurrentContext(im_ctx_overlay);
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetCurrentContext(im_ctx_main);

    bool show_main = (im_ctx_shown_main || dev_console::shown);
    static bool show_main_last = show_main;

    /* Prevent main context from messing with the cursor */
    if (show_main)
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    /* Reset cursor if necessary */
    if (show_main == false && show_main_last == true)
    {
        if (ImGui::GetMouseCursor() != ImGuiMouseCursor_Arrow)
            SDL_SetCursor(SDL_GetDefaultCursor());
        if (!ImGui::GetIO().MouseDrawCursor)
            SDL_ShowCursor();
    }
    show_main_last = show_main;

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    return !done;
}

void tetra::end_frame()
{
    if (!tetra::sdl_gpu::init_counter)
        return;

    tetra::configure_swapchain_if_needed();

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(tetra::gpu_device);
    SDL_GPUTexture* swapchain_texture = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, tetra::window, &swapchain_texture, nullptr, nullptr);

    tetra::end_frame(command_buffer, swapchain_texture, true);

    SDL_SubmitGPUCommandBuffer(command_buffer);

    tetra::limit_framerate();
}

void tetra::end_frame(SDL_GPUCommandBuffer* const command_buffer, SDL_GPUTexture* const texture, bool clear_texture)
{
    if (!tetra::sdl_gpu::init_counter)
        return;

    bool open = gui_demo_window.get();
    if (open)
    {
        ImGui::ShowDemoWindow(&open);
        if (open != gui_demo_window.get())
            gui_demo_window.set(open);
    }

    gui_registrar::render_menus();

    ImGui::SetCurrentContext(im_ctx_main);
    calc_dev_font_width("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
    dev_console::render();

    ImDrawData* draw_data_main = nullptr;
    ImDrawData* draw_data_over = nullptr;

    if (im_ctx_shown_main || dev_console::shown)
    {
        ImGui::Render();
        draw_data_main = ImGui::GetDrawData();
        if (draw_data_main->DisplaySize.x <= 0.0f || draw_data_main->DisplaySize.y <= 0.0f)
            draw_data_main = nullptr;
    }
    else
        ImGui::EndFrame();

    ImGui::SetCurrentContext(im_ctx_overlay);
    gui_registrar::render_overlays();
    if (im_ctx_shown_overlay)
    {
        ImGui::Render();
        draw_data_over = ImGui::GetDrawData();
        if (draw_data_over->DisplaySize.x <= 0.0f || draw_data_over->DisplaySize.y <= 0.0f)
            draw_data_over = nullptr;
    }
    else
        ImGui::EndFrame();
    ImGui::SetCurrentContext(im_ctx_main);

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = texture;
    target_info.load_op = (clear_texture ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD);
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.clear_color = SDL_FColor { 0, 0, 0, 1 };
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;

    ImDrawData* draw_data = nullptr;

    if (draw_data_main && !draw_data_over)
        draw_data = draw_data_main;

    if (!draw_data_main && draw_data_over)
        draw_data = draw_data_over;

    if (draw_data_main && draw_data_over)
    {
        draw_data = draw_data_main;
        for (auto it : draw_data_over->CmdLists)
            draw_data->AddDrawList(it);
    }

    if (draw_data != nullptr && command_buffer != nullptr && texture != nullptr)
    {
        SDL_PushGPUDebugGroup(command_buffer, "[tetra]: Render ImGui");
        ImGui::SetCurrentContext(im_ctx_main);
        Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
        if (render_pass)
        {
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
            SDL_EndGPURenderPass(render_pass);
        }

        SDL_PopGPUDebugGroup(command_buffer);
    }
}

void tetra::limit_framerate()
{
    static tetra::iteration_limiter_t limiter;

    limiter.set_limit(r_fps_limiter.get());
    limiter.wait();
}

void tetra::deinit_gui()
{
    if (!tetra::internal::is_initialized_core())
        dc_log_warn("[tetra_sdl_gpu]: Tetra core should be deinitialized *after* tetra_sdl_gpu");

    tetra::sdl_gpu::init_counter--;

    if (tetra::sdl_gpu::init_counter < 0)
    {
        dc_log_error("[tetra_sdl_gpu]: Init counter is less than 0, resetting to 0");
        tetra::sdl_gpu::init_counter = 0;
        return;
    }

    if (tetra::sdl_gpu::init_counter != 0)
        return;

    ImGui::SetCurrentContext(im_ctx_overlay);
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    im_ctx_overlay = NULL;

    ImGui::SetCurrentContext(im_ctx_main);
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    im_ctx_main = NULL;

    SDL_DestroyGPUDevice(gpu_device);
    gpu_device = NULL;
    SDL_DestroyWindow(window);
    window = NULL;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (gamepad_was_init)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gamepad_was_init = false;
    }
}

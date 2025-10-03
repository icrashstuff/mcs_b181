/* SPDX-License-Identifier: MIT
 *
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

#include "tetra_vulkan.h"
#include "tetra_core.h"

#include "tetra_internal.h"

#include "gui/imgui.h"
#include "gui/imgui/backends/imgui_impl_sdl3.h"
#include "gui/imgui/backends/imgui_impl_vulkan.h"

#include "util/convar.h"
#include "util/misc.h"

#include "gui/console.h"
#include "gui/gui_registrar.h"
#include "gui/proggy_tiny.cpp"
#include "gui/styles.h"

namespace tetra
{
convar_int_t r_vsync("r_vsync", 1, 0, 1, "Enable/Disable vsync", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);

ImGuiContext* im_ctx_main = NULL;
ImGuiContext* im_ctx_overlay = NULL;
namespace vulkan
{
    int init_counter = 0;

    vulkan_backend_init_info_t init_info;
}
}

static bool gamepad_was_init = false;

static bool im_ctx_shown_main = true;
static bool im_ctx_shown_overlay = true;

static convar_int_t cvr_width("width", 1280, -1, SDL_MAX_SINT32, "Initial window width", CONVAR_FLAG_SAVE);
static convar_int_t cvr_height("height", 720, -1, SDL_MAX_SINT32, "Initial window height", CONVAR_FLAG_SAVE);
static convar_int_t cvr_resizable("resizable", 1, 0, 1, "Enable/Disable window resizing", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_SAVE);

static convar_int_t cvr_x("x", -1, -1, SDL_MAX_SINT32, "Initial window position (X coordinate) [-1: Centered]");
static convar_int_t cvr_y("y", -1, -1, SDL_MAX_SINT32, "Initial window position (Y coordinate) [-1: Centered]");
static convar_int_t cvr_centered_display("centered_display", 0, 0, SDL_MAX_SINT32, "Display to use for window centering", CONVAR_FLAG_SAVE);

static convar_int_t r_fps_limiter("r_fps_limiter", 300, 0, SDL_MAX_SINT32 - 1, "Max FPS, 0 to disable", CONVAR_FLAG_SAVE);
static convar_int_t gui_demo_window("gui_demo_window", 0, 0, 1, "Show Dear ImGui demo window", CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY);

/**
 * Calculate a new value for dev_console::add_log_font_width by dividing the width of the string by it's length and adding some padding
 */
static void calc_dev_font_width(const char* str)
{
    float len = SDL_utf8strlen(str);
    dev_console::add_log_font_width = (ImGui::CalcTextSize(str).x / len) + ImGui::GetStyle().ItemSpacing.x * 2;
}

struct scoped_imgui_context_t
{
    scoped_imgui_context_t(ImGuiContext* ctx)
    {
        prev_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(ctx);
    }
    ~scoped_imgui_context_t() { ImGui::SetCurrentContext(prev_ctx); }
    ImGuiContext* prev_ctx = nullptr;
};

int tetra::init_gui(const vulkan_backend_init_info_t& _init_info)
{
    if (!tetra::internal::is_initialized_core())
    {
        dc_log_error("[tetra_vulkan]: Tetra core *must* be initialized before initializing tetra_vulkan");
        return 1;
    }

    if (tetra::vulkan::init_counter++)
    {
        dc_log_warn("[tetra_vulkan]: Skipping initialization as tetra_vulkan has already been initialized (You are probably doing something wrong!)");
        return 1;
    }

    dc_log("[tetra_vulkan]: Init started");
    scoped_imgui_context_t _set_ctx(nullptr);

    Uint64 start_tick = SDL_GetTicksNS();

    vulkan::init_info = _init_info;

    gamepad_was_init = SDL_Init(SDL_INIT_GAMEPAD);
    if (!gamepad_was_init)
        dc_log_error("Error: Unable to initialize SDL Gamepad Subsystem:\n%s\n", SDL_GetError());

    /* This weirdness is to trick DWM (The suckless project not the windows component) into making the window floating */
    SDL_HideWindow(vulkan::init_info.window);
    if (convar_t::dev() && cvr_resizable.get())
        SDL_SetWindowResizable(vulkan::init_info.window, 0);

    SDL_SetWindowSize(vulkan::init_info.window, cvr_width.get(), cvr_height.get());

    int win_x = cvr_x.get();
    int win_y = cvr_y.get();

    if (win_x == -1)
        win_x = SDL_WINDOWPOS_CENTERED_DISPLAY(cvr_centered_display.get());

    if (win_y == -1)
        win_y = SDL_WINDOWPOS_CENTERED_DISPLAY(cvr_centered_display.get());

    SDL_SetWindowPosition(vulkan::init_info.window, win_x, win_y);

    /* This weirdness is to trick DWM (The suckless project not the windows component) into making the window floating */
    SDL_ShowWindow(vulkan::init_info.window);
    if (convar_t::dev())
        SDL_SetWindowResizable(vulkan::init_info.window, cvr_resizable.get());

    cvr_resizable.set_pre_callback([=](int, int _new) -> bool { return SDL_SetWindowResizable(vulkan::init_info.window, _new); }, false);

    /* ================ BEGIN: Setup Main Dear ImGui context ================ */
    IMGUI_CHECKVERSION();
    ImGui::SetCurrentContext(im_ctx_main = ImGui::CreateContext());
    ImGuiIO& io_main = ImGui::GetIO();
    (void)io_main;
    io_main.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    if (gamepad_was_init)
        io_main.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io_main.IniFilename = NULL; // Disable imgui.ini

    style_colors_rotate_hue(0, 160, 1, 1);

    if (!ImGui_ImplSDL3_InitForVulkan(vulkan::init_info.window))
        util::die("Failed to initialize Dear Imgui SDL3 backend\n");

    const ImGuiBackendFlags sdl_backend_flags = io_main.BackendFlags;
    io_main.BackendFlags = ImGuiBackendFlags_None;

    ImGui_ImplVulkan_InitInfo cinfo_imgui = {};
    cinfo_imgui.ApiVersion = vulkan::init_info.instance_api_version;
    cinfo_imgui.Instance = vulkan::init_info.instance;
    cinfo_imgui.PhysicalDevice = vulkan::init_info.physical;
    cinfo_imgui.Device = vulkan::init_info.device;

    cinfo_imgui.Queue = vulkan::init_info.queue;
    cinfo_imgui.QueueFamily = vulkan::init_info.queue_family;
    cinfo_imgui.ImageCount = cinfo_imgui.MinImageCount = 2;

    cinfo_imgui.DescriptorPoolSize = (IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1) * 4;
    cinfo_imgui.UseDynamicRendering = true;

    cinfo_imgui.PipelineCache = vulkan::init_info.pipeline_cache;

    cinfo_imgui.QueueLockData = vulkan::init_info.queue_lock;
    cinfo_imgui.QueueLockFn = [](void* m) { SDL_LockMutex(static_cast<SDL_Mutex*>(m)); };
    cinfo_imgui.QueueUnlockFn = [](void* m) { SDL_UnlockMutex(static_cast<SDL_Mutex*>(m)); };

    cinfo_imgui.PipelineInfoMain = vulkan::init_info.pipeline_create_info;

    cinfo_imgui.Allocator = vulkan::init_info.allocation_callbacks;

    cinfo_imgui.MinAllocationSize = 256 * 1024;

    if (!ImGui_ImplVulkan_Init(&cinfo_imgui))
        util::die("Failed to initialize Dear Imgui Vulkan backend\n");

    const ImGuiBackendFlags vulkan_backend_flags = io_main.BackendFlags;
    io_main.BackendFlags |= sdl_backend_flags;

    io_main.Fonts->AddFontDefault();
    /* ================ END: Setup Main Dear ImGui context ================ */
    //
    //
    //
    //
    //
    /* ================ BEGIN: Setup Overlay Dear ImGui context ================ */
    ImFontConfig dc_overlay_fcfg;
    strncpy(dc_overlay_fcfg.Name, "Proggy Tiny 10px", IM_ARRAYSIZE(dc_overlay_fcfg.Name));
    dev_console::overlay_font = io_main.Fonts->AddFontFromMemoryCompressedBase85TTF(proggy_tiny_compressed_data_base85, 10.0f, &dc_overlay_fcfg);

    im_ctx_overlay = ImGui::CreateContext(io_main.Fonts);
    {
        ImGui::SetCurrentContext(im_ctx_overlay);
        ImGuiIO& io_overlay = ImGui::GetIO();
        io_overlay.IniFilename = NULL;
        io_overlay.ConfigFlags = ImGuiConfigFlags_None;
        io_overlay.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io_overlay.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
        io_overlay.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        io_overlay.BackendPlatformName = "tetra_vulkan_overlay_platform";
        io_overlay.BackendRendererUserData = io_main.BackendRendererUserData;
        io_overlay.BackendRendererName = "tetra_vulkan_overlay_renderer";
        io_overlay.BackendFlags |= vulkan_backend_flags;
    }
    ImGui::SetCurrentContext(im_ctx_main);
    /* ================ END: Setup Overlay Dear ImGui context ================ */

    dc_log("[tetra_vulkan]: Init finished in %.1f ms", ((SDL_GetTicksNS() - start_tick) / 100000) / 10.0f);

    return 0;
}

bool tetra::process_event(const SDL_Event& event)
{
    if (!tetra::vulkan::init_counter)
        return false;

    if (imgui_ctx_main_wants_input())
    {
        scoped_imgui_context_t _set_ctx(im_ctx_main);
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    if (event.type == SDL_EVENT_QUIT)
        return true;

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(vulkan::init_info.window))
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
    if (!tetra::vulkan::init_counter)
        return -1;

    scoped_imgui_context_t _set_null_ctx(nullptr);

    bool done = false;

    SDL_Event event;
    while (event_loop && !done && SDL_PollEvent(&event))
        done = process_event(event);

    scoped_imgui_context_t _set_ctx(im_ctx_main);
    ImGuiIO& io_main = ImGui::GetIO();
    ImGui::SetCurrentContext(im_ctx_overlay);
    ImGuiIO& io_overlay = ImGui::GetIO();
    ImGui::SetCurrentContext(im_ctx_main);

    bool show_main = (im_ctx_shown_main || dev_console::shown);
    static bool show_main_last = show_main;

    /* Prevent main context from messing with the cursor */
    if (show_main)
        io_main.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else
        io_main.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    /* Reset cursor if necessary */
    if (show_main == false && show_main_last == true)
    {
        if (ImGui::GetMouseCursor() != ImGuiMouseCursor_Arrow)
            SDL_SetCursor(SDL_GetDefaultCursor());
        if (!io_main.MouseDrawCursor)
            SDL_ShowCursor();
    }
    show_main_last = show_main;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetCurrentContext(im_ctx_overlay);
    io_overlay.DisplaySize = io_main.DisplaySize;
    io_overlay.DisplayFramebufferScale = io_main.DisplayFramebufferScale;
    io_overlay.DeltaTime = io_main.DeltaTime;
    ImGui::NewFrame();

    ImGui::SetCurrentContext(im_ctx_main);

    return !done;
}

void tetra::render_frame(VkCommandBuffer const command_buffer)
{
    if (!tetra::vulkan::init_counter)
        return;

    scoped_imgui_context_t _set_ctx(im_ctx_main);

    bool open = gui_demo_window.get();
    if (open)
    {
        ImGui::ShowDemoWindow(&open);
        if (open != gui_demo_window.get())
            gui_demo_window.set(open);
    }

    gui_registrar::render_menus();

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

    if (draw_data != nullptr && command_buffer != nullptr)
    {
        VkDebugUtilsLabelEXT linfo = {};
        linfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        linfo.pLabelName = "[tetra]: Render ImGui";
        linfo.color[0] = 0.5f;
        linfo.color[1] = 0.0f;
        linfo.color[2] = 1.0f;
        linfo.color[3] = 1.0f;
        if (vulkan::init_info.vkCmdBeginDebugUtilsLabelEXT)
            vulkan::init_info.vkCmdBeginDebugUtilsLabelEXT(command_buffer, &linfo);

        ImGui::SetCurrentContext(im_ctx_main);

        ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);

        if (vulkan::init_info.vkCmdEndDebugUtilsLabelEXT)
            vulkan::init_info.vkCmdEndDebugUtilsLabelEXT(command_buffer);
    }
}

void tetra::set_image_count(const Uint32 image_count)
{
    scoped_imgui_context_t _set_ctx(im_ctx_main);
    vulkan::init_info.image_count = image_count;
    ImGui_ImplVulkan_SetMinImageCount(image_count);
}

void tetra::set_pipeline_create_info(const ImGui_ImplVulkan_PipelineInfo& info)
{
    scoped_imgui_context_t _set_ctx(im_ctx_main);
    vulkan::init_info.pipeline_create_info = info;
    ImGui_ImplVulkan_CreateMainPipeline(&info);
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
        dc_log_warn("[tetra_vulkan]: Tetra core should be deinitialized *after* tetra_vulkan");

    tetra::vulkan::init_counter--;

    if (tetra::vulkan::init_counter < 0)
    {
        dc_log_error("[tetra_vulkan]: Init counter is less than 0, resetting to 0");
        tetra::vulkan::init_counter = 0;
        return;
    }

    if (tetra::vulkan::init_counter != 0)
        return;

    if (SDL_GetWindowRelativeMouseMode(vulkan::init_info.window) || SDL_GetWindowMouseGrab(vulkan::init_info.window))
    {
        int x = 0, y = 0;

        SDL_GetWindowSize(vulkan::init_info.window, &x, &y);

        x /= 2;
        y /= 2;

        SDL_WarpMouseInWindow(vulkan::init_info.window, x, y);
        SDL_SetWindowRelativeMouseMode(vulkan::init_info.window, 0);
        SDL_SetWindowMouseGrab(vulkan::init_info.window, 0);
    }

    ImGui::SetCurrentContext(im_ctx_overlay);
    ImGui::GetIO().BackendRendererUserData = nullptr;
    ImGui::GetIO().BackendRendererName = nullptr;
    ImGui::GetIO().BackendPlatformName = nullptr;
    ImGui::GetIO().BackendFlags = 0;
    ImGui::GetPlatformIO().ClearRendererHandlers();
    ImGui::DestroyContext();
    im_ctx_overlay = NULL;

    ImGui::SetCurrentContext(im_ctx_main);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    im_ctx_main = NULL;

    if (gamepad_was_init)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gamepad_was_init = false;
    }
}

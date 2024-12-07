/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2022, 2024 Ian Hangartner <icrashstuff at outlook dot com>
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
#include "overlay_performance.h"
#include "gui_registrar.h"
#include "imgui.h"
#include "tetra/util/convar.h"

static int performance_overlay_show_stack = 0;

static convar_int_t gui_performance_overlay("gui_performance_overlay", true, false, true, "Show performance overlay", CONVAR_FLAG_INT_IS_BOOL);

void overlay::performance::push() { performance_overlay_show_stack++; }

static float average_loop_time = 0.0;

#define NUM_LOOP_TIMES 64
void overlay::performance::calculate(float last_loop_time)
{
    ImGuiIO io = ImGui::GetIO();
    static float loop_times[NUM_LOOP_TIMES];
    static int loop_times_pos = 0;
    static int loop_times_fill = 0;
    loop_times[loop_times_pos] = last_loop_time;
    loop_times_pos = (loop_times_pos + 1) % NUM_LOOP_TIMES;
    if (loop_times_fill < NUM_LOOP_TIMES)
        loop_times_fill++;
    average_loop_time = 0.0;
    for (int i = 0; i < loop_times_fill; i++)
        average_loop_time += loop_times[i];
    average_loop_time /= loop_times_fill;
    if (average_loop_time < 0.0)
        average_loop_time = 0.0;
}
#undef NUM_LOOP_TIMES

/**
 * For some reason the loop usage calculation doesn't work when vsync is enabled
 */
static bool window_performance_overlay()
{
    bool show = performance_overlay_show_stack > 0 || gui_performance_overlay.get();
    if (show)
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration;
        window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
        window_flags |= ImGuiWindowFlags_NoSavedSettings;
        window_flags |= ImGuiWindowFlags_NoFocusOnAppearing;
        window_flags |= ImGuiWindowFlags_NoNav;
        window_flags |= ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoInputs;

        ImGuiIO io = ImGui::GetIO();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImVec2 pos = viewport->WorkPos;
        pos.x += viewport->WorkSize.x;

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        if (ImGui::BeginCVR("Performance Overlay", NULL, window_flags))
        {
            float percentage = average_loop_time * io.Framerate / 10.0;
            if (percentage > 100.0)
                percentage = 100.0;
            ImGui::Text("%02.0f FPS (%02.0f%%)", io.Framerate, percentage);
            ImGui::End();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }
    performance_overlay_show_stack = 0;

    return show;
}

static gui_register_menu reg_perf_overlay(window_performance_overlay);

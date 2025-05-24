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
#include "task_timer.h"

#include "tetra/gui/imgui.h"

#include <SDL3/SDL.h>

void task_timer_t::start()
{
    SDL_assert(!recording);
    current_start = SDL_GetTicksNS();
    recording = true;
}

void task_timer_t::cancel()
{
    SDL_assert(recording);
    recording = false;
}

void task_timer_t::finish()
{
    SDL_assert(recording);
    if (!recording)
        return;
    recording = false;
    push_back(SDL_GetTicksNS() - current_start);
}

void task_timer_t::push_back(const Uint64 elapsed_nanoseconds)
{
    double elapsed = (elapsed_nanoseconds / Uint64(100)) / 10000.0;
    accumulator_pos = (accumulator_pos + 1) % average_window_size;

    accumulator += elapsed / double(average_window_size);

    if (accumulator_pos != 0)
        return;

    data_pos = (data_pos + 1) % IM_ARRAYSIZE(data);
    data[data_pos] = accumulator;

    accumulator = 0;
}

void task_timer_t::scoped_task_t::finish()
{
    if (parent)
        parent->finish();
    parent = nullptr;
}

void task_timer_t::scoped_task_t::cancel()
{
    if (parent)
        parent->cancel();
    parent = nullptr;
}

task_timer_t::scoped_task_t::~scoped_task_t() { finish(); }

task_timer_t::scoped_task_t task_timer_t::start_scoped()
{
    start();
    return task_timer_t::scoped_task_t { this };
}

void task_timer_t::draw_imgui()
{
    ImVec2 graph_size(SDL_max(200, ImGui::GetContentRegionAvail().x), 120);

    ImGui::PushID(this);

    float sorted[IM_ARRAYSIZE(data)];

    if (ImGui::IsRectVisible(graph_size))
    {
        memcpy(sorted, data, sizeof(data));

        SDL_qsort(sorted, SDL_arraysize(sorted), sizeof(*sorted), [](const void* _a, const void* _b) -> int {
            const float a = *(float*)_a;
            const float b = *(float*)_b;

            if (a < b)
                return -1;
            if (a > b)
                return 1;
            return 0;
        });
    }

    float min = 0;
    /* Prevent stray lines from blowing out the graph as much */
    float max = sorted[SDL_arraysize(sorted) - SDL_arraysize(sorted) / 64];

    ImGui::PlotLines("##Frametimes", data, IM_ARRAYSIZE(data), data_pos, name, min, max, graph_size);

    ImGui::PopID();
}

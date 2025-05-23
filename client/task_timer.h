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
#pragma once

#include <SDL3/SDL_stdinc.h>

/**
 * Basic task timer for profiling, because why use someone else's tools when you can write your own :)
 *
 * Thread-Safety
 * It is not safe to access an instance from multiple threads at once
 * It is safe to access different instances from multiple threads at once
 */
struct task_timer_t
{
    /**
     * @param _name Name to use in draw_imgui(), (NULL for none)
     * @param _average_window_size Number of elements to average together before pushing data back
     */
    task_timer_t(const char* _name = NULL, const Uint32 _average_window_size = 2) { name = _name, average_window_size = SDL_max(1, _average_window_size); }

    /** Start recording a task */
    void start();
    /** Finish the current recording and push the elapsed time back */
    void finish();
    /** Cancel the current recording and discard the elapsed time */
    void cancel();

    struct scoped_task_t
    {
        /** Will be set to null the task if finished/canceled */
        task_timer_t* parent = nullptr;
        /** Calls parent->finish() */
        void finish();
        /** Calls parent->cancel() */
        void cancel();
        /** Calls parent->finish() if the task is still running */
        ~scoped_task_t();
    };

    /**
     * Start a scoped task
     *
     * WARNING: You must not stop the task from the provided handle
     */
    scoped_task_t start_scoped();

    /** Directly push a time into the buffer */
    void push_back(const Uint64 elapsed_nanoseconds);

    void draw_imgui();

    int data_pos = 0;
    float data[1024] {};

private:
    Uint32 average_window_size = 1;
    const char* name = NULL;

    double accumulator = 0.0;
    Uint32 accumulator_pos = 0;

    Uint64 current_start = 0;
    bool recording = 0;
};

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
#include "misc.h"

#include <SDL3/SDL.h>
#include <limits.h>

std::string format_memory(const size_t size, const bool rate)
{
    char buf[128];

    if (size < 1000u)
        snprintf(buf, ARR_SIZE(buf), "%zu bytes%s", size, rate ? "/s" : "");
    else if (size < (1000u * 1000u))
        snprintf(buf, ARR_SIZE(buf), "%.1f KB%s", (float)size / 1000.0f, rate ? "/s" : "");
    else if (size < (1000u * 1000u * 1000u))
        snprintf(buf, ARR_SIZE(buf), "%.2f MB%s", (float)(size / 1000u) / 1000.0f, rate ? "/s" : "");
    else if (size < (1000u * 1000u * 1000u * 1000u))
        snprintf(buf, ARR_SIZE(buf), "%.2f GB%s", (float)(size / (1000u * 1000u)) / 1000.0f, rate ? "/s" : "");
    else
        snprintf(buf, ARR_SIZE(buf), "%.2f TB%s", (float)(size / (1000u * 1000u * 1000u)) / 1000.0f, rate ? "/s" : "");

    return buf;
}

bool argv_from_str(std::vector<std::string>& argv, const std::string cmdline, const bool parse_quotes, const size_t max_argc)
{
    size_t pos = 0;

    argv.clear();

    bool in_quote = 0;

    argv.push_back("");

    char last = 0;

    while (pos < cmdline.length() && argv.size() < max_argc)
    {
        if (parse_quotes && cmdline[pos] == '"')
            in_quote = !in_quote;
        else if (cmdline[pos] == ' ' && !in_quote)
        {
            if (last != ' ')
                argv.push_back("");
        }
        else
        {
            argv[argv.size() - 1].append(1, cmdline[pos]);
        }
        last = cmdline[pos];
        pos++;
    }

    while (pos < cmdline.length())
        argv[argv.size() - 1].append(1, cmdline[pos++]);

    if (!argv[argv.size() - 1].length() && argv.size())
        argv.resize(argv.size() - 1);

    return in_quote == 0;
}

bool long_from_str(const std::string str, long& out)
{
    const char* s = str.c_str();
    errno = 0;
    char* endptr;
    long v = strtol(s, &endptr, 10);
    if (endptr == s || *endptr != '\0' || ((v == LONG_MIN || v == LONG_MAX) && errno == ERANGE))
        return 0;

    out = v;
    return 1;
}

bool int_from_str(const std::string str, int& out)
{
    long o;
    int r = long_from_str(str, o);
    if (r)
        out = o;
    return r;
}

void util::parallel_for(const int start, const int end, std::function<void(const int start, const int end)> func)
{
    /** Maximum number of threads available (Leave one thread alone for the system) */
    const int max_new_threads = SDL_GetNumLogicalCPUCores() - 1;

    assert(start <= end);

    /** Number of jobs to spawn */
    const int num_jobs = end - start;

    /** If we only have one thread available or only one job to do, then there is no point continuing */
    if (max_new_threads < 2 || num_jobs == 1)
    {
        func(start, end);
        return;
    }

    int quotient = num_jobs / max_new_threads;
    int remainder = num_jobs % max_new_threads;

    TRACE("Min jobs per thread: %d, Remaining jobs to unequally distribute: %d", quotient, remainder);

    struct thread_data_t
    {
        SDL_Thread* thread = nullptr;
        std::function<void(const int start, const int end)> func;
        int start = 0;
        int end = 0;
    };

    std::vector<thread_data_t> tdata;

    for (int job_it = start; job_it < end;)
    {
        int inc = quotient + (remainder-- > 0);
        thread_data_t dat;
        dat.func = func;
        dat.start = job_it;
        dat.end = job_it + inc;
        job_it = dat.end;
        dat.end = SDL_min(dat.end, end);

        assert(dat.start >= start);
        assert(dat.end <= end);

        tdata.push_back(dat);
    }

    int num_jobs_check = 0;
    for (thread_data_t thread : tdata)
        num_jobs_check += (thread.end - thread.start);
    assert(num_jobs_check == num_jobs);
    assert(int(tdata.size()) <= max_new_threads);

    SDL_ThreadFunction thread_func = [](void* data) -> int {
        thread_data_t* tdata = (thread_data_t*)data;
        TRACE("Thread: for(int i = %d; i < %d; i++) { do_something(); }", tdata->start, tdata->end);
        tdata->func(tdata->start, tdata->end);
        return 0;
    };

    for (size_t i = 1; i < tdata.size(); i++)
        tdata[i].thread = SDL_CreateThread(thread_func, "parallel_for", &tdata[i]);

    /* Execute job assigned to the main thread and any job that a thread was not created for */
    for (thread_data_t thread : tdata)
        if (!thread.thread)
            thread_func(&thread);

    /* Synchronize and cleanup */
    for (thread_data_t thread : tdata)
        SDL_WaitThread(thread.thread, NULL);
}

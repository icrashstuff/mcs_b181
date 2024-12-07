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
#include "gui_registrar.h"

#include <vector>

static void add_to_vector(std::vector<bool (*)()>* vec, bool (*func)())
{
    for (std::size_t i = 0; i < vec->size(); i++)
        if (vec->at(i) == func)
            return;
    vec->push_back(func);
}

static bool render_vector(std::vector<bool (*)()>* vec)
{
    std::size_t change = 0;
    for (std::size_t i = 0; i < vec->size(); i++)
        change += vec->at(i)();
    return change;
}

/**
 * This is a workaround for undefined behavior
 */
static inline std::vector<bool (*)()>* get_vector(int num)
{
    static std::vector<bool (*)()> vectors[2];
    return &vectors[num];
}

void gui_registrar::add_overlay(bool (*func)()) { add_to_vector(get_vector(0), func); }

bool gui_registrar::render_overlays() { return render_vector(get_vector(0)); }

void gui_registrar::add_menu(bool (*func)()) { add_to_vector(get_vector(1), func); }

bool gui_registrar::render_menus() { return render_vector(get_vector(1)); }

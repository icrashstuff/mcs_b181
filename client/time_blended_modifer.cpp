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

#include "time_blended_modifer.h"

#include <SDL3/SDL.h>

#define MIX(a, b, factor) ((1.0 - SDL_clamp(factor, 0.0, 1.0)) * a + SDL_clamp(factor, 0.0, 1.0) * b)
float time_blended_modifer_t::get_modifier()
{
    Uint64 diff = SDL_GetTicks() - time_of_change;

    if (diff > dur_of_change || time_of_change == 0)
        return (use) ? max : min;

    double mix = double(diff) / double(dur_of_change);

    return ((use) ? MIX(last, max, mix) : MIX(last, min, mix));
}

void time_blended_modifer_t::set_use(bool x)
{
    if (x == use)
        return;
    last = get_modifier();
    use = x;
    time_of_change = SDL_GetTicks();
}

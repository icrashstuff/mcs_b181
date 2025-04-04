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

#include <SDL3/SDL_stdinc.h>

class time_blended_modifer_t
{
    /** Time for a change to take effect in milliseconds */
    Uint64 dur_of_change = 0;

    float min;
    float max;
    float last;
    bool use;

    Uint64 time_of_change = 0;

public:
    /**
     * Create a time blended modifier
     *
     * @param _dur_of_change Time in milliseconds that a blend will last for
     * @param _min Target value of the modifier when time_blended_modifer_t::use == 0
     * @param _max Target value of the modifier when time_blended_modifer_t::use == 1
     * @param _use Initial value of time_blended_modifer_t::use
     */
    time_blended_modifer_t(Uint64 _dur_of_change, float _min, float _max, bool _use) { dur_of_change = _dur_of_change, min = _min, max = _max, use = _use; }

    /**
     * Get the current value of the modifier
     *
     * If a blend is in progress then the returned value will be a blend of time_blended_modifer_t::min and time_blended_modifer_t::max
     */
    float get_modifier();

    /**
     * Set the value of time_blended_modifer_t::use
     *
     * If the new value differs from the old, then a blend will be initiated for the duration of time_blended_modifer_t::dur_of_change
     *
     * @param new_use New value for time_blended_modifer_t::use
     */
    void set_use(bool new_use);
};

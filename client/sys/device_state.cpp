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

#include "device_state.h"

#define ADD_SWITCH_RETURN(CASE, RET) \
    case CASE:                       \
        return RET

const char* device_state::thermal_state_to_string(const device_state::thermal_state_t x)
{
    switch (x)
    {
        ADD_SWITCH_RETURN(THERMAL_STATE_DESKTOP, "Desktop");
        ADD_SWITCH_RETURN(THERMAL_STATE_MOBILE_NOMINAL, "Nominal");
        ADD_SWITCH_RETURN(THERMAL_STATE_MOBILE_FAIR, "Fair");
        ADD_SWITCH_RETURN(THERMAL_STATE_MOBILE_SERIOUS, "Serious");
        ADD_SWITCH_RETURN(THERMAL_STATE_MOBILE_CRITICAL, "Critical");
    default:
        return "Unknown";
    }
}

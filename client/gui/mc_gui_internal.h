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

#ifndef MCS_B181_CLIENT_MC_GUI_INTERNAL_H_INCLUDED
#define MCS_B181_CLIENT_MC_GUI_INTERNAL_H_INCLUDED

#include "tetra/gui/imgui_internal.h"

namespace mc_gui
{
/**
 * This is only used for rendering button widgets
 */
IMGUI_API void render_widget(const ImVec2 p_min, const ImVec2 p_max, const ImTextureID id, const ImVec2 uv_min, const ImVec2 uv_max, const ImVec2 tex_size);

/**
 * Wrapper around ImGui::RenderTextClipped(), that renders text with a shadow
 */
void render_text_clipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known,
    const ImVec2& align, const ImRect* clip_rect);
}

#endif
